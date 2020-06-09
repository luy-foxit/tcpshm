#include "logging.h"

#if !defined(BETEL)

using namespace std;

std::ofstream LogMessage::_log_file;
LogLevel      LogMessage::_base_log_level = INFO; 
#if defined(__ANDROID__)
#include <android/log.h>
static android_LogPriority androidLogLevel(LogLevel level) {
    switch(level)
    {
        case INFO:
            return ANDROID_LOG_INFO;
        case WARNING:
            return ANDROID_LOG_WARN;
        case ERROR:
            return ANDROID_LOG_ERROR;
        case FATAL:
            return ANDROID_LOG_FATAL;
    }
    return ANDROID_LOG_DEBUG;
}
#endif

LogMessage::LogMessage(const char* file, int line, LogLevel log_level)
     : _log_level(log_level)
{
    _log_stream << "[" << LogLevelStr() << "]"
      <<  "[" << _pretty_date.HumanDate() << "]" << file << ": " << line << ": ";
}

LogMessage::~LogMessage() {
    getStream() << _log_stream.str() << std::endl << std::flush;
#if defined(__ANDROID__)
    if((_log_level >= _base_log_level) && !_log_file.is_open()) {
        android_LogPriority level = androidLogLevel(_log_level);
        __android_log_print(level, "person-tracker", "%s", _log_stream.str().c_str());
    }
#endif

    if(FATAL == _log_level) {
        close();
        throw Error(_log_stream.str());
    }
}

void LogMessage::init(const std::string& log_filename, LogLevel log_level/*=FATAL*/) {
    _log_file.open(log_filename.c_str(), ios::out);
    _base_log_level = log_level;
}

void LogMessage::setLogLevel(LogLevel log_level) {
    _base_log_level = log_level;
}

void LogMessage::close() {
    if(_log_file.is_open())
        _log_file.close();
}

std::string LogMessage::LogLevelStr() {
    switch(_log_level)
    {
        case INFO:
            return "INFO";
        case WARNING:
            return "WARNING";
        case ERROR:
            return "ERROR";
        case FATAL:
            return "FATAL";
    }
    return "";
}

std::ostream& LogMessage::getStream() {
    if(_log_level < _base_log_level) {
        return _nullstream;
    }
    return _log_file.is_open() ? _log_file : (_log_level >= ERROR ? std::cerr : std::cout);
}

std::ostream& LogMessage::stream() {
    return _log_stream;
}
#endif
