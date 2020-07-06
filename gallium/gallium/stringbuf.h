#ifndef _DS_STRING_H
#define _DS_STRING_H

struct stringbuf {
    char    *   buf;
    size_t      buf_size;   /* size of the actual buffer */
    size_t      size;       /* size of the string */
};

#define STRINGBUF_INITIAL_SIZE  256
#define STRINGBUF_LEN(sb)       ((sb)->size)
#define STRINGBUF_VALUE(sb)     (char*)((sb)->buf)

void                    stringbuf_destroy(struct stringbuf *);
struct stringbuf    *   stringbuf_dup(struct stringbuf *);
struct stringbuf    *   stringbuf_new();
void                    stringbuf_append(struct stringbuf *, const char *);
void                    stringbuf_append_sb(struct stringbuf *, struct stringbuf *);
#endif
