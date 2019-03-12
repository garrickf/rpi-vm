/* simple fat32 read-only fs. */
#include "rpi.h"
#include "fat32.h"
#include "pi-fs.h"
#include "helper-macros.h"
#include "bzt-sd.h" // Include the code we copped (part 1)

// Constants for values we know
#define BYTES_PER_SECTOR 512

// allocate <nsec> worth of space, read in from SD card, return pointer.
// lba: logical block address
void *sec_read(uint32_t lba, uint32_t nsec) {
    printk("sec_read: lba: %x, nsec: %d\n", lba, nsec);
    // Malloc the buffer, baby
    unsigned char *buf = kmalloc(nsec * BYTES_PER_SECTOR);
    // Pass in nsec ONLY
    sd_readblock(lba, buf, nsec); // Only one-shot because start up cost and complexity a lot (not like read, failure is worse)
    return buf;
}

// there are other tags right?
static int is_fat32(int t) { return t == 0xb || t == 0xc; }

typedef struct fat32 {
    uint32_t fat_begin_lba,
            cluster_begin_lba,
            sectors_per_cluster,
            root_dir_first_cluster;
    uint32_t *fat;
    uint32_t n_fat;
} fat32_fs_t;

/*
    Field                       Microsoft's Name    Offset   Size        Value
    Bytes Per Sector            BPB_BytsPerSec      0x0B(11) 16 Bits     Always 512 Bytes
    Sectors Per Cluster         BPB_SecPerClus      0x0D(13) 8 Bits      1,2,4,8,16,32,64,128
    Number of Reserved Sectors  BPB_RsvdSecCnt      0x0E(14) 16 Bits     Usually 0x20
    Number of FATs              BPB_NumFATs         0x10(16) 8 Bits      Always 2
    Sectors Per FAT             BPB_FATSz32         0x24(36) 32 Bits     Depends on disk size
    Root Directory First ClusterBPB_RootClus        0x2C(44) 32 Bits     Usually 0x00000002
    Signature                   (none)              0x1FE(510)16 Bits     Always 0xAA55
*/
static struct fat32 fat32_mk(uint32_t lba_start, fat32_boot_sec_t *b) {
    struct fat32 fs;

    fs.fat_begin_lba = lba_start + b->reserved_area_nsec;
    printk("begin lba = %d\n", fs.fat_begin_lba);
    
    fs.cluster_begin_lba = fs.fat_begin_lba + b->nsec_per_fat * b->nfats;
    fs.sectors_per_cluster = b->sec_per_cluster;
    fs.root_dir_first_cluster = b->first_cluster;

    fs.fat = sec_read(fs.fat_begin_lba, b->nfats * b->nsec_per_fat); // Malloc the thing
    fs.n_fat = b->nfats;
    return fs;
}

// given cluster_number get lba
uint32_t cluster_to_lba(struct fat32 *f, uint32_t cluster_num) {
    return f->cluster_begin_lba + (cluster_num - 2) * f->sectors_per_cluster;
}

int fat32_dir_cnt_entries(fat32_dir_t *d, int n) {
    int cnt = 0;
    for(int i = 0; i < n; i++, d++)
        if(!fat32_dirent_free(d) && d->attr != FAT32_LONG_FILE_NAME)
                cnt++;
    return cnt;
}

// translate fat32 directories to dirents.
// pi_dir_t fat32_to_dirent(fat32_fs_t *fs, fat32_dir_t *d, uint32_t n) {
//     pi_dir_t p;
//     p.n = fat32_dir_cnt_entries(d,n);
//     p.dirs = kmalloc(p.n * sizeof *p.dirs); 

//     unimplemented();
//     return p;
// }
// fat32_dir_t is in fat32.h
// dirent_t is in pi-fs.h
pi_dir_t fat32_to_dirent(fat32_fs_t *fs, fat32_dir_t *d, uint32_t n) {
    pi_dir_t p;

    // macos
    p.n = fat32_dir_cnt_entries(d,n);
    p.dirs = kmalloc(p.n * sizeof *p.dirs);

    int used = 0;
    fat32_dir_t *end = d+n;
    for(; d < end; d++) {
        if(!fat32_dirent_free(d)) {
            dirent_t *de = &p.dirs[used++];
            assert(used <= p.n);

            d = fat32_dir_filename(de->name, d, end);
            // Add more information to the struct
            uint32_t cluster_id = (d->hi_start << 4 * sizeof(uint16_t)) | d->lo_start;
            de->cluster_id = cluster_id;
            de->nbytes = d->file_nbytes;
        }
    }
    return p;
}

// read in an entire file.  note: make sure you test both on short files (one cluster)
// and larger ones (more than one cluster).
pi_file_t fat32_read_file(fat32_fs_t *fs, dirent_t *d) {
    // the first cluster.
    uint32_t id = d->cluster_id;
    pi_file_t f;
    
    // compute how many clusters in the file.
    //      note: you know you are at the end of a file when 
    //          fat32_fat_entry_type(fat[id]) == LAST_CLUSTER.
    // compute how many sectors in cluster.
    // allocate this many bytes.
    // *** Begin my code ***
    f.n_data = d->nbytes;
    f.n_alloc = roundup(f.n_data, BYTES_PER_SECTOR);

    int totalSectors = f.n_alloc / BYTES_PER_SECTOR;
    int sectorsRead = 0;
    printk("cluster id: %d, ndata: %d, nalloc: %d\n totalSectors: %d", id, f.n_data, f.n_alloc, totalSectors);
    
    unsigned char *buf = kmalloc(f.n_alloc); // Alloc this much space

    while (sectorsRead < totalSectors) {
        // Read up to fs->sectors_per_cluster from the cluster
        int sectorsToRead = totalSectors - sectorsRead < fs->sectors_per_cluster ? 
            totalSectors - sectorsRead : 
            fs->sectors_per_cluster;
        
        sectorsRead += sd_readblock(cluster_to_lba(fs, id), 
            buf + sectorsRead * BYTES_PER_SECTOR, 
            sectorsToRead) / BYTES_PER_SECTOR; // sd_readblock returns bytes read, divide by BYTES_PER_SECTOR
        
        id = fs->fat[id];
        if (fat32_fat_entry_type(id) == LAST_CLUSTER) { // At end of cluster chain
            assert(sectorsRead == totalSectors);
            break;
        } else { // Trudge on
            assert(sectorsRead < totalSectors);
        }
    }

    // Old, simpler case: assume 1 sector file, read from the first cluster only
    // sd_readblock(cluster_to_lba(fs, id), buf, 1);

    f.data = (char *)buf;

    return f;
}

dirent_t *dir_lookup(pi_dir_t *d, char *name) {
    for(int i = 0; i < d->n; i++) {
        dirent_t *e = &d->dirs[i];
        if(strcmp(name, e->name) == 0)
            return e;
    }
    return 0;
}

void notmain() {
    uart_init();

    // Part1
    //    1. adapt bzt's sd card driver.
    //    2. write: pi_sd_init(), pi_sd_read,  sec_read.
    sd_init();
    unsigned char *data = sec_read(0, 1); // Read at lba 0, 1 sector
    printk("\n*** Part 1 ***\n\n> Last two bytes of block 0 should be 55aa: %x%x\n", 
        data[BYTES_PER_SECTOR - 2], 
        data[BYTES_PER_SECTOR - 1]);

    printk("\n*** Part 2 ***\n\n");
#define PART2
#ifdef PART2
    // PART2:
    //    1. define the master boot record.
    //    2. read in.
    //    3. pull out the partition and check it.
    // in particular, write the structures in fat32.h so that the following
    // code works and compiles:

    struct mbr *mbr = sec_read(0, 1);
    fat32_mbr_check(mbr);

    struct partition_entry p;
    memcpy(&p, mbr->part_tab1, sizeof p);
    fat32_partition_print("partition 1", &p);

    // fat32
    assert(is_fat32(p.part_type));
    assert(!fat32_partition_empty((uint8_t*)mbr->part_tab1));
    assert(fat32_partition_empty((uint8_t*)mbr->part_tab2));
    assert(fat32_partition_empty((uint8_t*)mbr->part_tab3));
    assert(fat32_partition_empty((uint8_t*)mbr->part_tab4));
#endif

    printk("\n*** Part 3 ***\n\n");
#define PART3
#ifdef PART3
    /*
        https://www.pjrc.com/tech/8051/ide/fat32.html
        The first step to reading the FAT32 filesystem is the read its
        first sector, called the Volume ID. The Volume ID is read using
        the LBA Begin address found from the partition table. From this
        sector, you will extract information that tells you everything
        you need to know about the physical layout of the FAT32 filesystem

        this is our <fat32_boot_sec_t>
    */

    fat32_boot_sec_t *b = 0;
    b = sec_read(p.lba_start, 1); // Check what p was in part 2

    fat32_volume_id_check(b);
    fat32_volume_id_print("boot sector", b);

    // bonus: read in the file info structure
    struct fsinfo *info = sec_read(p.lba_start+1, 1);
    fat32_fsinfo_check(info);
    fat32_fsinfo_print("info struct", info);

    /* 
        The disk is divided into clusters. The number of sectors per
        cluster is given in the boot sector byte 13. <sec_per_cluster>

        The File Allocation Table has one entry per cluster. This entry
        uses 12, 16 or 28 bits for FAT12, FAT16 and FAT32.
    */

    // implement this routine.
    struct fat32 fs = fat32_mk(p.lba_start, b);

    // read in the both copies of the FAT.
    printk("\n> Finished making the fat32 struct. Now compare the sectors. \n\n");
    uint32_t *fat = 0, *fat2 = 0;
    fat = fs.fat;
    fat2 = (uint32_t *)((char *)(fs.fat) + b->nsec_per_fat * BYTES_PER_SECTOR);
    int n_bytes = b->nsec_per_fat * BYTES_PER_SECTOR;

    // Try a rough alloc
    // fat = sec_read(fs.fat_begin_lba, 100);
    // fat2 = sec_read(fs.fat_begin_lba + b->nsec_per_fat, 100);
    // int n_bytes = 100 * BYTES_PER_SECTOR;    

    // check that they are the same.
    assert(memcmp(fat, fat2, n_bytes) == 0);
#endif

    printk("\n*** Part 4 ***\n\n");
#define PART4
#ifdef PART4
    int type = fat32_fat_entry_type(fat[2]);
    printk("fat[2] = %x, type=%s\n", fat[2], fat32_fat_entry_type_str(type));

    printk("lba.start=%d\n", p.lba_start);
    printk("cluster 2 to lba = %d\n", cluster_to_lba(&fs, 2));
    unsigned dir_lba = cluster_to_lba(&fs, b->first_cluster);
    printk("rood dir first cluster = %d\n", dir_lba);

    assert (type == LAST_CLUSTER); // See fat32.h

    // calculate the number of directory entries. Assume directory fits in a single cluster.
    uint32_t dir_n = b->sec_per_cluster * NDIR_PER_SEC;

    // read in the directories.
    // dir_nsecs is the number of sectors to read to get all of the dirents
    uint32_t dir_nsecs = b->sec_per_cluster;
    fat32_dir_t *dirs = sec_read(fs.cluster_begin_lba, dir_nsecs);

    // this should just work.
    for(int i = 0; i < dir_n; ) {
        if(fat32_dirent_free(dirs+i)) {
            i++;
        } else {
            printk("dir %d is not free!:", i);
            i += fat32_dir_print("", dirs+i, dir_n - i);
        } 
    }

    pi_dir_t pdir = fat32_to_dirent(&fs, dirs, dir_n);

    // print out the contents of the directory.  this should make sense!
    for(int i = 0; i < pdir.n; i++) {
        dirent_t *e = &pdir.dirs[i];
        printk("\t%s\t\t->\tcluster id=%d, type=%s, nbytes=%d\n", 
                    e->name, e->cluster_id, e->is_dir_p?"dir":"file", e->nbytes);
    }
#endif

    printk("\n*** Part 5 ***\n\n");
#define PART5 1
#ifdef PART5
    // this should succeed, and print the contents of config.
    dirent_t *e =dir_lookup(&pdir, "config.txt");
    assert(e);
    printk("FOUND: \t%s\t\t->\tcluster id=%x, type=%s, nbytes=%d\n", 
            e->name, e->cluster_id, e->is_dir_p?"dir":"file", e->nbytes);

    printk("config.txt:\n");
    printk("---------------------------------------------------------\n");
    pi_file_t f = fat32_read_file(&fs, e);
    for(int i = 0; i < f.n_data; i++)
        printk("%c", f.data[i]);
    printk("---------------------------------------------------------\n");
#endif

    printk("\n*** Part 6 ***\n\n");
    // We already have pdir from the 
    dirent_t *ent =dir_lookup(&pdir, "hello-fixed.0x100000f0.bin");
    assert(ent);
    printk("FOUND: \t%s\t\t->\tcluster id=%x, type=%s, nbytes=%d\n", 
            ent->name, ent->cluster_id, ent->is_dir_p?"dir":"file", ent->nbytes);
    
    printk("opening file...\n");
    pi_file_t file = fat32_read_file(&fs, ent);
    // Seems to be printing out okay.
    // for(int i = 0; i < file.n_data; i++)
    //     printk("%c", file.data[i]);
    memcpy((void *)0x100000f0, file.data, file.n_data); // Trash the address where we'll mount the executable!
    BRANCHTO(0x100000f0);
    // Our program will come back here!
    clean_reboot();
}
