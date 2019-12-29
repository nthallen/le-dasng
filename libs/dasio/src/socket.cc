/** @file socket.cc */
#include <stdio.h>
#include <string>
#include <string.h>
#include <strings.h>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <netdb.h>
#include <map>
#include "dasio/socket.h"
#include "dasio/loop.h"
#include "dasio/msg.h"
#include "nl.h"
#include "nl_assert.h"

#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX 108
#endif

namespace DAS_IO {

// TCP Socket
Socket::Socket(const char *iname, int bufsz, const char *hostname,
                        const char *service) :
  Interface(iname, bufsz),
  hostname(hostname),
  service(service),
  is_server(false),
  socket_state(Socket_disconnected),
  socket_type(hostname ? Socket_TCP : Socket_Unix)
{
  common_init();
}

// Server Socket
Socket::Socket(const char *iname, const char *service,
        socket_type_t socket_type) :
  Interface(iname, 0),
  service(service),
  hostname("0.0.0.0"),
  is_server(true),
  socket_state(Socket_disconnected),
  socket_type(socket_type)  
{
  common_init();
}

// Clone Socket
Socket::Socket(Socket *S, const char *iname, int bufsize, int fd) :
  Interface(iname, bufsize),
  service(S->service),
  hostname(0),
  is_server(false),
  socket_state(Socket_connected),
  socket_type(S->socket_type)
{
  common_init();
  this->fd = fd;
  is_server_client = true;
  flags = Fl_Except | Fl_Read;
}

Socket::~Socket() {
  if (fd >= 0) {
    close();
  }
  if (unix_name) {
    delete(unix_name);
    unix_name = 0;
  }
}

const char *Socket::company = "linkeng";

void Socket::common_init() {
  unix_name = 0;
  set_retries(-1, 5, 60);
  conn_fail_reported = false;
  is_server_client = false;
}

void Socket::connect() {
  switch (socket_type) {
    case Socket_Unix:
      if (unix_name == 0) {
        unix_name = new unix_name_t();
        if (!unix_name->set_service(service)) {
          msg(MSG_FATAL, "%s: Invalid service name", iname);
        }
      }
      if (is_server) {
        if (!unix_name->lock()) {
          socket_state = Socket_locking;
          TO.Set(0,200);
          flags |= Fl_Timeout;
          return;
        }
        if (!unix_name->claim_server()) {
          msg(MSG_FATAL, "%s: Unable to create server socket %s",
            iname, unix_name->get_svc_name());
        }
      }
      { struct hostent *hostinfo = 0;
        struct sockaddr_un local;
        nl_assert(iname != 0 && service != 0);
       
        fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0)
          msg(MSG_EXIT_ABNORM, "socket() failure in DAS_IO::Socket(%s): %s", iname,
            std::strerror(errno));
            
        local.sun_family = AF_UNIX;
        strncpy(local.sun_path, unix_name->get_svc_name(), UNIX_PATH_MAX);
        if (fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK) == -1) {
          msg(MSG_FATAL, "fcntl() failure in DAS_IO::Socket(%s): %s", iname,
            std::strerror(errno));
        }

        if (is_server) {
          unlink(local.sun_path);
          if (bind(fd, (struct sockaddr *)&local, SUN_LEN(&local)) < 0) {
            msg(MSG_FATAL, "bind() failure in DAS_IO::Socket(%s,%s): %s", iname,
              local.sun_path, std::strerror(errno));
          }

          if (listen(fd, MAXPENDING) < 0) {
            msg(MSG_FATAL, "listen() failure in DAS_IO::Socket(%s): %s", iname,
              std::strerror(errno));
          }
          socket_state = Socket_listening;
          flags = 0;
        } else {
          /* Establish the connection to the server */
          if (::connect(fd, (struct sockaddr *)&local,
                SUN_LEN(&local)) < 0) {
            if (errno != EINPROGRESS) {
              msg(MSG_ERROR, "connect() failure in DAS_IO::Socket(%s):%s: %s", iname,
                unix_name->get_svc_name(), std::strerror(errno));
              if (reset()) {
                msg(MSG_FATAL, "%s: Connect failure fatal after all retries", iname);
              }
              return;
            }
          }
          socket_state = Socket_connecting;
          flags = Fl_Write;
        }
        flags |= Fl_Read | Fl_Except;
      }
      break;
    case Socket_TCP:
      { nl_assert(iname != 0 && service != 0);
        char portname[6];
        
        if (get_service_port(service, portname)) {
          msg(MSG_FATAL, "%s: Unable to determine service port", iname);
        }
        
        fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0)
          msg(MSG_EXIT_ABNORM, "socket() failure in DAS_IO::Socket(%s): %s", iname,
            std::strerror(errno));

        if (fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK) == -1) {
          msg(MSG_FATAL, "fcntl() failure in DAS_IO::Socket(%s): %s", iname,
            std::strerror(errno));
        }

        // ### use hints to resolve portname even for server
        // ### refer to test/network/getaddrinfo.c for example code
        // struct sockaddr_in localAddr, srvrAddr;
        struct addrinfo hints;
        memset(&hints,0,sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = 0;
        hints.ai_flags = AI_ADDRCONFIG;
        struct addrinfo *res = 0;
        int err = getaddrinfo(hostname, portname, &hints, &res);
        if (err < 0) {
          msg(MSG_FATAL, "%s: getaddrinfo(%s, %s) failed with error %d: %s",
            iname, hostname, portname, err, gai_strerror(err));
        } else {
          
          
          /** Added in by Miles from getaddrinfo.c, 2019-11-01 */
          /* struct addrinfo *resi = res;
          while (resi) {
            struct sockaddr_in *addr = (struct sockaddr_in*)resi->ai_addr;
            printf("result port = %d\n", ntohs(addr->sin_port));
            printf("result addr = %d.%d.%d.%d\n",
              (addr->sin_addr.s_addr>>0)&0xFF,
              (addr->sin_addr.s_addr>>8)&0xFF,
              (addr->sin_addr.s_addr>>16)&0xFF,
              (addr->sin_addr.s_addr>>24)&0xFF);
            resi = resi->ai_next;
          } */
          /** End added code from getaddrinfo.c */
          
          
        }
        if (res == 0) {
          msg(MSG_FATAL, "%s: getaddrinfo(%s, %s) provided no result",
            iname, hostname, portname);
        }

        if (is_server) {
          if (bind(fd, res->ai_addr, sizeof(struct sockaddr_in)) < 0) {
            msg(MSG_FATAL, "%s: bind(%s, %s) failed with error %d: %s",
              iname, hostname, portname, errno, std::strerror(errno));
          }

          if (listen(fd, MAXPENDING) < 0) {
            msg(MSG_FATAL, "%s: listen() failure in DAS_IO::Socket(): %s", iname,
              std::strerror(errno));
          }
          socket_state = Socket_listening;
          flags = 0;
        } else {
          /* Establish the connection to the server */
          struct sockaddr_in *addr = (struct sockaddr_in *)res->ai_addr;
          if (::connect(fd, (struct sockaddr *)(addr),
                sizeof(struct sockaddr_in)) < 0) {
            if (errno != EINPROGRESS) {
              msg(MSG_ERROR,
                "%s: DAS_IO::Socket::connect() failure:%s(%d.%d.%d.%d):%s: %s",
                iname, hostname,
                (addr->sin_addr.s_addr>>0)&0xFF,
                (addr->sin_addr.s_addr>>8)&0xFF,
                (addr->sin_addr.s_addr>>16)&0xFF,
                (addr->sin_addr.s_addr>>24)&0xFF,
                portname,
                std::strerror(errno));
              if (reset()) {
                msg(MSG_FATAL, "%s: Connect failure fatal after all retries", iname);
              }
              return;
            }
          }
          socket_state = Socket_connecting;
          flags = Fl_Write;
        }
        flags |= Fl_Read | Fl_Except;
      }
      break;
    default:
      msg(MSG_FATAL, "DAS_IO_Socket::Socket::connect: not implemented for socket_type %d", socket_type);
  }
}

bool Socket::ProcessData(int flag) {
  int sock_err;

  // msg(0, "%s: Socket::ProcessData(%d)", iname, flag);
  switch (socket_state) {
    case Socket_locking:
      connect();
      return false;
    case Socket_connecting:
      if (!readSockError(&sock_err)) {
        return reset();
      }
      if ((flag & Fl_Write) &&
          (sock_err == EISCONN || sock_err == 0)) {
        msg(MSG_DEBUG, "%s: Connected", iname);
        socket_state = Socket_connected;
        TO.Clear();
        flags &= ~(Fl_Write|Fl_Timeout);
        flags |= Fl_Read;
        reconn_seconds = reconn_seconds_min;
        reconn_retries = 0;
        conn_fail_reported = false;
        return connected();
      } else {
        if (!conn_fail_reported) {
          if (TO.Expired()) {
            msg(MSG_ERROR, "%s: connect error: Client Timeout", iname);
          } else {
            msg(MSG_ERROR, "%s: connect error %d: %s", iname, sock_err,
              strerror(sock_err));
          }
          conn_fail_reported = true;
        }
        return( reset() || connect_failed());
      }
    case Socket_listening:
      if (flag & Fl_Read) {
        if (ELoop) {
          int new_fd = accept(fd, 0, 0);
          if (new_fd < 0) {
            msg(MSG_ERROR, "%s: Error from accept(): %s", iname, strerror(errno));
            return false;
          } else {
            if (fcntl(new_fd, F_SETFL, fcntl(new_fd, F_GETFL, 0) | O_NONBLOCK) == -1) {
              msg(MSG_FATAL, "fcntl() failure in DAS_IO::Socket(%s): %s", iname,
                std::strerror(errno));
            }
            Socket *client = new_client("clientcon", new_fd);
            return client->connected();
          }
        } else {
          msg(MSG_WARN, "Socket_listening: connection sensed, but no Loop");
        }
      }
      if (TO.Expired()) {
        return protocol_timeout();
      }
      break;
    case Socket_disconnected:
      // Handle timeout (i.e. attempt reconnection)
      if (TO.Expired()) {
        // msg(0, "%s: reconnecting after timeout", iname);
        TO.Clear();
        connect();
      }
      break;
    case Socket_connected:
      return Interface::ProcessData(flag);
    default:
      msg(MSG_EXIT_ABNORM, "DAS_IO::Socket::ProcessData: Invalid socket_state: %d", socket_state);
  }
  return false;
}

void Socket::set_retries(int max_retries, int min_dly, int max_foldback_dly) {
  reconn_max = max_retries;
  reconn_seconds = reconn_seconds_min = min_dly;
  reconn_seconds_max = max_foldback_dly;
}

void Socket::close() {
  if (fd >= 0) {
    ::close(fd);
    fd = -1;
    socket_state = Socket_disconnected;
    TO.Clear();
    flags &= ~(Fl_Write|Fl_Read|Fl_Except|Fl_Timeout);
    if (unix_name) {
      if (unix_name->is_server()) {
        unix_name->release_server();
      }
    }
  }
}

bool Socket::reset() {
  close();
  if (reconn_max == 0) {
    msg(MSG_ERROR, "%s: DAS_IO::Socket::reset(): No retries requested", iname);
    return true;
  } else if (reconn_max > 0 && reconn_retries++ >= reconn_max) {
    msg(MSG_ERROR, "%s: DAS_IO::Socket::reset(): Maximum connection retries exceeded", iname);
    return true;
  }
  int delay_secs = reconn_seconds;
  reconn_seconds *= 2;
  if (reconn_seconds > reconn_seconds_max)
    reconn_seconds = reconn_seconds_max;
  TO.Set(delay_secs,0);
  flags |= Fl_Timeout;
  return false;
}

bool Socket::read_error(int my_errno) {
  msg(MSG_ERROR, "%s: read error %d: %s", iname,
    my_errno, strerror(my_errno));
  
  if (is_server) {
    close();
    return true;
  } else if (is_server_client) {
    close();
    if (ELoop)
      ELoop->delete_child(this);
    return false;
  } else {
    return reset();
  }
}

bool Socket::iwrite_error(int my_errno) {
  msg(MSG_ERROR, "%s: write error %d: %s", iname,
    my_errno, strerror(my_errno));
  
  if (is_server) {
    close();
    return true;
  } else if (is_server_client) {
    close();
    if (ELoop)
      ELoop->delete_child(this);
    return false;
  } else {
    return reset();
  }
}

bool Socket::protocol_except() {
  msg(MSG_ERROR, "Socket::protocol_except not implemented");
  return true;
}

bool Socket::process_eof() {
  // msg(2, "%s: Socket closed", iname);
  
  if (is_server) {
    msg(MSG_ERROR, "%s: Should not have been reading from listening socket", iname);
    return true;
  } else if (is_server_client) {
    msg(MSG_DEBUG, "%s: Client disconnected", iname);
    if (ELoop)
      ELoop->delete_child(this);
    return false;
  } else {
    return true;
  }
}

bool Socket::connected() { return false; }

bool Socket::connect_failed() { return false; }

Socket *Socket::new_client(const char *iname, int fd) {
  Socket *rv = new Socket(this, iname, this->bufsize, fd);
  if (ELoop) ELoop->add_child(rv);
  return rv;
}

Socket *Socket::new_client() {
  Socket *rv = new_client(this->iname, this->fd);
  return rv;
}

bool Socket::readSockError(int *sock_err) {
  socklen_t optlen = sizeof(*sock_err);

  if (getsockopt(fd, SOL_SOCKET, SO_ERROR,
        sock_err, &optlen) == -1) {
    msg(MSG_ERROR, "%s: Error from getsockopt() %d: %s", iname, errno, strerror(errno));
    return false;
  }
  return true;
}

const char *Socket::get_version_string() {
  static char version[20]; // arbitrary string length
  FILE *fp = fopen("VERSION", "r");
  if (fp == 0) {
    msg(MSG_ERROR, "%s: VERSION file not found: defaulting to 1.0", iname);
    return "1.0";
  }
  if (fgets(version, 20, fp) == 0) {
    msg(MSG_ERROR, "%s: error %d reading VERSION file: %s: defaulting to 1.0",
      iname, errno, strerror(errno));
    fclose(fp);
    return "1.0";
  }
  fclose(fp);
  int i;
  for (i = strlen(version)-1; i >= 0 && isspace(version[i]); --i) {
    version[i] = '\0';
  }
  if (i < 0) {
    msg(MSG_ERROR, "%s: VERSION string is empty: defaulting to 1.0", iname);
    return "1.0";
  }
  return version;
}

/** Necessary global variables. */
bool read = false;
std::map<std::string, std::string> name_to_port;
const char * filename = "/services";

/** Changed by Miles on 2019-09-12 */
/** Returns true on error. */
bool Socket::get_service_port(const char *service, char *port) {
  /** First check if the file has already been read in. */
  if (!read) {
    /** Begin by opening and reading the file, line by line. */
    
    const char *tmbindir = getenv("TMBINDIR");
    if (tmbindir == 0) {
      //tmbindir = "bin/1.1";
      msg(MSG_FATAL, "TMBINDIR not defined!");
    }
    char *fullpath = (char *) new_memory(strlen(tmbindir) + strlen(filename) + 1);
    strcpy(fullpath, tmbindir);
    strcat(fullpath, filename);
    
    FILE * portfile = fopen(fullpath, "r");
    if (portfile == 0) {
      port[0] = '-';
      port[1] = '1';
      port[2] = '\0';
      msg(MSG_FATAL, "Cannot access %s!", fullpath);
      return true;
    }
    char current_line[16];
    char ch;
    
    bool name_captured;
    bool port_captured;
    int port_index;
    
    while (fgets(current_line, 16, portfile) != NULL) {
      name_captured = false;
      port_captured = false;
      port_index = 0;
      char name_accumulator[80];
      char port_accumulator[8];
      
      for (std::string::size_type i = 0; i < strlen(current_line); i++) {
        ch = current_line[i];
        
        if (ch == EOF) {
          read = true;
          break;
        } else {
          /** Capture the name of the service. */
          if (!name_captured) {
            if (i >= 80) {
              msg(MSG_FATAL, "arbitrary service name limit exceeded!");
            }
            if (isspace(ch)) {
              if (i > 0) {
                name_captured = true;
                name_accumulator[i] = '\0';
              } else {
                continue;
              }
            } else {
              name_accumulator[i] = ch;
            }
          }
          
          /** Capture the corresponding port number. */
          if (name_captured && !port_captured) {
            if (isspace(ch)) {
              if (isdigit(port_accumulator[0])) {
                port_captured = true;
                port_accumulator[port_index] = '\0';
              } else {
                continue;
              }
            }
            if (isdigit(ch)) {
              if (port_index >= 5) {
                msg(MSG_FATAL, "maximum port length exceeded!");
              } else {
                port_accumulator[port_index] = ch;
                port_index++;
              }
            } else {
              if (isdigit(port_accumulator[0])) {
                port_captured = true;
                port_accumulator[port_index] = '\0';
              } else {
                msg(MSG_FATAL, "error finding port for %s", service);
                port[0] = '-';
                port[1] = '1';
                port[2] = '\0';
                return true;
              }
            }
          }
        }
        
        /** Finally, store both in the map. */
        if (name_captured && port_captured) {
          //printf("%8s: %s\n", name_accumulator, port_accumulator);
          
          // TODO
          // make permanent std::string out of name_accumulator and port_accumulator
          // and then fill the map with those
          
          std::string name_string(name_accumulator);
          std::string port_string(port_accumulator);
          
          name_to_port[name_string] = port_string;
          memset(name_accumulator, 0, sizeof(name_accumulator));
          memset(port_accumulator, 0, sizeof(port_accumulator));
          break;
        }
      }
    }
    
    fclose(portfile);
    read = true;
    
    //TODO
    //make std::string for service just for lookup
    std::string service_string(service);
    std::string port_result = name_to_port[service_string];
    
    const char *port_c_str = port_result.c_str();
    strcpy(port, port_c_str);
    
    if (isdigit(port[0])) {
      return false;
    } else {
      msg(MSG_FATAL,"no port for %s!", service);
      port[0] = '-';
      port[1] = '1';
      port[2] = '\0';
      return true;
    }
  } else {
    std::string service_string(service);
    std::string port_result = name_to_port[service_string];
    
    const char *port_c_str = port_result.c_str();
    strcpy(port, port_c_str);
    
    if (isdigit(port[0])) {
      return false;
    } else {
      msg(MSG_FATAL,"no proper port number for %s!", service);
      port[0] = '-';
      port[1] = '1';
      port[2] = '\0';
      return true;
    }
  }
  
  /** If we really mess up. */
  msg(MSG_FATAL,"no port found for %s!", service);
  port[0] = '-';
  port[1] = '1';
  port[2] = '\0';
  return true;
}

}
