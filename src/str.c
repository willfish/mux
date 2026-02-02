#include "str.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STR_INITIAL_CAP 64

static void str_grow(Str *s, size_t needed) {
    if (s->len + needed + 1 <= s->cap) {
        return;
    }
    size_t new_cap = s->cap;
    while (new_cap < s->len + needed + 1) {
        new_cap = new_cap < STR_INITIAL_CAP ? STR_INITIAL_CAP : new_cap * 2;
    }
    s->data = realloc(s->data, new_cap);
    if (!s->data) {
        fprintf(stderr, "mux: out of memory\n");
        exit(1);
    }
    s->cap = new_cap;
}

Str str_new(void) {
    Str s = {0};
    str_grow(&s, 0);
    s.data[0] = '\0';
    return s;
}

Str str_with_capacity(size_t cap) {
    Str s = {0};
    s.data = malloc(cap + 1);
    if (!s.data) {
        fprintf(stderr, "mux: out of memory\n");
        exit(1);
    }
    s.cap = cap + 1;
    s.len = 0;
    s.data[0] = '\0';
    return s;
}

void str_free(Str *s) {
    free(s->data);
    s->data = NULL;
    s->len = 0;
    s->cap = 0;
}

void str_append(Str *s, const char *text) {
    if (!text) return;
    size_t tlen = strlen(text);
    str_grow(s, tlen);
    memcpy(s->data + s->len, text, tlen);
    s->len += tlen;
    s->data[s->len] = '\0';
}

void str_appendn(Str *s, const char *text, size_t n) {
    if (!text || n == 0) return;
    str_grow(s, n);
    memcpy(s->data + s->len, text, n);
    s->len += n;
    s->data[s->len] = '\0';
}

void str_appendf(Str *s, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    va_list ap2;
    va_copy(ap2, ap);
    int needed = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (needed < 0) {
        va_end(ap2);
        return;
    }
    str_grow(s, (size_t)needed);
    vsnprintf(s->data + s->len, (size_t)needed + 1, fmt, ap2);
    va_end(ap2);
    s->len += (size_t)needed;
}

void str_append_char(Str *s, char c) {
    str_grow(s, 1);
    s->data[s->len] = c;
    s->len++;
    s->data[s->len] = '\0';
}

void str_clear(Str *s) {
    s->len = 0;
    if (s->data) {
        s->data[0] = '\0';
    }
}

const char *str_cstr(const Str *s) {
    return s->data ? s->data : "";
}
