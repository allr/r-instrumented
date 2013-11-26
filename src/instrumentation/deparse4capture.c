/*
 *  R : A Computer Language for Statistical Data Analysis
 *  Copyright (C) 1995, 1996  Robert Gentleman and Ross Ihaka
 *  Copyright (C) 1997--2012  The R Core Team
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
 *
 *  IMPLEMENTATION NOTES:
 *
 *  Deparsing has 3 layers.  The user interface, do_deparse, should
 *  not be called from an internal function, the actual deparsing needs
 *  to be done twice, once to count things up and a second time to put
 *  them into the string vector for return.  Printing this to a file
 *  is handled by the calling routine.
 *
 *
 *  INDENTATION:
 *
 *  Indentation is carried out in the routine printtab2buff at the
 *  botton of this file.  It seems like this should be settable via
 *  options.
 *
 *
 *  GLOBAL VARIABLES:
 *
 *  linenumber:	 counts the number of lines that have been written,
 *		 this is used to setup storage for deparsing.
 *
 *  len:	 counts the length of the current line, it will be
 *		 used to determine when to break lines.
 *
 *  incurly:	 keeps track of whether we are inside a curly or not,
 *		 this affects the printing of if-then-else.
 *
 *  inlist:	 keeps track of whether we are inside a list or not,
 *		 this affects the printing of if-then-else.
 *
 *  startline:	 indicator TRUE=start of a line (so we can tab out to
 *		 the correct place).
 *
 *  indent:	 how many tabs should be written at the start of
 *		 a line.
 *
 *  buff:	 contains the current string, we attempt to break
 *		 lines at cutoff, but can unlimited length.
 *
 *  lbreak:	 often used to indicate whether a line has been
 *		 broken, this makes sure that that indenting behaves
 *		 itself.
 */

/*
 * The code here used to use static variables to share values
 * across the different routines. These have now been collected
 * into a struct named  LocalParseData and this is explicitly
 * passed between the different routines. This avoids the needs
 * for the global variables and allows multiple evaluators, potentially
 * in different threads, to work on their own independent copies
 * that are local to their call stacks. This avoids any issues
 * with interrupts, etc. not restoring values.

 * The previous issue with the global "cutoff" variable is now implemented
 * by creating a deparse1WithCutoff() routine which takes the cutoff from
 * the caller and passes this to the different routines as a member of the
 * LocalParseData struct. Access to the deparse1() routine remains unaltered.
 * This is exactly as Ross had suggested ...
 *
 * One possible fix is to restructure the code with another function which
 * takes a cutoff value as a parameter.	 Then "do_deparse" and "deparse1"
 * could each call this deeper function with the appropriate argument.
 * I wonder why I didn't just do this? -- it would have been quicker than
 * writing this note.  I guess it needs a bit more thought ...
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define R_USE_SIGNALS 1
#include <Defn.h>
#include <Internal.h>
#include <float.h> /* for DBL_DIG */
#include <Print.h>
#include <Fileio.h>

#define BUFSIZE 512

#define MIN_Cutoff 20
#define DEFAULT_Cutoff 60
#define MAX_Cutoff (BUFSIZE - 12)
/* ----- MAX_Cutoff  <	BUFSIZE !! */

#include "../main/RBufferUtils.h"

typedef R_StringBuffer DeparseBuffer;

typedef struct {
    int linenumber;
    int len; // FIXME: size_t
    int incurly;
    int inlist;
    Rboolean startline; /* = TRUE; */
    int indent;
    SEXP strvec;

    DeparseBuffer buffer;

    int cutoff;
    int backtick;
    int opts;
    int sourceable;
    int longstring;
    int maxlines;
    Rboolean active;
    int isS4;
} LocalParseData;

static SEXP deparse1WithCutoff(SEXP call, Rboolean abbrev, int cutoff,
        Rboolean backtick, int opts, int nlines);
static void args2buff(SEXP, int, int, LocalParseData *);
static void deparse2buff(SEXP, LocalParseData *);
static void print2buff(const char *, LocalParseData *);
static void printtab2buff(int, LocalParseData *);
static void writeline(LocalParseData *);
static void vector2buff(SEXP, LocalParseData *);
static void src2buff1(SEXP, LocalParseData *);
static Rboolean src2buff(SEXP, int, LocalParseData *);
static void vec2buff(SEXP, LocalParseData *);
static void linebreak(Rboolean *lbreak, LocalParseData *);
static void deparse2(SEXP, SEXP, LocalParseData *);

attribute_hidden
SEXP deparse4capture(SEXP call, int cutoff) {
    int opts = KEEPINTEGER | QUOTEEXPRESSIONS | SHOWATTRIBUTES | KEEPNA;
    return deparse1WithCutoff(call, FALSE, cutoff, TRUE, opts, -1);;
}

static SEXP deparse1WithCutoff(SEXP call, Rboolean abbrev, int cutoff,
        Rboolean backtick, int opts, int nlines) {
    /* Arg. abbrev:
     If abbrev is TRUE, then the returned value
     is a STRSXP of length 1 with at most 13 characters.
     This is used for plot labelling etc.
     */
    SEXP svec;
    int savedigits;
    Rboolean need_ellipses = FALSE;
    LocalParseData localData = { 0, 0, 0, 0, /*startline = */TRUE, 0, NULL,
    /*DeparseBuffer=*/{ NULL, 0, BUFSIZE }, DEFAULT_Cutoff, FALSE, 0, TRUE,
            FALSE, INT_MAX, TRUE, 0 };
    localData.cutoff = cutoff;
    localData.backtick = backtick;
    localData.opts = opts;
    localData.strvec = R_NilValue;

    PrintDefaults(); /* from global options() */
    savedigits = R_print.digits;
    R_print.digits = DBL_DIG;/* MAX precision */

    svec = R_NilValue;
    if (nlines > 0) {
        localData.linenumber = localData.maxlines = nlines;
    } else {
        deparse2(call, svec, &localData);/* just to determine linenumber..*/
        localData.active = TRUE;
        if (R_BrowseLines > 0 && localData.linenumber > R_BrowseLines) {
            localData.linenumber = localData.maxlines = R_BrowseLines + 1;
            need_ellipses = TRUE;
        }
    }
    PROTECT(svec = allocVector(STRSXP, localData.linenumber));
    deparse2(call, svec, &localData);
    if (abbrev) {
        char data[14];
        strncpy(data, CHAR(STRING_ELT(svec, 0)), 10);
        data[10] = '\0';
        if (strlen(CHAR(STRING_ELT(svec, 0))) > 10)
            strcat(data, "...");
        svec = mkString(data);
    } else if (need_ellipses) {
        SET_STRING_ELT(svec, R_BrowseLines, mkChar("  ..."));
    }
    if (nlines > 0 && localData.linenumber < nlines)
        svec = lengthgets(svec, localData.linenumber);
    UNPROTECT(1);
    PROTECT(svec);
    /* protect from warning() allocating, PR#14356 */
    R_print.digits = savedigits;
    if ((opts & WARNINCOMPLETE) && localData.isS4)
        warning(_("deparse of an S4 object will not be source()able"));
    else if ((opts & WARNINCOMPLETE) && !localData.sourceable)
        warning(_("deparse may be incomplete"));
    if ((opts & WARNINCOMPLETE) && localData.longstring)
        warning(_("deparse may be not be source()able in R < 2.7.0"));
    /* somewhere lower down might have allocated ... */
    R_FreeStringBuffer(&(localData.buffer));
    UNPROTECT(1);
    return svec;
}

#include "Rconnections.h"

static void linebreak(Rboolean *lbreak, LocalParseData *d) {
    if (d->len > d->cutoff) {
        if (!*lbreak) {
            *lbreak = TRUE;
            d->indent++;
        }
        writeline(d);
    }
}

static void deparse2(SEXP what, SEXP svec, LocalParseData *d) {
    d->strvec = svec;
    d->linenumber = 0;
    d->indent = 0;
    deparse2buff(what, d);
    writeline(d);
}

/* curlyahead looks at s to see if it is a list with
 the first op being a curly.  You need this kind of
 lookahead info to print if statements correctly.  */
static Rboolean curlyahead(SEXP s) {
    if (isList(s) || isLanguage(s))
        if (TYPEOF(CAR(s)) == SYMSXP && CAR(s) == R_BraceSymbol)
            return TRUE;
    return FALSE;
}

/* needsparens looks at an arg to a unary or binary operator to
 determine if it needs to be parenthesized when deparsed
 mainop is a unary or binary operator,
 arg is an argument to it, on the left if left == 1 */

static Rboolean needsparens(PPinfo mainop, SEXP arg, unsigned int left) {
    PPinfo arginfo;
    if (TYPEOF(arg) == LANGSXP) {
        if (TYPEOF(CAR(arg)) == SYMSXP) {
            if ((TYPEOF(SYMVALUE(CAR(arg))) == BUILTINSXP)
                    || (TYPEOF(SYMVALUE(CAR(arg))) == SPECIALSXP)) {
                arginfo = PPINFO(SYMVALUE(CAR(arg)));
                switch (arginfo.kind) {
                case PP_BINARY: /* Not all binary ops are binary! */
                case PP_BINARY2:
                    switch (length(CDR(arg))) {
                    case 1:
                        if (!left)
                            return FALSE;
                        if (arginfo.precedence == PREC_SUM) /* binary +/- precedence upgraded as unary */
                            arginfo.precedence = PREC_SIGN;
                    case 2:
                        break;
                    default:
                        return FALSE;
                    }
                case PP_ASSIGN:
                case PP_ASSIGN2:
                case PP_SUBSET:
                case PP_UNARY:
                case PP_DOLLAR:
                    if (mainop.precedence > arginfo.precedence
                            || (mainop.precedence == arginfo.precedence
                                    && left == mainop.rightassoc)) {
                        return TRUE;
                    }
                    break;
                case PP_FOR:
                case PP_IF:
                case PP_WHILE:
                case PP_REPEAT:
                    return left == 1;
                    break;
                default:
                    return FALSE;
                }
            } else if (isUserBinop(CAR(arg))) {
                if (mainop.precedence > PREC_PERCENT
                        || (mainop.precedence == PREC_PERCENT
                                && left == mainop.rightassoc)) {
                    return TRUE;
                }
            }
        }
    } else if ((TYPEOF(arg) == CPLXSXP) && (length(arg) == 1)) {
        if (mainop.precedence > PREC_SUM
                || (mainop.precedence == PREC_SUM && left == mainop.rightassoc)) {
            return TRUE;
        }
    }
    return FALSE;
}

/* check for attributes other than function source */
static Rboolean hasAttributes(SEXP s) {
    SEXP a = ATTRIB(s);
    if (length(a) > 2)
        return (TRUE);
    while (!isNull(a)) {
        if (TAG(a) != R_SrcrefSymbol
                && (TYPEOF(s) != CLOSXP || TAG(a) != R_SourceSymbol))
            return (TRUE);
        a = CDR(a);
    }
    return (FALSE);
}

static void attr1(SEXP s, LocalParseData *d) {
    if (hasAttributes(s))
        print2buff("structure(", d);
}

static void attr2(SEXP s, LocalParseData *d) {
    int localOpts = d->opts;

    if (hasAttributes(s)) {
        SEXP a = ATTRIB(s);
        while (!isNull(a)) {
            if (TAG(a) != R_SourceSymbol && TAG(a) != R_SrcrefSymbol) {
                print2buff(", ", d);
                if (TAG(a) == R_DimSymbol) {
                    print2buff(".Dim", d);
                } else if (TAG(a) == R_DimNamesSymbol) {
                    print2buff(".Dimnames", d);
                } else if (TAG(a) == R_NamesSymbol) {
                    print2buff(".Names", d);
                } else if (TAG(a) == R_TspSymbol) {
                    print2buff(".Tsp", d);
                } else if (TAG(a) == R_LevelsSymbol) {
                    print2buff(".Label", d);
                } else {
                    /* TAG(a) might contain spaces etc */
                    const char *tag = CHAR(PRINTNAME(TAG(a)));
                    d->opts = SIMPLEDEPARSE; /* turn off quote()ing */
                    if (isValidName(tag))
                        deparse2buff(TAG(a), d);
                    else {
                        print2buff("\"", d);
                        deparse2buff(TAG(a), d);
                        print2buff("\"", d);
                    }
                    d->opts = localOpts;
                }
                print2buff(" = ", d);
                deparse2buff(CAR(a), d);
            }
            a = CDR(a);
        }
        print2buff(")", d);
    }
}

static void printcomment(SEXP s, LocalParseData *d) {
    SEXP cmt;
    int i, ncmt;

    /* look for old-style comments first */

    if (isList(TAG(s)) && !isNull(TAG(s))) {
        for (s = TAG(s); s != R_NilValue; s = CDR(s)) {
            print2buff(translateChar(STRING_ELT(CAR(s), 0)), d);
            writeline(d);
        }
    } else {
        cmt = getAttrib(s, R_CommentSymbol);
        ncmt = length(cmt);
        for (i = 0; i < ncmt; i++) {
            print2buff(translateChar(STRING_ELT(cmt, i)), d);
            writeline(d);
        }
    }
}

static const char * quotify(SEXP name, int quote) {
    const char *s = CHAR(name);

    /* If a symbol is not a valid name, put it in quotes, escaping
     * any quotes in the string itself */

    if (isValidName(s) || *s == '\0')
        return s;

    return EncodeString(name, 0, quote, Rprt_adj_none);
}

/* check for whether we need to parenthesize a caller.  The unevaluated ones
 are tricky:
 We want
 x$f(z)
 x[n](z)
 base::mean(x)
 but
 (f+g)(z)
 (function(x) 1)(x)
 etc.
 */
static Rboolean parenthesizeCaller(SEXP s) {
    SEXP op, sym;
    if (TYPEOF(s) == LANGSXP) { /* unevaluated */
        op = CAR(s);
        if (TYPEOF(op) == SYMSXP) {
            if (isUserBinop(op))
                return TRUE; /* %foo% */
            sym = SYMVALUE(op);
            if (TYPEOF(sym) == BUILTINSXP || TYPEOF(sym) == SPECIALSXP) {
                if (PPINFO(sym).precedence >= PREC_DOLLAR
                        || PPINFO(sym).kind == PP_FUNCALL
                        || PPINFO(sym).kind == PP_PAREN
                        || PPINFO(sym).kind == PP_CURLY)
                    return FALSE; /* x$f(z) or x[n](z) or f(z) or (f) or {f} */
                else
                    return TRUE; /* (f+g)(z) etc. */
            }
            return FALSE; /* regular function call */
        } else
            return TRUE; /* something strange, like (1)(x) */
    } else
        return TYPEOF(s) == CLOSXP;
}

/* This is the recursive part of deparsing. */

#define SIMPLE_OPTS (~QUOTEEXPRESSIONS & ~SHOWATTRIBUTES & ~DELAYPROMISES)
/* keep KEEPINTEGER | USESOURCE | KEEPNA | S_COMPAT, also
 WARNINCOMPLETE but that is not used below this point. */

static void deparse2buff(SEXP s, LocalParseData *d) {
    PPinfo fop;
    Rboolean lookahead = FALSE, lbreak = FALSE, parens;
    SEXP op, t;
    int localOpts = d->opts, i, n;
    Rboolean alwaysDelay;

    if (!d->active)
        return;

    if (IS_S4_OBJECT(s))
        d->isS4 = TRUE;

    switch (TYPEOF(s)) {
    case NILSXP:
        print2buff("NULL", d);
        break;
    case SYMSXP:
        // capture:
        if (localOpts & QUOTEEXPRESSIONS) {
            d->sourceable = FALSE;
            print2buff("<symbol>", d);
            break;
        }
        if (localOpts & QUOTEEXPRESSIONS) {
            attr1(s, d);
            print2buff("quote(", d);
        }
        if (localOpts & S_COMPAT) {
            print2buff(quotify(PRINTNAME(s), '"'), d);
        } else if (d->backtick)
            print2buff(quotify(PRINTNAME(s), '`'), d);
        else
            print2buff(CHAR(PRINTNAME(s)), d);
        if (localOpts & QUOTEEXPRESSIONS) {
            print2buff(")", d);
            attr2(s, d);
        }
        break;
    case CHARSXP: {
        const char *ts = translateChar(s);
        /* versions of R < 2.7.0 cannot parse strings longer than 8192 chars */
        if (strlen(ts) >= 8192)
            d->longstring = TRUE;
        print2buff(ts, d);
        break;
    }
    case SPECIALSXP:
    case BUILTINSXP:
        print2buff(".Primitive(\"", d);
        print2buff(PRIMNAME(s), d);
        print2buff("\")", d);
        break;
    case PROMSXP:
        // CAPTURE: we don't want to eval promise
        // if promise not evaluated yet, don't force it.
        alwaysDelay = (PRVALUE(s) == R_UnboundValue);
        if ((d->opts & DELAYPROMISES) || alwaysDelay) {
            d->sourceable = FALSE;
            print2buff("<promise: ", d);
            d->opts &= ~QUOTEEXPRESSIONS; /* don't want delay(quote()) */
            deparse2buff(PREXPR(s), d);
            d->opts = localOpts;
            print2buff(">", d);
        } else {
            PROTECT(s = eval(s, NULL));
            /* eval uses env of promise */
            deparse2buff(s, d);
            UNPROTECT(1);
        }
        break;
    case CLOSXP:
        // capture
        d->sourceable = FALSE;
        print2buff("<closure>", d);
        break;
        // CAPTURE: just put a mark here rather than output the source
        if (localOpts & SHOWATTRIBUTES)
            attr1(s, d);
        if ((d->opts & USESOURCE) && !isNull(t = getAttrib(s, R_SrcrefSymbol)))
            src2buff1(t, d);
        else {
            /* We have established that we don't want to use the
             source for this function */
            d->opts &= SIMPLE_OPTS & ~USESOURCE;
            print2buff("function (", d);
            args2buff(FORMALS(s), 0, 1, d);
            print2buff(") ", d);

            writeline(d);
            deparse2buff(BODY_EXPR(s), d);
            d->opts = localOpts;
        }
        if (localOpts & SHOWATTRIBUTES)
            attr2(s, d);
        break;
    case ENVSXP:
        d->sourceable = FALSE;
        print2buff("<environment>", d);
        break;
    case VECSXP:
        if (localOpts & SHOWATTRIBUTES)
            attr1(s, d);
        print2buff("list(", d);
        vec2buff(s, d);
        print2buff(")", d);
        if (localOpts & SHOWATTRIBUTES)
            attr2(s, d);
        break;
    case EXPRSXP:
        // capture
        d->sourceable = FALSE;
        print2buff("<expression>", d);
        break;
        // CAPTURE: just put a mark here rather than output the source
        if (localOpts & SHOWATTRIBUTES)
            attr1(s, d);
        if (length(s) <= 0)
            print2buff("expression()", d);
        else {
            print2buff("expression(", d);
            d->opts &= SIMPLE_OPTS;
            vec2buff(s, d);
            d->opts = localOpts;
            print2buff(")", d);
        }
        if (localOpts & SHOWATTRIBUTES)
            attr2(s, d);
        break;
    case LISTSXP:
        if (localOpts & SHOWATTRIBUTES)
            attr1(s, d);
        print2buff("list(", d);
        d->inlist++;
        for (t = s; CDR(t) != R_NilValue; t = CDR(t)) {
            if (TAG(t) != R_NilValue) {
                d->opts = SIMPLEDEPARSE; /* turn off quote()ing */
                deparse2buff(TAG(t), d);
                d->opts = localOpts;
                print2buff(" = ", d);
            }
            deparse2buff(CAR(t), d);
            print2buff(", ", d);
        }
        if (TAG(t) != R_NilValue) {
            d->opts = SIMPLEDEPARSE; /* turn off quote()ing */
            deparse2buff(TAG(t), d);
            d->opts = localOpts;
            print2buff(" = ", d);
        }
        deparse2buff(CAR(t), d);
        print2buff(")", d);
        d->inlist--;
        if (localOpts & SHOWATTRIBUTES)
            attr2(s, d);
        break;
    case LANGSXP:
        // capture
        d->sourceable = FALSE;
        print2buff("<call>", d);
        break;
        // CAPTURE: just put a mark here rather than output the source
        printcomment(s, d);
        if (!isNull(ATTRIB(s)))
            d->sourceable = FALSE;
        if (localOpts & QUOTEEXPRESSIONS) {
            print2buff("quote(", d);
            d->opts &= SIMPLE_OPTS;
        }
        if (TYPEOF(CAR(s)) == SYMSXP) {
            int userbinop = 0;
            op = CAR(s);
            if ((TYPEOF(SYMVALUE(op)) == BUILTINSXP)
                    || (TYPEOF(SYMVALUE(op)) == SPECIALSXP) || (userbinop =
                            isUserBinop(op))) {
                if (userbinop) {
                    fop.kind = PP_BINARY2; /* not quite right for spacing, but can't be unary */
                    fop.precedence = PREC_PERCENT;
                    fop.rightassoc = 0;
                } else
                    fop = PPINFO(SYMVALUE(op));
                s = CDR(s);
                if (fop.kind == PP_BINARY) {
                    switch (length(s)) {
                    case 1:
                        fop.kind = PP_UNARY;
                        if (fop.precedence == PREC_SUM) /* binary +/- precedence upgraded as unary */
                            fop.precedence = PREC_SIGN;
                        break;
                    case 2:
                        break;
                    default:
                        fop.kind = PP_FUNCALL;
                        break;
                    }
                } else if (fop.kind == PP_BINARY2) {
                    if (length(s) != 2)
                        fop.kind = PP_FUNCALL;
                    else if (userbinop)
                        fop.kind = PP_BINARY;
                }
                switch (fop.kind) {
                case PP_IF:
                    print2buff("if (", d);
                    /* print the predicate */
                    deparse2buff(CAR(s), d);
                    print2buff(") ", d);
                    if (d->incurly && !d->inlist) {
                        lookahead = curlyahead(CAR(CDR(s)));
                        if (!lookahead) {
                            writeline(d);
                            d->indent++;
                        }
                    }
                    /* need to find out if there is an else */
                    if (length(s) > 2) {
                        deparse2buff(CAR(CDR(s)), d);
                        if (d->incurly && !d->inlist) {
                            writeline(d);
                            if (!lookahead)
                                d->indent--;
                        } else
                            print2buff(" ", d);
                        print2buff("else ", d);
                        deparse2buff(CAR(CDDR(s)), d);
                    } else {
                        deparse2buff(CAR(CDR(s)), d);
                        if (d->incurly && !lookahead && !d->inlist)
                            d->indent--;
                    }
                    break;
                case PP_WHILE:
                    print2buff("while (", d);
                    deparse2buff(CAR(s), d);
                    print2buff(") ", d);
                    deparse2buff(CADR(s), d);
                    break;
                case PP_FOR:
                    print2buff("for (", d);
                    deparse2buff(CAR(s), d);
                    print2buff(" in ", d);
                    deparse2buff(CADR(s), d);
                    print2buff(") ", d);
                    deparse2buff(CADR(CDR(s)), d);
                    break;
                case PP_REPEAT:
                    print2buff("repeat ", d);
                    deparse2buff(CAR(s), d);
                    break;
                case PP_CURLY:
                    print2buff("{", d);
                    d->incurly += 1;
                    d->indent++;
                    writeline(d);
                    while (s != R_NilValue) {
                        deparse2buff(CAR(s), d);
                        writeline(d);
                        s = CDR(s);
                    }
                    d->indent--;
                    print2buff("}", d);
                    d->incurly -= 1;
                    break;
                case PP_PAREN:
                    print2buff("(", d);
                    deparse2buff(CAR(s), d);
                    print2buff(")", d);
                    break;
                case PP_SUBSET:
                    deparse2buff(CAR(s), d);
                    if (PRIMVAL(SYMVALUE(op)) == 1)
                        print2buff("[", d);
                    else
                        print2buff("[[", d);
                    args2buff(CDR(s), 0, 0, d);
                    if (PRIMVAL(SYMVALUE(op)) == 1)
                        print2buff("]", d);
                    else
                        print2buff("]]", d);
                    break;
                case PP_FUNCALL:
                case PP_RETURN:
                    if (d->backtick)
                        print2buff(quotify(PRINTNAME(op), '`'), d);
                    else
                        print2buff(quotify(PRINTNAME(op), '"'), d);
                    print2buff("(", d);
                    d->inlist++;
                    args2buff(s, 0, 0, d);
                    d->inlist--;
                    print2buff(")", d);
                    break;
                case PP_FOREIGN:
                    print2buff(CHAR(PRINTNAME(op)), d); /* ASCII */
                    print2buff("(", d);
                    d->inlist++;
                    args2buff(s, 1, 0, d);
                    d->inlist--;
                    print2buff(")", d);
                    break;
                case PP_FUNCTION:
                    printcomment(s, d);
                    if (!(d->opts & USESOURCE) || !isString(CADDR(s))) {
                        print2buff(CHAR(PRINTNAME(op)), d); /* ASCII */
                        print2buff("(", d);
                        args2buff(FORMALS(s), 0, 1, d);
                        print2buff(") ", d);
                        deparse2buff(CADR(s), d);
                    } else {
                        s = CADDR(s);
                        n = length(s);
                        for (i = 0; i < n; i++) {
                            print2buff(translateChar(STRING_ELT(s, i)), d);
                            writeline(d);
                        }
                    }
                    break;
                case PP_ASSIGN:
                case PP_ASSIGN2:
                    if ((parens = needsparens(fop, CAR(s), 1)))
                        print2buff("(", d);
                    deparse2buff(CAR(s), d);
                    if (parens)
                        print2buff(")", d);
                    print2buff(" ", d);
                    print2buff(CHAR(PRINTNAME(op)), d); /* ASCII */
                    print2buff(" ", d);
                    if ((parens = needsparens(fop, CADR(s), 0)))
                        print2buff("(", d);
                    deparse2buff(CADR(s), d);
                    if (parens)
                        print2buff(")", d);
                    break;
                case PP_DOLLAR:
                    if ((parens = needsparens(fop, CAR(s), 1)))
                        print2buff("(", d);
                    deparse2buff(CAR(s), d);
                    if (parens)
                        print2buff(")", d);
                    print2buff(CHAR(PRINTNAME(op)), d); /* ASCII */
                    /*temp fix to handle printing of x$a's */
                    if (isString(CADR(s))
                            && isValidName(CHAR(STRING_ELT(CADR(s), 0))))
                        deparse2buff(STRING_ELT(CADR(s), 0), d);
                    else {
                        if ((parens = needsparens(fop, CADR(s), 0)))
                            print2buff("(", d);
                        deparse2buff(CADR(s), d);
                        if (parens)
                            print2buff(")", d);
                    }
                    break;
                case PP_BINARY:
                    if ((parens = needsparens(fop, CAR(s), 1)))
                        print2buff("(", d);
                    deparse2buff(CAR(s), d);
                    if (parens)
                        print2buff(")", d);
                    print2buff(" ", d);
                    print2buff(CHAR(PRINTNAME(op)), d); /* ASCII */
                    print2buff(" ", d);
                    linebreak(&lbreak, d);
                    if ((parens = needsparens(fop, CADR(s), 0)))
                        print2buff("(", d);
                    deparse2buff(CADR(s), d);
                    if (parens)
                        print2buff(")", d);
                    if (lbreak) {
                        d->indent--;
                        lbreak = FALSE;
                    }
                    break;
                case PP_BINARY2: /* no space between op and args */
                    if ((parens = needsparens(fop, CAR(s), 1)))
                        print2buff("(", d);
                    deparse2buff(CAR(s), d);
                    if (parens)
                        print2buff(")", d);
                    print2buff(CHAR(PRINTNAME(op)), d); /* ASCII */
                    if ((parens = needsparens(fop, CADR(s), 0)))
                        print2buff("(", d);
                    deparse2buff(CADR(s), d);
                    if (parens)
                        print2buff(")", d);
                    break;
                case PP_UNARY:
                    print2buff(CHAR(PRINTNAME(op)), d); /* ASCII */
                    if ((parens = needsparens(fop, CAR(s), 0)))
                        print2buff("(", d);
                    deparse2buff(CAR(s), d);
                    if (parens)
                        print2buff(")", d);
                    break;
                case PP_BREAK:
                    print2buff("break", d);
                    break;
                case PP_NEXT:
                    print2buff("next", d);
                    break;
                case PP_SUBASS:
                    if (d->opts & S_COMPAT) {
                        print2buff("\"", d);
                        print2buff(CHAR(PRINTNAME(op)), d); /* ASCII */
                        print2buff("\'(", d);
                    } else {
                        print2buff("`", d);
                        print2buff(CHAR(PRINTNAME(op)), d); /* ASCII */
                        print2buff("`(", d);
                    }
                    args2buff(s, 0, 0, d);
                    print2buff(")", d);
                    break;
                default:
                    d->sourceable = FALSE;
                    UNIMPLEMENTED("deparse2buff");
                }
            } else {
                SEXP val = R_NilValue; /* -Wall */
                if (isSymbol(CAR(s))) {
                    val = SYMVALUE(CAR(s));
                    if (TYPEOF(val) == PROMSXP) {
                        // CAPTURE: don't force promise
                        if (PRVALUE(val) != R_UnboundValue) {
//                            val = eval(val, R_BaseEnv);
                            val = PRVALUE(val);
                        } else {
                            d->sourceable = FALSE;
                            print2buff("<promise: ", d);
                            d->opts &= ~QUOTEEXPRESSIONS; /* don't want delay(quote()) */
                            deparse2buff(PREXPR(val), d);
                            d->opts = localOpts;
                            print2buff("> ", d);
                        }
                    }
                }
                if (isSymbol(CAR(s)) && TYPEOF(val) == CLOSXP
                && streql(CHAR(PRINTNAME(CAR(s))), "::")) { /*  :: is special case */
                    deparse2buff(CADR(s), d);
                    print2buff("::", d);
                    deparse2buff(CADDR(s), d);
                } else if (isSymbol(CAR(s)) && TYPEOF(val) == CLOSXP
                && streql(CHAR(PRINTNAME(CAR(s))), ":::")) { /*  ::: is special case */
                    deparse2buff(CADR(s), d);
                    print2buff(":::", d);
                    deparse2buff(CADDR(s), d);
                } else {
                    if (isSymbol(CAR(s))) {
                        if (d->opts & S_COMPAT)
                            print2buff(quotify(PRINTNAME(CAR(s)), '\''), d);
                        else
                            print2buff(quotify(PRINTNAME(CAR(s)), '`'), d);
                    } else
                        deparse2buff(CAR(s), d);
                    print2buff("(", d);
                    args2buff(CDR(s), 0, 0, d);
                    print2buff(")", d);
                }
            }
        } else if (TYPEOF(CAR(s)) == CLOSXP || TYPEOF(CAR(s)) == SPECIALSXP
                || TYPEOF(CAR(s)) == BUILTINSXP) {
            if (parenthesizeCaller(CAR(s))) {
                print2buff("(", d);
                deparse2buff(CAR(s), d);
                print2buff(")", d);
            } else
                deparse2buff(CAR(s), d);
            print2buff("(", d);
            args2buff(CDR(s), 0, 0, d);
            print2buff(")", d);
        } else { /* we have a lambda expression */
            if (parenthesizeCaller(CAR(s))) {
                print2buff("(", d);
                deparse2buff(CAR(s), d);
                print2buff(")", d);
            } else
                deparse2buff(CAR(s), d);
            print2buff("(", d);
            args2buff(CDR(s), 0, 0, d);
            print2buff(")", d);
        }
        if (localOpts & QUOTEEXPRESSIONS) {
            d->opts = localOpts;
            print2buff(")", d);
        }
        break;
    case STRSXP:
    case LGLSXP:
    case INTSXP:
    case REALSXP:
    case CPLXSXP:
    case RAWSXP:
        if (localOpts & SHOWATTRIBUTES)
            attr1(s, d);
        vector2buff(s, d);
        if (localOpts & SHOWATTRIBUTES)
            attr2(s, d);
        break;
    case EXTPTRSXP: {
        char tpb[32]; /* need 12+2+2*sizeof(void*) */
        d->sourceable = FALSE;
        snprintf(tpb, 32, "<pointer: %p>", R_ExternalPtrAddr(s));
        tpb[31] = '\0';
        print2buff(tpb, d);
    }
        break;
    case BCODESXP:
        d->sourceable = FALSE;
        print2buff("<bytecode>", d);
        break;
    case WEAKREFSXP:
        d->sourceable = FALSE;
        print2buff("<weak reference>", d);
        break;
    case S4SXP:
        d->sourceable = FALSE;
        d->isS4 = TRUE;
        print2buff("<S4 object of class ", d);
        deparse2buff(getAttrib(s, R_ClassSymbol), d);
        print2buff(">", d);
        break;
    default:
        d->sourceable = FALSE;
        UNIMPLEMENTED_TYPE("deparse2buff", s);
    }
}

/* If there is a string array active point to that, and */
/* otherwise we are counting lines so don't do anything. */

static void writeline(LocalParseData *d) {
    if (d->strvec != R_NilValue && d->linenumber < d->maxlines)
        SET_STRING_ELT(d->strvec, d->linenumber, mkChar(d->buffer.data));
    d->linenumber++;
    if (d->linenumber >= d->maxlines)
        d->active = FALSE;
    /* reset */
    d->len = 0;
    d->buffer.data[0] = '\0';
    d->startline = TRUE;
}

static void print2buff(const char *strng, LocalParseData *d) {
    size_t tlen, bufflen;

    if (d->startline) {
        d->startline = FALSE;
        printtab2buff(d->indent, d); /*if at the start of a line tab over */
    }
    tlen = strlen(strng);
    R_AllocStringBuffer(0, &(d->buffer));
    bufflen = strlen(d->buffer.data);
    R_AllocStringBuffer(bufflen + tlen, &(d->buffer));
    strcat(d->buffer.data, strng);
    d->len += (int) tlen;
}

static void vector2buff(SEXP vector, LocalParseData *d) {
    int tlen, i, quote;
    const char *strp;
    Rboolean surround = FALSE, allNA, addL = TRUE;

    tlen = length(vector);
    if (isString(vector))
        quote = '"';
    else
        quote = 0;
    if (tlen == 0) {
        switch (TYPEOF(vector)) {
        case LGLSXP:
            print2buff("logical(0)", d);
            break;
        case INTSXP:
            print2buff("integer(0)", d);
            break;
        case REALSXP:
            print2buff("numeric(0)", d);
            break;
        case CPLXSXP:
            print2buff("complex(0)", d);
            break;
        case STRSXP:
            print2buff("character(0)", d);
            break;
        case RAWSXP:
            print2buff("raw(0)", d);
            break;
        default:
            UNIMPLEMENTED_TYPE("vector2buff", vector);
        }
    } else if (TYPEOF(vector) == INTSXP) {
        /* We treat integer separately, as S_compatible is relevant.

         Also, it is neat to deparse m:n in that form,
         so we do so as from 2.5.0.
         */
        Rboolean intSeq = (tlen > 1);
        int *tmp = INTEGER(vector);

        for (i = 1; i < tlen; i++) {
            if (tmp[i] - tmp[i - 1] != 1) {
                intSeq = FALSE;
                break;
            }
        }
        if (intSeq) {
            strp = EncodeElement(vector, 0, '"', '.');
            print2buff(strp, d);
            print2buff(":", d);
            strp = EncodeElement(vector, tlen - 1, '"', '.');
            print2buff(strp, d);
        } else {
            addL = d->opts & KEEPINTEGER & !(d->opts & S_COMPAT);
            allNA = (d->opts & KEEPNA) || addL;
            for (i = 0; i < tlen; i++)
                if (tmp[i] != NA_INTEGER) {
                    allNA = FALSE;
                    break;
                }
            if ((d->opts & KEEPINTEGER && (d->opts & S_COMPAT))) {
                surround = TRUE;
                print2buff("as.integer(", d);
            }
            allNA = allNA && !(d->opts & S_COMPAT);
            if (tlen > 1)
                print2buff("c(", d);
            for (i = 0; i < tlen; i++) {
                if (allNA && tmp[i] == NA_INTEGER) {
                    print2buff("NA_integer_", d);
                } else {
                    strp = EncodeElement(vector, i, quote, '.');
                    print2buff(strp, d);
                    if (addL && tmp[i] != NA_INTEGER)
                        print2buff("L", d);
                }
                if (i < (tlen - 1))
                    print2buff(", ", d);
                if (tlen > 1 && d->len > d->cutoff)
                    writeline(d);
                if (!d->active)
                    break;
            }
            if (tlen > 1)
                print2buff(")", d);
            if (surround)
                print2buff(")", d);
        }
    } else {
        allNA = d->opts & KEEPNA;
        if ((d->opts & KEEPNA) && TYPEOF(vector) == REALSXP) {
            for (i = 0; i < tlen; i++)
                if (!ISNA(REAL(vector)[i])) {
                    allNA = FALSE;
                    break;
                }
            if (allNA && (d->opts & S_COMPAT)) {
                surround = TRUE;
                print2buff("as.double(", d);
            }
        } else if ((d->opts & KEEPNA) && TYPEOF(vector) == CPLXSXP) {
            Rcomplex *tmp = COMPLEX(vector);
            for (i = 0; i < tlen; i++) {
                if (!ISNA(tmp[i].r) && !ISNA(tmp[i].i)) {
                    allNA = FALSE;
                    break;
                }
            }
            if (allNA && (d->opts & S_COMPAT)) {
                surround = TRUE;
                print2buff("as.complex(", d);
            }
        } else if ((d->opts & KEEPNA) && TYPEOF(vector) == STRSXP) {
            for (i = 0; i < tlen; i++)
                if (STRING_ELT(vector, i) != NA_STRING) {
                    allNA = FALSE;
                    break;
                }
            if (allNA && (d->opts & S_COMPAT)) {
                surround = TRUE;
                print2buff("as.character(", d);
            }
        }
        if (tlen > 1)
            print2buff("c(", d);
        allNA = allNA && !(d->opts & S_COMPAT);
        for (i = 0; i < tlen; i++) {
            if (allNA && TYPEOF(vector) == REALSXP && ISNA(REAL(vector)[i])) {
                strp = "NA_real_";
            } else if (allNA && TYPEOF(vector) == CPLXSXP
                    && (ISNA(COMPLEX(vector)[i].r) || ISNA(COMPLEX(vector)[i].i))) {
                strp = "NA_complex_";
            } else if (allNA && TYPEOF(vector) == STRSXP
                    && STRING_ELT(vector, i) == NA_STRING) {
                strp = "NA_character_";
            } else if (TYPEOF(vector) == REALSXP && (d->opts & S_COMPAT)) {
                int w, d, e;
                formatReal(&REAL(vector)[i], 1, &w, &d, &e, 0);
                strp = EncodeReal2(REAL(vector)[i], w, d, e);
            } else if (TYPEOF(vector) == STRSXP) {
                const char *ts = translateChar(STRING_ELT(vector, i));
                /* versions of R < 2.7.0 cannot parse strings longer than 8192 chars */
                if (strlen(ts) >= 8192)
                    d->longstring = TRUE;
                strp = EncodeElement(vector, i, quote, '.');
            } else
                strp = EncodeElement(vector, i, quote, '.');
            print2buff(strp, d);
            if (i < (tlen - 1))
                print2buff(", ", d);
            if (tlen > 1 && d->len > d->cutoff)
                writeline(d);
            if (!d->active)
                break;
        }
        if (tlen > 1)
            print2buff(")", d);
        if (surround)
            print2buff(")", d);
    }
}

/* src2buff1: Deparse one source ref to buffer */

static void src2buff1(SEXP srcref, LocalParseData *d) {
    int i, n;
    PROTECT(srcref);

    PROTECT(srcref = lang2(install("as.character"), srcref));
    PROTECT(srcref = eval(srcref, R_BaseEnv));
    n = length(srcref);
    for (i = 0; i < n; i++) {
        print2buff(translateChar(STRING_ELT(srcref, i)), d);
        if (i < n - 1)
            writeline(d);
    }
    UNPROTECT(3);
}

/* src2buff : Deparse source element k to buffer, if possible; return FALSE on failure */

static Rboolean src2buff(SEXP sv, int k, LocalParseData *d) {
    SEXP t;

    if (TYPEOF(sv) == VECSXP && length(sv) > k && !isNull(t =
            VECTOR_ELT(sv, k))) {
        src2buff1(t, d);
        return TRUE;
    } else
        return FALSE;
}

/* vec2buff : New Code */
/* Deparse vectors of S-expressions. */
/* In particular, this deparses objects of mode expression. */

static void vec2buff(SEXP v, LocalParseData *d) {
    SEXP nv, sv;
    int i, n /*, localOpts = d->opts */;
    Rboolean lbreak = FALSE;

    n = length(v);
    nv = getAttrib(v, R_NamesSymbol);
    if (length(nv) == 0)
        nv = R_NilValue;

    if (d->opts & USESOURCE) {
        sv = getAttrib(v, R_SrcrefSymbol);
        if (TYPEOF(sv) != VECSXP)
            sv = R_NilValue;
    } else
        sv = R_NilValue;

    for (i = 0; i < n; i++) {
        if (i > 0)
            print2buff(", ", d);
        linebreak(&lbreak, d);
        if (!isNull(nv) && !isNull(STRING_ELT(nv, i))
        && *CHAR(STRING_ELT(nv, i))) { /* length test */
        /* d->opts = SIMPLEDEPARSE; This seems pointless */
        if ( isValidName(translateChar(STRING_ELT(nv, i))))deparse2buff
(            STRING_ELT(nv, i), d);
            else if(d->backtick) {
                print2buff("`", d);
                deparse2buff(STRING_ELT(nv, i), d);
                print2buff("`", d);
            } else {
                print2buff("\"", d);
                deparse2buff(STRING_ELT(nv, i), d);
                print2buff("\"", d);
            }
            /* d->opts = localOpts; */
            print2buff(" = ", d);
        }
        if (!src2buff(sv, i, d))
            deparse2buff(VECTOR_ELT(v, i), d);
    }
    if (lbreak)
        d->indent--;
}

static void args2buff(SEXP arglist, int lineb, int formals, LocalParseData *d) {
    Rboolean lbreak = FALSE;

    while (arglist != R_NilValue) {
        if (TYPEOF(arglist) != LISTSXP && TYPEOF(arglist) != LANGSXP)
            error(_("badly formed function expression"));
        if (TAG(arglist) != R_NilValue) {
            SEXP s = TAG(arglist);

            if (s == R_DotsSymbol)
                print2buff(CHAR(PRINTNAME(s)), d);
            else if (d->backtick)
                print2buff(quotify(PRINTNAME(s), '`'), d);
            else
                print2buff(quotify(PRINTNAME(s), '"'), d);

            if (formals) {
                if (CAR(arglist) != R_MissingArg) {
                    print2buff(" = ", d);
                    deparse2buff(CAR(arglist), d);
                }
            } else {
                print2buff(" = ", d);
                if (CAR(arglist) != R_MissingArg) {
                    deparse2buff(CAR(arglist), d);
                }
            }
        } else
            deparse2buff(CAR(arglist), d);
        arglist = CDR(arglist);
        if (arglist != R_NilValue) {
            print2buff(", ", d);
            linebreak(&lbreak, d);
        }
    }
    if (lbreak)
        d->indent--;
}

/* This code controls indentation.  Used to follow the S style, */
/* (print 4 tabs and then start printing spaces only) but I */
/* modified it to be closer to emacs style (RI). */

static void printtab2buff(int ntab, LocalParseData *d) {
    int i;

    for (i = 1; i <= ntab; i++)
        if (i <= 4)
            print2buff("    ", d);
        else
            print2buff("  ", d);
}