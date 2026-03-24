#include <stdlib.h>
#include <stdio.h>
#include "commands.h"
#include "flags.h"


int main(int argc, char** argv) {
    int flags = flags_strip(&argc, argv);
    if (flags < 0) exit(1);

    // first check for right argc
    if (argc < 2) {
        commands_usage();
        return 1;
    }

    commands_parse_args(argc, argv, flags);

    return 0;
}
