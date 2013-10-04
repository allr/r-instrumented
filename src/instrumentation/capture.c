/*
 * capture.c
 *
 *  TODO
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
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>

#define MAX_DNAME 1024
#define CAP_FILENAME "capture"
#define CUTOFF 1000
#define MAX_NUM_LINES 5

extern TraceInfo *trace_info;
extern SEXP deparse4capture(SEXP, int);

FILE *capture_fp = NULL;

Rboolean shouldKeep(SEXP);

void capR_start_capturing() {
	if (trace_info) {
		char str[MAX_DNAME];
		sprintf(str, "%s/%s", trace_info->directory, CAP_FILENAME);
		if ((capture_fp = fopen(str, "a")) == 0) {
			print_error_msg("Couldn't open file '%s' for appending", str);
			exit(1);
		}
	}
}

void capR_stop_capturing() {
	if (capture_fp != NULL) {
		fclose(capture_fp);
	}
}

static int depth = 0;

void capR_capture(SEXP fun, SEXP args, SEXP ret, char type) {
    SEXP t;
    if (trace_info && depth == 0) { // no recursive capture
        depth++;
        if (capture_fp != 0) {
            fprintf(capture_fp, "func: %s\n", PRIMNAME(fun));
            fprintf(capture_fp, "type: %c\n", type);
            t = deparse4capture(args, CUTOFF);;
            if (shouldKeep(t)) {
                for (int i = 0; i < LENGTH(t); i++)
                    fprintf(capture_fp, "args: %s\n", CHAR(STRING_ELT(t, i)));
            } else {
                fprintf(capture_fp, "args: <arguments too long, ignored>\n");
            }
            t = deparse4capture(ret, CUTOFF);
            if (shouldKeep(t)) {
                for (int i = 0; i < LENGTH(t); i++)
                    fprintf(capture_fp, "retn: %s\n", CHAR(STRING_ELT(t, i)));
            } else {
                fprintf(capture_fp, "retn: <returns too long, ignored>\n");
            }
        } else {
            print_error_msg("Cannot write to capture file\n");
        }
        depth--;
    }
}
/*
void capR_capture(SEXP fun, SEXP args, SEXP ret, char type) {
    SEXP t;
    if (trace_info && depth == 0) { // no recursive capture
        depth++;
        if (capture_fp != 0) {
            fprintf(capture_fp, "func: %s\n", PRIMNAME(fun));
            fprintf(capture_fp, "type: %c\n", type);
            t = deparse4capture(args, CUTOFF);
            if (shouldKeep(t)) {
                for (int i = 0; i < LENGTH(t); i++)
                    fprintf(capture_fp, "args: %s\n", CHAR(STRING_ELT(t, i)));
            } else {
                fprintf(capture_fp, "args: <arguments too long, ignored>\n");
            }
            t = deparse4capture(ret, CUTOFF);
            if (shouldKeep(t)) {
                for (int i = 0; i < LENGTH(t); i++)
                    fprintf(capture_fp, "retn: %s\n", CHAR(STRING_ELT(t, i)));
            } else {
                fprintf(capture_fp, "retn: <return value too long, ignored>\n");
            }
        } else {
            print_error_msg("Cannot write to capture file\n");
        }
        depth--;
    }
}

SEXP capR_capture_args(SEXP fun, SEXP args, char type) {
    SEXP t = NULL;
    if (trace_info && depth == 0) { // no recursive capture
        depth++;
        if (capture_fp != 0) {
            fprintf(capture_fp, "func: %s\n", PRIMNAME(fun));
            fprintf(capture_fp, "type: %c\n", type);
            t = deparse4capture(args, CUTOFF);
            if (shouldKeep(t)) {
                for (int i = 0; i < LENGTH(t); i++)
                    fprintf(capture_fp, "args: %s\n", CHAR(STRING_ELT(t, i)));
            } else {
                fprintf(capture_fp, "args: <arguments too long, ignored>\n");
            }
        } else {
            print_error_msg("Cannot write to capture file\n");
        }
        depth--;
    }
    return t;
}

void capR_capture_ret1(SEXP fun, SEXP args, SEXP ret, char type) {
	SEXP t;
	if (trace_info && depth == 0) { // no recursive capture
		depth++;
		if (capture_fp != 0) {
			fprintf(capture_fp, "func: %s\n", PRIMNAME(fun));
			fprintf(capture_fp, "type: %c\n", type);
			t = args;
			if (shouldKeep(t)) {
				for (int i = 0; i < LENGTH(t); i++)
					fprintf(capture_fp, "args: %s\n", CHAR(STRING_ELT(t, i)));
			} else {
				fprintf(capture_fp, "args: <arguments too long, ignored>\n");
			}
			t = deparse4capture(ret, CUTOFF);
			if (shouldKeep(t)) {
				for (int i = 0; i < LENGTH(t); i++)
					fprintf(capture_fp, "retn: %s\n", CHAR(STRING_ELT(t, i)));
			} else {
				fprintf(capture_fp, "retn: <return value too long, ignored>\n");
			}
		} else {
			print_error_msg("Cannot write to capture file\n");
		}
		depth--;
	}
}
*/
Rboolean shouldKeep(SEXP t) {
	return (LENGTH(t) <= MAX_NUM_LINES);
}


