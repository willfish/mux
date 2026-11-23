#include "template.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "str.h"

#define TAG_OPEN "<%="
#define TAG_CLOSE "%>"

static const char *find_setting_value(const char *key, size_t key_len, const char **settings,
                                      int setting_count) {
    if (!settings) return NULL;
    for (int i = 0; i + 1 < setting_count; i += 2) {
        if (settings[i] && strlen(settings[i]) == key_len &&
            memcmp(settings[i], key, key_len) == 0) {
            return settings[i + 1];
        }
    }
    return NULL;
}

/* Match prefix + ["key"] or ['key'] and return key pointer & length. */
static int match_bracket_string(const char *expr, size_t expr_len, const char *prefix,
                                const char **out_key, size_t *out_len) {
    size_t plen = strlen(prefix);
    if (expr_len < plen + 4) return 0; /* prefix + [ + q + q + ] */
    if (memcmp(expr, prefix, plen) != 0) return 0;
    if (expr[plen] != '[' || expr[expr_len - 1] != ']') return 0;

    char quote = expr[plen + 1];
    if ((quote != '"' && quote != '\'') || expr[expr_len - 2] != quote) return 0;

    *out_key = expr + plen + 2;
    *out_len = expr_len - plen - 4;
    return 1;
}

/* Match @args[N] where N is a non-negative integer. */
static int match_args_index(const char *expr, size_t expr_len, int *out_index) {
    const char *prefix = "@args[";
    size_t plen = strlen(prefix);
    if (expr_len < plen + 2) return 0;
    if (memcmp(expr, prefix, plen) != 0 || expr[expr_len - 1] != ']') return 0;

    size_t num_len = expr_len - plen - 1;
    const char *num = expr + plen;
    long val = 0;
    for (size_t i = 0; i < num_len; i++) {
        if (num[i] < '0' || num[i] > '9') return 0;
        if (val > 1000000) return 0;
        val = val * 10 + (num[i] - '0');
    }
    *out_index = (int)val;
    return 1;
}

char *template_substitute_ex(Arena *a, const char *input, size_t input_len, const char **settings,
                             int setting_count, const char **args, int arg_count) {
    if (!input) return NULL;

    /* Copy input_len bytes so embedded NUL or non-terminated slices are safe */
    char *src = arena_strndup(a, input, input_len);

    /* Fast path: no template open tag present */
    if (!strstr(src, TAG_OPEN)) {
        return src;
    }

    Str result = str_with_capacity(input_len);
    const char *p = src;

    while (*p) {
        const char *tag = strstr(p, TAG_OPEN);
        if (!tag) {
            str_append(&result, p);
            break;
        }

        /* Append text before the tag */
        str_appendn(&result, p, (size_t)(tag - p));

        /* Find closing tag */
        const char *close = strstr(tag + strlen(TAG_OPEN), TAG_CLOSE);
        if (!close) {
            /* No closing tag: append rest as-is */
            str_append(&result, tag);
            break;
        }

        /* Extract content between <%= and %> and trim whitespace */
        const char *content_start = tag + strlen(TAG_OPEN);
        size_t content_len = (size_t)(close - content_start);

        while (content_len > 0 && isspace((unsigned char)*content_start)) {
            content_start++;
            content_len--;
        }
        while (content_len > 0 && isspace((unsigned char)content_start[content_len - 1])) {
            content_len--;
        }

        const char *key = NULL;
        size_t key_len = 0;
        int arg_idx = -1;

        if (match_bracket_string(content_start, content_len, "@settings", &key, &key_len)) {
            const char *value = find_setting_value(key, key_len, settings, setting_count);
            if (value) {
                str_append(&result, value);
            }
        } else if (match_bracket_string(content_start, content_len, "ENV", &key, &key_len)) {
            char *env_name = arena_strndup(a, key, key_len);
            const char *value = getenv(env_name);
            if (value) {
                str_append(&result, value);
            }
        } else if (match_args_index(content_start, content_len, &arg_idx)) {
            if (args && arg_idx >= 0 && arg_idx < arg_count && args[arg_idx]) {
                str_append(&result, args[arg_idx]);
            }
        } else {
            /* Unknown tag: leave as-is */
            str_appendn(&result, tag, (size_t)(close + strlen(TAG_CLOSE) - tag));
        }

        p = close + strlen(TAG_CLOSE);
    }

    char *out = arena_strdup(a, str_cstr(&result));
    str_free(&result);
    return out;
}

char *template_substitute(Arena *a, const char *input, const char **settings, int setting_count) {
    if (!input) return NULL;
    return template_substitute_ex(a, input, strlen(input), settings, setting_count, NULL, 0);
}

int template_parse_setting(Arena *a, const char *arg, char **key, char **value) {
    if (!arg) return -1;
    const char *eq = strchr(arg, '=');
    if (!eq || eq == arg) return -1;

    *key = arena_strndup(a, arg, (size_t)(eq - arg));
    *value = arena_strdup(a, eq + 1);
    return 0;
}
