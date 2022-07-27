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

void                    GaStringBuilder_Destroy(struct stringbuf *);
struct stringbuf    *   GaStringBuilder_Dup(struct stringbuf *);
struct stringbuf    *   GaStringBuilder_New();
struct stringbuf    *   GaStringBuilder_FromCString(char *, size_t);
void                    GaStringBuilder_Append(struct stringbuf *, const char *);
void                    GaStringBuilder_AppendEx(struct stringbuf *, const char *, size_t);
void                    GaStringBuilder_Concat(struct stringbuf *, struct stringbuf *);

#endif
