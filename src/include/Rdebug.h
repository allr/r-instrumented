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

#include <setjmp.h> // so jumpbuf is known

#define ENABLE_SCOPING_DEBUG
//#undef ENABLE_SCOPING_DEBUG

#ifndef ENABLE_SCOPING_DEBUG
  /*
   * If scoping debug is disabled, all macros are changed to empty loops
   * and (hopefully) discarded during compilation
   */

  #define DEBUGSCOPE_ENABLEOUTPUT() do{} while(0)
  #define DEBUGSCOPE_DISABLEOUTPUT() do{} while(0)
  #define DEBUGSCOPE_ACTIVATE(scopeName) do {} while(0)
  #define DEBUGSCOPE_READFILE(fileName) do {} while(0)
  #define DEBUGSCOPE_START(scopeName) do {} while(0)
  #define DEBUGSCOPE_END(scopeName) do {} while(0)
  #define DEBUGSCOPE_PRINT(outText,...) do {} while (0)
  #define DEBUGSCOPE_PRINTSTACK() do {} while (0)
  #define DEBUGSCOPE_SAVEJUMP(jumpInfo) do {} while (0)
  #define DEBUGSCOPE_LOADJUMP(jumpInfo) do {} while (0)
  #define DEBUGSCOPE_SAVELOADJUMP(jumpInfo, jumpValue) do {} while (0)
  
#endif
#ifdef ENABLE_SCOPING_DEBUG
  #define SCOPENAME_MAX_SIZE (80)

  
  typedef struct debugScope_t{
    char* scopeName[SCOPENAME_MAX_SIZE+1];
    unsigned int depth;
    struct debugScope_t* parent;
    int enabled; // whether scope is enabled. 1 for yes.
  } debugScope;
  
  typedef struct scopes_linlist_t{
    char scopeName[SCOPENAME_MAX_SIZE+1] ;
    struct scopes_linlist_t* next;
  } activeScopesLinList;
  
  typedef struct jumpInfos_linlist_t{
    jmp_buf jumpInfo; // map index
    debugScope* targetScope;
    struct jumpInfos_linlist_t* next;
  } jumpInfos_linlist;
  
  #define DEBUGSCOPE_ENABLEOUTPUT(){debugScope_enableOutput();}
  #define DEBUGSCOPE_DISABLEOUTPUT(){debugScope_disableOutput();}
  #define DEBUGSCOPE_ISACTIVE(scopeName) (debugScope_isActive(scopeName))
  #define DEBUGSCOPE_ACTIVATE(scopeName){ debugScope_activate(scopeName); }
  #define DEBUGSCOPE_READFILE(fileName){ debugScope_readFile(fileName); }
  #define DEBUGSCOPE_START(scopeName) { debugScope_start(scopeName); }
  #define DEBUGSCOPE_END(scopeName) { debugScope_end(scopeName); }
  #define DEBUGSCOPE_PRINT(outText,...) { debugScope_print(outText,##__VA_ARGS__); }
  /*
   * Note: __VA_ARGS__ allows the macro to accept a variadic number of arguments.
   * However, at least one must be provided (in addition to outText).
   * The token paste operator '##' has to be used to allow
   * those additional arguments to be omitted.
   *
   * For additional information in this matter (at least for the gcc-cpp):
   * http://gcc.gnu.org/onlinedocs/cpp/Variadic-Macros.html
   */
  #define DEBUGSCOPE_PRINTSTACK() { debugScope_printStack(); }
  #define DEBUGSCOPE_SAVEJUMP(jumpInfo) { debugScope_saveJump(jumpInfo); }
  #define DEBUGSCOPE_LOADJUMP(jumpInfo) { debugScope_loadJump(jumpInfo); }
  #define DEBUGSCOPE_SAVELOADJUMP(jumpInfo, jumpValue) { debugScope_saveloadJump(jumpInfo, jumpValue);}
  
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
  
  //! Marks the given Scope as active - as in "print info from it"
  void debugScope_activate(char* scopeName);
  
  //! Read the given file as config and activate all scopes in it.
  void debugScope_readFile(char* fileName);
  
  //! returns whether a given debug Scope is active (enabled)
  int debugScope_isActive(char* scopeName);
  
  //! returns whether the current debug Scope is active
  int debugScope_isCurrentActive();
  
  void debugScope_start(char* scopeName);  
  void debugScope_end(char* scopeName);
  void debugScope_print(char* output,...);
  
  void debugScope_printStack();

  void debugScope_loadJump(jmp_buf jumpInfo);  
  void debugScope_saveJump(jmp_buf jumpInfo);  
  
  //! depending on jumpValue, save or load a jump
  void debugScope_saveloadJump(jmp_buf jumpInfo, int jumpValue);  
#endif

  
  
  
  
#endif // _RDEBUG_H
