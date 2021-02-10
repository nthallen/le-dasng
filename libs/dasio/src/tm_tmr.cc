/**
 * tm_tmr Object definitions
 */
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/timerfd.h>
#include "dasio/tm.h"
#include "dasio/tm_tmr.h"
#include "nl.h"
#include "nl_assert.h"

namespace DAS_IO {

tm_tmr::tm_tmr()
    : Interface("tmr", 0) {
  buf = (unsigned char *)&n_expirations;
  bufsize = sizeof(n_expirations);
  set_binary_mode();
  struct timespec ts;
  if (clock_getres(CLOCK_REALTIME, &ts))
    msg(MSG_EXIT_ABNORM, "Error from clock_getres()");
  timer_resolution_nsec =
    ts.tv_sec * (uint64_t)1000000000L + ts.tv_nsec;
  fd = timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK);
  if (fd < 0) {
    msg(MSG_FATAL, "timerfd_create returned error %d: %s",
      errno, strerror(errno));
  }
  if (fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK) == -1) {
    msg(3, "fcntl() error %d: %s", errno,
      strerror(errno));
  }
  flags = 0;
}

tm_tmr::~tm_tmr() {
  bufsize = 0; // avoid freeing buf
  buf = 0;
}

bool tm_tmr::protocol_input() {
  report_ok(nc);
  return false;
}

bool tm_tmr::protocol_unknown(bool &handled) {
  return false;
}

void tm_tmr::settime( int per_sec, int per_nsec ) {
  // ### Change this to start on the second
  struct itimerspec itime;

  itime.it_value.tv_sec = itime.it_interval.tv_sec = per_sec;
  itime.it_value.tv_nsec = itime.it_interval.tv_nsec = per_nsec;
  timerfd_settime(fd, 0, &itime, NULL);
  flags = (per_sec || per_nsec) ? Fl_Read : 0;
}

void tm_tmr::settime( uint64_t per_nsec ) {
  // ### Change this to start on the second
  struct itimerspec itime;
  itime.it_value.tv_sec = per_nsec / (uint64_t)1000000000;
  itime.it_value.tv_nsec = per_nsec % (uint64_t)1000000000;
  // nsec2timespec( &itime.it_value, per_nsec );
  itime.it_interval = itime.it_value;
  timerfd_settime(fd, 0, &itime, NULL);
  flags = per_nsec ? Fl_Read : 0;
}

}
