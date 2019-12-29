#ifndef TM_QUEUE_H_INCLUDED
#define TM_QUEUE_H_INCLUDED
#include "tm.h"

// I prefer not to allocate and free these structures routinely, but
// I'll start that way. It makes sense to keep a free list for the
// basic types: tstamp_q, tmq_tstamp_ref and tmq_data_ref. Actually,
// I guess that can be optimized via definition of new and delete
// operators.

// Define a hierarchy here. A tmq_descriptor can either hold
// a timestamp or reference data rows in the tmq.
// This works within tm_gen because we don't have readers starting
// and stopping. We have exactly one reader that will go
// through all the data.

namespace DAS_IO {

// enum tmqtype { tmq_tstamp, tmq_data  };

class tmq_tstamp_ref {
  public:
    tmq_tstamp_ref( mfc_t MFCtr, le_time_t time );
    tstamp_t TS;
    /** Counts tmq_ref references only */
    int ref_count;
};

class tmq_ref {
  public:
    tmq_ref(mfc_t MFCtr, int mfrow, int Qrow_in, int nrows_in, tmq_tstamp_ref *tsp);
    tmq_ref *add_last(tmq_ref *tmqr);
    /**
     * @param tmqr Pointer to the tmq_ref which we no longer wish to reference
     * @return If use_next, the following tmq_ref which we will now reference, else 0.
     */
    tmq_ref *dereference(bool use_next);
    tmq_ref *next_tmqr;
    /* Either data or timestamp */
    // tmqtype type;
    /** Used by bfr to keep track of (output) client references */
    int ref_count;
    
    void append_rows( int nrows );
    tmq_tstamp_ref *tsp;
    /** The MFCtr corresponding to the first unretired row
     *  that this tmq_ref references
     */
    mfc_t MFCtr_start;
    /** The MFCtr corresponding to the next row after
     *  all the rows that this tmq_ref references
     */
    mfc_t MFCtr_next;
    /** The minor frame row number of the first unretired Qrow
     *  0 <= row_start < nrowminf
     */
    int row_start;
    /** The minor frame row number of the next row after
     *  all the rows that this tmq_ref references.
     */
    int row_next;
    /** The number of Qrows that have been retired
     *  from this tmq_ref.
     */
    int Qrows_retired;
    /** The index in tm_queue of the first Qrow */
    int Qrow;
    /** The number of unretired Qrows this tmq_ref references */
    int n_Qrows;
};

// class tmq_data_ref : public tmq_ref {
  // public:
    // tmq_data_ref(mfc_t MFCtr, int mfrow, int Qrow_in, int nrows_in, tmq_ref *tsp);
    // void append_rows( int nrows );
    // tmq_ref *tsp;
    // mfc_t MFCtr_start, MFCtr_next;
    // int row_start, row_next, Qrows_retired;
    // int Qrow;
    // int n_rows;
// };

/** Semantics of tm_queue
   tm_queue.first, .last are indices into row and range from
     [0..total_Qrows)
   .first is where the next row will be read from
   .last is where the next row will be written to
   first==last means either full or empty, depending on the value of full.
   
   if collection is true, then allocate_rows will throw rather than block
*/
class tm_queue {
  public:
    tm_queue();
    void init( int n_Qrows, int low_water ); // allocate space for the queue

  protected:
    int allocate_rows(unsigned char **rowp);
    void commit_rows( mfc_t MFCtr, int mfrow, int n_rows );
    void commit_tstamp( mfc_t MFCtr, le_time_t time );
    void retire_rows( tmq_ref *tmqd, int n_rows );
    // void retire_tstamp( tmq_tstamp_ref *tmqts );
    bool next_tmqr(tmq_ref **tmqrp);
    virtual void lock(const char * by = 0, int line = -1);
    virtual void unlock();

    unsigned char *raw;
    unsigned char **row;
    tm_hdrw_t output_tm_type;
    int total_Qrows;
    int nbQrow; // may differ from nbrow if stripping MFCtr & Synch
    int nbDataHdr;
    /** Row index in the tm_queue where the next row of data will be read from */
    int first;
    /** Row index in the tm_queue where the next row of data will be written */
    int last;
    /** True if tm_queue is full */
    bool full;
    
    tmq_ref *first_tmqr;
    tmq_ref *last_tmqr;
    int tmq_low_water;
};

}

#endif
