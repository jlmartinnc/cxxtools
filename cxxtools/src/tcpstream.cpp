////////////////////////////////////////////////////////////////////////
// tcpstream.cpp
//

#include "tcpstream.h"
#include <stdexcept>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/poll.h>

namespace tcp
{

  using namespace std;

  namespace {
    class safeflags
    {
        int fd;
        int flags;
      public:
        safeflags(int _fd)
          : fd(_fd)
        {
          flags = fcntl(fd, F_GETFL);
          if (flags < 0)
          {
            int errnum = errno;
            throw Exception(strerror(errnum));
          }
        }

        ~safeflags()
        {
          if (flags >= 0)
            fcntl(fd, F_SETFL, flags);
        }
    };
  }

  Exception::Exception(int Errno, const string& msg)
    : runtime_error(msg + ": " + strerror(Errno)),
      m_Errno(Errno)
    { }

  Exception::Exception(const string& msg)
    : runtime_error(msg + ": " + strerror(errno)),
      m_Errno(errno)
    { }

  ////////////////////////////////////////////////////////////////////////
  // implementation of Socket
  //
  Socket::Socket(int domain, int type, int protocol) throw (Exception)
  {
    if ((m_sockFd = ::socket(domain, type, protocol)) < 0)
      throw Exception("cannot create socket");
  }

  Socket::~Socket()
  {
    if (m_sockFd >= 0)
    {
      if (::close(m_sockFd) < 0)
        fprintf(stderr, "error in close(%d)\n", (int)m_sockFd);
    }
  }

  void Socket::create(int domain, int type, int protocol) throw (Exception)
  {
    close();

    if ((m_sockFd = ::socket(domain, type, protocol)) < 0)
      throw Exception("cannot create socket");
  }


  void Socket::close()
  {
    if (m_sockFd >= 0)
    {
      ::close(m_sockFd);
      m_sockFd = -1;
    }
  }

  ////////////////////////////////////////////////////////////////////////
  // implementation of Server
  //
  Server::Server(int backlog) throw (Exception)
    : Socket(AF_INET, SOCK_STREAM, 0)
  {
    Listen(backlog);
  }

  Server::Server(const std::string& ipaddr, int port, int backlog)
                 throw (Exception)
    : Socket(AF_INET, SOCK_STREAM, 0)
  {
    Listen(ipaddr.c_str(), port, backlog);
  }

  Server::Server(const char* ipaddr, int port, int backlog) throw (Exception)
    : Socket(AF_INET, SOCK_STREAM, 0)
  {
    Listen(ipaddr, port, backlog);
  }

  void Server::Listen(int backlog) throw (Exception)
  {
    if (::listen(getFd(), backlog) < 0)
      throw Exception("error in listen");
  }

  void Server::Listen(const char* ipaddr, int port, int backlog) throw (Exception)
  {
    struct sockaddr_in servaddr;

    memset(&servaddr, 0, sizeof(servaddr));

    if (::inet_pton(AF_INET, ipaddr, &servaddr.sin_addr) <= 0)
      throw Exception("invalid ipaddress");

    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);

    int reuseAddr = 1;
    if (::setsockopt(getFd(), SOL_SOCKET, SO_REUSEADDR,
        &reuseAddr, sizeof(reuseAddr)) < 0)
      throw Exception("error in setsockopt");

    if (::bind(getFd(), (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
      throw Exception("error in bind");

    if (::listen(getFd(), backlog) < 0)
      throw Exception("error in listen");
  }

  ////////////////////////////////////////////////////////////////////////
  // implementation of Stream
  //
  Stream::Stream()
    : Socket(AF_INET, SOCK_STREAM, 0)
    { }

  Stream::Stream(const Server& server)
    : Socket(accept(server.getFd(), 0, 0))
  {
    if (bad())
      throw Exception("error in accept");
  }

  Stream::Stream(const string& ipaddr, int port)
    : Socket(AF_INET, SOCK_STREAM, 0)
  {
    Connect(ipaddr, port);
  }

  Stream::Stream(const char* ipaddr, int port)
    : Socket(AF_INET, SOCK_STREAM, 0)
  {
    Connect(ipaddr, port);
  }

  void Stream::Accept(const Server& server)
  {
    setFd(accept(server.getFd(), 0, 0));
    if (bad())
      throw Exception("error in accept");
  }

  void Stream::Connect(const char* ipaddr, int port)
  {
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);

    if (inet_pton(AF_INET, ipaddr, &servaddr.sin_addr) <= 0)
      throw Exception("invalid ipaddress");

    if (::connect(getFd(), (struct sockaddr *)&servaddr,
        sizeof(servaddr)) < 0)
      throw Exception("error in connect");
  }

  Stream::size_type Stream::Read(char* buffer,
    Stream::size_type bufsize, int timeout) const
  {
    if (timeout < 0)
    {
      size_type n = ::read(getFd(), buffer, bufsize);
      if (n < 0)
      {
        // das ging schief
        int errnum = errno;
        throw Exception(strerror(errnum));
      }

      return n;
    }
    else
    {
      ssize_t n;

      {
        safeflags flags(getFd());
        int f = fcntl(getFd(), F_SETFL, O_NONBLOCK);
        if (f < 0)
        {
          int errnum = errno;
          throw Exception(strerror(errnum));
        }

        n = ::read(getFd(), buffer, bufsize);
      }

      if (n == -1 && errno == EAGAIN)
      {
        // keine Daten da - dann verwenden wir poll
        struct pollfd fds;
        fds.fd = getFd();
        fds.events = POLLIN;
        int p = poll(&fds, 1, timeout);
        if (p < 0)
        {
          int errnum = errno;
          throw Exception(strerror(errnum));
        }
        else if (p == 0)
          throw Timeout();

        n = ::read(getFd(), buffer, bufsize);
      }

      return n;
    }
  }

  Stream::size_type Stream::Write(const char* buffer,
                                  Stream::size_type bufsize) const
  {
    size_t n = ::write(getFd(), buffer, bufsize);
    if (n <= 0)
      // au weia - das ging schief
      throw Exception("tcp::Stream: error in write");

    return n;
  }

  streambuf::streambuf(Stream& stream, unsigned bufsize, int timeout)
    : m_stream(stream),
      m_bufsize(bufsize),
      m_buffer(new char_type[bufsize]),
      m_timeout(timeout)
  { }

  streambuf::int_type streambuf::overflow(streambuf::int_type c)
  {
    if (pptr() != pbase())
    {
      int n = m_stream.Write(pbase(), pptr() - pbase());
      if (n <= 0)
        return traits_type::eof();
    }

    setp(m_buffer, m_buffer + m_bufsize);
    if (c != traits_type::eof())
    {
      *pptr() = (char_type)c;
      pbump(1);
    }

    return 0;
  }

  streambuf::int_type streambuf::underflow()
  {
    Stream::size_type n = m_stream.Read(m_buffer, m_bufsize, m_timeout);
    if (n <= 0)
      return traits_type::eof();

    setg(m_buffer, m_buffer, m_buffer + n);
    return (int_type)(unsigned char)m_buffer[0];
  }

  int streambuf::sync()
  {
    if (pptr() != pbase())
    {
      int n = m_stream.Write(pbase(), pptr() - pbase());
      if (n <= 0)
        return -1;
      else
        setp(m_buffer, m_buffer + m_bufsize);
    }
    return 0;
  }

} // namespace tcp

