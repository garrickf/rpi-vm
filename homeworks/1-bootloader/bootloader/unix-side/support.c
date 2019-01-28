/*
 * support.c: Support functions for simple-boot.c
 * ---
 * Exports two helper functions, one for reading in a file to a buffer and 
 * another for opening a connection to the rpi.
 */

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

/*
 * read_file
 * ---
 * Opens the file specified by name and reads it into a buffer. Returns
 * the buffer and writes the number of bytes read to an integer pointed
 * to by size.
 */
unsigned char *read_file(int *size, const char *name) {
	// printf("read_file: want to open %s\n", name); // DEBUG
  int fd = open(name, O_RDONLY); // Open file for reading

  // To get the file size, we use the stat() function
  struct stat st;
  if (stat(name, &st) == -1) panic("read_file: stat() failed");
  size_t nBytes = st.st_size;
  size_t bufsize = nBytes; // Note: no extra padding
 
  // printf("read_file: %zu bytes adj., padding to %zu\n", bufsize, bufsize + (bufsize % 4 != 0 ? 4 - bufsize % 4 : 0)); // DEBUG
  bufsize += bufsize % 4 != 0 ? 4 - bufsize % 4 : 0;
  unsigned char *buffer = calloc(bufsize, sizeof(unsigned char));
  
  // Use an unbuffered system call read, not fread, for the binary
  // Read exactly nBytes (the proper size of the file)
  if (read(fd, buffer, nBytes) == -1) panic("read_file: read() failed");

  // Write nBytes read to size
  *size = nBytes;
  // printf("read_file: first bit of buffer: %u\n", *(unsigned *)buffer); // DEBUG 
	return buffer;
}

#define _SVID_SOURCE
#include <dirent.h>
const char *ttyusb_prefixes[] = {
	"ttyUSB",	// Linux
	// "tty.SLAB_USB", 
	"cu.SLAB", 	// MacOS fix, look for this port!
	0
};

/*
 * filter_for_prefix
 * ---
 * Checks the passed-in dirent's name against the specified
 * ttyusb_prefixes. If any matches found, returns 1, oth. 0.
 */
int filter_for_prefix(const struct dirent *d) {
  int offset = 0;
  while (1) {
    const char *prefix = *(ttyusb_prefixes + offset);
    if (prefix == NULL) break;

    if (strncmp(d->d_name, prefix, strlen(prefix)) == 0) return 1;
    offset++;
  }
  return 0;
}

/*
 * open_tty
 * ---
 * Opens a connection to the rpi device (just a file descriptor). Uses
 * scandir to find a prefix given by ttyusb_prefixes. Returns an open fd to
 * the rpi, and creates a heap-allocated absolute pathname string put into 
 * *portname.
 */
int open_tty(const char **portname) {
  // Use scandir to find the location of our rpi (null for compare fn)
	struct dirent **namelist;
	int nEntries = scandir("/dev/", &namelist, filter_for_prefix, NULL);
  // Check to make sure we found just the rpi
  if (nEntries != 1) panic(
    "open_tty: # of dir entries not 1; perhaps rpi not found, or more than one is connected?"
  );

	// Open a connection to the rpi
	char *dirname = (*namelist)->d_name;
	char path[strlen("/dev/") + strlen(dirname)];
	strcpy(path, "/dev/");
	strcat(path, dirname);

	int fd = open(path, O_RDWR|O_NOCTTY|O_SYNC);
	// printf("open_tty: opening connection to rpi at fd %d\n", fd); // DEBUG
  *portname = strdup(path);	
	return fd; // Return a file descriptor to the pi
}
