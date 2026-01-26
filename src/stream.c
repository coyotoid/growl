#include "stream.h"
#include <stdio.h>

static int filestream_getc(void *f) { return fgetc((FILE *)f); }
static int filestream_ungetc(int c, void *f) { return ungetc(c, (FILE *)f); }
static int filestream_eof(void *f) { return feof((FILE *)f); }

static int bufstream_getc(void *f) {
  Buf *b = f;
  if (b->unread != -1) {
    int c = b->unread;
    b->unread = -1;
    return c;
  } else if (b->pos >= b->len) {
    return -1;
  }
  return b->data[b->pos++];
}

static int bufstream_ungetc(int c, void *f) { return ((Buf *)f)->unread = c; }

static int bufstream_eof(void *f) {
  Buf *b = f;
  if (b->unread != -1)
    return 0;
  return b->pos >= b->len;
}

// clang-format off
static const StreamVtable _filestream_vtable = {
  filestream_getc, filestream_ungetc, filestream_eof
};
const StreamVtable *filestream_vtable = &_filestream_vtable;

static const StreamVtable _bufstream_vtable = {
  bufstream_getc, bufstream_ungetc, bufstream_eof
};
const StreamVtable *bufstream_vtable = &_bufstream_vtable;
// clang-format on
