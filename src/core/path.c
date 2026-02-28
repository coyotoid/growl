#include "stdlib.h"
#include "unistd.h"
#include <growl.h>
#include <limits.h>
#include <string.h>

char *growl_dirname(const char *path, GrowlArena *arena) {
  if (!path || !*path)
    return growl_arena_strdup(arena, ".");
  size_t len = strlen(path);
  while (len > 1 && path[len - 1] == '/')
    len--;
  if (len == 0)
    return growl_arena_strdup(arena, "/");
  const char *last = strrchr(path, '/');
  if (!last)
    return growl_arena_strdup(arena, ".");
  size_t dir_len = last - path;
  if (dir_len == 0)
    return growl_arena_strdup(arena, "/");
  char *dir = growl_arena_new(arena, char, dir_len + 1);
  memcpy(dir, path, dir_len);
  dir[dir_len] = '\0';
  return dir;
}

char *growl_realpath(const char *path, GrowlArena *arena) {
  if (!path)
    return NULL;
  char *resolved = realpath(path, NULL);
  if (!resolved)
    return NULL;
  char *copy = growl_arena_strdup(arena, resolved);
  free(resolved);
  return copy;
}

char *growl_resolve_module_path(GrowlCompileContext *ctx, const char *path,
                                GrowlArena *arena) {
  char buf[PATH_MAX];
  if (ctx->file_dir && path[0] != '/') {
    snprintf(buf, sizeof(buf), "%s/%s", ctx->file_dir, path);
    char *resolved = growl_realpath(buf, arena);
    if (resolved && access(resolved, R_OK) == 0)
      return resolved;
  }

  // TODO: search paths...?

  if (path[0] == '/') {
    char *resolved = growl_realpath(path, arena);
    if (resolved && access(resolved, R_OK) == 0)
      return resolved;
  }

  return NULL;
}
