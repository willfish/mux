#ifndef MUX_ARENA_H
#define MUX_ARENA_H

#include <stddef.h>

#define ARENA_BLOCK_SIZE (64 * 1024)

typedef struct ArenaBlock {
    struct ArenaBlock *next;
    size_t size;
    size_t used;
    char data[];
} ArenaBlock;

typedef struct {
    ArenaBlock *head;
    ArenaBlock *current;
} Arena;

Arena arena_new(void);
void *arena_alloc(Arena *a, size_t size);
char *arena_strdup(Arena *a, const char *s);
char *arena_strndup(Arena *a, const char *s, size_t n);
void arena_reset(Arena *a);
void arena_free(Arena *a);

#endif
