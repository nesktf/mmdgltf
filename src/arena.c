#include "mmdgltf_internal.h"

#include <string.h>

#define ARENA_ALIGNMENT          16
#define ARENA_DEFAULT_BLOCK_SIZE (64 * 1024)

static mmdgltf_ArenaBlock* arena_new_block(mmdgltf_Arena* arena, size_t min_size) {
  size_t cap = arena->block_size;
  if (cap < min_size) {
    cap = min_size;
  }

  const size_t total_size = sizeof(mmdgltf_ArenaBlock) + cap;
  void* ptr = MMDGLTF_MALLOC(total_size);
  if (!ptr) {
    return NULL;
  }

  mmdgltf_ArenaBlock* block = (mmdgltf_ArenaBlock*)ptr;
  block->base = ptr + sizeof(mmdgltf_ArenaBlock);
  block->used = 0;
  block->capacity = cap;
  block->next = NULL;

  arena->total_allocated += total_size;
  if (arena->current) {
    arena->current->next = block;
  } else {
    arena->head = block;
  }
  arena->current = block;

  return block;
}

mmdgltf_Arena* mmdgltf__arena_create(size_t block_size) {
  mmdgltf_Arena* arena = (mmdgltf_Arena*)MMDGLTF_MALLOC(sizeof(mmdgltf_Arena));
  if (!arena) {
    return NULL;
  }

  arena->head = NULL;
  arena->current = NULL;
  arena->total_allocated = sizeof(mmdgltf_Arena);
  arena->block_size = block_size > 0 ? block_size : ARENA_DEFAULT_BLOCK_SIZE;

  return arena;
}

void* mmdgltf__arena_alloc(mmdgltf_Arena* arena, size_t size) {
  if (!arena || size == 0) {
    return NULL;
  }

  const size_t aligned = (size + (ARENA_ALIGNMENT - 1)) & ~(size_t)(ARENA_ALIGNMENT - 1);
  mmdgltf_ArenaBlock* block = arena->current;
  if (!block || block->used + aligned > block->capacity) {
    block = arena_new_block(arena, aligned);
    if (!block) {
      return NULL;
    }
  }

  void* ptr = block->base + block->used;
  block->used += aligned;
  return ptr;
}

char* mmdgltf__arena_strndup(mmdgltf_Arena* arena, const char* str, size_t len) {
  if (!arena || !str) {
    return NULL;
  }

  char* dst = (char*)mmdgltf__arena_alloc(arena, len + 1);
  if (!dst) {
    return NULL;
  }

  if (len > 0) {
    memcpy(dst, str, len);
  }
  dst[len] = '\0';
  return dst;
}

void mmdgltf__arena_destroy(mmdgltf_Arena* arena) {
  if (!arena) {
    return;
  }

  mmdgltf_ArenaBlock* block = arena->head;
  while (block) {
    mmdgltf_ArenaBlock* next = block->next;
    const size_t total_size = sizeof(mmdgltf_ArenaBlock) + block->capacity;
    MMDGLTF_FREE(block, total_size);
    block = next;
  }

  MMDGLTF_FREE(arena, sizeof(mmdgltf_Arena));
}
