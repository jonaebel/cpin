#ifndef FLAGS_H
#define FLAGS_H

typedef enum {
    FLAG_GLOBAL = 1 << 0,
    FLAG_JSON = 1 << 1,
    FLAG_MD = 1 << 2
} cmd_flags;


/**
 * @brief Strips specific command-line flags and returns them
 * @param argc Argument count
 * @param argv Argument vector
 * @return Flags as a bitmask
 * 
 * @warning Manipulates both argc and argv if flags are found
 */
int flags_strip(int* argc, char** argv);

#endif
