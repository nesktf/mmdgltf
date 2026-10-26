#include "mmdgltf_internal.h"

static void* mmd_alloc(size_t sz, void* user) {
  UNUSED(user);
  return malloc(sz);
}

static void* mmd_realloc(void* ptr, size_t newsz, void* user) {
  UNUSED(user);
  return realloc(ptr, newsz);
}

static void mmd_dealloc(void* ptr, void* user) {
  UNUSED(user);
  free(ptr);
}

_Thread_local mmdgltf_Alloc mmdgltf__alloc = {
  .alloc = &mmd_alloc,
  .realloc = &mmd_realloc,
  .dealloc = &mmd_dealloc,
  .user = NULL,
};

MMDGLTF_API void mmdgltf_set_alloc(const mmdgltf_Alloc* alloc) {
  if (!alloc) {
    return;
  }
  if (!alloc->dealloc || !alloc->realloc || !alloc->dealloc) {
    return;
  }
  mmdgltf__alloc = *alloc;
}
