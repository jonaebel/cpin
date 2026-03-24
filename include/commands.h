#ifndef COMMANDS_H
#define COMMANDS_H

/**
 * @brief Prints command usage
 */
void commands_usage(void);

/**
 * @brief Parses command-line arguments and runs commands
 * @param argc Argument count
 * @param argv Argument vector
 * @param flags Command-line flags as bitmask
 * @return 0 if success, otherwise 1
*/
int commands_parse_args(int argc, char** argv, int flags);

#endif
