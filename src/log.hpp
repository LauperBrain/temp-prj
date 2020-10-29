#ifndef __LOG_HPP_
#define __LOG_HPP_

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <unistd.h>
#include <sys/time.h>
#include <syslog.h>
#include <syscall.h>
#include <mutex>

/*
 * LOG LEVEL DEFINITION
 */
#define LOGGER              "logger"
#define LOGGER_EMERG        0
#define LOGGER_ALERT        1
#define LOGGER_CRIT         2
#define LOGGER_ERR          3
#define LOGGER_WARNING      4
#define LOGGER_NOTICE       5
#define LOGGER_INFO         6
#define LOGGER_DEBUG        7

#define __FILENAME__        strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__

typedef void(*logger_cb_t)(int, const char *, const void *);

class Logger
{
public:
    static Logger *inst() { static Logger *self = 0; if (self == 0) self = new Logger; return self; }

    ~Logger()
    {
        if (this->syslog_)  closelog();
        if (name_) free(name_);
    }

protected:
    Logger() {}

public:
    void redirect(logger_cb_t cb, const void *obj, bool copy)
    {
        cb_ = cb;
        obj_ = obj;
        copy_ = copy;
    }

    int level() { return this->level_; }
    void level(int value) { this->level_ = value; }

    void name(const char *name)
    {
        if (name_)
            free(name_);
        name_ = strdup(name);
    }

    const char *category(int level)
    {
        static const char *levels[] = { "E", "A", "C", "E", "W", "N", "I", "D" }; 
        return levels[level];
    }

    void syslog(bool value)
    {
        syslog_ = value;
        if (syslog_)
            openlog(name_ ? name_ : LOGGER, LOG_CONS | LOG_PID | LOG_NDELAY, LOG_DAEMON);
        else
            closelog();
    }

    void format(bool prefix, const char *fmt, ...)
    {
        va_list ap;
        va_start(ap, fmt);
        vsprintf(prefix ? prefix_ : suffix_, fmt, ap);
        va_end(ap);
    }

    void print(int level, const char *fmt, ...)
    {
        if (level > this->level_)
            return;

        std::lock_guard<std::mutex> locker(mutex_);
        char *msg = nullptr;

        va_list ap;
        va_start(ap, fmt);
        vasprintf(&msg, fmt, ap);
        va_end(ap);

        if (cb_) {
            (*cb_)(level, msg, obj_);

            if (!copy_)
                goto CLEANUP;
        }

        if (!this->syslog_)
            fprintf(stdout, "%s\n", msg);
        else
            ::syslog(LOG_USER | LOG_INFO, "%s", msg);

CLEANUP:
        free(msg);
    }

    void log(int level, const char *fmt, ...)
    {
        if (level > this->level_)
            return;
        
        std::lock_guard<std::mutex> locker(mutex_);

        char *msg;
        va_list ap;
        va_start(ap, fmt);
        vasprintf(&msg, fmt, ap);
        va_end(ap);

        if (this->cb_) {
            char *log;
            asprintf(&log, "[%s] %s [%ld] %s%s%s", this->category(level), 
                    this->name_ ? this->name_ : LOGGER, syscall(SYS_gettid), 
                    this->prefix_, msg, this->suffix_);

            (*this->cb_)(level, log, this->obj_);

            free(log);

            if (!this->copy_)
                goto CLEANUP;
        }

        if (!this->syslog_)
            fprintf(stdout, "%s[%s] %s [%ld] %s%s%s%s\n", level_color_start_[level], this->category(level),
                this->name_ ? this->name_ : LOGGER, syscall(SYS_gettid),
                this->prefix_, msg, this->suffix_, level_color_end_);

        else
            ::syslog(LOG_USER | LOG_INFO, "%s%s%s", this->prefix_, msg, this->suffix_);

CLEANUP:
        free(msg);
    }

    void stamp(int level, const char *fmt, ...)
    {
        if (level > this->level_) return;

        std::lock_guard<std::mutex> locker(this->mutex_);

        char *msg;

        va_list ap;
        va_start(ap, fmt);
        vasprintf(&msg, fmt, ap);
        va_end(ap);

        struct timeval tv;
        gettimeofday(&tv, NULL);
        struct tm tm;
        localtime_r(&tv.tv_sec, &tm);                                                                                                                                                                                         
        char tmp[32] = { 0 };
        strftime(tmp, sizeof(tmp), "%y-%m-%d %H:%M:%S", &tm);

        if (this->cb_) {
            char *log;
            asprintf(&log, "[%s] [%s] %s [%ld]: %s%s%s", tmp,
                    this->category(level), this->name_ ? this->name_ : LOGGER,
                    syscall(SYS_gettid), this->prefix_, msg, this->suffix_);

            (*this->cb_)(level, log, this->obj_);

            free(log);
            if (!this->copy_)
                goto CLEANUP;
        }

        if (!this->syslog_)
            fprintf(stdout, "%s[%s] [%s] %s [%ld]: %s%s%s%s\n", level_color_start_[level], tmp,
                    this->category(level), this->name_ ? this->name_ : LOGGER,
                    syscall(SYS_gettid), this->prefix_, msg, this->suffix_, level_color_end_);
        else
            ::syslog(LOG_USER | LOG_INFO, "%s%s%s", this->prefix_, msg, this->suffix_);

CLEANUP:
        free(msg);
    }

    void dump(const void *buf, uint16_t len)
    {

        if (LOGGER_DEBUG > this->level_) return;

        std::lock_guard<std::mutex> locker(this->mutex_);

        const size_t length = 16;
        char prefix[64] = { 0 };
        sprintf(prefix, "%s [%ld]", this->name_ ? this->name_ : LOGGER, syscall(SYS_gettid));
        const size_t padding = 18 + strlen(prefix) + 1;
        size_t index = 0;
        char hex[padding + length * 4], str[length + 1];

        memset(hex, 0x20, padding);

        auto bin_to_hex = [](char *hex, const uint8_t *bin, size_t bin_len, bool space, bool upper) {
            for (size_t i = 0, j = 0; i < bin_len; i++) {
                sprintf(hex + i * 2 + j, upper ? "%02X" : "%02x", bin[i]);

                if (!space)  continue;

                strcat((char *)hex, " ");
                j++;
            }
        };

        while (index < len) {
            size_t line_len = len - index > length ? length : len - index;
            bin_to_hex(hex + padding, (uint8_t *)buf + index, line_len, true, false);
            if (line_len < length) for (size_t i = 0; i < length - line_len; i++) strcat(hex, "   ");
            memcpy(str, (uint8_t *)buf + index, line_len);
            for (size_t i = 0; i < length; i++) 
                if ((uint8_t)str[i] < 0x20 || (uint8_t)str[i] > 0x7E)
                    str[i] = 0x2E;

            str[line_len] = '\0';
            index += line_len;

            this->print(LOGGER_DEBUG, "%s\t\t%s", hex, str);
        }
    }

    void dump(const char *tag, const void *buf, uint16_t len)
    {
        this->stamp(LOGGER_DEBUG, "%s: %d(bytes)", tag, len);
        this->dump(buf, len);
    }

private:
    std::mutex mutex_;

    logger_cb_t cb_ = nullptr;
    const void *obj_ = nullptr;
    bool copy_ = false;

    int level_ = LOGGER_DEBUG;

    bool syslog_ = false;

    char *name_ = nullptr;        // The porcess name of syslog output.

    char prefix_[128] = { 0 };
    char suffix_[128] = { 0 };
#define LOGGER_EMERG        0
#define LOGGER_ALERT        1
#define LOGGER_CRIT         2
#define LOGGER_ERR          3
#define LOGGER_WARNING      4
#define LOGGER_NOTICE       5
#define LOGGER_INFO         6
#define LOGGER_DEBUG        7
    const char *level_color_start_[8] = { "\033[31{m",/*EMERG red*/ "\033[31{m", /*ALERT red*/ "\033[31{m", /*CRIT red*/ "\033[31m", /*ERR red*/"\033[33m",/*WARN yellow*/ 
        "\033[35m", /*NOTICE purple*/ "\033[34m",/* INFO blue*/ "\033[36m" /* DEBUG green*/ };
    const char *level_color_end_     = "\033[0m";
};

/*
 * Marcos of Logger
 */

#define LOG_REDIRECT(callback_function, user_object, is_copy)     \
    Logger::inst()->redirect(callback_function, user_object, is_copy)

#define LOG_LEVEL(level)                Logger::inst()->level(level)
#define LOG_NAME(n)                     Logger::inst()->name(n)
#define LOG_CATEGORY(level)             Logger::inst()->category(level)
#define LOG_SYS(is_syslog)              Logger::inst()->syslog(is_syslog)

#define LOGM(fmt, ...)                                              \
    do {                                                            \
        Logger::inst()->format(true, "");                           \
        Logger::inst()->format(false, "");                          \
        Logger::inst()->stamp(LOGGER_INFO, fmt, ##__VA_ARGS__);     \
    } while(0)

#define LOGF(level, fmt, ...)                                       \
    do {                                                            \
        Logger::inst()->format(true, "%s: ", __FUNCTION__);         \
        Logger::inst()->format(false, " (%s:%d)", __FILENAME__, __LINE__);\
        Logger::inst()->stamp(level, fmt, ##__VA_ARGS__);           \
    } while(0)


#define LOGD(fmt, ...)                  LOGF(LOGGER_DEBUG, fmt, ##__VA_ARGS__)
#define LOGI(fmt, ...)                  LOGF(LOGGER_INFO, fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...)                  LOGF(LOGGER_WARNING, fmt, ##__VA_ARGS__)
#define LOGE(fmt, ...)                  LOGF(LOGGER_ERR, fmt, ##__VA_ARGS__)

#define LOGM_WITH_RETURN(ret, fmt, ...)		do { LOGM(fmt, ##__VA_ARGS__); return ret; } while(0)

#define LOGD_WITH_RETURN(ret, fmt, ...)		do { LOGD(fmt, ##__VA_ARGS__); return ret; } while(0)
#define LOGI_WITH_RETURN(ret, fmt, ...)		do { LOGI(fmt, ##__VA_ARGS__); return ret; } while(0)
#define LOGW_WITH_RETURN(ret, fmt, ...)		do { LOGW(fmt, ##__VA_ARGS__); return ret; } while(0)
#define LOGE_WITH_RETURN(ret, fmt, ...)		do { LOGE(fmt, ##__VA_ARGS__); return ret; } while(0)

#define LOGD_WITH_GOTO(label, fmt, ...)		do { LOGD(fmt, ##__VA_ARGS__); goto label; } while(0)
#define LOGI_WITH_GOTO(label, fmt, ...)		do { LOGI(fmt, ##__VA_ARGS__); goto label; } while(0)
#define LOGW_WITH_GOTO(label, fmt, ...)		do { LOGW(fmt, ##__VA_ARGS__); goto label; } while(0)
#define LOGE_WITH_GOTO(label, fmt, ...)		do { LOGE(fmt, ##__VA_ARGS__); goto label; } while(0)

#define LOG_HEX_DUMP(x, y)                  Logger::inst()->dump(x, y)

#define LOG_HEX_DUMP_WITH_TAG(x, y, z)                              \
    do {                                                            \
        Logger::inst()->format(true, "%s: ", __FUNCTION__);         \
        Logger::inst()->format(false, " (%s:%d)", __FILENAME__, __LINE__); \
        Logger::inst()->dump(x, y, z);                              \
    } while(0)

#define LOG_EXP_CHECK(exp, ok, fmt, ...)                            \
    do {                                                            \
        typeof(exp) ret; ret = (exp);                               \
        if (ret != ok) LOGE(fmt, ##__VA_ARGS__);                    \
    } while(0)

#define LOGGER_TERMINATE                    delete Logger::inst();

#endif // __LOG_HPP_

