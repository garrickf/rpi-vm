/*
 * Simple tracing memory implementation.  Used for cross-checking.
 */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "rpi.h"
#include "simple-check.h"

/* 
 * simplistic memory:  
 *	- all loads/stores are 32 bits.
 * 	- read returns value of last write, or random() if none.
 *	- load and store prints the addr,val
 */
typedef struct {
	void *addr;
	unsigned val;
} mem_t;

// don't change print_write/print_read so we can compare to everyone.
static void print_write(mem_t *m) {
	printf("\tWRITE:addr=%p, val=%u\n", m->addr, m->val);
}
static void print_read(mem_t *m) {
	printf("\tREAD:addr=%p, val=%u\n", m->addr, m->val);
}

/********************************************************************
 * implement these two functions.
 */

int kNumMemEntries = 1000;
int size = 0;
mem_t memarr[1000];

mem_t *getMemEntry(void *addr) {
  for (int i = 0; i < kNumMemEntries; i++) {
    mem_t temp = memarr[i];
    if (temp.addr == addr) return &memarr[i];
  }
  return NULL;
}

mem_t *createMemEntry() {
  size++;
  return &memarr[size-1];
}

unsigned (get32)(volatile void *addr) {
  mem_t *hit = getMemEntry((void *)addr);
  if (hit == NULL) {
    mem_t *new = createMemEntry();
    new->addr = (void *)addr;
    new->val = random();
    print_read(new);
    return new->val;
  } else {
    print_read(hit);
    return hit->val;
  }
}

void (put32)(volatile void *addr, unsigned val) {
  mem_t *hit = getMemEntry((void *)addr);
  if (hit == NULL) {
    mem_t *new = createMemEntry();
    new->addr = (void *)addr;
    new->val = val;
    print_write(new);
  } else {
    hit->val = val; // Update value
    print_write(hit);
  }
 
}

/***********************************************************************
 * harnesses to run checked code.
 */
void xc_run_fn_iu(const char *A, int (*a)(unsigned), gen_t g) {
	unsigned in;
	while(g(&in)) {
		printf("-----------------------------------------\n");
		printf("going to check <%s(%d)>\n", A, in);
		int r1 = a(in);
		printf("returned=%d\n", r1);
	}
}

void xc_run_fn_vv_once(const char *A, void (*a)(void)) {
	printf("--------------------------------------------\n");
	printf("going to check <%s>\n", A);
	a();
	printf("returned\n");
}

static int pin_gen(unsigned *out) {
        static unsigned u = 0;
        if(u > 70) {
                u = 0;
                return 0;
        }
        *out = (u < 64) ? u : random();
        u++;
        return 1;
}

gen_t xc_get_pin_gen(void) {
        return pin_gen;
}

