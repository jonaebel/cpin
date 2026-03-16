// errors.h
#ifndef ERRORS_H
#define ERRORS_H

typedef enum {
    CPIN_SUCCESS = 0,           // Operation completed successfully
    CPIN_ERR_INVALID_ARGS,      // Invalid function arguments provided
    CPIN_ERR_INVALID_TARGET,    // Target format is invalid (expected file:line)
    CPIN_ERR_FILE_NOT_FOUND,    // Specified file could not be found
    CPIN_ERR_NO_LINE_SPECIFIED,
    CPIN_ERR_STORAGE_INIT,      // Failed to initialize storage directory
    CPIN_ERR_NOTE_NOT_FOUND,    // Specified node/entry not found
    CPIN_ERR_WRITE_FAILED,      // Failed to write to file or storage
    CPIN_ERR_FILE_ACCESS,       // File access error
    CPIN_WARN_DUPLICATE_LINE,  // same file + same line, different content → warning, still saves
    CPIN_ERR_DUPLICATE_NOTE,   // same file + same line + same content    → error, blocks save
    CPIN_WARN_EMPTY_LINE,      // Line exists but is empty or contains only whitespace
    CPIN_ERR_LINE_OUT_OF_BOUNDS, // Line number exceeds the number of lines in the file
} cpin_error_t;

// Converts an error code to a human-readable string representation
// @error: the error code to convert
// Returns: pointer to static string describing the error
const char* error_to_string(cpin_error_t error);

#endif
