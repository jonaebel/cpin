#include "cpin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "errors.h"
#include "parser.h"
#include "fileio.h"

static void usage(void) {
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

static const char* resolve_notes_path(int global) {
    if (!global) return ".cpin/notes";

    // generates path to global .cpin dir
    static char path[4096];
    const char* home = getenv("HOME");
    if (!home) {
        fprintf(stderr, "Error: $HOME is not set\n");
        exit(1);
    }
    snprintf(path, sizeof(path), "%s/.cpin/notes", home);
    return path;
}

int main(int argc, char** argv) {
    // Strip --global flag from argv before command dispatch
    int global = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--global") == 0) {
            global = 1;
            for (int j = i; j < argc - 1; j++)
                argv[j] = argv[j + 1];
            argc--;
            break;
        }
    }

    // Strip --json flag from argv before command dispatch
    int json = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--json") == 0) {
            json = 1;
            for (int j = i; j < argc - 1; j++)
                argv[j] = argv[j + 1];
            argc--;
            break;
        }
    }

    // Strip --md flag from argv before command dispatch
    int md = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--md") == 0) {
            md = 1;
            for (int j = i; j < argc - 1; j++)
                argv[j] = argv[j + 1];
            argc--;
            break;
        }
    }

    // Strip --version flag and ignore other args - return 0 after printing version
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--version") == 0) {
            printf("cpin v%i.%i \n", CPIN_VERSION_MAJOR, CPIN_VERSION_MINOR);
            return 0;
        }
    }

    // first check for right argc
    if (argc < 2) {
        usage();
        return 1;
    }

    const char* notes_path = resolve_notes_path(global);
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
            printf("Error: %s\n", error_to_string(err));
            return 1;
        }

        char* content = parser_get_note(argv[3]);
        if (!content || content[0] == '\0') {
            printf("Error: note content cannot be empty\n");
            return 1;
        }

        err = fileio_file_exist(file);
        if (err != CPIN_SUCCESS) {
            printf("Error: %s\n", error_to_string(err));
            return 1;
        }

        cpin_note_t note = fileio_create_note(file, line, content);
        err = fileio_save(&note, notes_path);
          if (err == CPIN_WARN_DUPLICATE_LINE) {
              printf("Warning: a note at this line already exists\n");
              // fall through — note was still saved
          } else if (err != CPIN_SUCCESS) {
              printf("Error: %s\n", error_to_string(err));
              return 1;
          }
          printf("Note added: %s:%s\n", file, line);

        if (fileio_line_is_empty(file, line)) printf("Warning: note inserted in empty line\n");
        if (fileio_line_is_out_of_bound(file, line))printf("Warning: line %s is out of range\n", line);

          return 0;

    }

    // ── list ──────────────────────────────────────────────────────────────────
    if (!strcmp(cmd, "list")) {
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
            printf("Error: %s\n", error_to_string(err));
            return 1;
        }

        printf("%s", result);
        free(result);
        return 0;
    }

    // ── remove ────────────────────────────────────────────────────────────────
    if (!strcmp(cmd, "remove")) {
        if (argc < 3) {
            printf("Usage: cpin remove <file:line> [--global]\n");
            return 1;
        }

        char* file = NULL;
        char* line = NULL;
        cpin_error_t err = parser_split_target(argv[2], &file, &line);
        if (err != CPIN_SUCCESS) {
            printf("Error: %s\n", error_to_string(err));
            return 1;
        }

        err = fileio_delete(file, line, notes_path);
        if (err != CPIN_SUCCESS) {
            printf("Error: %s\n", error_to_string(err));
            return 1;
        }

        printf("Note removed: %s:%s\n", file, line);
        return 0;
    }

    // ── search ────────────────────────────────────────────────────────────────
    if (!strcmp(cmd, "search")) {
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
            printf("Error: %s\n", error_to_string(err));
            return 1;
        }

        printf("%s", result);
        free(result);
        return 0;
    }

    // ── export ────────────────────────────────────────────────────────────────
    if (!strcmp(cmd, "export")) {
        char* result = NULL;
        cpin_error_t err = fileio_load_all(notes_path, &result);
        if (err == CPIN_ERR_NOTE_NOT_FOUND || !result) {
            if (json) printf("[]\n");
            else      printf("No notes found\n");
            return 0;
        }
        if (err != CPIN_SUCCESS) {
            printf("Error: %s\n", error_to_string(err));
            return 1;
        }

        if (json && md) {
            printf("Error: --json and --md are mutually exclusive\n");
            free(result);
            return 1;
        }
        else if (json) {
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
        } else if (md) {
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
    }

    printf("Unknown command: %s\n", cmd);
    usage();
    return 1;
}
