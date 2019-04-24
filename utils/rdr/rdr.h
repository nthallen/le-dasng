#ifndef RDR_H_INCLUDED
#define RDR_H_INCLUDED

#include <pthread.h>
#include <semaphore.h>
#include "mlf.h"

#ifdef __cplusplus
#include "dasio/tm_gen_col.h"
#include "dasio/tm_client.h"
#include "dasio/tm_gen.h"

using namespace DAS_IO;

//class Rdr_quit_pulse;

class Reader : public tm_generator, public tm_client {
  public:
    Reader(int nQrows, int low_water, int bufsize, const char *path);
    void event(enum tm_gen_event evt);
    void *input_thread();
    void *output_thread();
    void control_loop();
    void service_row_timer();
  protected:
    void process_tstamp();
    void process_data();
    bool  process_eof();
    void lock(const char *by = 0, int line = -1);
    void unlock();
    const char *context();
    int it_blocked;
    sem_t it_sem;
    int ot_blocked;
    sem_t ot_sem;
    pthread_mutex_t tmq_mutex;
    bool have_tstamp;
  private:
    mlf_def_t *mlf;
    //Rdr_quit_pulse *RQP;
    const char *locked_by_file;
    int locked_by_line;
};

/* class Rdr_quit_pulse : public tmg_dispatch_client {
  public:
    Rdr_quit_pulse(Reader *rdr_ptr);
    ~Rdr_quit_pulse();
    void pulse();
    void attach();
    int ready_to_quit();
    Reader *rdr;
  private:
    int pulse_code;
    int coid;
}; */

extern "C" {
#endif

  void *input_thread(void *Reader_ptr);
  void *output_thread(void *Reader_ptr);
  void rdr_init( int argc, char **argv );

#ifdef __cplusplus
};
#endif

#define OT_BLOCKED_STOPPED 1
#define OT_BLOCKED_TIME 2
#define OT_BLOCKED_DATA 3
#define IT_BLOCKED_DATA 1
#define IT_BLOCKED_EOF 2

#endif
