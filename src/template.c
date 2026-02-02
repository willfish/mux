#include "template.h"

#include <string.h>

#include "str.h"

#define TAG_OPEN "<%="
#define TAG_CLOSE "%>"
#define SETTINGS_PREFIX "@settings[\""
#define SETTINGS_SUFFIX "\"]"

static const char *find_setting_value(const char *key, const char **settings, int setting_count) {
    for (int i = 0; i < setting_count; i += 2) {
        if (i + 1 < setting_count && strcmp(settings[i], key) == 0) {
            return settings[i + 1];
        }
    }
    return NULL;
}

char *template_substitute(Arena *a, const char *input, const char **settings, int setting_count) {
    if (!input) return NULL;
    if (!settings || setting_count == 0) return arena_strdup(a, input);

    Str result = str_new();
    const char *p = input;

    while (*p) {
        const char *tag = strstr(p, TAG_OPEN);
        if (!tag) {
            str_append(&result, p);
            break;
        }

        /* Append text before the tag */
        str_appendn(&result, p, (size_t)(tag - p));

        /* Find closing tag */
        const char *close = strstr(tag, TAG_CLOSE);
        if (!close) {
            /* No closing tag — append rest as-is */
            str_append(&result, tag);
            break;
        }

        /* Extract content between <%= and %> */
        const char *content_start = tag + strlen(TAG_OPEN);
        size_t content_len = (size_t)(close - content_start);

        /* Skip whitespace */
        while (content_len > 0 && *content_start == ' ') {
            content_start++;
            content_len--;
        }
        while (content_len > 0 && content_start[content_len - 1] == ' ') {
            content_len--;
        }

        /* Check for @settings["key"] pattern */
        size_t prefix_len = strlen(SETTINGS_PREFIX);
        size_t suffix_len = strlen(SETTINGS_SUFFIX);

        if (content_len > prefix_len + suffix_len &&
            strncmp(content_start, SETTINGS_PREFIX, prefix_len) == 0 &&
            strncmp(content_start + content_len - suffix_len, SETTINGS_SUFFIX, suffix_len) == 0) {

            /* Extract key */
            const char *key_start = content_start + prefix_len;
            size_t key_len = content_len - prefix_len - suffix_len;
            char *key = arena_strndup(a, key_start, key_len);

            const char *value = find_setting_value(key, settings, setting_count);
            if (value) {
                str_append(&result, value);
            }
            /* If key not found, substitute with empty string (remove the tag) */
        } else {
            /* Unknown tag — leave as-is */
            str_appendn(&result, tag, (size_t)(close + strlen(TAG_CLOSE) - tag));
        }

        p = close + strlen(TAG_CLOSE);
    }

    char *out = arena_strdup(a, str_cstr(&result));
    str_free(&result);
    return out;
}

int template_parse_setting(Arena *a, const char *arg, char **key, char **value) {
    const char *eq = strchr(arg, '=');
    if (!eq || eq == arg) return -1;

    *key = arena_strndup(a, arg, (size_t)(eq - arg));
    *value = arena_strdup(a, eq + 1);
    return 0;
}
