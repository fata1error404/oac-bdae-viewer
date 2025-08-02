#ifndef _GAME_LOGGER_H_
#define _GAME_LOGGER_H_

#include <iostream>

// #define MODEL_INFO

template <typename... Args>
inline void LOG(Args &&...args)
{
#ifdef MODEL_INFO
    (std::cout << ... << args) << std::endl;
#endif
}

// #include "Singleton.h"
// // #include "Thread.h"
// // #include "NetPacket.h"
// // #include "ThreadSafeBlockQueue.h"
// // #include "ThreadSafePool.h"

// enum LogColor
// {
// #ifdef WIN32

//     LogRed = FOREGROUND_RED | FOREGROUND_INTENSITY,
//     LogGreen = FOREGROUND_GREEN | FOREGROUND_INTENSITY,
//     LogYellow = FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY,
//     LogNormal = FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_BLUE,
//     LogWhite = LogNormal | FOREGROUND_INTENSITY,
//     LogBlue = FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY,
//     LogReverseRed = BACKGROUND_RED | LogNormal,
// #else
//     LogRed = 1,
//     LogGreen,
//     LogYellow,
//     LogNormal,
//     LogWhite,
//     LogBlue,
//     LogReverseRed,
//     LogClrCount,
// #endif
// };

// enum ALogEventID // the enum is from online
// {
//     CHAT_EVENT = 0x40, // ��64��ʼ 0-63 ����
// };

// class Logger : public Thread, public Singleton<Logger>
// {
//     friend class Singleton<Logger>;
//     THREAD_RUNTIME(Logger)
// private:
//     Logger();
//     ~Logger();
//     virtual int _Prepare();
//     virtual int _Kernel();
//     virtual int _Finish();
//     void _Update4SysLog();
//     void _Update4FileLog();
//     void _SwitchLogFile(time_t now);
//     void _ConsoleOutput(const std::string &str);

//     void Color(LogColor color);

// public:
//     void Init(int level, const string &ident, const string &dir, bool enable_syslog, bool enable_console_output);
//     void Log(const char *cur_file, const char *cur_fun, int cur_line, char type, const char *fmt, ...);
//     void ChatLog(int msgType, const char *from_gluid, const char *from, const char *to, uint32 fromId, uint32 toId, const char *mapName, const char *msgText);
//     void SetLevel(int level);
//     int GetLevel() { return _log_level; };

// protected:
//     std::string *AllocString();
//     void FreeString(std::string *s);
//     bool IsIncludeVerticalLine(const char *buf);
//     const char *EscapeVerticalLine(const char *buf);
//     int SprintChatLog(const char *key, const char *value, char *buf, int bufLen);
//     int SprintChatLog(const char *key, uint32 value, char *buf, int bufLen);

// private:
//     typedef ThreadSafeBlockQueue<std::string *> LogQueueType;
//     LogQueueType _log_queue;
//     ThreadSafePool<std::string, 256> _str_pool;
//     std::ofstream _log;
//     int _log_level;
//     int _last_switch_log_day;
//     bool _enable_filelog;
//     std::string _log_ident;
//     std::string _log_dir;
//     bool _enable_console_output;
//     int _discarded_log_count;
// #ifdef WIN32
//     HANDLE m_hStdOut;
// #endif
// };

// #define sLogger (*Logger::instance())

// // #define FORMAT_COMPILE_CHECK

// #ifdef FORMAT_COMPILE_CHECK

#define ELOG printf
#define WLOG printf
// #define NLOG printf
// #define DLOG printf
// #define ILOG printf
#define _LOG printf

// #else

// #define ELOG(...) sLogger.Log(__FILE__, __FUNCTION__, __LINE__, 'E', __VA_ARGS__) // error conditions
// #define WLOG(...) sLogger.Log(__FILE__, __FUNCTION__, __LINE__, 'W', __VA_ARGS__) // warning conditions
// #define NLOG(...) sLogger.Log(__FILE__, __FUNCTION__, __LINE__, 'N', __VA_ARGS__) // normal but significant condition
// #define CHATLOG sLogger.ChatLog

// #ifdef _DEBUG
// #define DLOG(...) sLogger.Log(__FILE__, __FUNCTION__, __LINE__, 'D', __VA_ARGS__) // debug-level messages
// #define ILOG(...) sLogger.Log(__FILE__, __FUNCTION__, __LINE__, 'I', __VA_ARGS__) // informational
// #else
// #define DLOG(...)
// #define ILOG(...)
// #endif

// void DebugLog(const char *msg, ...);

// #if defined(_DEBUG) || defined(_DEBUG_)
// #define _LOG(format, ...) DebugLog(format, ##__VA_ARGS__)
// #else
// #define _LOG(format, ...)
// #endif

// #endif

// #define NET_DEBUG

#endif
