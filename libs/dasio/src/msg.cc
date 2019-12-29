/* msg.c */
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>
#include "dasio/client.h"
#include "dasio/loop.h"
#include "dasio/appid.h"
#include "oui.h"
#include "nl.h"
#include "nl_assert.h"
#define MSG_INTERNAL
#include "dasio/msg.h"
// #include "tm.h" was needed for tm_dev_name
// I hacked that out, but this needs to interface to C++ to provide proper IPC
// with memo.

  static bool we_are_memo = false;
  
  void set_we_are_memo(void) {
    we_are_memo = true;
  }

  memo_client::memo_client() : DAS_IO::Client("memo", 1000, 0, "memo", 0) {
    //do stuff
    ELoop.is_memo_loop = true;
  }
  
  static memo_client *memo_client_instance;

  extern "C" {
    static void msg_cleanup(void) {
      if (memo_client_instance) {
        memo_client_instance->cleanup();
        DAS_IO::Interface::dereference(memo_client_instance);
        memo_client_instance = 0;
      }
    }
  };
  
  memo_client::~memo_client() {}
  
  bool memo_client::init() {
    atexit(msg_cleanup);
    ELoop.add_child(this);
    connect();
    ELoop.event_loop();
    return (fd >= 0);
  }
  
  void memo_client::cleanup() {
    bool deref = ref_check(2);
    if (!deref) msg(MSG_ERROR, "memo_client ref_count < 2 in cleanup");
    ELoop.remove_child(this, deref);
    msg = nl_err;
    msgv = nl_verr;
  }
  
  void memo_client::send(const char* msg) {
    if ((msg != 0) && (msg[0] != '\0')) {
      if (!iwrite(msg)) {
        ELoop.event_loop();
      }
    }
  }
  
  bool memo_client::app_connected() {
    return true;
  }
  
  bool memo_client::connect_failed() {
    return true;
  }
  
  bool memo_client::iwritten(int nb) {
    return (is_negotiated() && obuf_empty());
  }

static int write_to_memo = 0, write_to_stderr = 0, write_to_file = 0;
FILE *file_fp;

/*
<opts> "nvo:mV"

<sort>
  -n <name> Change AppID name
  -v add a level of verbosity
  -o <error filename> Write to specified file
  -m write to memo [default]
  -V write to stderr
*/
void msg_init_options(int argc, char **argv) {
  int c;

  optind = OPTIND_RESET;
  opterr = 0;
  while ((c = getopt(argc, argv, opt_string)) != -1) {
    switch (c) {
      case 'n':
        DAS_IO::AppID.new_name(optarg);
        break;
      case 'v': --nl_debug_level; break;
      case 'o':
        file_fp = fopen( optarg, "a" );
        if ( file_fp == NULL ) {
          fprintf( stderr, "Unable to open output file: '%s'\n", optarg );
          exit(1);
        }
        write_to_file = 1;
        break;
      case 'm': 
        write_to_memo = 1;
        if (we_are_memo) {
          msg(3, "memo cannot write to memo!\n");
        }
        break;
      case 'V': write_to_stderr = 1; break;
      case '?':
        fprintf( stderr, "Unrecognized option: '-%c'\n", optopt );
        exit(1);
      default: break; // could check for errors
    }
  }
  
  if (!write_to_stderr && !write_to_file && !write_to_memo) {
    if (we_are_memo) {
      write_to_stderr = 1;
    } else {
      write_to_memo = 1;
    }
  }
  
  if (write_to_memo && !we_are_memo) {
    // memo_fp = fopen( tm_dev_name( "memo" ), "w" );
    // memo_fp = fopen( "memo.log", "w" );
    nl_assert(memo_client_instance == 0);
    
    memo_client_instance = new memo_client();
    memo_client_instance->reference(); // for memo_client_instance
    if (!memo_client_instance->init()) {
      fprintf( stderr, "Unable to contact memo\n" );
      write_to_stderr = 1;
      write_to_memo = 0;
    }
  }
  
  msg = msg_func;
  msgv = msgv_func;
}

static void write_msg( const char *buf, int nb, FILE *fp, const char *dest ) {
  int rv = fwrite( buf, 1, nb, fp );
  if ( rv == -1 ) {
    fprintf( stderr, "Memo: error %s writing to %s\n",
       strerror(errno), dest );
  }
  fflush(fp);
}

/**
 * msg_func() implements the msg() interface, but provides
 * a considerable amount of additional functionality to
 * support logging of messages within an application with
 * multiple executables. Through command-line options,
 * msg_func() allows msg() to be configured to log to
 * stderr and/or to a log file and/or to the memo application,
 * and adds a timestamp to each message. See msg() for definition
 * of the level options.
 * @return the level argument.
 */
int msg_func( int level, const char *fmt, ...) {
  va_list args;
  int rv;

  va_start(args, fmt);
  rv = msgv( level, fmt, args );
  va_end(args);
  return rv;
}

/**
 * msgv_func() implements the msgv() interface,
 * taking a va_list for format arguments, allowing
 * more complex reporting functions to be built on
 * top of the basic msg() functionality. Internally
 * msg_func() calls msgv_vunc().
 * @return the level argument.
 */
#define MSG_MAX_INTERNAL 250
int msgv_func( int level, const char *fmt, va_list args ) {
  const char *lvlmsg;
  char msgbuf[MSG_MAX_INTERNAL+2];
  
  //New, millisecond time!
  clockid_t clk_id = CLOCK_REALTIME;
  struct timespec timespec, resolution;
  clock_getres(clk_id, &resolution);
  clock_gettime(clk_id, &timespec);
  
  int milliseconds = ((timespec.tv_nsec+500000)/1000000);
  if (milliseconds >= 1000) {
    milliseconds -= 1000;
    timespec.tv_sec += 1;
  }
  
  char *tbuf = asctime(gmtime(&timespec.tv_sec));
  
  //Below lies second-time
  /* le_time_t now = time(NULL);
  struct tm *tm = gmtime(&now);
  char *tbuf = asctime(tm);
  */
  int nb;

  switch ( level ) {
    case -1:
    case 0: lvlmsg = ""; break;
    case 1: lvlmsg = "[WARNING] "; break;
    case 2: lvlmsg = "[ERROR] "; break;
    case 3: lvlmsg = "[FATAL] "; break;
    default:
      if ( level >= 4 ) lvlmsg = "[INTERNAL] ";
      else if ( level < nl_debug_level ) return level;
      else lvlmsg = "[DEBUG] ";
      break;
  }
  //Here's the part where it gets fuzzy - for Miles, at least
  strncpy(msgbuf, tbuf+11, 8); // index, length of time string
  snprintf(msgbuf+8, 6, ".%03d ", milliseconds);
  strncpy( msgbuf+13, lvlmsg, MSG_MAX_INTERNAL-13 );
  nb = 13 + strlen(lvlmsg);
  nb += snprintf( msgbuf+nb, MSG_MAX_INTERNAL-nb, "%s: ", DAS_IO::AppID.name );
  // I am guaranteed that we have not yet overflowed the buffer
  nb += vsnprintf( msgbuf+nb, MSG_MAX_INTERNAL-nb, fmt, args );
  if ( nb > MSG_MAX_INTERNAL ) nb = MSG_MAX_INTERNAL;
  if ( msgbuf[nb-1] != '\n' ) msgbuf[nb++] = '\n';
  // msgbuf[nb] = '\0';
  // nb may be as big as MSG_MAX_INTERNAL+1
  // we don't need to transmit the trailing nul
  return msg_internal(level, msgbuf, nb);
}

int msg_internal(int level, const char *msgbuf, int nb) {
  if ( write_to_memo && memo_client_instance ) {
    memo_client_instance->send(msgbuf);
  }
  if ( write_to_file ) write_msg( msgbuf, nb, file_fp, "file" );
  if ( write_to_stderr ) write_msg( msgbuf, nb, stderr, "stderr" );
  if ( level >= 4 ) abort();
  if ( level == 3 ) exit(1);
  if ( level == -1 ) exit(0);
  return level;
}
