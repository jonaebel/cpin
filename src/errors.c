#include <stdio.h>
#include <stdlib.h>
#include "errors.h"

const char* error_to_string(cpin_error_t error) {
    switch (error) {
        case CPIN_SUCCESS:              return "success";
        case CPIN_ERR_INVALID_ARGS:    return "invalid arguments";
        case CPIN_ERR_INVALID_TARGET:  return "invalid target (expected file:line)";
        case CPIN_ERR_FILE_NOT_FOUND:  return "file not found";
        case CPIN_ERR_NO_LINE_SPECIFIED: return "no line specified (expected file:line)";
        case CPIN_ERR_STORAGE_INIT:    return "failed to initialize .cpin storage";
        case CPIN_ERR_NOTE_NOT_FOUND:  return "note not found";
        case CPIN_ERR_WRITE_FAILED:    return "write failed";
        case CPIN_ERR_FILE_ACCESS:      return "Cannot access file";
        case CPIN_WARN_DUPLICATE_LINE: return "a note at this line already exists";
        case CPIN_ERR_DUPLICATE_NOTE:  return "identical note already exists at this line";
        case CPIN_WARN_EMPTY_LINE:     return "line is empty or contains only whitespace";
        case CPIN_ERR_LINE_OUT_OF_BOUNDS: return "line number exceeds the number of lines in the file";
        default:                       return "unknown error";
    }
}

void cpin_report(cpin_severity_t severity, cpin_error_t error) {
    const char* prefix = (severity == CPIN_SEVERITY_WARNING) ? "warning" : "error";
    fprintf(stderr, "%s: %s\n", prefix, error_to_string(error));
    if (severity == CPIN_SEVERITY_ERROR) {
        exit(1);
    }
}
