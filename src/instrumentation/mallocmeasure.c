/*
 *  r-instrumented : Various measurements for R
 *  Copyright (C) 2014  TU Dortmund Informatik LS XII
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, a copy is available at
 *  http://www.r-project.org/Licenses/
 *
 *  mallocmeasure.c: memory allocation measurements
 */

#define _GNU_SOURCE
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dlfcn.h>
#include <limits.h>
#include <malloc.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "mallocmeasure.h"

#ifndef __linux__

/* the code needs RTLD_NEXT and malloc_usable_size */

unsigned int mallocmeasure_quantum = 1;
size_t mallocmeasure_values[1];
size_t mallocmeasure_current_slot = 0; // if this is 0, the output is omitted

void mallocmeasure_finalize(void) {}

#else

#define PEAKSLOTS 86400  // enough for one day at one-second resolution

static void *(*real_calloc)(size_t nmemb, size_t size);
static void *(*real_malloc)(size_t size);
static void *(*real_realloc)(void *ptr, size_t size);
static void (*real_free)(void *ptr);
static volatile bool in_init   = false;
static char init_mem[1024];
static char *cur_init = init_mem;
static size_t current_alloc = 0;

static size_t current_peak;
static struct timespec peak_slot_starttime;
unsigned int mallocmeasure_quantum = 1;
size_t mallocmeasure_values[PEAKSLOTS + 1];
size_t mallocmeasure_current_slot;

static void update_memstats(void) {
  /* check if more than mallocmeasure_quantum seconds have elapsed */
  struct timespec now, diff;
  clock_gettime(CLOCK_REALTIME_COARSE, &now);

  if (now.tv_nsec < peak_slot_starttime.tv_nsec) {
    diff.tv_sec  = now.tv_sec - peak_slot_starttime.tv_sec - 1;
    diff.tv_nsec = 1000000000 + now.tv_nsec - peak_slot_starttime.tv_nsec;
  } else {
    diff.tv_sec  = now.tv_sec  - peak_slot_starttime.tv_sec;
    diff.tv_nsec = now.tv_nsec - peak_slot_starttime.tv_nsec;
  }

  if (diff.tv_sec >= mallocmeasure_quantum) {
    /* store as many times as neccessary to reduce the difference below the quantum */
    while (diff.tv_sec >= mallocmeasure_quantum) {
      mallocmeasure_values[mallocmeasure_current_slot++] = current_peak;

      if (mallocmeasure_current_slot > PEAKSLOTS) {
        /* overflow, reduce resolution by half */
        for (unsigned int i=0; i < PEAKSLOTS/2; i++) {
          if (mallocmeasure_values[2*i] > mallocmeasure_values[2*i+1]) {
            mallocmeasure_values[i] = mallocmeasure_values[2*i];
          } else {
            mallocmeasure_values[i] = mallocmeasure_values[2*i+1];
          }
        }

        mallocmeasure_quantum     *= 2;
        mallocmeasure_current_slot /= 2;
      }

      diff.tv_sec                -= mallocmeasure_quantum;
      peak_slot_starttime.tv_sec += mallocmeasure_quantum;
    }

    current_peak = 0;
  }

  if (current_peak < current_alloc)
    current_peak = current_alloc;
}

void mallocmeasure_finalize(void) {
  /* write the final memory peak into the list */
  mallocmeasure_values[mallocmeasure_current_slot++] = current_peak;
}  

static void init_hooks(void) {
  in_init = true;

  real_calloc  = dlsym(RTLD_NEXT, "calloc");
  real_malloc  = dlsym(RTLD_NEXT, "malloc");
  real_realloc = dlsym(RTLD_NEXT, "realloc");
  real_free    = dlsym(RTLD_NEXT, "free");

  in_init = false;

  clock_gettime(CLOCK_REALTIME_COARSE, &peak_slot_starttime);
}

/* --- hooks --- */

void *calloc(size_t nmemb, size_t size) {
  size_t realsize = nmemb * size;

  if (real_calloc == NULL) {
    if (!in_init)
      init_hooks();
    else {
      /* miniature static allocator for dlsym */
      char *cur = cur_init;
      cur_init += realsize;
      return cur;
    }
  }

  void *result = real_calloc(nmemb, size);

  if (result != NULL) {
    current_alloc += malloc_usable_size(result);
    update_memstats();
  }

  return result;
}

void *malloc(size_t size) {
  if (real_malloc == NULL)
    init_hooks();

  void *result = real_malloc(size);
  if (result != NULL) {
    current_alloc += malloc_usable_size(result);
    update_memstats();
  }

  return result;
}

void *realloc(void *ptr, size_t size) {
  if (real_realloc == NULL)
    init_hooks();

  size_t tmp = current_alloc - malloc_usable_size(ptr);

  void *result = real_realloc(ptr, size);

  if (result != NULL) {
    current_alloc = tmp + malloc_usable_size(result);
    update_memstats();
  }

  return result;
}

void free(void *ptr) {
  if (real_free == NULL)
    init_hooks();

  current_alloc -= malloc_usable_size(ptr);
  update_memstats();

  real_free(ptr);
}

#endif // ifdef RTLD_NEXT
