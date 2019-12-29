/** @file dasio/timeout.h */

#ifndef DASIO_TIMEOUT_H_INCLUDED
#define DASIO_TIMEOUT_H_INCLUDED

#include "tm.h"
#include <time.h>
#include <sys/select.h>

namespace DAS_IO {

class Timeout {
  public:
    Timeout();
    /**
     * Specifies a desired timeout relative to the current time. The event loop
     * will subtract the then-current time to determine the correct relative
     * timeout value.
     * @param secs Seconds
     * @param msecs Milleseconds
     */
    void Set( le_time_t secs, long msecs );
    /**
     * Clears any current timeout setting.
     */
    void Clear();
    /**
     * @return True if a timeout value is set, regardless of whether
     * the time has expired or not.
     */
    bool Set();
    /**
     * @return True if a timeout value is set and has expired.
     */
    bool Expired();
    struct timespec when;
};

class TimeoutAccumulator {
  public:
    TimeoutAccumulator();
    bool Set();
    void Set( Timeout * );
    void Set_Min( Timeout * );
    struct timeval *timeout_val();
    struct timespec *timeout_val_ns();
  private:
    struct timespec when;
    struct timeval how_soon;
    struct timespec how_soon_ns;
};

}

#endif
