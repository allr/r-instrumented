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

#define TRACE_ZIPPED   // needs external zlib, version 1.2.5 or higher
#define TRACER_COMPILED_IN

// Output defines
#ifdef TRACE_ZIPPED
#  include <zlib.h>
#  define TRACE_NAME "trace.gz"
#  define SRC_MAP_NAME "source.map.gz"
#  define MEMORY_MAP_FILE "memory.map.gz"
#else
#  define TRACE_NAME "trace"
#  define SRC_MAP_NAME "source.map"
#  define MEMORY_MAP_FILE "memory.map"
#endif
#define SUMMARY_NAME "trace_summary"

#define EOS -1
#define MAX_FNAME 128
#define MAX_DNAME 1024
#define TIME_BUFF 200


typedef enum {
    TR_ALL,
    TR_REPL,
    TR_BOOTSTRAP,
    TR_SUMMARY,
    TR_NONE
} TR_TYPE;

#ifdef TRACE_ZIPPED
  typedef gzFile TRACEFILE;
#else
  typedef FILE * TRACEFILE;
#endif

typedef struct TraceInfo_ {
    int tracing;

    char directory[MAX_DNAME];
    char trace_file_name[MAX_FNAME];

    TRACEFILE src_map_file;

    char trace_version[12];
} TraceInfo;

/* Trace Instrumentation - global pointer for logging */
extern TraceInfo *trace_info;

unsigned int fatal_err_cnt, nonfatal_err_cnt;

// Trace output codes
#define NO_PROLOGUE		0x01
#define UNIT 			0x20
#define NEW_PROMISE		0x40
//this count starts after the basic SEXPTYPES
#define FUNC_END 		0x86
#define BND_PROM_START		0x87
#define UBND_PROM_START		0x88
#define PROM_END 		0x89
#define UNIMPL_TYPE		0x8A
/* 0x8B is unused */
#define R_ERROR_SEEN		0x8C
/* 0x8D is unused */
#define UBND			0x8E /* or 0xCE for new*/
#define BND			0x8F /* or 0xCF for new*/
#define BLTIN_ID		0x90
#define SPEC_ID			0x92
#define CLOS_ID			0x94
#define PROL_START		0x96
#define PROL_END		0x97

// Addresses for special closures
#define C_D_ADDR  1 //any context_drop()
#define UM_ADDR   2 // usemethod()
#define D_UM_ADDR 3 // do_usemethod()
#define D_B_ADDR  4 // do_bind()
#define AM_ADDR   5 // applyMethod()


// Utility macros
// note: traceR_is_active could be implemented as static inline,
//       but it's less error-prone as a macro
#define traceR_is_active (trace_info && trace_info->tracing)
#define SEXP2ID(x) ((uintptr_t)x)
#define ID2SEXP(x) ((SEXP) x)
#define SXPEXISTS(x) (x && (x != R_NilValue))

#ifdef TRACE_ZIPPED
#  define FOPEN(file) gzopen(file, "w9")
#  define FCLOSE(hdl) gzclose(hdl)
#  define WRITE_FUN(f, data, size) gzwrite(f, data, size)
#  define FPRINTF gzprintf
#else
#  define FOPEN(file) fopen(file, "w")
#  define FCLOSE(hdl) fclose(hdl)
#  define WRITE_FUN(f, data, size) fwrite(data, size, 1, f)
#  define FPRINTF fprintf
#endif

void initialize_trace_defaults(TR_TYPE mode);

void start_tracing();

void terminate_tracing();
void write_summary();
void write_trace_summary(FILE *out);
void write_allocation_summary(FILE *out);

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

void trace_context_add();
void trcR_internal_trace_context_drop(void);
void trcR_internal_change_top_context(void);
void trcR_internal_goto_top_context(void);
void goto_abs_top_context();

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
static inline void trace_cnt_nonfatal_err() { nonfatal_err_cnt++; }

static void trace_exit(int ecode);

#endif /* TRACE_H_ */
