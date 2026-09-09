/* Vita syslog stub - 日志不可用时丢弃 */
#ifndef _VITA_SHIM_SYSLOG_H
#define _VITA_SHIM_SYSLOG_H

#define LOG_EMERG   0
#define LOG_ALERT   1
#define LOG_CRIT    2
#define LOG_ERR     3
#define LOG_WARNING 4
#define LOG_NOTICE  5
#define LOG_INFO    6
#define LOG_DEBUG   7

void openlog (const char *ident, int logopt, int facility);
void syslog (int priority, const char *format, ...);
void closelog (void);

#endif /* _VITA_SHIM_SYSLOG_H */
