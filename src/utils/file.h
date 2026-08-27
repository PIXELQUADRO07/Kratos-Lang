#ifndef KRATOS_FILE_H
#define KRATOS_FILE_H

#include <stddef.h>

char *kratos_copy_string(const char *source);

/* Restituisce il contenuto del file, o NULL se non e' leggibile. */
char *kratos_read_file(const char *path);

char *kratos_dirname_dup(const char *path);
char *kratos_join_path(const char *dir, const char *rel);

/* NULL se il percorso non esiste. */
char *kratos_realpath_dup(const char *path);

#endif
