#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "cpin.h"
#include "errors.h"
#include "fileio.h"

#define MAX_LINE 4096

// ── note lifecycle ────────────────────────────────────────────────────────────

cpin_note_t fileio_create_note(char* file, char* line, char* content) {
    cpin_note_t note;
    note.file    = file    ? strdup(file)    : NULL;
    note.line    = line    ? strdup(line)    : NULL;
    note.content = content ? strdup(content) : NULL;
    return note;
}

void fileio_note_free(cpin_note_t* note) {
    if (!note) return;
    free(note->file);
    free(note->line);
    free(note->content);
    note->file = note->line = note->content = NULL;
}

// ── in-memory representation ──────────────────────────────────────────────────
//
// Storage format:
//
//   version: 0.3
//   [src/main.c:2]
//   10:first note
//   42:added later
//   [src/parser.c:1]
//   7:some note
//
// Each block header is [filepath:notecount]. Notes within a block are sorted
// by line number so future binary search over line numbers is possible.
// The note count in the header allows callers to skip a block without reading
// its note lines.

typedef struct {
    int   line_num;  // 0 means no specific line
    char* line_str;  // original string (may be "" for no-line notes)
    char* content;
} note_entry_t;

typedef struct {
    char*        filepath;
    note_entry_t* notes;
    size_t        count;
    size_t        capacity;
} file_block_t;

typedef struct {
    int          version;
    file_block_t* blocks;
    size_t        block_count;
    size_t        block_capacity;
} notes_db_t;

static void free_note_entry(note_entry_t* e) {
    free(e->line_str);
    free(e->content);
}

static void free_block(file_block_t* b) {
    free(b->filepath);
    for (size_t i = 0; i < b->count; i++)
        free_note_entry(&b->notes[i]);
    free(b->notes);
}

static void notes_db_free(notes_db_t* db) {
    for (size_t i = 0; i < db->block_count; i++)
        free_block(&db->blocks[i]);
    free(db->blocks);
    db->blocks = NULL;
    db->block_count = db->block_capacity = 0;
}

// ── parsing ───────────────────────────────────────────────────────────────────

static void strip_newline(char* s) {
    size_t n = strlen(s);
    if (n > 0 && s[n - 1] == '\n') s[n - 1] = '\0';
}

// Parse "[filepath:count]" in-place. Writes NUL bytes into buf.
// Returns 0 on success, -1 on malformed input.
static int parse_block_header(char* buf, char** out_path, size_t* out_count) {
    if (buf[0] != '[') return -1;
    char* end = strrchr(buf, ']');
    if (!end) return -1;
    *end = '\0';

    char* inner = buf + 1;
    char* colon = strrchr(inner, ':');
    if (!colon) return -1;
    *colon = '\0';

    *out_path  = inner;
    *out_count = (size_t)atoi(colon + 1);
    return 0;
}

// Parse "linenum:content" in-place. Writes one NUL byte into buf.
// Returns 0 on success, -1 on malformed input.
static int parse_note_line(char* buf, char** out_line_str, int* out_line_num, char** out_content) {
    char* colon = strchr(buf, ':');
    if (!colon) return -1;
    *colon        = '\0';
    *out_line_str = buf;
    *out_line_num = (*buf) ? atoi(buf) : 0;
    *out_content  = colon + 1;
    return 0;
}

// ── db I/O ────────────────────────────────────────────────────────────────────

// Load the entire notes file into an in-memory notes_db_t.
// Returns CPIN_SUCCESS, CPIN_ERR_NOTE_NOT_FOUND (file absent), or error.
static cpin_error_t db_read(const char* notes_path, notes_db_t* db) {
    db->version        = CPIN_VERSION_MAJOR;
    db->blocks         = NULL;
    db->block_count    = 0;
    db->block_capacity = 0;

    FILE* f = fopen(notes_path, "r");
    if (!f) return CPIN_ERR_NOTE_NOT_FOUND;

    char buf[MAX_LINE];

    // First line must be the version header.
    if (!fgets(buf, sizeof(buf), f)) { fclose(f); return CPIN_SUCCESS; }
    strip_newline(buf);
    if (strncmp(buf, "version: ", 9) == 0)
        db->version = atoi(buf + 9);  // reads major only; minor after '.' ignored for now

    // Read block headers and their note lines.
    while (fgets(buf, sizeof(buf), f)) {
        strip_newline(buf);
        if (buf[0] != '[') continue;

        char tmp[MAX_LINE];
        strncpy(tmp, buf, sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = '\0';

        char*  path;
        size_t note_count;
        if (parse_block_header(tmp, &path, &note_count) != 0) continue;

        // Grow blocks array if needed.
        if (db->block_count == db->block_capacity) {
            size_t new_cap = db->block_capacity ? db->block_capacity * 2 : 8;
            file_block_t* grown = realloc(db->blocks, new_cap * sizeof(file_block_t));
            if (!grown) { fclose(f); notes_db_free(db); return CPIN_ERR_WRITE_FAILED; }
            db->blocks         = grown;
            db->block_capacity = new_cap;
        }

        file_block_t* blk = &db->blocks[db->block_count++];
        blk->filepath = strdup(path);
        blk->capacity = note_count > 0 ? note_count : 4;
        blk->count    = 0;
        blk->notes    = malloc(blk->capacity * sizeof(note_entry_t));
        if (!blk->filepath || !blk->notes) { fclose(f); notes_db_free(db); return CPIN_ERR_WRITE_FAILED; }

        // Read exactly note_count note lines for this block.
        // Using the stored count lets us skip entire blocks in the future.
        for (size_t i = 0; i < note_count; i++) {
            if (!fgets(buf, sizeof(buf), f)) break;
            strip_newline(buf);

            char ntmp[MAX_LINE];
            strncpy(ntmp, buf, sizeof(ntmp) - 1);
            ntmp[sizeof(ntmp) - 1] = '\0';

            char* line_str;
            int   line_num;
            char* content;
            if (parse_note_line(ntmp, &line_str, &line_num, &content) != 0) continue;

            if (blk->count == blk->capacity) {
                size_t new_cap = blk->capacity * 2;
                note_entry_t* grown = realloc(blk->notes, new_cap * sizeof(note_entry_t));
                if (!grown) { fclose(f); notes_db_free(db); return CPIN_ERR_WRITE_FAILED; }
                blk->notes    = grown;
                blk->capacity = new_cap;
            }

            note_entry_t* e = &blk->notes[blk->count++];
            e->line_num = line_num;
            e->line_str = strdup(line_str);
            e->content  = strdup(content);
        }
    }

    fclose(f);
    return CPIN_SUCCESS;
}

// Write the in-memory notes_db_t back to disk.
static cpin_error_t db_write(const char* notes_path, const notes_db_t* db) {
    FILE* f = fopen(notes_path, "w");
    if (!f) return CPIN_ERR_WRITE_FAILED;

    fprintf(f, "version: %d.%d\n", CPIN_VERSION_MAJOR, CPIN_VERSION_MINOR);

    for (size_t i = 0; i < db->block_count; i++) {
        const file_block_t* b = &db->blocks[i];
        if (b->count == 0) continue;  // omit empty blocks

        fprintf(f, "[%s:%zu]\n", b->filepath, b->count);
        for (size_t j = 0; j < b->count; j++) {
            const note_entry_t* e = &b->notes[j];
            fprintf(f, "%s:%s\n", e->line_str, e->content);
        }
    }

    fclose(f);
    return CPIN_SUCCESS;
}

// ── storage helpers ───────────────────────────────────────────────────────────

// Ensures the directory containing notes_path exists.
static cpin_error_t fileio_initialize(const char* notes_path) {
    char dir[4096];
    strncpy(dir, notes_path, sizeof(dir) - 1);
    dir[sizeof(dir) - 1] = '\0';

    char* slash = strrchr(dir, '/');
    if (!slash) return CPIN_SUCCESS;
    *slash = '\0';

    struct stat st = {0};
    if (stat(dir, &st) == -1) {
        if (mkdir(dir, 0755) != 0)
            return CPIN_ERR_STORAGE_INIT;
    }
    return CPIN_SUCCESS;
}

// Find a block by filepath. Returns NULL if not found.
static file_block_t* find_block(notes_db_t* db, const char* filepath) {
    for (size_t i = 0; i < db->block_count; i++) {
        if (strcmp(db->blocks[i].filepath, filepath) == 0)
            return &db->blocks[i];
    }
    return NULL;
}

// Append a new empty block for filepath. Returns NULL on OOM.
static file_block_t* add_block(notes_db_t* db, const char* filepath) {
    if (db->block_count == db->block_capacity) {
        size_t new_cap = db->block_capacity ? db->block_capacity * 2 : 8;
        file_block_t* grown = realloc(db->blocks, new_cap * sizeof(file_block_t));
        if (!grown) return NULL;
        db->blocks         = grown;
        db->block_capacity = new_cap;
    }

    file_block_t* b = &db->blocks[db->block_count++];
    b->filepath = strdup(filepath);
    b->notes    = malloc(4 * sizeof(note_entry_t));
    b->count    = 0;
    b->capacity = 4;
    if (!b->filepath || !b->notes) return NULL;
    return b;
}

// Insert a note into a block maintaining ascending sort order by line_num.
// Notes with the same line_num are appended after existing ones at that line.
static cpin_error_t block_insert(file_block_t* b, const char* line_str,int line_num, const char* content) {
    if (b->count == b->capacity) {
        size_t new_cap = b->capacity * 2;
        note_entry_t* grown = realloc(b->notes, new_cap * sizeof(note_entry_t));
        if (!grown) return CPIN_ERR_WRITE_FAILED;
        b->notes    = grown;
        b->capacity = new_cap;
    }

    // Find the insertion position; check for duplicates along the way.
    size_t pos = b->count;
    int warn = 0;
    for (size_t i = 0; i < b->count; i++) {
        if (b->notes[i].line_num == line_num) {
            if (strcmp(b->notes[i].content, content) == 0)
                return CPIN_ERR_DUPLICATE_NOTE;
            warn = 1;
        }
        if (b->notes[i].line_num > line_num) { pos = i; break; }
    }

    memmove(&b->notes[pos + 1], &b->notes[pos],
            (b->count - pos) * sizeof(note_entry_t));

    b->notes[pos].line_num = line_num;
    b->notes[pos].line_str = strdup(line_str);
    b->notes[pos].content  = strdup(content);
    b->count++;
    return warn ? CPIN_WARN_DUPLICATE_LINE : CPIN_SUCCESS;
}

// ── output helpers ────────────────────────────────────────────────────────────

// Append display lines matching an optional line filter to *out / *out_len.
// Display format: "filepath:line_str:content\n" (compatible with main.c parsers).
static cpin_error_t append_matching(char** out, size_t* out_len,
                                    const file_block_t* b, const char* line_filter) {
    for (size_t i = 0; i < b->count; i++) {
        const note_entry_t* e = &b->notes[i];
        if (line_filter && strcmp(e->line_str, line_filter) != 0) continue;

        char display[MAX_LINE];
        int n = snprintf(display, sizeof(display), "%s:%s:%s\n",
                         b->filepath, e->line_str, e->content);
        if (n <= 0) continue;

        char* tmp = realloc(*out, *out_len + (size_t)n + 1);
        if (!tmp) return CPIN_ERR_WRITE_FAILED;
        *out = tmp;
        memcpy(*out + *out_len, display, (size_t)n);
        *out_len += (size_t)n;
        (*out)[*out_len] = '\0';
    }
    return CPIN_SUCCESS;
}

// ── public API ────────────────────────────────────────────────────────────────

cpin_error_t fileio_save(cpin_note_t* node, const char* notes_path) {
    if (!node || !node->file || !node->content) return CPIN_ERR_INVALID_ARGS;

    cpin_error_t err = fileio_initialize(notes_path);
    if (err != CPIN_SUCCESS) return err;

    notes_db_t db;
    err = db_read(notes_path, &db);
    if (err != CPIN_SUCCESS && err != CPIN_ERR_NOTE_NOT_FOUND) return err;

    file_block_t* block = find_block(&db, node->file);
    if (!block) {
        block = add_block(&db, node->file);
        if (!block) { notes_db_free(&db); return CPIN_ERR_WRITE_FAILED; }
    }

    const char* line_str = (node->line && node->line[0] != '\0') ? node->line : "";
    int line_num = line_str[0] ? atoi(line_str) : 0;

    cpin_error_t insert_err = block_insert(block, line_str, line_num, node->content);
    if (insert_err != CPIN_SUCCESS && insert_err != CPIN_WARN_DUPLICATE_LINE) {
        notes_db_free(&db);
        return insert_err;
    }

    cpin_error_t write_err = db_write(notes_path, &db);
    notes_db_free(&db);
    return (write_err != CPIN_SUCCESS) ? write_err : insert_err;
}

cpin_error_t fileio_load(char* file, char* line, const char* notes_path, char** result) {
    if (!file || !result) return CPIN_ERR_INVALID_ARGS;
    *result = NULL;

    notes_db_t db;
    cpin_error_t err = db_read(notes_path, &db);
    if (err != CPIN_SUCCESS) return err;

    const file_block_t* block = find_block(&db, file);
    if (!block) { notes_db_free(&db); return CPIN_ERR_NOTE_NOT_FOUND; }

    char*  out     = NULL;
    size_t out_len = 0;
    err = append_matching(&out, &out_len, block, line);
    if (err != CPIN_SUCCESS) { free(out); notes_db_free(&db); return err; }

    notes_db_free(&db);
    *result = out;
    return (out_len > 0) ? CPIN_SUCCESS : CPIN_ERR_NOTE_NOT_FOUND;
}

cpin_error_t fileio_load_all(const char* notes_path, char** result) {
    if (!result) return CPIN_ERR_INVALID_ARGS;
    *result = NULL;

    notes_db_t db;
    cpin_error_t err = db_read(notes_path, &db);
    if (err != CPIN_SUCCESS) return err;

    char*  out     = NULL;
    size_t out_len = 0;
    for (size_t i = 0; i < db.block_count; i++) {
        err = append_matching(&out, &out_len, &db.blocks[i], NULL);
        if (err != CPIN_SUCCESS) { free(out); notes_db_free(&db); return err; }
    }

    notes_db_free(&db);
    *result = out;
    return (out_len > 0) ? CPIN_SUCCESS : CPIN_ERR_NOTE_NOT_FOUND;
}

cpin_error_t fileio_delete(char* file, char* line, const char* notes_path) {
    if (!file) return CPIN_ERR_INVALID_ARGS;

    notes_db_t db;
    cpin_error_t err = db_read(notes_path, &db);
    if (err != CPIN_SUCCESS) return err;

    file_block_t* block = find_block(&db, file);
    if (!block) { notes_db_free(&db); return CPIN_ERR_NOTE_NOT_FOUND; }

    int deleted = 0;
    for (size_t i = 0; i < block->count; ) {
        int match = (!line || strcmp(block->notes[i].line_str, line) == 0);
        if (match) {
            free_note_entry(&block->notes[i]);
            memmove(&block->notes[i], &block->notes[i + 1],
                    (block->count - i - 1) * sizeof(note_entry_t));
            block->count--;
            deleted = 1;
        } else {
            i++;
        }
    }

    if (!deleted) { notes_db_free(&db); return CPIN_ERR_NOTE_NOT_FOUND; }

    err = db_write(notes_path, &db);
    notes_db_free(&db);
    return err;
}

cpin_error_t fileio_search(const char* keyword, const char* notes_path, char** result) {
    if (!keyword || !result) return CPIN_ERR_INVALID_ARGS;
    *result = NULL;

    notes_db_t db;
    cpin_error_t err = db_read(notes_path, &db);
    if (err != CPIN_SUCCESS) return err;

    char*  out     = NULL;
    size_t out_len = 0;

    for (size_t i = 0; i < db.block_count; i++) {
        const file_block_t* b = &db.blocks[i];
        for (size_t j = 0; j < b->count; j++) {
            const note_entry_t* e = &b->notes[j];
            if (!strstr(e->content, keyword)) continue;

            char display[MAX_LINE];
            int n = snprintf(display, sizeof(display), "%s:%s:%s\n",
                             b->filepath, e->line_str, e->content);
            if (n <= 0) continue;

            char* tmp = realloc(out, out_len + (size_t)n + 1);
            if (!tmp) { free(out); notes_db_free(&db); return CPIN_ERR_WRITE_FAILED; }
            out = tmp;
            memcpy(out + out_len, display, (size_t)n);
            out_len += (size_t)n;
            out[out_len] = '\0';
        }
    }

    notes_db_free(&db);
    *result = out;
    return (out_len > 0) ? CPIN_SUCCESS : CPIN_ERR_NOTE_NOT_FOUND;
}

// Checks if a file exists on disk.
// Returns: CPIN_SUCCESS if file exists, CPIN_ERR_FILE_NOT_FOUND if not found,
// or CPIN_ERR_FILE_ACCESS for other errors.
cpin_error_t fileio_file_exist(char* file) {
    if (!file) return CPIN_ERR_INVALID_ARGS;

    FILE* found_file = fopen(file, "r");
    if (found_file == NULL) {
        if (FILE_NOT_FOUND(errno))
            return CPIN_ERR_FILE_NOT_FOUND;
        return CPIN_ERR_FILE_ACCESS;
    }

    fclose(found_file);
    return CPIN_SUCCESS;
}
