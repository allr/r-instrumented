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
#  define TRACE_NAME      "trace.gz"
#  define SRC_MAP_NAME    "source.map.gz"
#  define MEMORY_MAP_FILE "memory.map.gz"
#  define EXTCALLS_NAME   "external_calls.txt.gz"
#else
#  define TRACE_NAME      "trace"
#  define SRC_MAP_NAME    "source.map"
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
    TR_SUMMARY
} TR_TYPE;


/* vector allocation statistics struct */
typedef struct {
    unsigned long allocs;
    unsigned long elements;
    unsigned long size;
    unsigned long asize;
} vec_alloc_stats_t;

/* Warning: All vars here are defined in main.c because Defn.h includes trace.h! */
extern int traceR_is_active;
extern unsigned int fatal_err_cnt; // FIXME: Rename or remove
extern Rboolean     traceR_TraceExternalCalls;

// counters for the three classes of arguments
// (implicit parameters for trcR_count_closure_args and emit_closure)
extern unsigned int trcR_by_position, trcR_by_keyword, trcR_by_dots;

// Trace output codes
#define BINTRACE_NO_PROLOGUE		0x01
#define BINTRACE_UNIT 			0x20 // null
#define BINTRACE_NEW_PROMISE		0x40
//this count starts after the basic SEXPTYPES
#define BINTRACE_FUNC_END 		0x86
#define BINTRACE_BND_PROM_START		0x87
#define BINTRACE_UBND_PROM_START	0x88
#define BINTRACE_PROM_END 		0x89
#define BINTRACE_UNIMPL_TYPE		0x8A
/* 0x8B is unused */
#define BINTRACE_R_ERROR_SEEN		0x8C
/* 0x8D is unused */
#define BINTRACE_UBND			0x8E /* or 0xCE for new*/
#define BINTRACE_BND			0x8F /* or 0xCF for new*/
#define BINTRACE_BLTIN_ID		0x90
#define BINTRACE_SPEC_ID		0x92
#define BINTRACE_CLOS_ID		0x94
#define BINTRACE_PROL_START		0x96
#define BINTRACE_PROL_END		0x97

// Addresses for special closures
#define C_D_ADDR  1 //any context_drop()
#define UM_ADDR   2 // usemethod()
#define D_UM_ADDR 3 // do_usemethod()
#define D_B_ADDR  4 // do_bind()
#define AM_ADDR   5 // applyMethod()


// Utility macros
#define SEXP2ID(x) ((uintptr_t)x)
#define ID2SEXP(x) ((SEXP) x)
#define SXPEXISTS(x) (x && (x != R_NilValue))

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
void traceR_finish_abort(void);

void trcR_internal_emit_simple_type(SEXP expr);
void trcR_internal_emit_error_type(unsigned int type);
void trcR_internal_emit_bnd_promise(SEXP prom);
void trcR_internal_emit_unbnd_promise(SEXP prom);
void trcR_internal_emit_unbnd_promise_return(SEXP prom);
void trcR_internal_emit_prologue_start(void);
void trcR_internal_emit_prologue_end(SEXP clos);
void trcR_internal_emit_closure(SEXP clos, unsigned int type, unsigned long int alt_addr);
void trcR_internal_emit_empty_closure(SEXP clos, unsigned long int type);
void trcR_internal_emit_primitive_function(SEXP fun, unsigned int type, unsigned int bparam, unsigned int bparam_ldots);
void trcR_internal_emit_function_return(SEXP fun, SEXP ret_val);
void trcR_internal_emit_quit_seq(void);

/* Note: The arg counters are implicitly passed via globals: */
/*   trcR_by_position, trcR_by_keyword, trcR_by_dots         */
void trcR_count_closure_args(SEXP op);

void trace_context_add();
void trcR_internal_trace_context_drop(void);
void trcR_internal_change_top_context(void);
void trcR_internal_goto_top_context(void);

void print_src_addr(SEXP src);

/* a set of wrappers for fasthpathing the trace-disabled-case */
static inline void trcR_emit_simple_type(SEXP expr) {
    if (traceR_is_active)
	trcR_internal_emit_simple_type(expr);
}

static inline void trcR_emit_error_type(unsigned int type) {
    if (traceR_is_active)
	trcR_internal_emit_error_type(type);
}

static inline void trcR_emit_bnd_promise(SEXP prom) {
    if (traceR_is_active)
	trcR_internal_emit_bnd_promise(prom);
}

static inline void trcR_emit_unbnd_promise(SEXP prom) {
    if (traceR_is_active)
	trcR_internal_emit_unbnd_promise(prom);
}

static inline void trcR_emit_unbnd_promise_return(SEXP prom) {
    if (traceR_is_active)
	trcR_internal_emit_unbnd_promise_return(prom);
}

static inline void trcR_emit_prologue_start(void) {
    if (traceR_is_active)
	trcR_internal_emit_prologue_start();
}

static inline void trcR_emit_prologue_end(SEXP clos) {
    if (traceR_is_active)
	trcR_internal_emit_prologue_end(clos);
}

static inline void trcR_emit_closure(SEXP clos, unsigned int type,
				     unsigned long int alt_addr) {
    if (traceR_is_active)
	trcR_internal_emit_closure(clos, type, alt_addr);
}

static inline void trcR_emit_empty_closure(SEXP clos, unsigned long int type) {
    if (traceR_is_active)
	trcR_internal_emit_empty_closure(clos, type);
}

static inline void trcR_emit_primitive_function(SEXP fun, unsigned int type,
						unsigned int bparam,
						unsigned int bparam_ldots) {
    if (traceR_is_active)
	trcR_internal_emit_primitive_function(fun, type, bparam, bparam_ldots);
}

static inline void trcR_emit_function_return(SEXP fun, SEXP ret_val) {
    if (traceR_is_active)
	trcR_internal_emit_function_return(fun, ret_val);
}

static inline void trcR_emit_quit_seq(void) {
    if (traceR_is_active)
	trcR_internal_emit_quit_seq();
}

static inline void trcR_trace_context_drop(void) {
    if (traceR_is_active)
	trcR_internal_trace_context_drop();
}

static inline void trcR_change_top_context(void) {
    if (traceR_is_active)
	trcR_internal_change_top_context();
}

static inline void trcR_goto_top_context(void) {
    if (traceR_is_active)
	trcR_internal_goto_top_context();
}

static inline void trace_cnt_fatal_err() { fatal_err_cnt++; }


/* external call tracing */
void traceR_report_external_int(int /*NativeSymbolType*/ type,
				char *buf,
				char *name,
				void /*DL_FUNC*/ *fun);

static inline void traceR_report_external(int /*NativeSymbolType*/ type,
					  char *buf,
					  char *name,
					  void /*DL_FUNC*/ *fun) {
    if (traceR_TraceExternalCalls)
	traceR_report_external_int(type, buf, name, fun);
}

#endif /* TRACE_H_ */
