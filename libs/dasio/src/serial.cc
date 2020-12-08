/** @file serial.cc */
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include "nl.h"
#include "dasio/msg.h"
#include "dasio/serial.h"

namespace DAS_IO {
  
Serial::Serial(const char *iname, int bufsz) : Interface(iname, bufsz) {
  termios_init = false;
}

Serial::Serial(const char *iname, int bufsz, const char *path, int open_flags)
    : Serial(iname, bufsz) {
  init(path, open_flags);
}

Serial::~Serial() {}

void Serial::init(const char *path, int open_flags) {
  termios_init = false;
  if (path == 0) {
    fd = -1;
  } else {
    fd = open(path, open_flags);
    if (fd < 0) {
      if (nl_response > NLRSP_QUIET)
        msg(nl_response, "%s: Unable to open device at %s: %s",
          iname, path, strerror(errno));
    } else {
      switch (open_flags & O_ACCMODE) {
        case O_RDWR:
        case O_RDONLY:
          flags |= Fl_Read;
          break;
      }
    }
  }
}

#ifdef PARSTK
  #define STICK_OPT PARSTK
#else
  #ifdef CMSPAR
    #define STICK_OPT CMSPAR
  #endif
#endif

void Serial::setup( int baud, int bits, char par, int stopbits,
                int min, int time ) {
  int bitsflag;

  if ( fd < 0 ) return;
  if ( termios_init == false && tcgetattr( fd, &termios_p) ) {
    msg( MSG_ERROR, "Error on tcgetattr: %s", strerror(errno) );
    return;
  }
  termios_init = true;
  termios_p.c_iflag = 0;
  termios_p.c_lflag &= ~(ECHO|ICANON|ISIG|ECHOE|ECHOK|ECHONL|IEXTEN);
  termios_p.c_cflag = CLOCAL|CREAD;
  termios_p.c_oflag &= ~(OPOST);
  switch (bits) {
    case 5: bitsflag = CS5; break;
    case 6: bitsflag = CS6; break;
    case 7: bitsflag = CS7; break;
    case 8: bitsflag = CS8; break;
    default:
      msg( MSG_FATAL, "Invalid bits value: %d", bits );
  }
  termios_p.c_cflag |= bitsflag;
  switch (par) {
    case 'n': bitsflag = 0; break;
    case 'e': bitsflag = PARENB; break;
    case 'o': bitsflag = PARENB|PARODD; break;
#ifdef STICK_OPT
    case 'm': bitsflag = PARENB|PARODD|STICK_OPT; break;
    case 's': bitsflag = PARENB|STICK_OPT; break;
#endif
    default:
      msg( MSG_FATAL, "Invalid parity selector: '%c'", par );
  }
  termios_p.c_cflag |= bitsflag;
  switch (stopbits) {
    case 1: break;
    case 2: termios_p.c_cflag |= CSTOPB; break;
    default:
      msg( MSG_FATAL, "Invalid number of stop bits: %d", stopbits );
  }
  cfsetispeed(&termios_p, get_baud_code(baud));
  cfsetospeed(&termios_p, get_baud_code(baud));
  termios_p.c_cc[VMIN] = min;
  termios_p.c_cc[VTIME] = time;
  if ( tcsetattr(fd, TCSANOW, &termios_p) )
    msg( MSG_ERROR, "Error on tcsetattr: %s", strerror(errno) );
}

void Serial::hwflow_enable(bool enable) {
  if (enable) {
    termios_p.c_cflag |= CRTSXOFF | CRTSCTS;
  } else {
    termios_p.c_cflag &= ~(CRTSXOFF | CRTSCTS);
  }
  if ( tcsetattr(fd, TCSANOW, &termios_p) )
    msg( MSG_ERROR, "Error on tcsetattr: %s", strerror(errno) );
}

void Serial::update_tc_vmin(int new_vmin, int new_vtime) {
  if (! termios_init) {
    if (tcgetattr(fd, &termios_p)) {
      msg( MSG_ERROR, "Error from tcgetattr: %s",
        strerror(errno));
    }
    termios_init = true;
  }
  if (new_vmin < 1) new_vmin = 0;
  if (new_vtime < 0) new_vtime = termios_p.c_cc[VTIME];
  if (new_vmin != termios_p.c_cc[VMIN] || new_vtime != termios_p.c_cc[VTIME]) {
    termios_p.c_cc[VMIN] = new_vmin;
    termios_p.c_cc[VTIME] = new_vtime;
    if (tcsetattr(fd, TCSANOW, &termios_p)) {
      msg( MSG_ERROR, "Error from tcsetattr: %s",
        strerror(errno));
    }
  }
}

void Serial::flush_input() {
  do {
    nc = cp = 0;
    if (fillbuf(bufsize, Fl_Timeout)) return;
  } while (nc > 0);
}

}
