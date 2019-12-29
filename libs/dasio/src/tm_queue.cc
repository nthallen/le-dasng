/* tm_queue.cc */
#include "dasio/msg.h"
#include "dasio/tm_queue.h"
#include "nl.h"
#include "nl_assert.h"

namespace DAS_IO {

/**
 * These make up part of the control structure of tm_queue.
 */
tmq_ref::tmq_ref(mfc_t MFCtr, int mfrow, int Qrow_in, int nrows_in,
        tmq_tstamp_ref *tsp)
    : tsp(tsp) {
  next_tmqr = 0;
  ref_count = 0;
  nl_assert(tsp);
  ++tsp->ref_count;
  MFCtr_start = MFCtr_next = MFCtr;
  row_start = row_next = mfrow;
  Qrow = Qrow_in;
  n_Qrows = 0;
  Qrows_retired = 0;
  append_rows( nrows_in );
}

/**
 * Convenience function to append an item to the end of the linked list and return the new item
 */
tmq_ref *tmq_ref::add_last(tmq_ref *tmqr) {
  next_tmqr = tmqr;
  return tmqr;
}

tmq_ref *tmq_ref::dereference(bool use_next) {
  tmq_ref *rv = use_next ? next_tmqr : 0;
  nl_assert(ref_count > 0);
  if (rv && use_next)
    ++rv->ref_count;
  --ref_count;
  return rv;
}

void tmq_ref::append_rows( int nrows ) {
  row_next += nrows;
  MFCtr_next += row_next/tm_info.nrowminf;
  row_next = row_next % tm_info.nrowminf;
  n_Qrows += nrows;
}

tmq_tstamp_ref::tmq_tstamp_ref( mfc_t MFCtr, le_time_t time ) {
  TS.mfc_num = MFCtr;
  TS.secs = time;
  ref_count = 0;
}

/**
  * tm_queue base class constructor.
  * Determines the output_tm_type and allocates the queue storage.
  */
tm_queue::tm_queue( ) {
  total_Qrows = 0;
  tmq_low_water = 0;
  raw = 0;
  row = 0;
  first = last = 0;
  full = false;
  last_tmqr = first_tmqr = 0;
}

/**
 * General tm_gen initialization. Assumes tm_info structure has been defined.
 * Initializes the queue itself.
 */
void tm_queue::init( int n_Qrows, int low_water) {
  total_Qrows = n_Qrows;
  tmq_low_water = low_water;
  if ( low_water > n_Qrows )
    msg(MSG_FATAL, "low_water must be <= n_Qrows" );
  
  // Determine the output_tm_type
  nbQrow = tmi(nbrow);
  tm_info.nrowminf = tmi(nbminf)/tmi(nbrow);
  if (tm_info.nrowminf > 2) {
    output_tm_type = TMTYPE_DATA_T2;
    nbDataHdr = 10;
  } else if ( tmi(mfc_lsb)==0 && tmi(mfc_msb)==1 ) {
    output_tm_type = TMTYPE_DATA_T3;
    nbQrow -= 4;
    nbDataHdr = 8;
  } else {
    output_tm_type = TMTYPE_DATA_T1;
    nbDataHdr = 6;
  }
  if (nbQrow <= 0) msg(MSG_FATAL,"nbQrow <= 0");
  int total_size = nbQrow * total_Qrows;
  raw = new unsigned char[total_size];
  if ( ! raw )
    msg(MSG_FATAL, "memory allocation failure: raw" );
  row = new unsigned char*[total_Qrows];
  if ( ! row )
    msg(MSG_FATAL, "memory allocation failure: row" );
  int i;
  unsigned char *currow = raw;
  for ( i = 0; i < total_Qrows; i++ ) {
    row[i] = currow;
    currow += nbQrow;
  }
}

void tm_queue::lock(const char * by, int line) {
  by = by;
  line = line;
}

void tm_queue::unlock() {}

/**
  no longer a blocking function. Returns the largest number of contiguous rows currently free.
  Caller can decide whether that is adequate.
  Assumes the tmq is locked.
  */
int tm_queue::allocate_rows(unsigned char **rowp) {
  int na;
  if ( full ) na = 0;
  else if ( first > last ) {
    na = first - last;
  } else na = total_Qrows - last;
  if ( rowp != NULL) *rowp = row[last];
  return na;
}

/**
 * @param MFCtr Minor frame counter of the first row being committed
 * @param mfrow Minor frame row of the first row being committed
 * @param nrows The number of rows being committed.
 * The row data must have already been written into the tm_queue
 * buffers previously allocated via tm_queue::allocate_rows.
 * This function does not signal whoever is reading the queue.
 * Locks tmq and unlocks upon completion.
 */
void tm_queue::commit_rows( mfc_t MFCtr, int mfrow, int nrows ) {
  // we (the writer thread) own the last pointer, so we can read it without a lock,
  // but we must lock before writing
  nl_assert( !full );
  nl_assert( last+nrows <= total_Qrows );
  nl_assert(last_tmqr); // must commit a time stamp first
  lock(__FILE__,__LINE__);
  // We need a new tmqr if the last one is a tmq_tstamp or my MFCtr,mfrow don't match the 'next'
  // elements in the current tmqr
  tmq_tstamp_ref *cur_tstamp = 0;
  tmq_ref *tmqdr = 0;
  if (last_tmqr) {
    tmqdr = last_tmqr;
    cur_tstamp = tmqdr->tsp;
    if (tmqdr->n_Qrows &&
        (MFCtr != tmqdr->MFCtr_next || mfrow != tmqdr->row_next )) {
      tmqdr = 0;
    }
  }
  nl_assert(cur_tstamp);
  if ( tmqdr == 0 ) {
    tmqdr = new tmq_ref(MFCtr, mfrow, last, nrows, cur_tstamp); // or retrieve from the free list?
    if ( last_tmqr )
      last_tmqr = last_tmqr->add_last(tmqdr);
    else first_tmqr = last_tmqr = tmqdr;
  } else tmqdr->append_rows(nrows);
  last += nrows;
  if ( last == total_Qrows ) last = 0;
  if ( last == first ) full = 1;
  unlock();
}

/**
 * Does not signal whoever is reading the queue
 */
void tm_queue::commit_tstamp( mfc_t MFCtr, le_time_t time ) {
  tmq_tstamp_ref *tmqt = new tmq_tstamp_ref(MFCtr, time);
  tmq_ref *tmqd = new tmq_ref(MFCtr, 0, last, 0, tmqt);
  lock(__FILE__,__LINE__);
  if ( last_tmqr ) last_tmqr = last_tmqr->add_last(tmqd);
  else first_tmqr = last_tmqr = tmqd;
  unlock();
}

void tm_queue::retire_rows(tmq_ref *tmqd, int n_rows ) {
  lock(__FILE__,__LINE__);
  nl_assert( n_rows >= 0 );
  nl_assert( tmqd->n_Qrows >= n_rows);
  nl_assert( tmqd->Qrow == first );
  if ( first < last ) {
    first += n_rows;
    if ( first > last )
      msg( MSG_EXIT_ABNORM, "Underflow in retire_rows" );
  } else {
    first += n_rows;
    if ( first >= total_Qrows ) {
      first -= total_Qrows;
      if ( first > last )
        msg( MSG_EXIT_ABNORM, "Underflow after wrap in retire_rows" );
    }
  }
  if (n_rows > 0) full = false;
  tmqd->Qrow = first;
  tmqd->n_Qrows -= n_rows;
  tmqd->Qrows_retired += n_rows;
  if ( tmqd->n_Qrows == 0 && tmqd->next_tmqr && tmqd->ref_count <= 0) {
    tmq_ref *nxt = tmqd->next_tmqr;
    --tmqd->tsp->ref_count; // because tmqd is being deleted
    nl_assert(tmqd == first_tmqr);
    if (nxt->tsp != tmqd->tsp) {
      nl_assert(tmqd->tsp->ref_count == 0);
      delete(tmqd->tsp);
    }
    // first_tmqr = tmqd->next_tmqr;
    first_tmqr = nxt;
    delete( tmqd );
  } else {
    tmqd->row_start += n_rows;
    tmqd->MFCtr_start += tmqd->row_start / tm_info.nrowminf;
    tmqd->row_start %= tm_info.nrowminf;
  }
  unlock();
}

/**
 * @param tmqrp Point to the current tmq_ref*
 * Updates the contents of tmqrp to point to the next
 * tmq_ref*, if such exists. *tmqrp will not be changed
 * if there is no next tmq_ref*.
 * @return true if a new tmq_ref* value has been written
 * into *tmqrp.
 */
bool tm_queue::next_tmqr(tmq_ref **tmqrp) {
  if (*tmqrp) {
    tmq_ref *tmqr = *tmqrp;
    if (tmqr->next_tmqr) {
      --tmqr->ref_count;
      tmqr = tmqr->next_tmqr;
      ++tmqr->ref_count;
      *tmqrp = tmqr;
      return true;
    } else return false;
  } else if (first_tmqr) {
    ++first_tmqr->ref_count;
    *tmqrp = first_tmqr;
    return true;
  } else return false;
}

}