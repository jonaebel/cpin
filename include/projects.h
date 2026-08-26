#ifndef CPIN_PROJECTS_H
#define CPIN_PROJECTS_H

/**
 * @brief Returns the path to the project registry file (~/.cpin/projects).
 * @return Static buffer with the path, or NULL if $HOME is unset.
 */
const char* projects_registry_path(void);

/**
 * @brief Finds the most specific registered project root that contains cwd.
 * @return Heap-allocated absolute path of the project root, or NULL if none matches.
 *         Caller must free() the returned string.
 */
char* projects_find_root(void);

/**
 * @brief Registers an absolute path as a project root.
 * @param path  Path to register (resolved to absolute via realpath).
 * @return 0 on success, 1 on error.
 */
int projects_add_root(const char* path);

/**
 * @brief Removes a path from the project registry.
 * @param path  Path to unregister.
 * @return 0 on success, 1 on error.
 */
int projects_remove_root(const char* path);

/**
 * @brief Prints all registered project roots to stdout.
 */
void projects_list_roots(void);

#endif
