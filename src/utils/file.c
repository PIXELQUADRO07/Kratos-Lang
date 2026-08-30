#include "utils/file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *kratos_copy_string(const char *source)
{
    if (source == NULL) {
        return NULL;
    }

    size_t length = strlen(source);
    char *copy = malloc(length + 1);
    if (copy == NULL) {
        fprintf(stderr, "kratos: out of memory\n");
        exit(EXIT_FAILURE);
    }
    memcpy(copy, source, length + 1);
    return copy;
}


char *kratos_read_file(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }

    long size = ftell(file);
    if (size < 0) {
        fclose(file);
        return NULL;
    }

    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }

    char *buffer = malloc((size_t)size + 1);
    if (buffer == NULL) {
        fclose(file);
        fprintf(stderr, "kratos: out of memory\n");
        exit(EXIT_FAILURE);
    }

    size_t read_count = fread(buffer, 1, (size_t)size, file);
    fclose(file);
    buffer[read_count] = '\0';
    return buffer;
}


char *kratos_dirname_dup(const char *path)
{
    if (path == NULL || path[0] == '\0') {
        return kratos_copy_string(".");
    }

    const char *slash = strrchr(path, '/');
    if (slash == NULL) {
        return kratos_copy_string(".");
    }

    if (slash == path) {
        return kratos_copy_string("/");
    }

    size_t length = (size_t)(slash - path);
    char *dir = malloc(length + 1);
    if (dir == NULL) {
        fprintf(stderr, "kratos: out of memory\n");
        exit(EXIT_FAILURE);
    }
    memcpy(dir, path, length);
    dir[length] = '\0';
    return dir;
}


char *kratos_join_path(const char *dir, const char *rel)
{
    if (rel != NULL && rel[0] == '/') {
        return kratos_copy_string(rel);
    }

    if (dir == NULL || dir[0] == '\0' || (dir[0] == '.' && dir[1] == '\0')) {
        return kratos_copy_string(rel);
    }

    size_t dir_len = strlen(dir);
    size_t rel_len = strlen(rel);
    int need_slash = dir[dir_len - 1] != '/';
    char *out = malloc(dir_len + rel_len + (need_slash ? 2 : 1));
    if (out == NULL) {
        fprintf(stderr, "kratos: out of memory\n");
        exit(EXIT_FAILURE);
    }

    memcpy(out, dir, dir_len);
    size_t n = dir_len;
    if (need_slash) {
        out[n++] = '/';
    }
    memcpy(out + n, rel, rel_len + 1);
    return out;
}


char *kratos_realpath_dup(const char *path)
{
    if (path == NULL) {
        return NULL;
    }

    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return NULL;
    }
    fclose(file);
    return kratos_copy_string(path);
}
