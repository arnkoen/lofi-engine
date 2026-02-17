/*
Minimal arena allocator. Needs to be compiled for c11.

Copyright (c) Arne Koenig 2025
Redistribution and use in source and binary forms, with or without modification, are permitted.
THIS SOFTWARE IS PROVIDED 'AS-IS', WITHOUT ANY EXPRESS OR IMPLIED WARRANTY. IN NO EVENT WILL THE AUTHORS BE HELD LIABLE FOR ANY DAMAGES ARISING FROM THE USE OF THIS SOFTWARE.
*/

#ifndef ARENA_H
#define ARENA_H

#include <stddef.h> //size_t
#include <stdalign.h>

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(ARENA_MALLOC) || !defined(ARENA_FREE)
#include <stdlib.h>
#define ARENA_MALLOC(size) malloc(size)
#define ARENA_FREE(ptr) free(ptr)
#endif


typedef struct ArenaAlloc {
    char* buffer;
    size_t capacity;
    size_t offset;
    size_t prev_offset;
} ArenaAlloc;

int arena_init(ArenaAlloc* arena, void* buffer, size_t capacity);
void* arena_alloc(ArenaAlloc* arena, size_t size, size_t alignment);
void arena_pop(ArenaAlloc* arena);
void arena_reset(ArenaAlloc* arena);
#define arena_alloc_type(arena, T, count) \
    (T*)arena_alloc((arena), sizeof(T) * (count), alignof(T))

#ifdef __cplusplus
}
#endif

#endif // ARENA_H

#ifdef ARENA_IMPL

int arena_init(ArenaAlloc* arena, void* buffer, size_t capacity) {
    if(!buffer) return 0;
    arena->buffer = (char*)buffer;
    arena->capacity = capacity;
    arena->offset = 0;
    arena->prev_offset = 0;
    return 1;
}

static size_t _align_forward(size_t ptr, size_t align) {
    size_t mod = ptr % align;
    return mod == 0 ? ptr : ptr + (align - mod);
}

void* arena_alloc(ArenaAlloc* arena, size_t size, size_t align) {
    size_t current = (size_t)arena->buffer + arena->offset;
    size_t alignment = align == 0 ? sizeof(void*) : align;
    size_t aligned = _align_forward(current, alignment);
    size_t new_offset = aligned - (size_t)arena->buffer + size;

    if(new_offset > arena->capacity) return NULL;

    void* ret = (void*)(arena->buffer + (aligned - (size_t)arena->buffer));
    arena->prev_offset = arena->offset;
    arena->offset = new_offset;
    return ret;
}

void arena_pop(ArenaAlloc* arena) {
    arena->offset = arena->prev_offset;
}

void arena_reset(ArenaAlloc* arena) {
    arena->offset = 0;
    arena->prev_offset = 0;
}


#endif //IMPL
