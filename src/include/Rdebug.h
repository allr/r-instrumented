/*
 * ------------------------------------------------------------------
 * This material is distributed under the GNU General Public License
 * Version 2. You may review the terms of this license at
 * http://www.gnu.org/licenses/gpl-2.0.html
 *
 * Copyright (c) 2013, Markus Kuenne
 * TU Dortmund University
 *
 * All rights reserved.
 * ------------------------------------------------------------------
 */

#ifndef _RDEBUG_H
#define _RDEBUG_H

#include "Rinternals.h"
#include <setjmp.h> // so jumpbuf is known

#ifndef HAVE_DEBUGSCOPES
    /*
     * If scoping debug is disabled, all macros are changed to empty loops
     * and (hopefully) discarded during compilation
     */

    #define SCOPENAME_MAX_SIZE 1

    #define DEBUGSCOPE_ENABLEOUTPUT()                        do {} while (0)
    #define DEBUGSCOPE_DISABLEOUTPUT()                       do {} while (0)
    #define DEBUGSCOPE_ACTIVATE(scopeName)                   do {} while (0)
    #define DEBUGSCOPE_DEACTIVATE(scopeName)                 do {} while (0)
    #define DEBUGSCOPE_READFILE(fileName)                    do {} while (0)
    #define DEBUGSCOPE_START(scopeName)                      do {} while (0)
    #define DEBUGSCOPE_END(scopeName)                        do {} while (0)
    #define DEBUGSCOPE_PRINT(outText,...)                    do {} while (0)
    #define DEBUGSCOPE_PRINTSTACK()                          do {} while (0)
    #define DEBUGSCOPE_SAVEJUMP(jumpInfo)                    do {} while (0)
    #define DEBUGSCOPE_LOADJUMP(jumpInfo)                    do {} while (0)
    #define DEBUGSCOPE_IGNORENEXTCONTEXT()                   do {} while (0)
    #define DEBUGSCOPE_PRINTBEGINCONTEXT(fun1,fun2)          do {} while (0)
    #define DEBUGSCOPE_PRINTBEGINPSEUDOCONTEXT(contextName)  do {} while (0)
    #define DEBUGSCOPE_PRINTENDCONTEXT(fun1,fun2)            do {} while (0)
    #define DEBUGSCOPE_SETCONTEXTPREFIX(newPrefix)           do {} while (0)
    #define DEBUGSCOPE_PRINTCONTEXTSTACK()                   do {} while (0)

    /* avoid "variable not used" warnings by using empty inline functions */
    static inline void DEBUGSCOPE_SAVELOADJUMP(jmp_buf jumpInfo, int jumpValue) { }
    static inline void extractFunctionName(char *extraction, SEXP environment)  { }

#endif
#ifdef HAVE_DEBUGSCOPES
    #define SCOPENAME_MAX_SIZE (80)

    typedef struct debugScope_t {
	char scopeName[SCOPENAME_MAX_SIZE+1];
	unsigned int depth;
	struct debugScope_t* parent;
	Rboolean enabled; // whether scope is enabled.
    } debugScope;

    typedef struct scopes_linlist_t {
	char scopeName[SCOPENAME_MAX_SIZE+1];
	struct scopes_linlist_t* next;
    } activeScopesLinList;

    typedef struct jumpInfos_linlist_t {
	jmp_buf jumpInfo; // map index
	debugScope* targetScope;
	struct jumpInfos_linlist_t* next;
    } jumpInfos_linlist;

    char currentContextPrefix[SCOPENAME_MAX_SIZE+1];
    char oldContextPrefix[SCOPENAME_MAX_SIZE+1];
    char ignoreNextContext;
    
    #define DEBUGSCOPE_ENABLEOUTPUT()        { debugScope_enableOutput(); }
    #define DEBUGSCOPE_DISABLEOUTPUT()       { debugScope_disableOutput(); }
    #define DEBUGSCOPE_ISACTIVE(scopeName)   (debugScope_isActive(scopeName))
    #define DEBUGSCOPE_ACTIVATE(scopeName)   { debugScope_activate(scopeName); }
    #define DEBUGSCOPE_DEACTIVATE(scopeName) { debugScope_deactivate(scopeName); }
    #define DEBUGSCOPE_READFILE(fileName)    { debugScope_readFile(fileName); }
    #define DEBUGSCOPE_START(scopeName)      { debugScope_start(scopeName); }
    #define DEBUGSCOPE_END(scopeName)        { debugScope_end(scopeName); }
    #define DEBUGSCOPE_PRINT(outText,...)    { debugScope_print(outText, ##__VA_ARGS__); }
    /*
     * Note: __VA_ARGS__ allows the macro to accept a variadic number of arguments.
     * However, at least one must be provided (in addition to outText).
     * The token paste operator '##' has to be used to allow
     * those additional arguments to be omitted.
     *
     * For additional information in this matter (at least for the gcc-cpp):
     * http://gcc.gnu.org/onlinedocs/cpp/Variadic-Macros.html
     */
    #define DEBUGSCOPE_PRINTSTACK()                         { debugScope_printStack(); }
    #define DEBUGSCOPE_SAVEJUMP(jumpInfo)                   { debugScope_saveJump(jumpInfo); }
    #define DEBUGSCOPE_LOADJUMP(jumpInfo)                   { debugScope_loadJump(jumpInfo); }
    #define DEBUGSCOPE_SAVELOADJUMP(jumpInfo, jumpValue)    { debugScope_saveloadJump(jumpInfo, jumpValue); }
    #define DEBUGSCOPE_IGNORENEXTCONTEXT()                  { debugScope_ignoreNextContext(); }
    #define DEBUGSCOPE_PRINTBEGINCONTEXT(fun1,fun2)         { debugScope_printBeginContext(fun1,fun2); }
    #define DEBUGSCOPE_PRINTENDCONTEXT(fun1,fun2)           { debugScope_printEndContext(fun1,fun2); }
    #define DEBUGSCOPE_PRINTBEGINPSEUDOCONTEXT(contextName) { debugScope_printBeginPseudoContext(contextName); }
    #define DEBUGSCOPE_SETCONTEXTPREFIX(newPrefix)          { debugScope_setContextPrefix(newPrefix); }
    #define DEBUGSCOPE_PRINTCONTEXTSTACK()                  { debugScope_printContextStack(); }
    #define DEBUGSCOPE_CONTEXTSCOPESISACTIVE()              (debugScope_contextScopesIsActive())

    /*!
     * \brief enables output globally.
     *
     * Enables the debugscope output on a global level.
     * If this is not called, no function will print anything, even if the
     * scope is activated.
     *
     * This is useful to filter large chunks of a run not relevant to
     * the analysis you are trying to do.
     */
    void debugScope_enableOutput();
    
    //! prepare context output - for later activation
    void debugScope_prepareContextOutEnable();
    //! activate context output - if prepared
    void debugScope_enableContextOut();
    

    //void debugScope_setContextFile(char* outFile);
    
    /*!
     * \brief disables output globally
     *
     * Disables the debugscope output on a global level.
     * Once this was called, no function will print anything - even
     * if the scope is activated.
     *
     * This is useful to filter large chungs of a run not relevant to
     * whatever you are trying to analise (e.g. initialisation)
     */
    void debugScope_disableOutput();

    /*!
     * \brief Marks the given Scope as active - as in "print info from it"
     *
     * This function activates the given scope. This is to be read as
     * "print messages from this" (especially entering and exiting).
     * Note that this doesn't start the scope.
     *
     * It is possible to activate the same scope several times. Depending
     * on the implementation, additional memory may be allocated in a tradeoff
     * for speed. There is no error it the given scopeName already has been
     * activated, before
     *
     * \param[in] scopeName   name of the scope to activate
     */
    void debugScope_activate(char* scopeName);

    /*!
     * \brief Marks the given Scope as inactive - as in "do not print info from it"
     *
     * This function disactivates the given scope. This is to be read as
     * "print no debug messages from this". Note that this doesn't end the scope.
     *
     * If this function is called, the scope will be deactivated even if it
     * has been activated several times. Depending on the implementation,
     * a larger list has to be iterated and checked.
     *
     * Note: this is a silent function. There is no error if the given scopeName
     * was found (and deleted) more or less than once.
     *
     * \param[in] scopeName   name of the scope to deactivate
     */
    void debugScope_deactivate(char* scopeName);


    //! Read the given file as config and activate all scopes in it.
    void debugScope_readFile(char* fileName);

    //! returns whether a given debug Scope is active (enabled)
    Rboolean debugScope_isActive(char* scopeName);

    //! returns whether the current debug Scope is active
    Rboolean debugScope_isCurrentActive();

    void debugScope_start(char* scopeName);
    void debugScope_end(char* scopeName);
    void debugScope_print(char* output,...);

    void debugScope_printStack();
    //! prints debugscopes stack in a single line
    void debugScope_flatStack();

    void debugScope_loadJump(jmp_buf jumpInfo);
    void debugScope_saveJump(jmp_buf jumpInfo);

    //! depending on jumpValue, save or load a jump
    void debugScope_saveloadJump(jmp_buf jumpInfo, int jumpValue);

    void extractFunctionName(char* extraction, SEXP environment);
    
    
    void debugScope_printBeginContext(SEXP from, SEXP to);
    void debugScope_printEndContext(SEXP from, SEXP to);
    void debugScope_setContextPrefix(const char* newPrefix);
    // note that the next context is not real!
    void debugScope_ignoreNextContext();
    void debugScope_printContextStack();
    
    //! returns whether contextscopes are to be printed
    Rboolean debugScope_contextScopesIsActive();

    

#endif



#endif // _RDEBUG_H
