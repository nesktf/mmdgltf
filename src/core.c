#include "mmdgltf_internal.h"

#include <string.h>

size_t mmdgltf__bytevec_append(mmdgltf__ByteVec* bv, const void* src, size_t sz) {
  if (!bv) {
    return 0;
  }
  const size_t aligned_offset = (bv->size + 3) & ~((size_t)3);
  const size_t needed = aligned_offset + sz;
  if (needed > bv->capacity) {
    size_t new_cap = bv->capacity ? bv->capacity * 2 : 1024 * 1024;
    while (new_cap < needed) {
      new_cap *= 2;
    }
    bv->data = MMDGLTF_REALLOC(bv->data, bv->capacity, new_cap);
    bv->capacity = new_cap;
  }
  while (bv->size < aligned_offset) {
    bv->data[bv->size++] = 0;
  }
  if (sz > 0 && src) {
    memcpy(bv->data + aligned_offset, src, sz);
  }
  bv->size = aligned_offset + sz;
  return aligned_offset;
}

tg3_str mmdgltf__tg3_make_str(mmdgltf_Arena* arena, const char* s) {
  tg3_str r;
  if (!s) {
    r.data = "";
    r.len = 0;
    return r;
  }
  const size_t len = strlen(s);
  char* copy = mmdgltf__arena_strndup(arena, s, len);
  r.data = copy ? copy : "";
  r.len = (u32)len;
  return r;
}

tg3_value mmdgltf__tg3_make_str_value(mmdgltf_Arena* arena, const char* s) {
  tg3_value v;
  memset(&v, 0, sizeof(v));
  v.type = TG3_VALUE_STRING;
  v.string_val = mmdgltf__tg3_make_str(arena, s);
  return v;
}

tg3_value mmdgltf__tg3_make_int_value(i64 val) {
  tg3_value v;
  memset(&v, 0, sizeof(v));
  v.type = TG3_VALUE_INT;
  v.int_val = val;
  return v;
}

tg3_value mmdgltf__tg3_make_real_value(f64 val) {
  tg3_value v;
  memset(&v, 0, sizeof(v));
  v.type = TG3_VALUE_REAL;
  v.real_val = val;
  return v;
}

tg3_kv_pair mmdgltf__tg3_make_kv(mmdgltf_Arena* arena, const char* key, tg3_value val) {
  tg3_kv_pair p;
  p.key = mmdgltf__tg3_make_str(arena, key);
  p.value = val;
  return p;
}

void mmdgltf__derive_bin_paths(mmdgltf_Arena* arena, const char* gltf_path, size_t path_len,
                               char** out_bin_path, char** out_bin_uri) {
  if (!arena || !gltf_path || path_len == 0 || !out_bin_path || !out_bin_uri) {
    return;
  }
  size_t last_slash = path_len;
  for (size_t i = path_len; i > 0; --i) {
    if (gltf_path[i - 1] == '/' || gltf_path[i - 1] == '\\') {
      last_slash = i - 1;
      break;
    }
  }

  const char* file_part = (last_slash < path_len) ? (gltf_path + last_slash + 1) : gltf_path;
  const size_t file_len = (last_slash < path_len) ? (path_len - (last_slash + 1)) : path_len;

  size_t stem_len = file_len;
  for (size_t i = file_len; i > 0; --i) {
    if (file_part[i - 1] == '.') {
      stem_len = i - 1;
      break;
    }
  }

  const size_t uri_len = stem_len + 4;
  char* uri = mmdgltf__arena_alloc(arena, uri_len + 1);
  memcpy(uri, file_part, stem_len);
  memcpy(uri + stem_len, ".bin", 5);
  *out_bin_uri = uri;

  if (last_slash < path_len) {
    const size_t dir_len = last_slash + 1;
    const size_t full_len = dir_len + stem_len + 4;
    char* full = mmdgltf__arena_alloc(arena, full_len + 1);
    memcpy(full, gltf_path, dir_len);
    memcpy(full + dir_len, file_part, stem_len);
    memcpy(full + dir_len + stem_len, ".bin", 5);
    *out_bin_path = full;
  } else {
    *out_bin_path = uri;
  }
}

static void* mmd_alloc(size_t sz, void* user) {
  UNUSED(user);
  return malloc(sz);
}

static void* mmd_realloc(void* ptr, size_t oldsz, size_t newsz, void* user) {
  UNUSED(user);
  UNUSED(oldsz);
  return realloc(ptr, newsz);
}

static void mmd_dealloc(void* ptr, size_t size, void* user) {
  UNUSED(size);
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
  if (!alloc->alloc || !alloc->realloc || !alloc->dealloc) {
    return;
  }
  mmdgltf__alloc = *alloc;
}
