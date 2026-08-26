#include "projects.h"
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>


const char* projects_registry_path(void) {
    static char path[4096];
    const char* home = getenv("HOME");
    if (!home) return NULL;
    snprintf(path, sizeof(path), "%s/.cpin/projects", home);
    return path;
}

static int ensure_global_cpin_dir(void) {
    const char* home = getenv("HOME");
    if (!home) return -1;
    char dir[4096];
    snprintf(dir, sizeof(dir), "%s/.cpin", home);
    if (mkdir(dir, 0755) != 0 && errno != EEXIST) return -1;
    return 0;
}

char* projects_find_root(void) {
    const char* reg_path = projects_registry_path();
    if (!reg_path) return NULL;

    FILE* f = fopen(reg_path, "r");
    if (!f) return NULL;

    char cwd[PATH_MAX];
    if (!getcwd(cwd, sizeof(cwd))) {
        fclose(f);
        return NULL;
    }
    size_t cwd_len = strlen(cwd);

    char line[4096];
    char* best = NULL;
    size_t best_len = 0;

    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = '\0';
        if (len == 0) continue;

        /* cwd must equal the root or be a direct/indirect subdirectory */
        if (cwd_len >= len &&
            memcmp(cwd, line, len) == 0 &&
            (cwd[len] == '/' || cwd[len] == '\0')) {
            /* prefer the longest (most-specific) match */
            if (len > best_len) {
                free(best);
                best = strdup(line);
                best_len = len;
            }
        }
    }

    fclose(f);
    return best;
}

int projects_add_root(const char* path) {
    char abs_path[PATH_MAX];
    if (!realpath(path, abs_path)) {
        fprintf(stderr, "error: cannot resolve path '%s': %s\n", path, strerror(errno));
        return 1;
    }

    if (ensure_global_cpin_dir() != 0) {
        fprintf(stderr, "error: cannot create ~/.cpin directory\n");
        return 1;
    }

    const char* reg_path = projects_registry_path();

    /* check for existing registration */
    FILE* f = fopen(reg_path, "r");
    if (f) {
        char line[4096];
        while (fgets(line, sizeof(line), f)) {
            size_t len = strlen(line);
            while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
                line[--len] = '\0';
            if (strcmp(line, abs_path) == 0) {
                fclose(f);
                fprintf(stderr, "warning: '%s' is already a registered project root\n", abs_path);
                return 0;
            }
        }
        fclose(f);
    }

    f = fopen(reg_path, "a");
    if (!f) {
        fprintf(stderr, "error: cannot open project registry: %s\n", strerror(errno));
        return 1;
    }
    fprintf(f, "%s\n", abs_path);
    fclose(f);

    printf("project root set: %s\n", abs_path);
    return 0;
}

int projects_remove_root(const char* path) {
    /* try to resolve; if path no longer exists on disk, use it as-is */
    char abs_path[PATH_MAX];
    if (!realpath(path, abs_path)) {
        strncpy(abs_path, path, sizeof(abs_path) - 1);
        abs_path[sizeof(abs_path) - 1] = '\0';
    }

    const char* reg_path = projects_registry_path();
    FILE* f = fopen(reg_path, "r");
    if (!f) {
        fprintf(stderr, "error: no project roots registered\n");
        return 1;
    }

    char** lines = NULL;
    int count = 0, found = 0;
    char line[4096];

    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = '\0';
        if (len == 0) continue;
        if (strcmp(line, abs_path) == 0) {
            found = 1;
            continue;
        }
        char** tmp = realloc(lines, (size_t)(count + 1) * sizeof(char*));
        if (!tmp) {
            fclose(f);
            for (int i = 0; i < count; i++) free(lines[i]);
            free(lines);
            fprintf(stderr, "error: out of memory\n");
            return 1;
        }
        lines = tmp;
        lines[count++] = strdup(line);
    }
    fclose(f);

    if (!found) {
        for (int i = 0; i < count; i++) free(lines[i]);
        free(lines);
        fprintf(stderr, "error: '%s' is not a registered project root\n", abs_path);
        return 1;
    }

    f = fopen(reg_path, "w");
    if (!f) {
        for (int i = 0; i < count; i++) free(lines[i]);
        free(lines);
        fprintf(stderr, "error: cannot write project registry\n");
        return 1;
    }
    for (int i = 0; i < count; i++) {
        fprintf(f, "%s\n", lines[i]);
        free(lines[i]);
    }
    free(lines);
    fclose(f);

    printf("project root removed: %s\n", abs_path);
    return 0;
}

void projects_list_roots(void) {
    const char* reg_path = projects_registry_path();
    FILE* f = fopen(reg_path, "r");
    if (!f) {
        printf("no project roots registered\n");
        return;
    }

    char line[4096];
    int count = 0;
    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = '\0';
        if (len == 0) continue;
        printf("%s\n", line);
        count++;
    }
    fclose(f);

    if (count == 0) printf("no project roots registered\n");
}
