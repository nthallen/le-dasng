/** @file tm_relay.cc
 *  To serve as middleman between two points of communication
 *  @date 2019-11-14
 */

#include "dasio/tm.h"
#include "dasio/tm_gen.h"
#include "dasio/tm_client.h"
#include "dasio/msg.h"
#include "dasio/appid.h"
#include "dasio/loop.h"
#include "tm_relay.h"
#include "oui.h"
#include <string.h>

using namespace DAS_IO;

#define TMR_BUFSIZE 16384

tm_relay::tm_relay() : tm_generator(), tm_client(4096, false, tm_client::tm_client_hostname) {
  autostart = true;
  regulated = false;
  have_tstamp = false;
  msg(0, "pointless message from tm_relay constructor");
}

tm_relay::~tm_relay() {}

void tm_relay::process_data() {
  /** Code copied from rdr.cc, 2019-12-10 */
  static int nrows_full_rec = 0;
  static int last_rec_full = 1;
  static unsigned short frac_MFCtr;

  if ( ! have_tstamp ) {
    msg(MSG_WARN, "process_data() without initialization" );
  }
  tm_data_t3_t *data = &tm_msg->body.data3;
  unsigned char *raw = &data->data[0];
  int n_rows = data->n_rows;
  unsigned short MFCtr = data->mfctr;

  // Can check here for time limits
  // Given MFCtr, timestamp, we can calculate the time. We can
  // simply skip the commit_rows() call until the start time
  // is reached, and we could trigger termination if the end
  // time is reached.
  while ( n_rows ) {
    unsigned char *dest;
    lock(__FILE__,__LINE__);
    int n_room = allocate_rows(&dest);
    if ( n_room ) {
      unlock();
      if ( n_room > n_rows ) n_room = n_rows;
      int rawsize = n_room*tm_rcvr::nbQrow;
      memcpy( dest, raw, rawsize );
      commit_rows( MFCtr, 0, n_room );
      raw += rawsize;
      n_rows -= n_room;
      MFCtr += n_room;
    } else {
      unlock();
    }
    if (!regulated) transmit_data(false);
  }
}

void tm_relay::process_tstamp() {
  have_tstamp = true;
  tm_rcvr::process_tstamp();
  mfc_t MFCtr = tm_info.t_stmp.mfc_num;
  le_time_t time = tm_info.t_stmp.secs;
  tm_queue::commit_tstamp(MFCtr, time);
}

void tm_relay::service_row_timer() {}

int main(int argc, char **argv) {
  oui_init_options(argc, argv);
  tm_relay *new_tm_relay = new tm_relay();
  ::load_tmdac(0);
  new_tm_relay->tm_generator::ELoop.add_child(new_tm_relay);
  
  int nQrows = TMR_BUFSIZE/tmi(nbrow);
  if (nQrows < 2) nQrows = 2;
  
  new_tm_relay->init(nQrows, 0, false);
  msg(0, "tm_relay starting");
  new_tm_relay->connect();
  new_tm_relay->Start(Server::server_type);
  msg(0, "tm_relay terminating");
  return 0;
}
