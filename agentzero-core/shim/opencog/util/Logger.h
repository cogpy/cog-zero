/*
 * Minimal Logger shim compatible with OpenCog logger() streaming API
 * and simple string overloads used by some Agent-Zero sources.
 */
#ifndef _AGENTZERO_SHIM_LOGGER_H
#define _AGENTZERO_SHIM_LOGGER_H

#include <iostream>
#include <sstream>
#include <string>
#include <mutex>

namespace opencog {

class Logger {
public:
    enum Level { NONE, ERROR, WARN, INFO, DEBUG, FINE };

    class LogStream {
    public:
        LogStream(Logger& logger, Level level)
            : _logger(logger), _level(level), _active(level <= logger.get_level()) {}

        ~LogStream() {
            if (_active) {
                _logger.emit(_level, _ss.str());
            }
        }

        template <typename T>
        LogStream& operator<<(const T& v) {
            if (_active) _ss << v;
            return *this;
        }

        LogStream& operator<<(std::ostream& (*)(std::ostream&)) {
            return *this;
        }

    private:
        Logger& _logger;
        Level _level;
        bool _active;
        std::ostringstream _ss;
    };

    Logger() = default;

    void set_level(Level l) { _level = l; }
    Level get_level() const { return _level; }
    void set_timestamp_flag(bool) {}
    void set_print_level_flag(bool) {}
    void set_print_to_stdout_flag(bool v) { _to_stdout = v; }

    // Streaming API: logger().info() << "msg"
    LogStream error() { return LogStream(*this, ERROR); }
    LogStream warn()  { return LogStream(*this, WARN); }
    LogStream info()  { return LogStream(*this, INFO); }
    LogStream debug() { return LogStream(*this, DEBUG); }
    LogStream fine()  { return LogStream(*this, FINE); }

    // String API: logger().info("msg")
    void error(const std::string& msg) { emit(ERROR, msg); }
    void warn(const std::string& msg)  { emit(WARN, msg); }
    void info(const std::string& msg)  { emit(INFO, msg); }
    void debug(const std::string& msg) { emit(DEBUG, msg); }
    void fine(const std::string& msg)  { emit(FINE, msg); }

    // Support logger().error() << x and also char* via template above.
    // For ambiguous cases where code does logger().info() without <<,
    // LogStream destructor emits empty string — fine.

    void emit(Level level, const std::string& msg) {
        if (!_to_stdout || msg.empty()) return;
        if (level > _level) return;
        std::lock_guard<std::mutex> lock(_mu);
        const char* tag = "INFO";
        switch (level) {
            case ERROR: tag = "ERROR"; break;
            case WARN:  tag = "WARN"; break;
            case INFO:  tag = "INFO"; break;
            case DEBUG: tag = "DEBUG"; break;
            case FINE:  tag = "FINE"; break;
            default: break;
        }
        std::cerr << "[" << tag << "] " << msg << std::endl;
    }

private:
    Level _level = WARN; // quieter default for tests
    bool _to_stdout = false;
    std::mutex _mu;
};

inline Logger& logger() {
    static Logger log;
    return log;
}

} // namespace opencog

#endif
