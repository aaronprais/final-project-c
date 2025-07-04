#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

// Log levels enum
typedef enum {
    LOG_DEBUG = 0,
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR,
    LOG_CRITICAL
} LogLevel;

// Error types enum
typedef enum {
    ERROR_NONE = 0,
    ERROR_FILE_NOT_FOUND,
    ERROR_FILE_NOT_OPENING,
    ERROR_PERMISSION_DENIED,
    ERROR_MEMORY_ALLOCATION,
    ERROR_INVALID_PARAMETER,
    ERROR_NETWORK_CONNECTION,
    ERROR_TIMEOUT,
    ERROR_AUTHENTICATION_FAILED,
    ERROR_DATABASE_CONNECTION,
    ERROR_BUFFER_OVERFLOW,
    ERROR_UNKNOWN
} ErrorType;

// Functions
void log_error(ErrorType error_num);
void log_message(LogLevel level, const char* message);

// Helper functions
const char* get_log_level_string(LogLevel level);
const char* get_error_description(ErrorType error);
void write_to_log_file(const char* log_entry);
char* get_current_datetime(void);

#endif // LOGGER_H