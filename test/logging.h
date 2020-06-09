#pragma once

#if defined(BETEL)
#include <glog/logging.h>

#else

#include <assert.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <ctime>

enum LogLevel {
    INFO = 0,
    WARNING,
    ERROR,
    FATAL
};

#ifdef WIN32
#define filename(x) strrchr(x,'\\')?strrchr(x,'\\')+1:x
#else
#define filename(x) strrchr(x,'/')?strrchr(x,'/')+1:x
#endif

#define LOGINIT(logfile, level)     LogMessage::init(logfile, level)
#define LOGLEVEL(level)             LogMessage::setLogLevel(level)
#define LOGCLOSE()                  LogMessage::close()
#define LOG(severity) \
    LogMessage(filename(__FILE__), __LINE__, severity).stream()

#define CHECK(x) \
    if(!(x)) \
        LogMessage(filename(__FILE__), __LINE__, FATAL).stream() << "Check failed: " #x << ' '

#define ABORT() \
    LogMessage(filename(__FILE__), __LINE__, FATAL).stream() << " Abort " << ' '

#if defined(NDEBUG)
#define DLOG(severity) \
  true ? (void) 0 : LogMessageVoidify() & LOG(severity)
#define DCHECK(x)           do{}while(0)
#else
#define DLOG(severity)      LOG(severity)
#define DCHECK(x)           CHECK(x) 
#endif

class LogMessageVoidify {
public:
    LogMessageVoidify() { }
    // This has to be an operator with a precedence lower than << but
    // higher than ?:
    void operator&(std::ostream&) { }
};

#if defined(_WIN32)
template<class _Elem,
	class _Traits
> class BasicNullStream : public std::basic_ostream<_Elem, _Traits>{
public:
	typedef std::basic_ostream<_Elem, _Traits> _Mybase;
	typedef std::basic_streambuf<_Elem, _Traits> _Mysb;
	BasicNullStream()
		: _Mybase((_Mysb*)nullptr) {}
};
using OrionNullStream = BasicNullStream<char, std::char_traits<char> >;
#else
class OrionNullStream : public std::ostream {
public:
	OrionNullStream() : std::ostream(NULL) {}
};
#endif

class Error : public std::runtime_error {
public:
    explicit Error(const std::string& s) : std::runtime_error(s){}
};

class DateLogger{
public:
    DateLogger(){}
    const char* HumanDate(){
        time_t tv = time(NULL);
        struct tm now;
#if defined(_WIN32)
        localtime_s(&now, &tv);
#else
        localtime_r(&tv, &now);
#endif
        snprintf(_buffer, sizeof(_buffer), "%02d:%02d:%02d", now.tm_hour, now.tm_min, now.tm_sec);
        return _buffer;
    }
private:
    char _buffer[9];
};

class LogMessage {
public:
    LogMessage(const char* file, int line, LogLevel log_level);
    ~LogMessage();

    std::ostream& stream();

public:
    static void init(const std::string& log_filename, LogLevel log_level = FATAL);
    static void setLogLevel(LogLevel log_level);
    static void close();

private:
    std::ostream& getStream();
    std::string   LogLevelStr();

private:
    static std::ofstream _log_file;
    static LogLevel      _base_log_level;

    std::ostringstream _log_stream;

    OrionNullStream  _nullstream;
    LogLevel    _log_level;
    DateLogger  _pretty_date;
};

#endif
