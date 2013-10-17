#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h> // for isspace(int)


#include <Rdebug.h>

static debugScope* currentScope = (debugScope*)NULL;
static activeScopesLinList* activeScopes = (activeScopesLinList*)NULL;


void debugScope_activate(char* scopeName){
  activeScopesLinList* newScope = malloc(sizeof(activeScopesLinList));
  newScope->next = activeScopes;
  strncpy(newScope->scopeName,scopeName,SCOPENAME_MAX_SIZE);
  (newScope->scopeName)[SCOPENAME_MAX_SIZE]='\0'; // safety string termination
  
  activeScopes = newScope;
}

int debugScope_isActive(char* scopeName){
  // this function just iterates the linked list. There may be faster solutions
  activeScopesLinList* scopeSearcher = activeScopes;
  while(NULL != scopeSearcher){ // still some list left
    if (0==strcmp(scopeSearcher->scopeName,scopeName)){ // found
      return (1==1);
    }else{ // not (yet) found
      scopeSearcher = scopeSearcher->next;
    }
  }
  // not found (finally)
  return (1!=1);
}
  
  
  

void debugScope_readFile(char* fileName){
  DEBUGSCOPE_START("debugScope_readFile");
  FILE* configFile;
  configFile = fopen(fileName,"r");
  if (NULL == configFile){ // config file missing
    // only report if debug is on for this function
    DEBUGSCOPE_PRINT("Config File ");
    DEBUGSCOPE_PRINT(fileName);
    DEBUGSCOPE_PRINT(" could not be opened\n");
  }else{ // file could be opened
    char extractedLine[SCOPENAME_MAX_SIZE+1];
    while(1==1){
      fgets(extractedLine,SCOPENAME_MAX_SIZE, configFile);
      if (feof(configFile)){ // reached end of file
        break;
      }
      if (0==strncmp(extractedLine,"//",2)){ // comment line
        /*
         * scope names starting with "//" are considered
         * "commented out" and not to be activated!
         */
        continue;
      }
      unsigned int lineLength = strlen(extractedLine);
      while(
        (lineLength >0) &&
        (isspace(extractedLine[lineLength-1]))
        )
        {
          extractedLine[lineLength-1]='\0';
          lineLength--;
        }
      
      if(lineLength>0){ // finally: activate
        DEBUGSCOPE_PRINT("Activating Scope '");
        DEBUGSCOPE_PRINT(extractedLine);
        DEBUGSCOPE_PRINT("'\n");
        
        debugScope_activate(extractedLine);
      }
    }
    // file was read completely
    fclose(configFile);
  }
  
  DEBUGSCOPE_END("debugScope_readFile");
  
}

void debugScope_start(char* scopeName){
  debugScope* newScope = malloc(sizeof(debugScope));
  if (NULL == newScope){ // malloc failed
    printf("ERROR: Malloc failed for debug scope %s\n",scopeName);
    return;
  }
  newScope->scopeName = scopeName;
  if (NULL == currentScope){ // first Scope
    newScope->parent = NULL;
    newScope->depth  = 0;
  }else{
    newScope->parent = currentScope;
    newScope->depth = newScope->parent->depth + 1;
  }
  newScope->enabled = debugScope_isActive(scopeName);

  currentScope = newScope;
  
  if (currentScope->enabled){
    printf("[%u] -> ENTER: %s\n",newScope->depth, scopeName);
  }
}
 
void debugScope_end(char* scopeName){
  if (NULL == currentScope){
    printf("Trying to exit scope %s but current Scope is NULL - this should not happen!\n",scopeName);
  }else{ // current scope exists
    if (0!=strcmp(scopeName,currentScope->scopeName)){ // not equal
      printf(
        "Trying to exit scope %s but current Scope is %s - this should not happen!\n",
        scopeName, 
        currentScope
        );
    }else{ // scopenames match
      if (currentScope->enabled){
        printf("[%u] <- EXIT: %s\n",currentScope->depth, scopeName);
      }
      debugScope* endingScope = currentScope;
      if (NULL == endingScope->parent){
        printf("This was root Scope!\n");
        currentScope=NULL;
      }else{
        currentScope = endingScope->parent;
      }
      // either way: the previous scope has ended
      free(endingScope);
    }
  }
}
      
  
  

void debugScope_print(char* output){
  if (NULL != currentScope){ // safety check
    if ((1==1)==currentScope->enabled){
      printf(output);
    }else{
      /* debug scope not enabled - do not print */
    }
  }else{ // NULL
    printf("Current Scope is NULL - this should not happen!\n");
  }
}   
  



