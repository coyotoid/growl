#ifndef STREAM_H
#define STREAM_H

typedef struct StreamVtable {
	int (*__sgetc)(void *);
	int (*__sungetc)(int, void *);
	int (*__seof)(void *);
} StreamVtable;

typedef struct Stream {
	const StreamVtable *vtable;
	void *data;
} Stream;

typedef struct Buf {
	const char *data;
	int len, pos;
	int unread;
} Buf;

#define ST_GETC(R) ((R)->vtable->__sgetc((R)->data))
#define ST_UNGETC(C, R) ((R)->vtable->__sungetc(C, (R)->data))
#define ST_EOF(R) ((R)->vtable->__seof((R)->data))

#define BUF(s) ((Buf){s, sizeof(s)-1, 0, -1})

extern const StreamVtable *filestream_vtable;
extern const StreamVtable *bufstream_vtable;

#endif
