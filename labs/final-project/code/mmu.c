/*
    b4-6: before mmu enabled, the icache should be disabled and invalidated.
     icache can then be enabled at the same time as the mmu is enabled.
     clean_inv_caches();

    b4-7: strongly recommended that the code which enables or disables the 
     mmu has identical virtual and physical addresses.
*/
#include "rpi.h"
#include "mmu.h"
#include "cp15-arm.h"
#include "helper-macros.h"

static fld_t fld_mk(void) {
    fld_t f;
    // all unused fields can have 0 as default.
    memset(&f, 0, sizeof f);
    f.tag = 0b10;
    return f;
}

static void fld_check_valid(fld_t *f) {
    assert(f->_sbz1 == 0);
    assert(f->tag == 0b10);
    assert(f->S == 0);
    assert(f->IMP == 0);
    assert(f->C == 0);
    assert(f->B == 0);
    assert(f->super == 0);
}

static void fld_check(void) {
    fld_t f = fld_mk();
    fld_check_valid(&f);

    AssertNow(sizeof f == 4);

    //                    field     offset  nbits
    check_bitfield(fld_t, tag,      0,      2);
    check_bitfield(fld_t, B,        2,      1);
    check_bitfield(fld_t, C,        3,      1);
    check_bitfield(fld_t, XN,       4,      1);
    check_bitfield(fld_t, domain,   5,      4);
    check_bitfield(fld_t, IMP,      9,      1);
    check_bitfield(fld_t, AP,       10,     2);
    check_bitfield(fld_t, TEX,      12,     3);
    check_bitfield(fld_t, APX,      15,     1);
    check_bitfield(fld_t, S,        16,     1);
    check_bitfield(fld_t, nG,       17,     1);
    check_bitfield(fld_t, super,       18,     1);
    check_bitfield(fld_t, sec_base_addr, 20,     12);
}

void fld_print(fld_t *f) {
    printk("------------------------------\n");
    printk("0x%x:\n", f);
    print_field(f, sec_base_addr);
        printk("full base:=0x%8x\n", f->sec_base_addr);
    printk("\t  --> va=0x%8x\n", f->sec_base_addr<<20);
    printk("\t           76543210\n");

    print_field(f, nG);
    print_field(f, S);
    print_field(f, APX);
    print_field(f, TEX);
    print_field(f, AP);
    print_field(f, IMP);
    print_field(f, domain);
    print_field(f, XN);
    print_field(f, C);
    print_field(f, B);
    print_field(f, tag);

    fld_check_valid(f);
}

// Debug function that prints out all the fields of the small page page entry.
void sm_page_desc_print (sm_page_desc_t *pte) {
    printk("------------------------------\n");
    printk("PTE (sm. page) @ 0x%x = [binary] %b\n", pte, *(unsigned*)pte);
    // printk("0x%x:\n", pte);
    print_field(pte, base);
    printk("\t  --> va=0x%8x\n", pte->base << 12);
    printk("\t      bit: 76543210\n");

    print_field(pte, nG);
    print_field(pte, S);
    print_field(pte, APX);
    print_field(pte, TEX);
    print_field(pte, AP);
    print_field(pte, C);
    print_field(pte, B);
    print_field(pte, tag);

    // fld_check_valid(f);
}

// Debug function that prints out all the fields of the large page entry.
void lg_page_desc_print (lg_page_desc_t *pte) {
    printk("------------------------------\n");
    printk("PTE (lg. page) @ 0x%x = [binary] %b\n", pte, *(unsigned*)pte);
    print_field(pte, base);
    printk("translates to --> va=0x%8x\n", pte->base << 16);
    printk("                  bit: 76543210\n");

    print_field(pte, XN);
    print_field(pte, TEX);
    print_field(pte, nG);
    print_field(pte, S);
    print_field(pte, APX);
    print_field(pte, AP);
    print_field(pte, C);
    print_field(pte, B);
    print_field(pte, tag);

    // fld_check_valid(f);
}

// Lookup first level descriptor. Shifting by 20 works because the index starts at bit 20, and 4 bits are skipped at a time via indexing
fld_t *mmu_lookup(fld_t *pt, uint32_t va) {
    assert(is_aligned(va, 20));
    return &pt[va>>20];
}

// create a mapping for <va> to <pa> in the page table <pt>
fld_t *mmu_map_section(fld_t *pt, uint32_t va, uint32_t pa) {
    assert(is_aligned(va, 20));
    assert(is_aligned(pa, 20));

    fld_t *pte = mmu_lookup(pt, va);
    demand(!pte->tag, already set);
    *pte = fld_mk();

    pte->nG = 0;
    pte->APX = 0;
    pte->TEX = 0; 
    pte->AP = 0b11;
    pte->domain = 0;
    pte->XN = 0;
    pte->sec_base_addr = pa >> 20;

    fld_print(pte);
    printk("my.pte@ 0x%x = %b\n", pt, *(unsigned*)pte);
    return pte;
}

// Naive: we assume a single page table, which forces it to be 4096 entries.
// Why 4096:
//      - ARM = 32-bit addresses.
//      - 2^32 = 4GB
//      - we use 1MB sections (i.e., "page size" = 1MB).
//      - there are 4096 1MB sections in 4GB (4GB = 1MB * 4096).
//      - therefore page table must have 4096 entries.
//
// Note: If you want 4k pages, need to use the ARM 2-level page table format.
// These also map 1MB (otherwise hard to mix 1MB sections and 4k pages).
fld_t *mmu_pt_alloc(unsigned sz) {
    demand(sz = 4096, we only handling a single page table right now);

    // first-level page table is 4096 entries.
    fld_t *pt = kmalloc_aligned(4096 * 4, 1<<14);
    printk("pt made at address %x\n", pt); // Test the address, where is it?
    AssertNow(sizeof *pt == 4);
    demand(is_aligned((unsigned)pt, 14), must be 14-bit aligned!);
    return pt;
}



// i think turning caches on after off doesn't need anything special.
void mmu_all_cache_on(void) {
    cp15_ctrl_reg1_t r = cp15_ctrl_reg1_rd();
        // there are some other things too in the aux i believe: see arm1176.pdf
        mmu_dcache_on(r);
        mmu_icache_on(r);
        mmu_write_buffer_on(r); 
        mmu_predict_on(r); 
        mmu_l2cache_on(r);
    cp15_ctrl_reg1_wr(r);

    // i don't think you need to invalidate again?
    cp15_sync();
}

// turning caches off after on may need some careful thinking.
void mmu_all_cache_off(void) {
    cp15_ctrl_reg1_t r = cp15_ctrl_reg1_rd();
    mmu_dcache_off(r);
    mmu_l2cache_off(r);
    mmu_icache_off(r);
    mmu_write_buffer_off(r); 
    mmu_predict_off(r); 

    // think this has to be in assembly since the C code will do some loads and stores
    // in the window b/n disabling the dcache and cleaning an invalidating it, so 
    // there could be a dirty line in the cache that we will not handle right.
    cp15_ctrl_reg1_wr(r);
    cp15_barrier();

    cp15_dcache_clean_inv();
    cp15_icache_inv();
    cp15_sync();
}

void mmu_init(void) { 
    mmu_reset(); 

    struct control_reg1 c = cp15_ctrl_reg1_rd();
    c.XP_pt = 1;
    cp15_ctrl_reg1_wr(c);
}

// verify that interrupts are disabled.
void mmu_enable_set(cp15_ctrl_reg1_t c) {
    assert(c.MMU_enabled);
    mmu_enable_set_asm(c);
}
void mmu_disable_set(cp15_ctrl_reg1_t c) {
    assert(!c.MMU_enabled);
    assert(!c.C_unified_enable);
    mmu_disable_set_asm(c);
}
void mmu_disable(void) {
    cp15_ctrl_reg1_t c = cp15_ctrl_reg1_rd();
    assert(c.MMU_enabled);
    c.MMU_enabled=0;
    c.C_unified_enable = 0;
    mmu_disable_set_asm(c);
}
void mmu_enable(void) {
    cp15_ctrl_reg1_t c = cp15_ctrl_reg1_rd();
    assert(!c.MMU_enabled);
    c.MMU_enabled = 1;
    mmu_enable_set(c);
}

// Functions for creating a new small page
#define COARSE_PAGE_TABLE_TAG 0b01
#define SECTION_TAG 0b10
#define UNUSED_TAG 0b00

// Make a coarse page table.
fld_t mk_coarse_page_table() {
    // Coarse page tables are 1KB in size, with 256 4-byte (32-bit) entries. (Mapping out an entire 1MB section).
    // They have to be 10-bit aligned for the translation base, which is 22 bits
    fld_t *pt = kmalloc_aligned(256 * 4, 0b1<<10); // kmalloc() sets to 0, mind!
    printk("coarse pt made at address %x\n", pt); // Test the address, where is it?
    AssertNow(sizeof *pt == 4);
    
    fld_t f; // The actual fld to return
    memset(&f, 0, sizeof(fld_t)); // Unused fields can have 0 as default

    coarse_pt_desc_t *entry = (coarse_pt_desc_t *)&f; // Cast + indirection to do some work
    entry->tag = 1;
    entry->base = (unsigned)pt >> 10; // Take upper 22 bits (IMPORTANT)
    // entry->domain = 1;
    // TODO: set domain
    printk("coarse_pt_desc_t->base is set to %x\n", entry->base); // Test the address, where is it?
    // assert(f.tag == COARSE_PAGE_TABLE_TAG);
    return f;
}

/* Virtual address to key/index functions */

// From virtual address, get 12-bit large page index, bits [31:20]. See pg. B4-33
uint32_t get_first_level_table_idx(uint32_t va) {
    return va >> 20;
}

// From virtual address, get 8-bit second-level table index, bits [19:12]. See pg. B4-33
uint32_t get_second_level_table_idx(uint32_t va) {
    return (va >> 12) & 0xFF; // 0xFF = 8 0b1's
}

// From virtual address, get 12-bit small page index, bits [11:0]. See pg. B4-31
uint32_t get_sm_page_idx(uint32_t va) {
    return va & 0xFFF; // 12 0b1's
}

// From virtual address, get 16-bit large page index, bits [15:0]. See pg. B4-31
uint32_t get_lg_page_idx(uint32_t va) {
    return va & 0xFFFF; // 16 0b1's
}

// Lookup first level descriptor. 
// We use the first_level_table_idx to directly index the array (skipping over by 
// sizeof(fld_t) = 4 bytes each index).
fld_t *mmu_first_level_lookup(fld_t *pt, uint32_t va) {
    return &pt[get_first_level_table_idx(va)];
}

// Retrieve the second level descriptor/page table entry associated with the page directory entry, offset 
// by the second-level table index portion of the virtual address.
void *mmu_second_level_lookup(void *pde, uint32_t va) {
    // Create a pointer to the start of the coarse page table
    sld_t *cpt = (sld_t *)(((coarse_pt_desc_t *)pde)->base << 10); // TODO: define 10 offset
    return &cpt[get_second_level_table_idx(va)];
}

#define SMALL_PAGE_BIT_1 0b1
#define LARGE_PAGE_BIT_1 0b0

#define LARGE_PAGE_TAG 0b1

// Create and return a small page table entry.
sm_page_desc_t mk_sm_page() {
    sm_page_desc_t pte;
    memset(&pte, 0, sizeof(sm_page_desc_t));
    // The tag is set as a 1-bit field and read as a 2-bit one, pg. B4-31
    pte.tag = SMALL_PAGE_BIT_1; 
    return pte;
}

// Create and return a large page table entry.
lg_page_desc_t mk_lg_page() {
    lg_page_desc_t pte;
    memset(&pte, 0, sizeof(lg_page_desc_t));
    pte.tag = LARGE_PAGE_TAG;
    return pte;
}

/*
 * function: map a small page in virtual memory
 * ---
 * Maps a small (4KB) page in VM. 
 * 
 * @param pt: The page table
 * @param va: The virtual address to be mapped
 * @param pa: The physical address to map to
 * @return: A generic second-level descriptor (sld_t *); the small page table entry
 */
sld_t *mmu_map_sm_page(fld_t *pt, uint32_t va, uint32_t pa) {
    printk("Welcome to my small mapper!\n");

    // Small pages map out 2^12 bytes (4KB) of memory. Make sure addresses are aligned.
    assert(is_aligned(va, 12));
    assert(is_aligned(pa, 12));

    // First-level descriptor/page directory entry
    fld_t *pde = mmu_first_level_lookup(pt, va);

    assert(pde->tag == UNUSED_TAG || pde->tag == COARSE_PAGE_TABLE_TAG);

    // If fld is unused, allocate a coarse table
    if (pde->tag == UNUSED_TAG) {
        *pde = mk_coarse_page_table();
        printk("my pde @ 0x%x = %b\n", pde, *(unsigned*)pde);
        // return (sld_t *)pde;
        // Note: making a normal section works okay...
        // *pde = fld_mk();
        // pde->sec_base_addr = pa >> 20;
        // return (sld_t *)pde;
    }

    // Navigate to small page entry; make small page entry
    sm_page_desc_t *pte = mmu_second_level_lookup(pde, va);
    
    assert(pte->tag == UNUSED_TAG);
    *pte = mk_sm_page();

    pte->nG = 0;
    pte->APX = 0;
    pte->TEX = 0; 
    pte->AP = 0b11;
    pte->XN = 0;
    pte->base = pa >> 12; // Base address is upper 20 bits

    // TODO: make a print pte function
    sm_page_desc_print(pte);
    // fld_print(pte);
    // printk("my.pte@ 0x%x = %b\n", pt, *(unsigned*)pte);

    return (sld_t *)pte;
}

// Large pages need to be replicated 16 times, and there are 256 entries in a coarse page table.
// We shouldn't allocate a large page at cpt[256 - 16 = 240] onwards, or we'll bleed past the end.
#define MAX_LARGE_PAGE_IDX 240

/*
 * function: map a large page in virtual memory
 * ---
 * Maps a large (64KB) page in VM. Large pages are replicated 16x in a coarse page 
 * table for the hardware lookup (see pg. B4-31, the low order bits of the second-level
 * table index overlap with the page index, so we want any variation of those bits to
 * correspond with a single large page entry).
 * 
 * @param pt: The page table
 * @param va: The virtual address to be mapped
 * @param pa: The physical address to map to
 * @return: A generic second-level descriptor (sld_t *); the large page table entry
 */
sld_t *mmu_map_lg_page(fld_t *pt, uint32_t va, uint32_t pa) {
    // Large pages map out 2^16 bytes (16KB) of memory. Make sure addresses are aligned.
    assert(is_aligned(va, 16));
    assert(is_aligned(pa, 16));

    // Avoid allocating large pages towards the end of a coarse page table. A compromise;
    // could track large pages that bleed across multiple coarse page tables if we wanted.
    assert(get_second_level_table_idx(va) < MAX_LARGE_PAGE_IDX);

    // Grab the first-level descriptor/page directory entry (PDE)
    fld_t *pde = mmu_first_level_lookup(pt, va);

    assert(pde->tag == UNUSED_TAG || pde->tag == COARSE_PAGE_TABLE_TAG);

    // If fld is unused, allocate a coarse table
    if (pde->tag == UNUSED_TAG) {
        *pde = mk_coarse_page_table();
        // printk("my pde @ 0x%x = %b\n", pde, *(unsigned*)pde);
    }

    // Navigate to small page entry; make small page entry
    lg_page_desc_t *pte = mmu_second_level_lookup(pde, va);
    
    for (int i = 0; i < 16; i++) assert(pte[i].tag == UNUSED_TAG);
    for (int i = 0; i < 16; i++) {
        pte[i] = mk_lg_page();

        pte[i].XN = 0;
        pte[i].TEX = 0; 
        pte[i].nG = 0;
        pte[i].S = 0;
        pte[i].APX = 0;
        pte[i].AP = 0b11;
        pte[i].base = pa >> 16; // Base address is upper 16 bits
    }

    lg_page_desc_print(pte);

    return (sld_t *)pte;
}

#define MMUTABLEBASE 0x304000
unsigned int mmu_small ( unsigned int vadd, unsigned int padd, unsigned int flags, unsigned int mmubase )
{
    unsigned int ra;
    unsigned int rb;
    unsigned int rc;

    ra=vadd>>20;
    rb=MMUTABLEBASE|(ra<<2);
    rc=(mmubase&0xFFFFFC00)/*|(domain<<5)*/|1;
    //hexstrings(rb); hexstring(rc);
    PUT32(rb,rc); //first level descriptor
    coarse_pt_desc_t *p = (coarse_pt_desc_t *)rb;
    printk("field: 0x%x\n", p->base);
    printk("my.pte @ 0x%x = %b\n", rb, rc);
    // ra=(vadd>>12)&0xFF;
    // rb=(mmubase&0xFFFFFC00)|(ra<<2);
    // rc=(padd&0xFFFFF000)|(0xFF0)|flags|2;
    // //hexstrings(rb); hexstring(rc);
    // PUT32(rb,rc); //second level descriptor
    return(0);
}
