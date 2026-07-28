#include "commands.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "flags.h"
#include "parser.h"
#include "errors.h"
#include "fileio.h"


void commands_usage(void) {
    printf("Usage:\n");
    printf("  cpin add <file:line> \"<note>\" [--global]\n");
    printf("  cpin list [file] [line] [--global]\n");
    printf("  cpin remove <file:line> [--global]\n");
    printf("  cpin search <keyword> [--global]\n");
    printf("  cpin export [--json|--md] [--global]\n");
}

static void print_json_string(const char* s) {
    putchar('"');
    for (; *s; s++) {
        if (*s == '"')       printf("\\\"");
        else if (*s == '\\') printf("\\\\");
        else if (*s == '\n') printf("\\n");
        else if (*s == '\r') printf("\\r");
        else if (*s == '\t') printf("\\t");
        else                 putchar(*s);
    }
    putchar('"');
}

static const char* resolve_notes_path(int flags) {
    if (!(flags & FLAG_GLOBAL)) return ".cpin/notes";

    // generates path to global .cpin dir
    static char path[4096];
    const char* home = getenv("HOME");
    if (!home) {
        fprintf(stderr, "error: $HOME is not set\n");
        exit(1);
    }
    snprintf(path, sizeof(path), "%s/.cpin/notes", home);
    return path;
}

int commands_parse_args(int argc, char** argv, int flags) {
    const char* notes_path = resolve_notes_path(flags);
    char* cmd = argv[1];

    // ── add ───────────────────────────────────────────────────────────────────
    if (!strcmp(cmd, "add")) {
        if (argc < 4) {
            printf("Usage: cpin add <file:line> \"<note>\" [--global]\n");
            return 1;
        }

        char* file = NULL;
        char* line = NULL;
        cpin_error_t err = parser_split_target(argv[2], &file, &line);
        if (err != CPIN_SUCCESS) {
            cpin_report(CPIN_SEVERITY_ERROR, err);
            return 1;
        }

        char* content = parser_get_note(argv[3]);
        if (!content || content[0] == '\0') {
            fprintf(stderr, "error: note content cannot be empty\n");
            return 1;
        }

        err = fileio_file_exist(file);
        if (err != CPIN_SUCCESS) {
            cpin_report(CPIN_SEVERITY_ERROR, err);
            return 1;
        }

        cpin_error_t line_check = fileio_check_line(file, line);
        if (line_check == CPIN_WARN_EMPTY_LINE) {
            cpin_report(CPIN_SEVERITY_WARNING, CPIN_WARN_EMPTY_LINE);
        } else if (line_check == CPIN_ERR_LINE_OUT_OF_BOUNDS) {
            cpin_report(CPIN_SEVERITY_ERROR, CPIN_ERR_LINE_OUT_OF_BOUNDS);
            return 1;
        }

        cpin_note_t note = fileio_create_note(file, line, content);
        err = fileio_save(&note, notes_path);
        if (err == CPIN_WARN_DUPLICATE_LINE) {
            cpin_report(CPIN_SEVERITY_WARNING, CPIN_WARN_DUPLICATE_LINE);
            // fall through — note was still saved
        } else if (err != CPIN_SUCCESS) {
            cpin_report(CPIN_SEVERITY_ERROR, err);
            return 1;
        }
          printf("Note added: %s:%s\n", file, line);

          return 0;

    // ── list ──────────────────────────────────────────────────────────────────
    } else if (!strcmp(cmd, "list")) {
        char* result = NULL;
        cpin_error_t err;

        if (argc < 3) {
            err = fileio_load_all(notes_path, &result);
            if (err == CPIN_ERR_NOTE_NOT_FOUND || !result) {
                printf("No notes found\n");
                return 0;
            }
        } else {
            char* file = argv[2];
            char* line = (argc >= 4) ? argv[3] : NULL;
            err = fileio_load(file, line, notes_path, &result);
            if (err == CPIN_ERR_NOTE_NOT_FOUND || !result) {
                printf("No notes found for %s%s%s\n",
                       file, line ? ":" : "", line ? line : "");
                return 0;
            }
        }

        if (err != CPIN_SUCCESS) {
            cpin_report(CPIN_SEVERITY_ERROR, err);
            return 1;
        }

        printf("%s", result);
        free(result);
        return 0;

    // ── remove ────────────────────────────────────────────────────────────────
    } else if (!strcmp(cmd, "remove")) {
        if (argc < 3) {
            printf("Usage: cpin remove <file:line> [--global]\n");
            return 1;
        }

        char* file = NULL;
        char* line = NULL;
        cpin_error_t err = parser_split_target(argv[2], &file, &line);
        if (err != CPIN_SUCCESS) {
            cpin_report(CPIN_SEVERITY_ERROR, err);
            return 1;
        }

        err = fileio_delete(file, line, notes_path);
        if (err != CPIN_SUCCESS) {
            cpin_report(CPIN_SEVERITY_ERROR, err);
            return 1;
        }

        printf("Note removed: %s:%s\n", file, line);
        return 0;

    // ── search ────────────────────────────────────────────────────────────────
    } else if (!strcmp(cmd, "search")) {
        if (argc < 3) {
            printf("Usage: cpin search <keyword> [--global]\n");
            return 1;
        }

        char* result = NULL;
        cpin_error_t err = fileio_search(argv[2], notes_path, &result);
        if (err == CPIN_ERR_NOTE_NOT_FOUND || !result) {
            printf("No notes matching \"%s\"\n", argv[2]);
            return 0;
        }
        if (err != CPIN_SUCCESS) {
            cpin_report(CPIN_SEVERITY_ERROR, err);
            return 1;
        }

        printf("%s", result);
        free(result);
        return 0;

    // ── export ────────────────────────────────────────────────────────────────
    } else if (!strcmp(cmd, "export")) {
        char* result = NULL;
        cpin_error_t err = fileio_load_all(notes_path, &result);
        if (err == CPIN_ERR_NOTE_NOT_FOUND || !result) {
            if (flags & FLAG_JSON) printf("[]\n");
            else      printf("No notes found\n");
            return 0;
        }
        if (err != CPIN_SUCCESS) {
            cpin_report(CPIN_SEVERITY_ERROR, err);
            return 1;
        }

        if (flags & FLAG_JSON && flags & FLAG_MD) {
            fprintf(stderr, "error: --json and --md are mutually exclusive\n");
            free(result);
            return 1;
        }
        else if (flags & FLAG_JSON) {
            printf("[\n");
            int first = 1;
            char* line = result;
            while (*line) {
                char* newline = strchr(line, '\n');
                if (newline) *newline = '\0';

                char tmp[4096];
                strncpy(tmp, line, sizeof(tmp) - 1);
                tmp[sizeof(tmp) - 1] = '\0';

                // Split on first two ':'
                char* first_colon = strchr(tmp, ':');
                if (first_colon) {
                    *first_colon = '\0';
                    char* tok_file = tmp;
                    char* second_colon = strchr(first_colon + 1, ':');
                    if (second_colon) {
                        *second_colon = '\0';
                        char* tok_line    = first_colon + 1;
                        char* tok_content = second_colon + 1;

                        if (!first) printf(",\n");
                        printf("  {\"file\": ");
                        print_json_string(tok_file);
                        printf(", \"line\": %s, \"note\": ", tok_line);
                        print_json_string(tok_content);
                        printf("}");
                        first = 0;
                    }
                }

                if (!newline) break;
                line = newline + 1;
            }
            printf("\n]\n");
        } else if (flags & FLAG_MD) {
            char* line = result;
            char prev_file[4096] = {0};
            while (*line) {
                char* newline = strchr(line, '\n');
                if (newline) *newline = '\0';

                char tmp[4096];
                strncpy(tmp, line, sizeof(tmp) - 1);
                tmp[sizeof(tmp) - 1] = '\0';

                char* first_colon = strchr(tmp, ':');
                if (first_colon) {
                    *first_colon = '\0';
                    char* tok_file = tmp;
                    char* second_colon = strchr(first_colon + 1, ':');
                    if (second_colon) {
                        *second_colon = '\0';
                        char* tok_line    = first_colon + 1;
                        char* tok_content = second_colon + 1;

                        if (strcmp(tok_file, prev_file) != 0) {
                            printf("## %s\n\n", tok_file);
                            strncpy(prev_file, tok_file, sizeof(prev_file) - 1);
                        }
                        printf("Line %s: %s\n", tok_line, tok_content);
                    }
                }

                if (!newline) break;
                line = newline + 1;
            }
        } else {
            printf("%s", result);
        }

        free(result);
        return 0;
    } else {
        printf("Unknown command: %s\n", cmd);
        commands_usage();
        return 0;
    }
}
