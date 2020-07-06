#include <stdlib.h>
#include <string.h>
#include <gallium/stringbuf.h>

void
stringbuf_destroy(struct stringbuf *sb)
{
    free(sb->buf);
    free(sb); 
}

struct stringbuf *
stringbuf_dup(struct stringbuf *sb)
{
    struct stringbuf *new_sb = calloc(sizeof(struct stringbuf), 1);
    new_sb->buf = calloc(sb->buf_size, 1);
    new_sb->buf_size = sb->buf_size;
    new_sb->size = sb->size;
    memcpy(new_sb->buf, sb->buf, sb->size);
    return new_sb;
}

struct stringbuf *
stringbuf_new()
{
    struct stringbuf *sb = calloc(sizeof(struct stringbuf), 1);
    sb->buf = calloc(STRINGBUF_INITIAL_SIZE, 1); 
    sb->buf_size = STRINGBUF_INITIAL_SIZE;

    return sb;
}

void
stringbuf_append(struct stringbuf *sb, const char *val)
{
    size_t val_len = strlen(val);
    
    if (val_len + sb->size >= sb->buf_size) {
        sb->buf_size = (sb->buf_size + val_len) * 2;
        sb->buf = realloc(sb->buf, sb->buf_size);
    }

    strncpy(&sb->buf[sb->size], val, val_len);
    sb->size += val_len;
}

void
stringbuf_append_sb(struct stringbuf *sb, struct stringbuf *val)
{
    size_t val_len = STRINGBUF_LEN(val);

    if (val_len + sb->size >= sb->buf_size) {
        sb->buf_size = (sb->buf_size + val_len) * 2;
        sb->buf = realloc(sb->buf, sb->buf_size);
    }

    strncpy(&sb->buf[sb->size], STRINGBUF_VALUE(val), val_len);
    sb->size += val_len;
}

