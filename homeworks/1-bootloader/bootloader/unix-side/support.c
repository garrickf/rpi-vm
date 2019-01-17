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
	printf("%s\n", name);
  FILE *fptr = fopen(name, "r"); // g: open file for reading
 
  // We don't know the file size ahead of time, so we can seek to the end
  // of the file and get the filepos using ftell
  if (fseek(fptr, 0L, SEEK_END) != 0) panic("read_file: could not seek to EOF");
  size_t bufsize = ftell(fptr) + 1; // Include extra byte for padding

  printf("%zu bytes, padding to %zu\n", bufsize, bufsize + (bufsize % 4 != 0 ? 4 - bufsize % 4 : 0));
  bufsize += bufsize % 4 != 0 ? 4 - bufsize % 4 : 0;
  unsigned char *buffer = malloc(sizeof(char) * bufsize);
  
  if (fseek(fptr, 0L, SEEK_SET) != 0) panic("read_file: could not seek to beginning");

  size_t bytesRead = fread(buffer, sizeof(char), bufsize, fptr);
  if (ferror(fptr) != 0) {
    panic("read_file: error reading file");
  } else {
    buffer[bytesRead++] = '\0'; // Pad end with null-teminating chars
  }
  
  printf("%s\n", buffer); 
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
	fprintf(stdout, "<%s>%d\n", "opened at fd:", fd); // DEBUG

	// Return a file descriptor to the pi
	return fd;
}
