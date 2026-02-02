#include "arena.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ArenaBlock *arena_block_new(size_t min_size) {
    size_t size = min_size > ARENA_BLOCK_SIZE ? min_size : ARENA_BLOCK_SIZE;
    ArenaBlock *b = malloc(sizeof(ArenaBlock) + size);
    if (!b) {
        fprintf(stderr, "mux: out of memory\n");
        exit(1);
    }
    b->next = NULL;
    b->size = size;
    b->used = 0;
    return b;
}

Arena arena_new(void) {
    Arena a;
    a.head = arena_block_new(ARENA_BLOCK_SIZE);
    a.current = a.head;
    return a;
}

void *arena_alloc(Arena *a, size_t size) {
    /* Align to 8 bytes */
    size = (size + 7) & ~(size_t)7;

    if (a->current->used + size > a->current->size) {
        ArenaBlock *b = arena_block_new(size);
        a->current->next = b;
        a->current = b;
    }
    void *ptr = a->current->data + a->current->used;
    a->current->used += size;
    return ptr;
}

char *arena_strdup(Arena *a, const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *copy = arena_alloc(a, len + 1);
    memcpy(copy, s, len + 1);
    return copy;
}

char *arena_strndup(Arena *a, const char *s, size_t n) {
    if (!s) return NULL;
    char *copy = arena_alloc(a, n + 1);
    memcpy(copy, s, n);
    copy[n] = '\0';
    return copy;
}

void arena_reset(Arena *a) {
    ArenaBlock *b = a->head;
    while (b) {
        b->used = 0;
        b = b->next;
    }
    a->current = a->head;
}

void arena_free(Arena *a) {
    ArenaBlock *b = a->head;
    while (b) {
        ArenaBlock *next = b->next;
        free(b);
        b = next;
    }
    a->head = NULL;
    a->current = NULL;
}
