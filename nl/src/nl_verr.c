/** @file nl_verr.c contains nl_verror() which allows easy expansion of
 * the nl_error capabilities in many cases.
 */
#include <stdio.h>
#include <stdlib.h>
#include "nl.h"

/**
Invocation:

~~~~~~~~~~~~~~~~~~
#include "nl.h"
~~~~~~~~~~~~~~~~~~

nl_verror() provides the same error message functionality as
nl_err() but with stdarg.h-style arguments. (nl_err() is
actually implemented by calling nl_verror()). This makes it
possible to create error message functions that do a little more
work on the message and then call nl_verror() to do the final
processing. compile_error() is written this way in order to
output the current input filename and line number before each
message.

@return The level argument unless level dictates termination.
*/
int nl_verror(FILE *ef, int level, const char *fmt, va_list args) {
  char *lvlmsg;

  if (level < -1 && nl_debug_level > level) return(level);
  switch (level) {
    case -1: lvlmsg = ""; break;
    case 0: lvlmsg = ""; break;
    case 1: lvlmsg = "Warning: "; break;
    case 2: lvlmsg = "Error: "; break;
    case 3: lvlmsg = "Fatal: "; break;
    default:
      if (level <= -2) lvlmsg = "Debug: ";
      else lvlmsg = "Internal: ";
      break;
  }
  fprintf(ef, "%s", lvlmsg);
  vfprintf(ef, fmt, args);
  fputc('\n', ef);
  fflush(ef); // for testing
  if (level > 3 ) {
    fflush( ef );
    abort();
  }
  if (level > 2 || level == -1) exit(level > 0 ? level : 0);
  return(level);
}
