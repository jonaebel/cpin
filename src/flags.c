#include "flags.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "cpin.h"


static inline void flags_remove_arg(int* i, int* argc, char** argv) {
    for (int j = *i; j < *argc - 1; j++)
        argv[j] = argv[j + 1];
    (*argc)--;
    *i = 1;
}

int flags_strip(int* argc, char** argv) {
    if (argc == NULL || argv == NULL) {
        fprintf(stderr, "flags_strip: invalid parameters\n");
        return -1;
    }

    int flags = 0;

    for (int i = 1; i < *argc; i++) {
        // Strip --global flag from argv
        if (strcmp(argv[i], "--global") == 0) {
            flags |= FLAG_GLOBAL;
            flags_remove_arg(&i, argc, argv);

        // Strip --json flag from argv
        } else if (strcmp(argv[i], "--json") == 0) {
            flags |= FLAG_JSON;
            flags_remove_arg(&i, argc, argv);
        
        // Strip --md flag from argv
        } else if (strcmp(argv[i], "--md") == 0) {
            flags |= FLAG_MD;
            flags_remove_arg(&i, argc, argv);

        // Strip --version flag and ignore other args - exit with status 0 after printing version
        } else if (strcmp(argv[i], "--version") == 0) {
            printf("cpin v%i.%i \n", CPIN_VERSION_MAJOR, CPIN_VERSION_MINOR);
            exit(0);
        }
    }

    return flags;
}
