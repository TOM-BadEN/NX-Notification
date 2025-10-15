

void log_info_impl(const char *file, int line, const char *fmt, ...);
void log_warning_impl(const char *file, int line, const char *fmt, ...);
void log_error_impl(const char *file, int line, const char *fmt, ...);
void log_debug_impl(const char *file, int line, const char *fmt, ...);

#define log_info(fmt, ...)    log_info_impl(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define log_warning(fmt, ...) log_warning_impl(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define log_error(fmt, ...)   log_error_impl(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define log_debug(fmt, ...)   log_debug_impl(__FILE__, __LINE__, fmt, ##__VA_ARGS__)