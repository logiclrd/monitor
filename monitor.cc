#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <cstdio>

#include "my_popen.h"

#define PERSISTENT_LOG_FILE_HANDLE
#define LOG_FILE_NAME "/var/log/pppd_monitor"
// #define PERSISTENT_DEBUG_OUT_FILE_HANDLE
#define DEBUG_OUT
#define DEBUG_OUT_FILE_NAME "debug_out"

using namespace std;

int pid = -1;

char **environ;

FILE *log_file;
FILE *debug_out_file;

char *timestamp()
{
  static char timestamp_buf[500];

  time_t timeval = time(NULL);

  struct tm *tm_val = localtime(&timeval);

  sprintf(timestamp_buf, "%04d/%02d/%02d %02d:%02d:%02d", tm_val->tm_year + 1900, tm_val->tm_mon + 1, tm_val->tm_mday, tm_val->tm_hour, tm_val->tm_min, tm_val->tm_sec);

  return timestamp_buf;
}

void log(char *text, int arg = 0)
{
#ifndef PERSISTENT_LOG_FILE_HANDLE
  log_file = fopen(LOG_FILE_NAME, "a");
  if (log_file == NULL)
    return;
#endif

  fprintf(log_file, "%s ", timestamp());
  fprintf(log_file, text, arg);
  fprintf(log_file, "\n");
  fflush(log_file);

#ifndef PERSISTENT_LOG_FILE_HANDLE
  fclose(log_file);
  log_file = NULL;
#endif
}

void debug_out(char *text, int arg = 0)
{
#ifdef DEBUG_OUT

#ifndef PERSISTENT_DEBUG_OUT_FILE_HANDLE
  debug_out_file = fopen(DEBUG_OUT_FILE_NAME, "a");
  if (debug_out_file == NULL)
    debug_out_file = stderr;
#endif

  fprintf(debug_out_file, text, arg);
  fflush(debug_out_file);

#ifndef PERSISTENT_DEBUG_OUT_FILE_HANDLE
  if (debug_out_file != stderr)
    fclose(debug_out_file);
  debug_out_file = NULL;
#endif

#endif // DEBUG_OUT
}

void debug_out(char *text, void *arg)
{
  debug_out(text, (int)arg);
}

void start_process(bool dc0)
{
  pid = fork();

  if (pid > 0)
    return;

  char *params[4];

  params[0] = "ppp";
  params[1] = "-ddial";
  if (dc0)
    params[2] = "mts_dc0";
  else
    params[2] = "mts";
  params[3] = NULL;

  execve("/usr/sbin/ppp", params, environ);

  // if execve returns at all, then an error has occurred

  pid = -1;

  _exit(1);
}

void stop_process()
{
  if (pid > 0)
    kill(pid, 9);

  unlink("/var/pppctl");

  pid = -1;
}

void noop(int signal)
{
  pid_t pid;
  int wstat;

  pid = wait(&wstat);
}

int main(int argc, char *argv[], char *envp[])
{
  signal(SIGCHLD, noop);

  environ = envp;

  bool using_dc0 = false;

#ifdef PERSISTENT_LOG_FILE_HANDLE
  log_file = fopen(LOG_FILE_NAME, "a");
#endif
#ifdef PERSISTENT_DEBUG_OUT_FILE_HANDLE
  debug_out_file = fopen(DEBUG_OUT_FILE_NAME, "a");
#endif

  log("--------------");
  log("starting up");

  if (argc > 1)
  {
    pid = atoi(argv[1]);
    log("using existing pid %d", pid);
  }

  while (true)
  {
    using_dc0 = !using_dc0;

    if (pid <= 0)
    {
      if (using_dc0)
        log("starting pppd (using interface dc0)");
      else
        log("starting pppd (using interface ed0)");

      start_process(using_dc0);

      if (pid <= 0)
      {
        int en = errno;

        debug_out("error %d occurred while spawning the ppp process\n", en);
        log("failed to start pppd, errno = %d (waiting for 15 seconds)", en);
        sleep(15);
        continue;
      }

      log("started pppd as pid %d", pid);
    }

    debug_out("sleeping to allow the ppp process to start up.");

    for (int i=0; i<10; i++)
    {
      sleep(1);
      debug_out(".");
    }

    debug_out("\n");

    FILE *process = my_popen("/sbin/ping -i 5 205.200.28.26 2>&1", "r+");

    log("started checking for pings");

    while (true)
    {
      int ch;
      char line[1000];

      memset(line, 0, 1000);

      for (int j=0; j<1000; j++)
      {
        int ch = fgetc(process);
        if (ch < 0)
          break;
        if (ch == 10)
          ch = 0;
        line[j] = ch;
        if (ch == 0)
          break;
      }

      debug_out("%s\n", line);

      if (ch < 0)
      {
        log("failed to parse a line of output from 'ping'");
        break;
      }

      line[999] = 0;

      if (strstr(line, "No buffer space available"))
      {
        log("the 'No buffer space available' error has occurred!");
        break;
      }

      if (strstr(line, "Host is down"))
      {
        log("the 'Host is down' error has occurred!");
        break;
      }
    }

    log("killing off 'ping'");
    my_pkill(process);

    log("killing off 'pppd'");
    stop_process();
  }
}

