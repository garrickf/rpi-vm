/*
 * cmd-watch: Command Watcher in C
 * ---
 * Monitors all of the files *.[chsS] in a directory 
 * and runs the program with args when a modification is detected.
 * 
 * Usage:
 * 		cmd-watch ./program-name arg1 arg2 
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h> // For string comparison
#include <sys/stat.h>
#include <sys/wait.h> // For waitpid
#include <unistd.h> // For sleep, fork

static char *suffixes[] = { ".c", ".h", ".S", 0 };
static time_t last_mtime;

// Checks to see if dirent's name matches a suffix
static int has_suffix(const struct dirent *d, char **suffixes) {
    int dir_n = strlen(d->d_name);
    const char *dir_end = d->d_name + dir_n;

    for(char **p = suffixes; *p; p++) {
        int n = strlen(*p);
        if(dir_n <= n)
            continue;
        if(strcmp(dir_end-n,*p) == 0)
            return 1;
    }
    return 0;
}

// Forks off the child process and execs
static void fork_and_exec(char *args[]) {
	pid_t pid = fork();
	if (pid == 0) {
		execvp(args[0], args);
		printf("Something went wrong.\n");
		exit(1);
	}
	int status;
	waitpid(pid, &status, 0);
	if (WIFEXITED(status)) {
		printf("Process exited normally.\n");
	} else {
		printf("Process terminated abnormally with exit code {}.\n");
	}
}

// Checks to see if there's been any activity
static int check_activity(/*const char *_dirname, int scan_all_p*/) {
	DIR * dirents = opendir("."); // Open current directory
	struct dirent *e;
	time_t most_recent_mtime = -1;

	while ((e = readdir(dirents)) != NULL) {
		if (has_suffix(e, suffixes)) {
			//printf("%s\n", e->d_name);
			struct stat st;
			stat(e->d_name, &st);
			// printf("%lu\n", st.st_mtime);
			most_recent_mtime = st.st_mtime > most_recent_mtime ? st.st_mtime : most_recent_mtime;
		}
	}

	closedir(dirents);

	if (most_recent_mtime > last_mtime) {
		last_mtime = most_recent_mtime;
		return 1;
	}
	return 0;
}

int main(int argc, char *argv[]) {
	if (!argv[1]) {
		printf("Usage: cmd-watch ./program-name [arg1 arg2...]\n");
		exit(0);
	}

	last_mtime = -1;
	while (1) {
		// printf("Checking for updates...\n");
		if(check_activity()) {
			printf("Files updated!\n");
			fork_and_exec(argv + 1);
		}

		sleep(1); // TODO: sleep 250 ms
	}
	return 0;
}