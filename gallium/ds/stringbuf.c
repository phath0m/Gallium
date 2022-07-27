/*
 * stringbuf.c - Variable length, mutable string implementation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include <stdlib.h>
#include <string.h>
#include <gallium/stringbuf.h>

void
GaStringBuilder_Destroy(struct stringbuf *sb)
{
    free(sb->buf);
    free(sb); 
}

struct stringbuf *
GaStringBuilder_Dup(struct stringbuf *sb)
{
    struct stringbuf *new_sb = calloc(sizeof(struct stringbuf), 1);
    new_sb->buf = calloc(sb->buf_size, 1);
    new_sb->buf_size = sb->buf_size;
    new_sb->size = sb->size;
    memcpy(new_sb->buf, sb->buf, sb->size);
    return new_sb;
}

struct stringbuf *
GaStringBuilder_New()
{
    struct stringbuf *sb = calloc(sizeof(struct stringbuf), 1);
    sb->buf = calloc(STRINGBUF_INITIAL_SIZE, 1); 
    sb->buf_size = STRINGBUF_INITIAL_SIZE;

    return sb;
}

struct stringbuf *
GaStringBuilder_FromCString(char *buf, size_t buf_size)
{
    struct stringbuf *sb = calloc(sizeof(struct stringbuf), 1);
    sb->buf = buf;
    sb->buf_size = buf_size;
    return sb;
}

void
GaStringBuilder_Append(struct stringbuf *sb, const char *val)
{
    size_t val_len = strlen(val);
    
    if (val_len + sb->size >= sb->buf_size) {
        sb->buf_size = (sb->buf_size + val_len) * 2;
        sb->buf = realloc(sb->buf, sb->buf_size);
    }

    strcpy(&sb->buf[sb->size], val);
    sb->size += val_len;
}

void
GaStringBuilder_AppendEx(struct stringbuf *sb, const char *val, size_t val_len)
{
    if (val_len + sb->size >= sb->buf_size) {
        sb->buf_size = (sb->buf_size + val_len) * 2;
        sb->buf = realloc(sb->buf, sb->buf_size);
    }
    strncpy(&sb->buf[sb->size], val, val_len);
    sb->size += val_len;
}

void
GaStringBuilder_Concat(struct stringbuf *sb, struct stringbuf *val)
{
    size_t val_len = STRINGBUF_LEN(val);

    if (val_len + sb->size >= sb->buf_size) {
        sb->buf_size = (sb->buf_size + val_len) * 2;
        sb->buf = realloc(sb->buf, sb->buf_size);
    }

    strncpy(&sb->buf[sb->size], STRINGBUF_VALUE(val), val_len);
    sb->size += val_len;
}