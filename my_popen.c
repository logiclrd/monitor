#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/wait.h>

#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <paths.h>

extern char **environ;

static struct pid {
	struct pid *next;
	FILE *fp;
	pid_t pid;
} *pidlist;

#define	THREAD_LOCK()
#define	THREAD_UNLOCK()

FILE *
my_popen(command, type)
	const char *command, *type;
{
	struct pid *cur;
	FILE *iop;
	int pdes[2], pid, twoway;
	char *argv[4];
	struct pid *p;

	/*
	 * Lite2 introduced two-way popen() pipes using _socketpair().
	 * FreeBSD's pipe() is bidirectional, so we use that.
	 */
	if (strchr(type, '+')) {
		twoway = 1;
		type = "r+";
	} else  {
		twoway = 0;
		if ((*type != 'r' && *type != 'w') || type[1])
			return (NULL);
	}
	if (pipe(pdes) < 0)
		return (NULL);

	if ((cur = malloc(sizeof(struct pid))) == NULL) {
		(void)_close(pdes[0]);
		(void)_close(pdes[1]);
		return (NULL);
	}

	argv[0] = "sh";
	argv[1] = "-c";
	argv[2] = (char *)command;
	argv[3] = NULL;

	THREAD_LOCK();
	switch (pid = vfork()) {
	case -1:			/* Error. */
		THREAD_UNLOCK();
		(void)_close(pdes[0]);
		(void)_close(pdes[1]);
		free(cur);
		return (NULL);
		/* NOTREACHED */
	case 0:				/* Child. */
		if (*type == 'r') {
			/*
			 * The _dup2() to STDIN_FILENO is repeated to avoid
			 * writing to pdes[1], which might corrupt the
			 * parent's copy.  This isn't good enough in
			 * general, since the _exit() is no return, so
			 * the compiler is free to corrupt all the local
			 * variables.
			 */
			(void)_close(pdes[0]);
			if (pdes[1] != STDOUT_FILENO) {
				(void)_dup2(pdes[1], STDOUT_FILENO);
				(void)_close(pdes[1]);
				if (twoway)
					(void)_dup2(STDOUT_FILENO, STDIN_FILENO);
			} else if (twoway && (pdes[1] != STDIN_FILENO))
				(void)_dup2(pdes[1], STDIN_FILENO);
		} else {
			if (pdes[0] != STDIN_FILENO) {
				(void)_dup2(pdes[0], STDIN_FILENO);
				(void)_close(pdes[0]);
			}
			(void)_close(pdes[1]);
		}
		for (p = pidlist; p; p = p->next) {
			(void)_close(fileno(p->fp));
		}
		_execve(_PATH_BSHELL, argv, environ);
		_exit(127);
		/* NOTREACHED */
	}
	THREAD_UNLOCK();

	/* Parent; assume fdopen can't fail. */
	if (*type == 'r') {
		iop = fdopen(pdes[0], type);
		(void)_close(pdes[1]);
	} else {
		iop = fdopen(pdes[1], type);
		(void)_close(pdes[0]);
	}

	/* Link into list of file descriptors. */
	cur->fp = iop;
	cur->pid = pid;
	THREAD_LOCK();
	cur->next = pidlist;
	pidlist = cur;
	THREAD_UNLOCK();

	return (iop);
}

/*
 * pclose --
 *	Pclose returns -1 if stream is not associated with a `popened' command,
 *	if already `pclosed', or waitpid returns an error.
 */
int
my_pclose(iop)
	FILE *iop;
{
	struct pid *cur, *last;
	int pstat;
	pid_t pid;

	/*
	 * Find the appropriate file pointer and remove it from the list.
	 */
	THREAD_LOCK();
	for (last = NULL, cur = pidlist; cur; last = cur, cur = cur->next)
		if (cur->fp == iop)
			break;
	if (cur == NULL) {
		THREAD_UNLOCK();
		return (-1);
	}
	if (last == NULL)
		pidlist = cur->next;
	else
		last->next = cur->next;
	THREAD_UNLOCK();

	(void)fclose(iop);

	do {
		pid = _wait4(cur->pid, &pstat, 0, (struct rusage *)0);
	} while (pid == -1 && errno == EINTR);

	free(cur);

	return (pid == -1 ? -1 : pstat);
}

int
my_pkill(iop)
	FILE *iop;
{
	struct pid *cur, *last;
	int pstat;
	pid_t pid;

	for (last = NULL, cur = pidlist; cur; last = cur, cur = cur->next)
		if (cur->fp == iop)
			break;
	if (cur == NULL)
		return -1;

	if (last == NULL)
		pidlist = cur->next;
	else
		last->next = cur->next;

	fclose(iop);

	kill(cur->pid, SIGINT);
	kill(cur->pid, SIGKILL);

	free(cur);

	return 0;
}

