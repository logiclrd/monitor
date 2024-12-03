#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <cstdio>

void log(char *line);

void noop(int signo)
{
  pid_t pid;
  int wstat;

  log("received SIGCHLD!");

  pid = wait(&wstat);
}

void log(char *line)
{
  fprintf(stderr, "%s\n", line);
  fflush(stderr);
}

int main(int argc, char *argv[], char *envp[])
{
  signal(SIGCHLD, noop);

  pid_t pid = vfork();

  if (pid < 0)
    log("vfork() returned a negative value, so an error occurred");
  else if (pid > 0)
  {
    log("vfork() returned a positive value, so I must be the parent. sleeping");

    sleep(10);

    log("parent exiting...");
  }
  else
  {
    log("vfork() returned 0, so I must be the child. sleeping first...");

    sleep(5);

    log("execve'ing now!");
    
    execve("/bin/echo", argv, envp);
    log("execve() failed, exiting now");
    _exit(1);
  }
}
