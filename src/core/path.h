#ifndef GROWL_PATH_H
#define GROWL_PATH_H

#include <growl.h>

char *growl_dirname(const char *path, GrowlArena *arena);
char *growl_realpath(const char *path, GrowlArena *arena);
char *growl_resolve_module_path(GrowlCompileContext *ctx, const char *path,
                                GrowlArena *arena);

#endif // GROWL_PATH_H
