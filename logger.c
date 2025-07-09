#include "logger.h"

#define LOG_FILE_NAME "logging.log"
#define MAX_LOG_ENTRY_SIZE 1024
#define MAX_DATETIME_SIZE 64

// Array of error descriptions corresponding to ErrorType enum
static const char* error_descriptions[] = {
    "No error",
    "File not found",
    "File not Opening",
    "Permission denied",
    "Memory allocation failed",
    "Invalid parameter provided",
    "Network connection failed",
    "Operation timeout",
    "Authentication failed",
    "Database connection failed",
    "Buffer overflow detected",
    "Unknown error occurred"
};

// Array of log level strings corresponding to LogLevel enum
static const char* log_level_strings[] = {
    "DEBUG",
    "INFO",
    "WARNING",
    "ERROR",
    "CRITICAL"
};

/**
 * Gets the string representation of a log level
 */
const char* get_log_level_string(LogLevel level) {
    if (level >= 0 && level < sizeof(log_level_strings) / sizeof(log_level_strings[0])) {
        return log_level_strings[level];
    }
    return "UNKNOWN";
}

/**
 * Gets the error description for a given error type
 */
const char* get_error_description(ErrorType error) {
    if (error >= 0 && error < sizeof(error_descriptions) / sizeof(error_descriptions[0])) {
        return error_descriptions[error];
    }
    return "Unknown error";
}

/**
 * Gets current date and time as a formatted string
 */
char* get_current_datetime(void) {
    static char datetime_buffer[MAX_DATETIME_SIZE];
    time_t now = time(NULL);
    struct tm* local_time = localtime(&now);

    strftime(datetime_buffer, sizeof(datetime_buffer), "%Y-%m-%d %H:%M:%S", local_time);
    return datetime_buffer;
}

/**
 * Writes a log entry to the log file
 */
void write_to_log_file(const char* log_entry) {
    FILE* log_file = fopen(LOG_FILE_NAME, "a");
    if (log_file == NULL) {
        fprintf(stderr, "Failed to open log file: %s\n", LOG_FILE_NAME);
        return;
    }

    fprintf(log_file, "%s\n", log_entry);
    fclose(log_file);
}

/**
 * Logs an error based on error number enum
 * Prints to console and writes to log file
 */
void log_error(ErrorType error_num) {
    if (error_num == ERROR_NONE) {
        return; // No error to log
    }

    const char* error_desc = get_error_description(error_num);
    char* datetime = get_current_datetime();
    char log_entry[MAX_LOG_ENTRY_SIZE];

    // Format: datetime -- ERROR ("description") -- "Error occurred"
    snprintf(log_entry, sizeof(log_entry),
         "%s -- %-8s -- %d -> %s",
         datetime, "ERROR", error_num, error_desc);

    // Print to console
    printf("ERROR: %s\n", error_desc);

    // Write to log file
    write_to_log_file(log_entry);
}

/**
 * Logs a message with specified log level
 * Writes to log file only
 */
void log_message(LogLevel level, const char* message) {
    if (message == NULL) {
        return;
    }

    const char* level_str = get_log_level_string(level);
    char* datetime = get_current_datetime();
    char log_entry[MAX_LOG_ENTRY_SIZE];

    // Format: datetime -- LOGLEVEL ("description") -- "message"
    snprintf(log_entry, sizeof(log_entry),
         "%s -- %-8s -- %s",
         datetime, level_str, message);

    // Write to log file
    write_to_log_file(log_entry);
}