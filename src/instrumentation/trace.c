/*
 * trace.c
 *
 *  This system emits trace codes to create a trace of a given run of an R program
 *
 *  Created on: Jan 24, 2011
 *      Author: r-core@purdue.edu
 */


#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#define R_USE_SIGNALS 1
#include <Defn.h>
#include <trace.h>

#include <time.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/times.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <zlib.h>

#include "mallocmeasure.h"
#include "tracer_freemem.h"

#ifdef TRACE_ZIPPED
  typedef gzFile TRACEFILE;
#else
  typedef FILE  *TRACEFILE;
#endif

typedef struct TraceInfo_ {
    char filename[MAX_DNAME];

    TRACEFILE extcalls_fd;
} TraceInfo;

static TraceInfo trace_info;

// fork support
static char **childfiles;
static unsigned int childfiles_count;
static unsigned int childfiles_max;
static struct timeval start_time_us, end_time_us;

// Trace counters
extern unsigned long duplicate_object, duplicate_elts, duplicate1_elts;

// SEXP (*56)
extern unsigned long allocated_cons, allocated_prom, allocated_env; // bytes
extern unsigned long allocated_external, allocated_sexp, allocated_noncons; // bytes
extern unsigned long allocated_cons_current, allocated_cons_peak; // count

// String buffers count
extern unsigned long allocated_sb, allocated_sb_size, allocated_sb_elts;


extern unsigned long duplicate_object, duplicate_elts, duplicate1_elts;

extern unsigned long allocated_list, allocated_list_elts;
extern int gc_count;


/*
 *
 * closure argument histogram
 *
 */

typedef struct {
    /* number of calls with <index> args */
    unsigned int calls;

    /* straight total_args -> number_of_foo mapping */
    unsigned int by_position;
    unsigned int by_keyword;
    unsigned int by_dots;

    /* number_of_foo -> call_count mapping */
    unsigned int nposition_count;
    unsigned int nkeyword_count;
    unsigned int ndots_count;
} argcounter_t;

static argcounter_t *arg_histogram;
static int           max_hist_args   = -1;
static Rboolean      argcount_failed = FALSE;

/* update the counters for closure argument histograms */
void trcR_count_closure_args(SEXP op) {
    (void) op; // this simple version does not support per-closure counting

    int cur_args = trcR_by_position + trcR_by_keyword + trcR_by_dots;

    if (cur_args > max_hist_args) {
	argcounter_t *newhist = realloc(arg_histogram, (cur_args + 1) * sizeof(argcounter_t));

	if (newhist == NULL) {
	    // realloc failed, ignore this call
	    argcount_failed = TRUE;
	    return;
	}

	/* clear new space */
	memset((unsigned char *)newhist +
	       (max_hist_args + 1) * sizeof(argcounter_t), 0,
	       (cur_args - max_hist_args) * sizeof(argcounter_t));

	max_hist_args = cur_args;
	arg_histogram = newhist;
    }

    arg_histogram[cur_args].calls++;

    /* count arguments indexed by total number of args */
    arg_histogram[cur_args].by_position += trcR_by_position;
    arg_histogram[cur_args].by_keyword  += trcR_by_keyword;
    arg_histogram[cur_args].by_dots     += trcR_by_dots;

    /* count calls indexed by number of pos/key/dot args */
    arg_histogram[trcR_by_position].nposition_count++;
    arg_histogram[trcR_by_keyword].nkeyword_count++;
    arg_histogram[trcR_by_dots].ndots_count++;
}

/* write argument histogram to trace file */
static void write_arg_histogram(FILE *fd) {
    if (argcount_failed) {
	fprintf(fd, "# argument count histogram calculation failed\n");
	fprintf(fd, "ArgHistogramFailed\t1\n");
	return;
    }

    fprintf(fd, "# argument count histogram\n");
    fprintf(fd, "#!LABEL\tcount\tcalls\tby_position\tby_keyword\tby_dots\tnpos_calls\tnkey_calls\tndots_calls\n");
    fprintf(fd, "#!TABLE\tArgCount\tArgumentCounts\n"); // FIXME: Enough parameters?
    for (int i = 0; i <= max_hist_args; i++) {
	fprintf(fd, "ArgCount\t%d\t%u\t%u\t%u\t%u\t%u\t%u\t%u\n", i,
		arg_histogram[i].calls,
		arg_histogram[i].by_position,
		arg_histogram[i].by_keyword,
		arg_histogram[i].by_dots,
		arg_histogram[i].nposition_count,
		arg_histogram[i].nkeyword_count,
		arg_histogram[i].ndots_count);
    }
}


/*
 * utility functions
 */

static void add_childfile(char *orig_name) {
  char *name = strdup(orig_name);
  if (!name)
    abort();

  if (childfiles == NULL) {
    childfiles_max = 100;
    childfiles = malloc(childfiles_max * sizeof(long));
    if (childfiles == NULL) {
      perror("malloc childfiles");
      abort();
    }
  }

  if (childfiles_count >= childfiles_max) {
    char **newfiles = realloc(childfiles, 2*childfiles_max*sizeof(long));
    if (newfiles == NULL) {
      perror("realloc childfiles");
      abort();
    }
    childfiles = newfiles;
    childfiles_max *= 2;
  }

  childfiles[childfiles_count++] = name;
}


static void __attribute__((__format__(printf, 1, 2)))
  print_error_msg(const char *format, ...) {
    fprintf(stderr, "[Error] ");
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    if (format[strlen(format) - 1] != '\n')
	fprintf(stderr, "\n");
}


#ifdef TRACE_ZIPPED

/* borrow gzio replacement functions from connections.c
   which includes them from gzio.h */

gzFile R_gzopen (const char *path, const char *mode);
int R_gzclose (gzFile file);
int R_gzwrite (gzFile file, voidpc buf, unsigned len);

// gzprintf replacement because it's not even in gzio.h
static int __attribute__((format(printf, 2, 3)))
  gz_printf(TRACEFILE file, const char *format, ...) {
    char strbuf[2000];
    int res;
    va_list args;

    va_start(args, format);
    res = vsnprintf(strbuf, sizeof(strbuf), format, args);
    va_end(args);
    WRITE_FUN(file, strbuf, res);
    return res;
}

#endif


/*
 * tracing directory init/cleanup
 */
static void create_tracedir() {
    if (!traceR_TraceExternalCalls && R_TraceLevel == TR_DISABLED)
	return;

    /* copy the trace directory name */
    if (R_TraceDir) {
        /* create the directory */
        if (mkdir(R_TraceDir, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH)
            && errno != EEXIST)
          print_error_msg("Can't create directory: %s\n", R_TraceDir);

        snprintf(trace_info.filename, sizeof(trace_info.filename), "%s/%s", R_TraceDir, SUMMARY_NAME);
    } else {
      strncpy(trace_info.filename, R_TraceFile, sizeof(trace_info.filename)-1);
    }
}

static void initialize_trace_defaults(TR_TYPE mode) {
    if (mode == TR_DISABLED)
	return;

    // initialize
    traceR_is_active = 0;
}

static void init_externalcalls() {
    char str[MAX_DNAME];

    if (!traceR_TraceExternalCalls)
	return;

    /* open the externalcalls file for writing */
    if (R_TraceDir) {
      sprintf(str, "%s/%s", R_TraceDir, EXTCALLS_NAME);
    } else {
      strcpy(str, EXTCALLS_NAME);
    }
    trace_info.extcalls_fd = FOPEN(str);
    if (trace_info.extcalls_fd == NULL) {
	print_error_msg("Could not open file '%s' for writing", str);
	abort();
    }
}

static void start_tracing() {
    if (!traceR_is_active) {
	traceR_is_active = 1;
	freemem_spawn(trace_info.filename);
    }
}


/*
 * Summary output
 */

void close_memory_map();
void write_missing_results(FILE *out);
static void write_vector_allocs(FILE *out);
void traceR_count_all_promises(void);

static void write_allocation_summary(FILE *out) {
    fprintf(out, "PtrSize\t%lu\n", sizeof(void*));
    fprintf(out, "#!LABEL\tSEXPREC\tSEXPREC_ALIGN\n");
    fprintf(out, "StructSize\t%u\t%u\n",
	    (unsigned int)sizeof(SEXPREC),
	    (unsigned int)sizeof(SEXPREC_ALIGN));

    /* memory allocations */
    fprintf(out, "AllocatedCons\t%lu\n", allocated_cons);
    if (!R_isForkedChild) {
      /* measuring just the child peak is annoying, so this is left out */
      fprintf(out, "AllocatedConsPeak\t%lu\n", allocated_cons_peak * sizeof(SEXPREC)); // convert to bytes too
    }
    fprintf(out, "AllocatedNonCons\t%lu\n", allocated_noncons);
    fprintf(out, "AllocatedEnv\t%lu\n", allocated_env);
    fprintf(out, "AllocatedPromises\t%lu\n", allocated_prom);
    fprintf(out, "AllocatedSXP\t%lu\n", allocated_sexp);
    fprintf(out, "AllocatedExternal\t%lu\n", allocated_external);
    fprintf(out, "#!LABEL\tallocs\telements\n");
    fprintf(out, "AllocatedList\t%lu\t%lu\n", allocated_list, allocated_list_elts);
    fprintf(out, "#!LABEL\tallocs\telements\tsize\n");
    fprintf(out, "AllocatedStringBuffer\t%lu\t%lu\t%lu\n", allocated_sb, allocated_sb_elts, allocated_sb_size);

    write_vector_allocs(out);

    fprintf(out, "GC_count\t%d\n", gc_count);

    /* promises */
    traceR_count_all_promises();

    fprintf(out, "HighestPromiseStack\t%u\n", R_PendingPromiseMaxHeight);

    fprintf(out, "#!LABEL\tallocated\tcollected\tunevaled\n");
    fprintf(out, "Promises\t%lu\t%lu\t%lu\n",
	    traceR_promise_stats.created,
	    traceR_promise_stats.collected,
	    traceR_promise_stats.created - traceR_promise_stats.collected_evaled);

    fprintf(out, "#!LABEL\tsame\tlower\thigher\tfail\treset\n");
    fprintf(out, "PromiseSetval\t%lu\t%lu\t%lu\t%lu\t%lu\n",
	    traceR_promise_stats.same,
	    traceR_promise_stats.lower,
	    traceR_promise_stats.higher,
	    traceR_promise_stats.fail,
	    traceR_promise_stats.reset);

    fprintf(out, "#!LABEL\tlower\thigher\n");
    fprintf(out, "PromiseMaxDiff\t%u\t%u\n",
	    traceR_promise_stats.maxdiff_lower,
	    traceR_promise_stats.maxdiff_higher);

    fprintf(out, "#!LABEL\tlevel_difference\tcount\n");
    fprintf(out, "#!TABLE\tPromiseLevelDifference\tPromiseLevelDifference\n");

    for (unsigned int i = 0;
	 i < TRACER_PROMISE_LOWER_LIMIT + TRACER_PROMISE_HIGHER_LIMIT + 1;
	 i++) {
	fprintf(out, "PromiseLevelDifference\t%d\t%lu\n",
		(int)i - TRACER_PROMISE_LOWER_LIMIT,
		traceR_promise_stats.diff_plain[i]);
    }

    /* misc */
    fprintf(out, "#!LABEL\tobject\telements\t1elements\n");
    fprintf(out, "Duplicate\t%lu\t%lu\t%lu\n", duplicate_object, duplicate_elts, duplicate1_elts);

    /* memory over time */
    mallocmeasure_finalize();
    if (mallocmeasure_current_slot) {
	fprintf(out, "MallocmeasureQuantum\t%u\n", mallocmeasure_quantum);
	fprintf(out, "#!LABEL\ttime\tmemory\n");
	fprintf(out, "#!TABLE\tPeakMemory\tMemoryOverTime\n");
	for (unsigned int i = 0; i < mallocmeasure_current_slot; i++) {
	    fprintf(out, "PeakMemory\t%u\t%lu\n", i, mallocmeasure_values[i]);
	}
    }
}

static void write_trace_summary(FILE *out) {
    R_gc();
    char str[TIME_BUFF > MAX_DNAME? TIME_BUFF : MAX_DNAME];
    time_t current_time = time(0);
    struct tm *local_time = localtime(&current_time);
    struct rusage my_rusage;

    if (gethostname(str, sizeof(str))) {
      perror("gethostname");
      abort();
    }

    fprintf(out, "Hostname\t%s\n", str);

    if (getcwd(str, MAX_DNAME) == NULL)
      abort();
    fprintf(out, "Workdir\t%s\n", str);
    fprintf(out, "Args\t"); write_commandArgs(out);
    // TODO print trace_type all/repl/bootstrap

    strftime (str, TIME_BUFF, "%c", local_time);
    fprintf(out, "TraceDate\t%s\n", str);
    getrusage(RUSAGE_SELF, &my_rusage);
    fprintf(out, "RusageMaxResidentMemorySet\t%ld\n", my_rusage.ru_maxrss);
    fprintf(out, "RusageSharedMemSize\t%ld\n", my_rusage.ru_ixrss);
    fprintf(out, "RusageUnsharedDataSize\t%ld\n", my_rusage.ru_idrss);
    fprintf(out, "RusagePageReclaims\t%ld\n", my_rusage.ru_minflt);
    fprintf(out, "RusagePageFaults\t%ld\n", my_rusage.ru_majflt);
    fprintf(out, "RusageSwaps\t%ld\n", my_rusage.ru_nswap);
    fprintf(out, "RusageBlockInputOps\t%ld\n", my_rusage.ru_inblock);
    fprintf(out, "RusageBlockOutputOps\t%ld\n", my_rusage.ru_oublock);
    fprintf(out, "RusageIPCSends\t%ld\n", my_rusage.ru_msgsnd);
    fprintf(out, "RusageIPCRecv\t%ld\n", my_rusage.ru_msgrcv);
    fprintf(out, "RusageSignalsRcvd\t%ld\n", my_rusage.ru_nsignals);
    fprintf(out, "RusageVolnContextSwitches\t%ld\n", my_rusage.ru_nvcsw);
    fprintf(out, "RusageInvolnContextSwitches\t%ld\n", my_rusage.ru_nivcsw);

    write_allocation_summary(out);
    write_arg_histogram(out);
}

static void write_summary() {
    FILE *summary_fp;
    char str[MAX_DNAME];

    if (R_isForkedChild) {
      sprintf(str, "%s_%d", trace_info.filename, getpid());
    } else {
      str[MAX_DNAME-1] = 0;
      strncpy(str, trace_info.filename, MAX_DNAME-1);
    }

    // Write a summary file
    summary_fp = fopen(str, "a");
    if (!summary_fp) {
	print_error_msg ("Couldn't open file '%s' for writing", str);
	return;
    }

    fprintf(summary_fp, "StartTimeUsec\t%ld\n", start_time_us.tv_sec * 1000000UL + start_time_us.tv_usec);
    fprintf(summary_fp, "EndTimeUsec\t%ld\n", end_time_us.tv_sec * 1000000UL + end_time_us.tv_usec);
    struct tms ustimes;
    long ticks_per_sec = sysconf(_SC_CLK_TCK);
    times(&ustimes);
    fprintf(summary_fp, "UserTime\t%f\n", ustimes.tms_utime / (double)ticks_per_sec);
    fprintf(summary_fp, "SystemTime\t%f\n", ustimes.tms_stime / (double)ticks_per_sec);

    write_trace_summary(summary_fp);

    /* if on parent: combine all child summary files */
    if (childfiles_count) {
      fprintf(summary_fp, "childcount\t%d\n", childfiles_count);
      for (unsigned int i = 0; i < childfiles_count; i++) {
        FILE *childfd = fopen(childfiles[i], "r");
        if (!childfd) {
          fprintf(stderr, "WARNING: Unable to open %s: %s\n", childfiles[i], strerror(errno));
          continue;
        }

        unlink(childfiles[i]);

        fprintf(summary_fp, "#!CHILD\t%d\n", i+1);

        while (fgets(str, sizeof(str), childfd)) {
          fprintf(summary_fp, "%s", str);
        }

        fclose(childfd);
      }
    }

    fclose(summary_fp);
}

/*
 * end of tracing
 */
static void terminate_tracing() {
    // Stop tracing
    gettimeofday(&end_time_us, NULL);
    if (traceR_is_active) {
	traceR_is_active = 0;
	write_summary();
    }
}



/*
 * external interface for init/deinit of tracing
 */

/* early initialisation, just after the command line was parsed */
void traceR_initialize(void) {
    create_tracedir();
    initialize_trace_defaults(R_TraceLevel);
    init_externalcalls();

    if (R_TraceLevel == TR_ALL || R_TraceLevel == TR_BOOTSTRAP)
	start_tracing();

    gettimeofday(&start_time_us, NULL);
}

/* called just before the first entry in the REPL */
void traceR_start_repl(void) {
    if (R_TraceLevel == TR_REPL)
	start_tracing();
    else if (R_TraceLevel == TR_BOOTSTRAP)
	terminate_tracing();
}

/* normal exit of the R interpreter */
void traceR_finish_clean(void) {
    if (traceR_is_active) {
	freemem_stop();
	terminate_tracing();
    }

    if (traceR_TraceExternalCalls) {
	FCLOSE(trace_info.extcalls_fd);
    }
}

/* log calls to external code */
void traceR_report_external_int(int /*NativeSymbolType*/ type,
				char *funcname,
				void /*DL_FUNC*/ *fun) {
    if (funcname[0] == 0) {
	/* function name is not available */
	FPRINTF(trace_info.extcalls_fd, "%d @%p %p\n",
		type, fun, fun);
    } else {
	FPRINTF(trace_info.extcalls_fd, "%d %s %p\n",
		type, funcname, fun);
    }
}


/*
 * vector allocation logginc
 */

/* exact number of elements are counted up to (1 << this) - 1 */
#define VECTOR_EXACT_LIMIT_LD 4

/* total number of bins, 64 should be sufficient for LIMIT_LD=4 on a x86_64 machine */
#define VECTOR_BYELEMENT_BINS 64

static vec_alloc_stats_t vectors_byclass[TR_VECCLASS_TOTAL + 1];
static vec_alloc_stats_t vectors_byelements[VECTOR_BYELEMENT_BINS];
static unsigned int      vecalloc_max_bin;

static void count_vecalloc(vec_alloc_stats_t *stat,
			   size_t elements,
			   size_t size,
			   size_t asize) {
    stat->allocs++;
    stat->elements += elements;
    stat->size     += size;
    stat->asize    += asize;
}

static unsigned int vector_bin_number(size_t elements) {
    if (elements < (1 << VECTOR_EXACT_LIMIT_LD)) {
	/* exact count below bound */
	return elements;
    } else {
	/* one bin per binary power above bound */
	unsigned int ld_e = 0;

	while (elements > 0) {
	    elements >>= 1;
	    ld_e++;
	}

	/* skip first k bins, put first batch bin directly after that */
	return ld_e +
	    (1 << VECTOR_EXACT_LIMIT_LD) -
	    VECTOR_EXACT_LIMIT_LD - 1;
    }
}

static size_t vector_bin_lower(unsigned int bin) {
    if (bin < (1 << VECTOR_EXACT_LIMIT_LD)) {
	return bin;
    } else {
	return 1 << (bin - (1 << VECTOR_EXACT_LIMIT_LD) +
		     VECTOR_EXACT_LIMIT_LD);
    }
}

static size_t vector_bin_upper(unsigned int bin) {
    if (bin < (1 << VECTOR_EXACT_LIMIT_LD)) {
	return bin;
    } else {
	return (1 << (bin - (1 << VECTOR_EXACT_LIMIT_LD) +
		      VECTOR_EXACT_LIMIT_LD + 1)) - 1;
    }
}

void traceR_count_vector_alloc(traceR_vector_class_t type,
			       size_t elements,
			       size_t size,
			       size_t asize) {
    /* add to per-class counts */
    count_vecalloc(&vectors_byclass[TR_VECCLASS_TOTAL], elements, size, asize);
    count_vecalloc(&vectors_byclass[type],              elements, size, asize);

    /* add to per-size histogram */
    unsigned int bin = vector_bin_number(elements);

    if (bin > vecalloc_max_bin)
	vecalloc_max_bin = bin;

    count_vecalloc(&vectors_byelements[bin], elements, size, asize);
}

static void report_vectorstats(FILE *out, const char *name, vec_alloc_stats_t *stats) {
    fprintf(out, "%s\t%lu\t%lu\t%lu\t%lu\n", name,
	    stats->allocs, stats->elements,
	    stats->size,   stats->asize);
}

static void write_vector_allocs(FILE *out) {
    fprintf(out, "#!LABEL\tallocs\telements\tsize\tasize\n");
    report_vectorstats(out, "AllocatedVectors",      &vectors_byclass[TR_VECCLASS_TOTAL]);
    report_vectorstats(out, "AllocatedZeroVectors",  &vectors_byclass[TR_VECCLASS_ZERO]);
    report_vectorstats(out, "AllocatedOneVectors",   &vectors_byclass[TR_VECCLASS_ONE]);
    report_vectorstats(out, "AllocatedSmallVectors", &vectors_byclass[TR_VECCLASS_SMALL]);
    report_vectorstats(out, "AllocatedLargeVectors", &vectors_byclass[TR_VECCLASS_LARGE]);

    unsigned int i;

    fprintf(out, "VectorAllocExactLimit\t%d\n", (1 << (VECTOR_EXACT_LIMIT_LD)) - 1);

    fprintf(out, "#!LABEL\tbin_id\tlower_limit\tupper_limit\tallocs\telements\tsize\tasize\n");
    fprintf(out, "#!TABLE\tVectorAllocBin\tVectorAllocationHistogram\n");
    for (i = 0; i <= vecalloc_max_bin; i++) {
	fprintf(out,"VectorAllocBin\t%u\t%lu\t%lu", i,
		vector_bin_lower(i), vector_bin_upper(i));
	report_vectorstats(out, "", &vectors_byelements[i]);
    }
}

static void traceR_reset(void) {
  // FIXME: Extcalls not reset, would require close+reopen of the log file

  allocated_cons        = 0;
  allocated_prom        = 0;
  allocated_env         = 0;
  allocated_external    = 0;
  allocated_sexp        = 0;
  allocated_noncons     = 0;
  allocated_sb          = 0;
  allocated_sb_size     = 0;
  allocated_sb_elts     = 0;
  duplicate_object      = 0;
  duplicate_elts        = 0;
  duplicate1_elts       = 0;
  allocated_list        = 0;
  allocated_list_elts   = 0;
  gc_count              = 0;

  memset(&traceR_promise_stats, 0, sizeof(traceR_promise_stats));

  free(arg_histogram);
  arg_histogram = NULL;
  max_hist_args = -1;

  memset(vectors_byclass,    0, sizeof(vectors_byclass));
  memset(vectors_byelements, 0, sizeof(vectors_byelements));
  vecalloc_max_bin = 0;

  mallocmeasure_reset();
}

void traceR_forked(long childpid) {
  if (childpid == 0) {
    /* in child */
    if (R_isForkedChild) {
      fprintf(stderr, "*** ERROR: Forking from child processes is currently not supported!\n");
      abort();
    }

    freemem_fork();
    traceR_reset();
    for (unsigned int i = 0; i < childfiles_count; i++)
      free(childfiles[i]);
    free(childfiles);
    childfiles       = NULL;
    childfiles_max   = 0;
    childfiles_count = 0;
    gettimeofday(&start_time_us, NULL);
    return;
  }

  /* in parent, add child data file name */
  if (trace_info.extcalls_fd) {
    fprintf(stderr, "*** ERROR: Cannot handle parallel operation while external call logging is active!\n");
    abort();
  }

  char childfn[1024];
  childfn[sizeof(childfn)-1] = 0;
  snprintf(childfn, sizeof(childfn)-1, "%s_%ld", trace_info.filename, childpid);
  add_childfile(childfn);
}

static unsigned int childcounter = 0;

void traceR_getchildfile(char *buffer) {
  FILE *fd;

  childcounter++;

  // FIXME: Alternatively check for / at start of path and just prefix getcwd()?
  /* first create an empty file from our side with the given prefix */
  sprintf(buffer, "%s_%d", trace_info.filename, childcounter);
  fd = fopen(buffer, "wb");
  if (fd == NULL) {
    perror("create statfile");
    abort();
  }
  fclose(fd);

  /* now we can use realpath to get an absolute path for it */
  char *rp = realpath(buffer, NULL);
  if (rp == NULL) {
    perror("realpath statsfile");
    abort();
  }
  strcpy(buffer, rp);
  free(rp);

  add_childfile(buffer);
}
