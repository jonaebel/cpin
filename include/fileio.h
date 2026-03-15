#ifndef FILEIO_H
#define FILEIO_H

#include "errors.h"

typedef struct {
    char* file;      // Path to the file containing the note
    char* line;      // Line number in the file (can be NULL)
    char* content;   // Actual content of the note
} cpin_note_t;

// Checks if the error code indicates 'No such file or directory'
// @e: the errno value to check
// Returns: true if the error is ENOENT, false otherwise
#define FILE_NOT_FOUND(e) ((e) == ENOENT)

// Creates a new cpin note structure with the specified file, line, and content
// @file: path to the file where the note belongs
// @line: line number in the file (can be NULL)
// @content: the actual note content
// Returns: initialized cpin_note_t structure
cpin_note_t fileio_create_note(char* file, char* line, char* content);

// Frees the memory allocated for a cpin note structure
// @note: pointer to the note structure to free (must not be NULL)
void fileio_note_free(cpin_note_t* note);

// Storage format (v1): grouped block layout.
//
//   version: 1
//   [src/main.c:2]
//   10:first note
//   42:added later
//   [src/parser.c:1]
//   7:some note
//
// Each block header is "[filepath:notecount]". Notes within a block are kept
// sorted by line number, which prepares the ground for binary search. The note
// count in the header makes it possible to skip over a block without reading
// its note lines.
//
// NOTE: this format is incompatible with the old flat "file:line:content" layout.
// Existing .cpin/notes files must be deleted before using the new version.

// Saves a note to the given notes file path.
// Inserts the note into the correct file block in sorted line-number order,
// creating the block if it does not yet exist.
// @node: the note to save
// @notes_path: path to the notes file (local or global)
// Returns: CPIN_SUCCESS on success, or appropriate error code on failure
cpin_error_t fileio_save(cpin_note_t* node, const char* notes_path);

// Loads notes matching file (and optionally line) into a newly allocated string (*result).
// Seeks directly to the [file:N] block — no full-file scan needed.
// @file: path to the file to filter by
// @line: specific line number to filter by (can be NULL for entire file)
// @notes_path: path to the notes file (local or global)
// @result: pointer to store the loaded results — caller must free(*result)
// Returns: CPIN_SUCCESS on success, or appropriate error code on failure
cpin_error_t fileio_load(char* file, char* line, const char* notes_path, char** result);

// Loads all notes from the notes file into a newly allocated string (*result).
// Output is naturally grouped by file (one block per file, notes sorted by line).
// @notes_path: path to the notes file (local or global)
// @result: pointer to store the loaded results — caller must free(*result)
// Returns: CPIN_SUCCESS on success, CPIN_ERR_NOTE_NOT_FOUND if no notes exist
cpin_error_t fileio_load_all(const char* notes_path, char** result);

// Deletes all notes matching file (and optionally line), then rewrites the file.
// Empty blocks are pruned from the file automatically.
// @file: path to the file containing the note
// @line: line number to match (NULL removes all notes for that file)
// @notes_path: path to the notes file (local or global)
// Returns: CPIN_SUCCESS on success, or appropriate error code on failure
cpin_error_t fileio_delete(char* file, char* line, const char* notes_path);

// Searches all notes for entries whose content contains the given keyword.
// Entire file blocks whose notes do not match can be skipped early.
// @keyword: substring to search for in note content
// @notes_path: path to the notes file (local or global)
// @result: pointer to store the matching results — caller must free(*result)
// Returns: CPIN_SUCCESS on success, CPIN_ERR_NOTE_NOT_FOUND if no matches
cpin_error_t fileio_search(const char* keyword, const char* notes_path, char** result);

// Checks if a file exists on disk.
// @file: path to the file to be checked on the disk
// Returns: CPIN_SUCCESS if file exists, CPIN_ERR_FILE_NOT_FOUND if not found,
//          or CPIN_ERR_FILE_ACCESS for other errors
cpin_error_t fileio_file_exist(char* file);

// Checks if a specific line in a file is empty.
// @file: path to the file to check
// @line: line number to check (as string)
// Returns: 1 if the line is empty, 0 if it contains content, -1 on error
int fileio_line_is_empty(char* file, char* line);

// Checks if a specific line number is out of bounds for a file.
// @file: path to the file to check
// @line: line number to check (as string)
// Returns: 1 if the line number is out of bounds, 0 if valid, -1 on error
int fileio_line_is_out_of_bound(char* file, char* line);

#endif
