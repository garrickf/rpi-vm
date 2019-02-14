#include "rpi.h"
#include "rpi-thread.h"

// typedef rpi_thread_t E;
#define E rpi_thread_t
#include "Q.h"

static struct Q runq, freeq;
static unsigned nthreads;

static rpi_thread_t *cur_thread;  // current running thread.
static rpi_thread_t *scheduler_thread; // first scheduler thread.

// call kmalloc if no more blocks, otherwise keep a cache of freed threads.
static rpi_thread_t *mk_thread(void) {
	rpi_thread_t *t;

	if(!(t = Q_pop(&freeq)))
 		t = kmalloc(sizeof(rpi_thread_t));

	return t;
}

// create a new thread.
rpi_thread_t *rpi_fork(void (*code)(void *arg), void *arg) {
	rpi_thread_t *t = mk_thread();

	// you can use these for setting the values in the first thread.
	// if your context switching code stores these values at different
	// stack offsets, change them!
	enum { 
		// register offsets are in terms of byte offsets!
		LR_offset = 56/4,  
		CPSR_offset = 0/4,
		R0_offset = 4/4, 
		R1_offset = 8/4, 
		// Old offsets
		// LR_offset = 52/4,  
		// CPSR_offset = 56/4,
		// R0_offset = 0/4, 
		// R1_offset = 4/4, 
	};

	// write this so that it calls code, arg.
	void rpi_init_trampoline(void);

	// do the brain-surgery on the new thread stack here.
	t->sp = &t->stack[1024 * 8 - 1]; // Goto end of stack (since it's decrementing)
	t->sp -= 60/4; // Go to where we know the stuff will be pushed on
	t->sp[LR_offset] = rpi_init_trampoline; // Literally branch here at end
	t->sp[CPSR_offset] = rpi_get_cpsr();
	t->sp[R0_offset] = arg;
	t->sp[R1_offset] = code;

	Q_append(&runq, t);
	return t;
}

// exit current thread.
void rpi_exit(int exitcode) {
	/*
	 * 1. free current thread.
	 *
	 * 2. if there are more threads, dequeue one and context
 	 * switch to it.
	 
	 * 3. otherwise we are done, switch to the scheduler thread 
	 * so we call back into the client code.
	 */
	printk("exit: reached\n");

	rpi_thread_t *t;
	Q_append(&freeq, cur_thread);
	if(!(t = Q_pop(&runq))) { // No more threads in to-runq, go to scheduler
		rpi_cswitch(&cur_thread->sp, &scheduler_thread->sp);
	} else { // Switch to another thread
		rpi_cswitch(&cur_thread->sp, &t->sp);
	}
}

// yield the current thread.
void rpi_yield(void) {
	// if cannot dequeue a thread from runq
	//	- there are no more runnable threads, return.  
	// otherwise: 
	//	1. enqueue current thread to runq.
	// 	2. context switch to the new thread.
	rpi_thread_t *t;
	if(!(t = Q_pop(&runq))) { // No more threads in to-runq, return
		return;
	} else { // Switch to another thread
		rpi_cswitch(&cur_thread->sp, &t->sp);
	}
}

// starts the thread system: nothing runs before.
// 	- <preemptive_p> = 1 implies pre-emptive multi-tasking.
void rpi_thread_start(int preemptive_p) {
	assert(!preemptive_p);

	// if runq is empty, return.
	// otherswise:
	//    1. create a new fake thread 
	//    2. dequeue a thread from the runq
	//    3. context switch to it, saving current state in
 	//	  <scheduler_thread>
	scheduler_thread = mk_thread();

	if ((cur_thread = Q_pop(&runq))) {
		printk("rpi_thread_start: context switch...\n");
		rpi_cswitch(&scheduler_thread->sp, &cur_thread->sp);
	}
	// Note: rpi exit handles returning to us, the dummy thread
	printk("THREAD: done with all threads, returning\n");
}

// pointer to the current thread.  
//	- note: if pre-emptive is enabled this can change underneath you!
rpi_thread_t *rpi_cur_thread(void) {
	return cur_thread;
}

// call this an other routines from assembler to print out different
// registers!
void check_regs(unsigned r0, unsigned r1, unsigned sp, unsigned lr) {
	printk("r0=%x, r1=%x lr=%x cpsr=%x\n", r0, r1, lr, sp);
	clean_reboot();
}
