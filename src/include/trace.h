/*
 * trace.h
 *
 * This system emits trace codes to create a trace of a given run of an R program
 *
 *  Created on: Jan 24, 2011
 *      Author: r-core@purdue.edu
 */
#ifndef TRACE_H_
#define TRACE_H_

#include "Rinternals.h"

#define TRACE_ZIPPED
#define TRACER_COMPILED_IN

// Output defines
#ifdef TRACE_ZIPPED
#  define MEMORY_MAP_FILE "memory.map.gz"
#  define EXTCALLS_NAME   "external_calls.txt.gz"
#else
#  define MEMORY_MAP_FILE "memory.map"
#  define EXTCALLS_NAME   "external_calls.txt"
#endif
#define SUMMARY_NAME      "trace_summary"

#define EOS -1
#define MAX_FNAME 128
#define MAX_DNAME 1024
#define TIME_BUFF 200


/* trace "verbosity" */
typedef enum {
    TR_DISABLED,
    TR_BOOTSTRAP,
    TR_REPL,
    TR_ALL,
} TR_TYPE;


/* vector allocation statistics struct */
typedef struct {
    unsigned long allocs;
    unsigned long elements;
    unsigned long size;
    unsigned long asize;
} vec_alloc_stats_t;

/* vector allocation classes */
typedef enum {
    TR_VECCLASS_ZERO = 0,
    TR_VECCLASS_ONE,
    TR_VECCLASS_SMALL,
    TR_VECCLASS_LARGE,
    TR_VECCLASS_TOTAL  // must be the last entry
} traceR_vector_class_t;

/* lower and higher function call depth limit for the promise eval histogram */
#define TRACER_PROMISE_LOWER_LIMIT  20
#define TRACER_PROMISE_HIGHER_LIMIT 20

typedef struct {
    unsigned long created;
    unsigned long collected;
    unsigned long collected_evaled;

    // FIXME: same, lower, higher are redundant (data is in the histogram)
    unsigned long same;
    unsigned long lower;
    unsigned long higher;
    unsigned long fail;
    unsigned long reset;
    /* histogram for level differences */
    unsigned long diff_plain[TRACER_PROMISE_LOWER_LIMIT + TRACER_PROMISE_HIGHER_LIMIT + 1];
    // FIXME: Use a separate histogram for delayedAssign/makeLazy?

    unsigned int  maxdiff_higher;
    unsigned int  maxdiff_lower;
} traceR_promise_stats_t;


/* Warning: All vars here are defined in main.c because Defn.h includes trace.h! */
extern traceR_promise_stats_t traceR_promise_stats;
extern int                    traceR_is_active;
extern Rboolean               traceR_TraceExternalCalls;

// counters for the three classes of arguments
// (implicit parameters for trcR_count_closure_args and emit_closure)
extern unsigned int trcR_by_position, trcR_by_keyword, trcR_by_dots;


#ifdef TRACE_ZIPPED
#  define FOPEN(file)              R_gzopen(file, "wb")
#  define FCLOSE(hdl)              R_gzclose(hdl)
#  define WRITE_FUN(f, data, size) R_gzwrite(f, data, size)
#  define FPRINTF                  gz_printf
#else
#  define FOPEN(file) fopen(file, "w")
#  define FCLOSE(hdl) fclose(hdl)
#  define WRITE_FUN(f, data, size) fwrite(data, size, 1, f)
#  define FPRINTF fprintf
#endif

void traceR_initialize(void);
void traceR_start_repl(void);
void traceR_finish_clean(void);

/* Note: The arg counters are implicitly passed via globals: */
/*   trcR_by_position, trcR_by_keyword, trcR_by_dots         */
void trcR_count_closure_args(SEXP op);


/* external call tracing */
void traceR_report_external_int(int /*NativeSymbolType*/ type,
				char *funcname,
				void /*DL_FUNC*/ *fun);

static inline void traceR_report_external(int /*NativeSymbolType*/ type,
					  char *funcname,
					  void /*DL_FUNC*/ *fun) {
    if (traceR_TraceExternalCalls)
	traceR_report_external_int(type, funcname, fun);
}

/* vector allocation logging */
void traceR_count_vector_alloc(traceR_vector_class_t type, size_t elements,
			       size_t size, size_t asize);

#endif /* TRACE_H_ */
