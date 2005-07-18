/* log.cpp
   Copyright (C) 2003,2004 Tommi Maekitalo

This file is part of cxxtools.

Cxxtools is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

Cxxtools is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with cxxtools; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330,
Boston, MA  02111-1307  USA
*/

#include <cxxtools/log/cxxtools_init.h>
#include <cxxtools/thread.h>
#include <cxxtools/udp.h>
#include <cxxtools/udpstream.h>
#include <list>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>

namespace cxxtools
{
  class LoggerImpl : public Logger
  {
      static std::ofstream outfile;
      static net::UdpSender loghost;
      static net::udpostream udpmessage;

    public:
      LoggerImpl(const std::string& c, log_level_type l)
        : Logger(c, l)
        { }
      std::ostream& getAppender();
      void endMessage();
      static void setFile(const std::string& fname);
      static void setLoghost(const std::string& host, unsigned short int port);
  };

  std::ostream& LoggerImpl::getAppender()
  {
    return outfile.is_open() ? outfile
         : loghost.isConnected() ? udpmessage : std::cout;
  }

  void LoggerImpl::endMessage()
  {
    if (outfile.is_open())
      outfile.flush();
    else if (loghost.isConnected())
      udpmessage.send();
    else
      std::cout.flush();
  }

  std::ofstream LoggerImpl::outfile;
  net::UdpSender LoggerImpl::loghost;
  net::udpostream LoggerImpl::udpmessage(LoggerImpl::loghost);

  void LoggerImpl::setFile(const std::string& fname)
  {
    outfile.open(fname.c_str(), std::ios::out | std::ios::app);
  }

  void LoggerImpl::setLoghost(const std::string& host, unsigned short int port)
  {
    loghost.connect(host.c_str(), port);
  }

  typedef std::list<Logger*> loggers_type;
  static loggers_type loggers;
  RWLock Logger::rwmutex;
  Mutex Logger::mutex;

  Logger::log_level_type Logger::std_level = LOG_LEVEL_ERROR;

  Logger* Logger::getLogger(const std::string& category)
  {
    // search existing Logger
    RdLock rdLock(rwmutex);

    loggers_type::iterator lower_bound_it = loggers.begin();
    while (lower_bound_it != loggers.end()
        && (*lower_bound_it)->getCategory() < category)
      ++lower_bound_it;

    if (lower_bound_it != loggers.end()
     && (*lower_bound_it)->getCategory() == category)
        return *lower_bound_it;

    // Logger not in list - change to write-lock
    rdLock.unlock();
    WrLock wrLock(rwmutex);

    // we have to do it again after gaining write-lock
    lower_bound_it = loggers.begin();
    while (lower_bound_it != loggers.end()
        && (*lower_bound_it)->getCategory() < category)
      ++lower_bound_it;

    if (lower_bound_it != loggers.end()
     && (*lower_bound_it)->getCategory() == category)
        return *lower_bound_it;

    // Logger still not in list, but we have a position to insert

    // search best-fit Logger
    std::string::size_type best_len = 0;
    log_level_type best_level = std_level;

    for (loggers_type::iterator it = loggers.begin();
         it != loggers.end(); ++it)
    {
      if ((*it)->getCategory().size() > best_len
        && (*it)->getCategory().size() < category.size()
        && category.at((*it)->getCategory().size()) == '.'
        && category.compare(0, (*it)->getCategory().size(), (*it)->getCategory()) == 0)
      {
        best_len = (*it)->getCategory().size();
        // update log-level
        best_level = (*it)->getLogLevel();
      }
    }

    // insert the new Logger in list and return pointer to the new list-element
    return *(loggers.insert(lower_bound_it, new LoggerImpl(category, best_level)));
  }

  Logger* Logger::setLevel(const std::string& category, log_level_type l)
  {
    WrLock lock(rwmutex);

    // search for existing Logger
    loggers_type::iterator it = loggers.begin();
    while (it != loggers.end()
        && (*it)->getCategory() < category)
      ++it;

    if (it == loggers.end() || (*it)->getCategory() != category)
    {
      // Logger not found - create new
      it = loggers.insert(it, new LoggerImpl(category, l));
    }
    else
      (*it)->setLogLevel(l); // Logger found - set level

    // return pointer to object in list
    return *it;
  }

  std::ostream& Logger::logentry(const char* level)
  {
    struct timeval t;
    struct tm tt;
    gettimeofday(&t, 0);
    time_t sec = static_cast<time_t>(t.tv_sec);
    localtime_r(&sec, &tt);

    std::ostream& out = getAppender();
    out << 1900 + tt.tm_year << '-'
        << ((tt.tm_mon + 1) / 10) << ((tt.tm_mon + 1) % 10) << '-'
        << (tt.tm_mday / 10) << (tt.tm_mday % 10) << ' '
        << (tt.tm_hour / 10) << (tt.tm_hour % 10) << ':'
        << (tt.tm_min / 10) << (tt.tm_min % 10) << ':'
        << (tt.tm_sec / 10)
        << (tt.tm_sec % 10 + t.tv_usec / 1e6)
        << " [" << pthread_self() << "] "
        << level << ' '
        << category << " - ";
    endMessage();

    return out;
  }

  LogTracer::~LogTracer()
  {
    if (msg)
    {
      if (l->isEnabled(Logger::LOG_LEVEL_TRACE))
      {
        cxxtools::MutexLock lock(cxxtools::Logger::mutex);
        l->logentry("TRACE")
          << "EXIT " << msg->str() << std::endl;
      }
      delete msg;
    }
  }

  std::ostream& LogTracer::logentry()
  {
    if (!msg)
      msg = new std::ostringstream();
    return *msg;
  }

  void LogTracer::enter()
  {
    if (msg && l->isEnabled(Logger::LOG_LEVEL_TRACE))
    {
      cxxtools::MutexLock lock(cxxtools::Logger::mutex);
      l->logentry("TRACE")
        << "ENTER " << msg->str() << std::endl;
    }
  }
}

void log_init(const std::string& propertyfilename)
{
  log_init(cxxtools::Logger::LOG_LEVEL_ERROR);

  std::ifstream in(propertyfilename.c_str());

  enum state_type {
    state_0,
    state_token,
    state_tokensp,
    state_category,
    state_level,
    state_rootlevel,
    state_filename0,
    state_filename,
    state_host0,
    state_host,
    state_port,
    state_skip
  };
  
  state_type state = state_0;

  char ch;
  std::string token;
  std::string category;
  std::string filename;
  std::string host;
  unsigned short int port;

  cxxtools::Logger::log_level_type level;
  while (in.get(ch))
  {
    switch (state)
    {
      case state_0:
        if (std::isalnum(ch))
        {
          token = std::toupper(ch);
          state = state_token;
        }
        else if (!std::isspace(ch))
          state = state_skip;
        break;

      case state_token:
        if (ch == '.')
        {
          if (token == "LOGGER")
            state = state_category;
          else
          {
            token.clear();
            state = state_token;
          }
        }
        else if (ch == '=' && token == "ROOTLOGGER")
          state = state_rootlevel;
        else if (ch == '=' && token == "FILE")
          state = state_filename0;
        else if (ch == '=' && token == "HOST")
          state = state_host0;
        else if (ch == '\n')
          state = state_0;
        else if (std::isspace(ch))
          state = state_tokensp;
        else if (std::isalnum(ch))
          token += std::toupper(ch);
        else
        {
          token.clear();
          state = state_skip;
        }
        break;

      case state_tokensp:
        if (ch == '=' && token == "ROOTLOGGER")
          state = state_rootlevel;
        else if (ch == '=' && token == "FILE")
          state = state_filename0;
        else if (ch == '\n')
          state = state_0;
        else if (!std::isspace(ch))
          state = state_skip;
        break;

      case state_category:
        if (std::isalnum(ch) || ch == '.')
          category += ch;
        else if (ch == '=')
          state = state_level;
        else
        {
          category.clear();
          token.clear();
          state = (ch == '\n' ? state_0 : state_skip);
        }
        break;

      case state_level:
      case state_rootlevel:
        if (ch != '\n' && std::isspace(ch))
          break;

        switch (ch)
        {
          case 'F': level = cxxtools::Logger::LOG_LEVEL_FATAL; break;
          case 'E': level = cxxtools::Logger::LOG_LEVEL_ERROR; break;
          case 'W': level = cxxtools::Logger::LOG_LEVEL_WARN; break;
          case 'I': level = cxxtools::Logger::LOG_LEVEL_INFO; break;
          case 'D': level = cxxtools::Logger::LOG_LEVEL_DEBUG; break;
          case 'T': level = cxxtools::Logger::LOG_LEVEL_TRACE; break;
          default:  level = cxxtools::Logger::getStdLevel(); break;
        }
        if (state == state_rootlevel)
          cxxtools::Logger::setRootLevel(level);
        else
          cxxtools::Logger::setLevel(category, level);
        category.clear();
        token.clear();
        state = state_skip;
        break;

      case state_filename0:
        if (ch != '\n' && std::isspace(ch))
          break;

      case state_filename:
        if (ch == '\n')
        {
          cxxtools::LoggerImpl::setFile(filename);
          token.clear();
          filename.clear();
          state = state_0;
        }
        else
          filename += ch;
        break;

      case state_host0:
        if (ch != '\n' && std::isspace(ch))
          break;

      case state_host:
        if (ch == ':')
        {
          port = 0;
          state = state_port;
        }
        else if (std::isspace(ch))
          state = state_skip;
        else
          host += ch;
        break;

      case state_port:
        if (std::isdigit(ch))
          port = port * 10 + ch - '0';
        else if (port > 0)
        {
          cxxtools::LoggerImpl::setLoghost(host, port);
          state = (ch == '\n' ? state_0 : state_skip);
        }
        break;

      case state_skip:
        if (ch == '\n')
          state = state_0;
        break;
    }
  }
}

void log_init()
{
  char* LOGPROPERTIES = ::getenv("LOGPROPERTIES");
  if (LOGPROPERTIES)
    log_init(LOGPROPERTIES);
  else
  {
    struct stat s;
    if (stat("log.properties", &s) == 0)
      log_init("log.properties");
    else
      log_init(cxxtools::Logger::LOG_LEVEL_ERROR);
  }
}

