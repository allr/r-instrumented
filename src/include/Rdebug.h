#ifndef _RDEBUG_H

#define ENABLE_SCOPING_DEBUG
//#undef ENABLE_SCOPING_DEBUG

#ifndef ENABLE_SCOPING_DEBUG
  /*
   * If scoping debug is disabled, all macros are changed to empty loops
   * and (hopefully) discarded during compilation
   */

  #define DEBUGSCOPE_ACTIVATE(scopeName) do {} while(0)
  #define DEBUGSCOPE_READFILE(fileName) do {} while(0)
  #define DEBUGSCOPE_START(scopeName) do {} while(0)
  #define DEBUGSCOPE_END(scopeName) do {} while(0)
  #define DEBUGSCOPE_PRINT(outText) do {} while (0)
  
#endif
#ifdef ENABLE_SCOPING_DEBUG
  #define SCOPENAME_MAX_SIZE (80)

  
  typedef struct debugScope_t{
    char* scopeName;
    unsigned int depth;
    struct debugScope_t* parent;
    int enabled; // whether scope is enabled. 1 for yes.
  } debugScope;
  
  typedef struct scopes_linlist_t{
    char scopeName[SCOPENAME_MAX_SIZE+1] ;
    struct scopes_linlist_t* next;
  } activeScopesLinList;
  
  #define DEBUGSCOPE_ISACTIVE(scopeName) (debugScope_isActive(scopeName))
  #define DEBUGSCOPE_ACTIVATE(scopeName){ debugScope_activate(scopeName); }
  #define DEBUGSCOPE_READFILE(fileName){ debugScope_readFile(fileName); }
  #define DEBUGSCOPE_START(scopeName) { debugScope_start(scopeName); }
  #define DEBUGSCOPE_END(scopeName) { debugScope_end(scopeName); }
  #define DEBUGSCOPE_PRINT(outText) { debugScope_print(outText); }

  
  
  //! Marks the given Scope as active - as in "print info from it"
  void debugScope_activate(char* scopeName);
  
  //! Read the given file as config and activate all scopes in it.
  void debugScope_readFile(char* fileName);
  
  //! returns whether a given debug Scope is active (enabled)
  int debugScope_isActive(char* scopeName);
  
  void debugScope_start(char* scopeName);  
  void debugScope_end(char* scopeName);
  void debugScope_print(char* output);  
#endif

  
  
  
  
#endif // _RDEBUG_H
