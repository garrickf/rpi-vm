#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "demand.h"
#include "../shared-code/simple-boot.h"
#include "support.h"

// read entire file into buffer.  return it, write total bytes to <size>
unsigned char *read_file(int *size, const char *name) {
	printf("read_file: want to open %s\n", name); // DEBUG
  int fd = open(name, O_RDONLY); // g: open file for reading

  // To get the file size, we use the stat() function
  struct stat st;
  if (stat(name, &st) == -1) panic("read_file: stat() failed");
  size_t nBytes = st.st_size;
  size_t bufsize = nBytes + 1; // Extra padding TODO: do I need this?
  // printf("buffer size, stat: %zu\n", bufsz); // DEBUG: Print buffer size
 
  printf("read_file: %zu bytes adj., padding to %zu\n", bufsize, bufsize + (bufsize % 4 != 0 ? 4 - bufsize % 4 : 0)); // DEBUG
  bufsize += bufsize % 4 != 0 ? 4 - bufsize % 4 : 0;
  unsigned char *buffer = malloc(sizeof(unsigned char) * bufsize);
  
  // Use an unbuffered system call read, not fread, for the binary
  // Read exactly nBytes (the proper size of the file)
  if (read(fd, buffer, nBytes) == -1) panic("read_file: read() failed");
  buffer[nBytes] = '\0'; // Pad end with null-teminating char // TODO: do i need this? a better way?

  // Write nBytes read to size
  *size = nBytes;
  
  printf("read_file: first bit of buffer: %s\n", buffer); // DEBUG 
	return buffer;
}

#define _SVID_SOURCE
#include <dirent.h>
const char *ttyusb_prefixes[] = {
	"ttyUSB",	// linux
	// "tty.SLAB_USB", // mac os
	"cu.SLAB", 	// mac os fix?
	0
};

// G: use strncmp for comparing in filter functions
// Cycle through array of prefixes, printing each one out
int filter_for_prefix(const struct dirent *d) {
  int offset = 0;
  while (1) {
    const char *prefix = *(ttyusb_prefixes + offset);
    if (prefix == NULL) break;

    // printf("%s", d->d_name);
    // printf("\n");
    if (strncmp(d->d_name, prefix, strlen(prefix)) == 0) {
      printf("%s\n", d->d_name);	
      return 1;
    }
    offset++;
  }
  return 0;
}

// open the TTY-usb device:
//	- use <scandir> to find a device with a prefix given by ttyusb_prefixes
//	- returns an open fd to it
// 	- write the absolute path into <pathname> if it wasn't already
//	  given.
//      G: portname is the pathname
int open_tty(const char **portname) {
  // Use scandir to find the location of our rpi
	struct dirent **namelist;
	int nEntries = scandir("/dev/", &namelist, filter_for_prefix, NULL);
  // Check to make sure we found just the rpi
  if (nEntries != 1) panic(
    "Number of directory entries not 1. Perhaps rpi not found, or more than one is connected?"
  );
  // TODO: print the path to **portname (a pointer to a string)
	// printf("%s", (*namelist)->d_name);
  // sprintf
	// strdup

	// Open a connection to the rpi
	char *dirname = (*namelist)->d_name;
	char path[5 + strlen(dirname)];
	strcpy(path, "/dev/");
	strcat(path, dirname);
	// fprintf(stderr, "<%s>\n", path); // DEBUG
	int fd = open(path, O_RDWR|O_NOCTTY|O_SYNC);
	fprintf(stdout, "open_tty: opening connection to rpi at fd %d\n", fd); // DEBUG

  // sprintf(*portname, "%s", dirname); // Copy path to **portname
  // strcpy(*portname, dirname);	

  // Return a file descriptor to the pi
	return fd;
}
