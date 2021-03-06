
#include <stdio.h>

#include "source.h"
#include "csm.h"
#include "gla.h"

#include "err.h"
#include "SrcFileStack.h"
#include "Include.h"
#include "CoordMap.h"

#include <fcntl.h>

#if defined(unix) || defined (_unix)
#include <unistd.h>
#endif

#if defined(_WIN32) || defined(MSDOS) || defined(_MSDOS)
#include <io.h> 
#endif

#ifdef MONITOR
#include "clp_dapto.h"
#endif


/* Action to switch to a new input file: */

#if defined(__cplusplus) || defined(__STDC__)
int NewInput(char *name)
#else
int NewInput(name) char *name;
#endif
/*
This operation saves the current position in SrcBuffer, creates a new
source buffer, and pushes it onto a stack.  When we encounter the end of
the file, we need to reverse this process.

1 is returned if the file could be opened; 0 otherwise.
*/
{ SrcFile temp;
  int descr;

  if (TokenEnd) TEXTSTART = TokenEnd;   /* First unscanned character */

  descr = open (name, 0);
  if (descr < 0) return 0;

  temp.buffer = SrcBuffer;
  temp.name = MapFile(LineNum);
  temp.line = MapLine(LineNum);
  SrcFileStackPush(temp);

  initBuf(name, descr);
  AddBreak(name, 1);
#ifdef MONITOR
  _dapto_source_changed (name, 1, LineNum, 1, 1, 1);
#endif

  TokenEnd = TEXTSTART;
  StartLine = TokenEnd - 1;

  return 1;
}

#if defined(__cplusplus) || defined(__STDC__)
char *auxEOF(char *c, int l)
#else
char *auxEOF(c, l) char *c; int l;
#endif
/* Action to return to a suspended input file: */
{ SrcFile temp;

  if (!SrcFileStackEmpty) {
    do {
      (void)close(finlBuf());
      temp = SrcFileStackPop; 
      SrcBuffer = temp.buffer;
      AddBreak(temp.name, temp.line);
#ifdef MONITOR
      _dapto_source_changed (temp.name, 0, LineNum, 1, temp.line, 1);
#endif
      if (!*TEXTSTART) refillBuf(TEXTSTART);
    } while (!SrcFileStackEmpty && !*TEXTSTART);
  }

  TokenEnd = TEXTSTART;
  StartLine = TokenEnd - 1;
  return TEXTSTART;
}
