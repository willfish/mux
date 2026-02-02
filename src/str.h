#ifndef MUX_STR_H
#define MUX_STR_H

#include <stddef.h>

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} Str;

Str str_new(void);
Str str_with_capacity(size_t cap);
void str_free(Str *s);
void str_append(Str *s, const char *text);
void str_appendn(Str *s, const char *text, size_t n);
void str_appendf(Str *s, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
void str_append_char(Str *s, char c);
void str_clear(Str *s);
const char *str_cstr(const Str *s);

#endif
