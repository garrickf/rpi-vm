#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "redirect.h"
#include "demand.h"

// redir:
//  fork/exec <pi_process>
// return two file descripts:
//  rd_fd = the file descriptor the parent reads from to see the child's
//      stdout/stderr output.
//  wr_fd = the file descriptor the parent writes to in order to write data
//      to the child's stdin.
//
// you will override the child's stdin/stdout/stderr using pipes and dup2.
//      recall: pipe[0] = read in, pipe[1] = write end.
int redir(int *rd_fd, int *wr_fd, char * const pi_process) {
	// Pipes: supplyInput {R: 0, W: 1} and ingestOuput {R: 2, W: 3}
    int fds[4];
    pipe(fds);
    pipe(fds + 2);

    pid_t pid = fork();
    if (pid == 0) {
    	dup2(fds[0], STDIN_FILENO);
    	dup2(fds[3], STDOUT_FILENO);
    	
    	// Close all in pipes
    	close(fds[0]);
    	close(fds[1]);
    	close(fds[2]);
    	close(fds[3]);

    	char *args[2];
    	args[0] = pi_process;
    	args[1] = NULL;

    	execvp(pi_process, args);
   		printf("Uh oh, exec did not work...");
    	exit(1);
    }

    close(fds[0]);
	*wr_fd = fds[1];
	*rd_fd = fds[2];
	close(fds[3]);
    return 0;
}



int fd_putc(int fd, char c) {
    if(write(fd, &c, 1) != 1)
        sys_die(write, write failed);
    return 0;
}
void fd_puts(int fd, const char *msg) {
    while(*msg)
        fd_putc(fd, *msg++);
}


