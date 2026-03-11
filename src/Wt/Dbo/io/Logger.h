// Dbo-local logging facade, intentionally decoupled from Wt::WLogger/WStringStream.
#ifndef WT_DBO_IO_LOGGER_H_
#define WT_DBO_IO_LOGGER_H_

#include <Wt/fmtlog.h>

#define LOGGER(s) static constexpr const char* logger = s
#define WT_LOGGER logger

#if !defined(NDEBUG) || defined(WT_DEBUG_ENABLED)
#define LOG_DEBUG_S(s, m, ...) logd(m, ##__VA_ARGS__)
#define LOG_DEBUG(m, ...) logd(m, ##__VA_ARGS__)
#else
#define LOG_DEBUG_S(s, m, ...)
#define LOG_DEBUG(m, ...)
#endif

#define LOG_INFO_S(s, m, ...) logi(m, ##__VA_ARGS__)
#define LOG_INFO(m, ...) logi(m, ##__VA_ARGS__)
#define LOG_WARN_S(s, m, ...) logw(m, ##__VA_ARGS__)
#define LOG_WARN(m, ...) logw(m, ##__VA_ARGS__)
#define LOG_SECURE_S(s, m, ...) logs(m, ##__VA_ARGS__)
#define LOG_SECURE(m, ...) logs(m, ##__VA_ARGS__)
#define LOG_ERROR_S(s, m, ...) loge(m, ##__VA_ARGS__)
#define LOG_ERROR(m, ...) loge(m, ##__VA_ARGS__)

#endif // WT_DBO_IO_LOGGER_H_
