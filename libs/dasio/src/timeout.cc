/** @file timeout.cc */

#include <string.h>
#include <errno.h>
#include "dasio/timeout.h"
#include "nl.h"
#include "dasio/msg.h"

namespace DAS_IO {

Timeout::Timeout() {
  Clear();
}

void Timeout::Set( le_time_t secs, long msecs ) {
  int whole_secs;
  int rv = clock_gettime(CLOCK_REALTIME, &when);
  if ( rv == -1 ) msg(MSG_FATAL, "Error from clock_gettime(); '%s'", strerror(errno) );
  when.tv_nsec += msecs*1000000L;
  when.tv_sec += secs;
  whole_secs = when.tv_nsec/1000000000L;
  when.tv_sec += whole_secs;
  when.tv_nsec -= whole_secs*1000000000L;
}

/**
 * Clears the timeout, indicating no timeout required.
 */
void Timeout::Clear() {
  when.tv_sec = 0L;
  when.tv_nsec = 0L;
}

/**
 * @return true if a timeout value is set.
 */
bool Timeout::Set() {
  return (when.tv_sec != 0 || when.tv_nsec != 0);
}

/**
 * @return true if a timeout value has been set and has expired.
 */
bool Timeout::Expired() {
  struct timespec now;
  int rv;
  if (!Set()) return false;
  rv = clock_gettime(CLOCK_REALTIME, &now);
  return (now.tv_sec > when.tv_sec ||
          ((now.tv_sec == when.tv_sec) &&
           (now.tv_nsec > when.tv_nsec)));
}

TimeoutAccumulator::TimeoutAccumulator() {
  when.tv_sec = 0L;
  when.tv_nsec = 0L;
  how_soon.tv_sec = 0L;
  how_soon.tv_usec = 0L;
}

void TimeoutAccumulator::Set( Timeout *to ) {
  if ( to == NULL ) {
    when.tv_sec = 0L;
    when.tv_nsec = 0L;
    how_soon.tv_sec = 0L;
    how_soon.tv_usec = 0L;
  } else {
    when = to->when;
  }
}

void TimeoutAccumulator::Set_Min( Timeout *to ) {
  if ( to != NULL ) {
    if ( ( when.tv_sec == 0 && when.tv_nsec == 0 ) ||
         ( (to->when.tv_sec != 0 || to->when.tv_nsec != 0) &&
           ( ( to->when.tv_sec < when.tv_sec ) ||
             ( to->when.tv_sec == when.tv_sec && to->when.tv_nsec < when.tv_nsec ) ) ) ) {
      Set(to);
    }
  }
}

/**
 * @return true if a timeout value is set.
 */
bool TimeoutAccumulator::Set() {
  return (when.tv_sec != 0 || when.tv_nsec != 0);
}

/**
 * \returns struct timeval pointer appropriate for select().
 * If no timeout is specified, returns NULL. If the timeout has passed,
 * timeout is set at zero.
 */
struct timeval *TimeoutAccumulator::timeout_val() {
  struct timespec now;
  clock_gettime(CLOCK_REALTIME, &now);
  if ( when.tv_sec == 0 && when.tv_nsec == 0 ) {
    return NULL;
  } else if ( ( now.tv_sec > when.tv_sec ) ||
       ( now.tv_sec == when.tv_sec && now.tv_nsec > when.tv_nsec ) ) {
    how_soon.tv_sec = 0L;
    how_soon.tv_usec = 0L;
  } else {
    how_soon.tv_usec = (when.tv_nsec - now.tv_nsec)/1000;
    how_soon.tv_sec = (when.tv_sec - now.tv_sec);
    if (how_soon.tv_usec < 0) {
      --how_soon.tv_sec;
      how_soon.tv_usec += 1000000L;
    }
  }
  return &how_soon;
}

/**
 * \returns struct timespect pointer appropriate for pselect().
 * If no timeout is specified, returns 0. If the timeout has passed,
 * timeout is set at zero.
 */
struct timespec *TimeoutAccumulator::timeout_val_ns() {
  struct timespec now;
  clock_gettime(CLOCK_REALTIME, &now);
  if ( when.tv_sec == 0 && when.tv_nsec == 0 ) {
    return NULL;
  } else if ( ( now.tv_sec > when.tv_sec ) ||
       ( now.tv_sec == when.tv_sec && now.tv_nsec > when.tv_nsec ) ) {
    how_soon_ns.tv_sec = 0L;
    how_soon_ns.tv_nsec = 0L;
  } else {
    how_soon_ns.tv_nsec = (when.tv_nsec - now.tv_nsec);
    how_soon_ns.tv_sec = (when.tv_sec - now.tv_sec);
    if (how_soon_ns.tv_nsec < 0) {
      --how_soon_ns.tv_sec;
      how_soon_ns.tv_nsec += 1000000000L;
    }
  }
  return &how_soon_ns;
}

}
