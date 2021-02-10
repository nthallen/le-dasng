#ifndef DAS_IO_TM_TMR_H
#define DAS_IO_TM_TMR_H

#include <time.h>
#include <stdint.h>
#include "nl.h"
#include "interface.h"

namespace DAS_IO {

class tm_tmr : public Interface {
  public:
    tm_tmr();
    void settime( int per_sec, int per_nsec );
    void settime( uint64_t nsecs );
    uint64_t timer_resolution_nsec;
    uint64_t n_expirations;
  protected:
    ~tm_tmr();
    bool protocol_input();
    bool protocol_unknown(bool &handled);
};

}

#endif

