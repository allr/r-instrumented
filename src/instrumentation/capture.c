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
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>

#define CAP_FILENAME "capture"
#define CUTOFF 500
#define MAX_NUM_LINES 10
#define MAX_CAPTURE_FILE_SIZE 500*1024*1024
//#define MAX_CAPTURE_FILE_SIZE 10*1024*1024

extern TraceInfo *trace_info;
extern SEXP deparse4capture(SEXP, int);

static char* blacklist[] = { "lazyLoadDBfetch", };
static const int BLACKLIST_LEN = 1;

static Rboolean on_blacklist(SEXP fun) {
    for(int i=0;i<BLACKLIST_LEN;i++) {
        if (!strcmp(PRIMNAME(fun), blacklist[i])) return TRUE;
    }
    return FALSE;
}

static Rboolean shouldKeep(SEXP t) {
    return (LENGTH(t) <= MAX_NUM_LINES);
}

struct flock fl = { F_WRLCK, SEEK_SET, 0, 0, 0 };
struct stat fst;
FILE *capture_fp = NULL;
static int findex = 0;

static void lock_file(FILE *fp) {
	int fd = fileno(fp);
	fl.l_type = F_WRLCK;
	if (fcntl(fd, F_SETLKW, &fl) == -1) {
		print_error_msg("fcntl lock");
		exit(1);
	}
}

static void unlock_file(FILE *fp) {
	int fd = fileno(fp);
	fl.l_type = F_UNLCK;
	if (fcntl(fd, F_SETLK, &fl) == -1) {
		print_error_msg("fcntl unlock");
		exit(1);
	}
}

static FILE *ensure_file() {
	while (1) {
		if (capture_fp != NULL) {
			int fd = fileno(capture_fp);
			fstat(fd, &fst);
			if (fst.st_size < MAX_CAPTURE_FILE_SIZE) {
				return capture_fp;
			} else {
				fclose(capture_fp);
			}
		}
		char str[MAX_DNAME];
		sprintf(str, "%s/%s.%d", trace_info->directory, CAP_FILENAME, findex++);
		if ((capture_fp = fopen(str, "a")) == 0) {
	    print_error_msg("Couldn't open file '%s' for writing.", str);
	    exit(1);
		}
	}	
}

void capR_start_capturing() {
	if (trace_info == NULL) return;
	fl.l_pid = getpid();
}

void capR_stop_capturing() {
	if (capture_fp != NULL) {
		fclose(capture_fp);
	}
}

void capR_capture_primitive(SEXP fun, SEXP args, SEXP ret) {
    static int depth = 0;
    if (trace_info == NULL || depth > 0) return; // no recursive capture
		depth++;
		if (!on_blacklist(fun)) {
			PROTECT(args);
			PROTECT(ret);
			char type = PRIMINTERNAL(fun) ? 'I' : 'P';
			SEXP arg_exp, ret_exp;
			arg_exp = deparse4capture(args, CUTOFF);
			PROTECT(arg_exp);
			ret_exp = deparse4capture(ret, CUTOFF);
			PROTECT(ret_exp);
			FILE * fp = ensure_file();
			lock_file(fp);
			fprintf(fp, "func: %s\n", PRIMNAME(fun));
			fprintf(fp, "type: %c\n", type);
			if (shouldKeep(arg_exp)) {
				for (int i = 0; i < LENGTH(arg_exp); i++) {
					fprintf(fp, "args: %s\n", CHAR(STRING_ELT(arg_exp, i)));
				}
			} else {
				fprintf(fp, "args: <arguments too long, ignored>\n");
			}
			if (shouldKeep(ret_exp)) {
				for (int i = 0; i < LENGTH(ret_exp); i++) {
					fprintf(fp, "retn: %s\n", CHAR(STRING_ELT(ret_exp, i)));
				}
			} else {
				fprintf(fp, "retn: <returns too long, ignored>\n");
			}
			fflush(fp);
			unlock_file(fp);
			UNPROTECT(4);
		}
		depth--;
}



