#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "tracer_freemem.h"

static pid_t child_pid;
static volatile int terminate_query;
static struct sigaction usr1_act;

static uint64_t freemem_values[86400];
static unsigned int current_slot;

static void usr1_handler(int signal) {
  terminate_query = 1;
}

static int64_t query_freemem(void) {
  FILE *fd = fopen("/proc/meminfo", "r");
  char buf[1024];
  int64_t result = -1;

  if (fd == NULL)
    return -1;

  /* MemTotal:       11989664 kB */
  if (fgets(buf, sizeof(buf), fd) == NULL)
    goto out;

  /* MemFree:         4549212 kB */
  if (fscanf(fd, "MemFree: %ld", &result) <= 0)
    result = -1;

 out:
  fclose(fd);
  return result;
}

static void kill_child(void) {
  if (child_pid) {
    kill(child_pid, SIGKILL);
  }
}

void freemem_spawn(const char *statsfile) {
  pid_t p = fork();

  if (p != 0) {
    child_pid = p;
    atexit(kill_child);
    return;
  }

  /* install signal handler */
  usr1_act.sa_handler = usr1_handler;
  usr1_act.sa_flags   = SA_RESETHAND;
  sigaction(SIGUSR1, &usr1_act, NULL);

  /* start measurement loop */
  struct timeval next_query, cur_query;
  gettimeofday(&next_query, NULL);

  while (!terminate_query) {
    /* wait until next_query has arrived or passed */
    while (1) {
      struct timeval diff;
      gettimeofday(&cur_query, NULL);

      if (timercmp(&cur_query, &next_query, >=))
        break;

      timersub(&next_query, &cur_query, &diff);
      usleep(diff.tv_usec);
    }

    /* do the query */
    int64_t res = query_freemem();
    if (res >= 0)
      freemem_values[current_slot] = res;

    current_slot++; // FIXME: Handle overflow

    // FIXME: Handle the case where query_memory needs more than one second
    next_query.tv_sec += 1;
  }

  FILE *fd = fopen(statsfile, "w");
  if (!fd) {
    perror("freequery fopen");
    exit(99);
  }

  fprintf(fd, "freeslots\t%u\n", current_slot);
  fprintf(fd, "#!LABEL\ttime\tmemory\n");
  fprintf(fd, "#!TABLE\tfreeslot\tFreeMemoryOverTime\n");

  uint64_t minvalue = UINT64_MAX;

  for (unsigned int i = 0; i < current_slot; i++) {
    fprintf(fd, "freeslot\t%u\t%lu\n", i, freemem_values[i]);
    if (freemem_values[i] < minvalue)
      minvalue = freemem_values[i];
  }
  fprintf(fd, "minfree\t%ld\n", minvalue);

  fclose(fd);

  exit(0);
}


void freemem_stop(void) {
  if (child_pid) {
    kill(child_pid, SIGUSR1);
    while (!waitpid(child_pid, NULL, WNOHANG)) {
      usleep(10000);
    }
    child_pid = 0;
  }
}

void freemem_fork(void) {
  /* we don't need to measure more than once per machine, but we shouldn't kill the measurement either */
  child_pid = 0;
}
