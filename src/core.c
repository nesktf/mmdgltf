#include "mmdgltf_internal.h"

#include <stdio.h>
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

i32 mmdgltf__texture_list_get_or_add(mmdgltf__TextureList* tl, mmdgltf_Arena* arena,
                                     const char* path) {
  if (!tl || !path || path[0] == '\0') {
    return -1;
  }
  for (size_t i = 0; i < tl->count; ++i) {
    if (strcmp(tl->items[i].path, path) == 0) {
      return (i32)i;
    }
  }
  if (tl->count >= tl->capacity) {
    const size_t new_cap = tl->capacity ? tl->capacity * 2 : 16;
    mmdgltf_PmxTexture* new_items =
      mmdgltf__arena_alloc(arena, new_cap * sizeof(mmdgltf_PmxTexture));
    if (!new_items) {
      return -1;
    }
    if (tl->count > 0) {
      memcpy(new_items, tl->items, tl->count * sizeof(mmdgltf_PmxTexture));
    }
    tl->items = new_items;
    tl->capacity = new_cap;
  }
  const size_t idx = tl->count++;
  tl->items[idx].path = mmdgltf__arena_strndup(arena, path, strlen(path));
  return (i32)idx;
}

i32 mmdgltf__texture_list_add(mmdgltf__TextureList* tl, mmdgltf_Arena* arena, const char* path) {
  return mmdgltf__texture_list_get_or_add(tl, arena, path);
}

i32 mmdgltf__fastpath_tex_list_add(mmdgltf__FastpathTexList* tl, mmdgltf_Arena* arena,
                                   const char* path) {
  if (!tl || !path || path[0] == '\0') {
    return -1;
  }
  for (size_t i = 0; i < tl->count; ++i) {
    if (strcmp(tl->paths[i], path) == 0) {
      return (i32)i;
    }
  }
  if (tl->count >= tl->capacity) {
    const size_t new_cap = tl->capacity ? tl->capacity * 2 : 16;
    char** new_paths = mmdgltf__arena_alloc(arena, new_cap * sizeof(char*));
    if (!new_paths) {
      return -1;
    }
    if (tl->count > 0) {
      memcpy(new_paths, tl->paths, tl->count * sizeof(char*));
    }
    tl->paths = new_paths;
    tl->capacity = new_cap;
  }
  const size_t idx = tl->count++;
  tl->paths[idx] = mmdgltf__arena_strndup(arena, path, strlen(path));
  return (i32)idx;
}

bool mmdgltf__ends_with_ci(const char* str, const char* suffix) {
  if (!str || !suffix) {
    return false;
  }
  const size_t str_len = strlen(str);
  const size_t suf_len = strlen(suffix);
  if (str_len < suf_len) {
    return false;
  }
  const char* p = str + (str_len - suf_len);
  for (size_t i = 0; i < suf_len; ++i) {
    char c1 = p[i];
    char c2 = suffix[i];
    if (c1 >= 'A' && c1 <= 'Z') {
      c1 = (char)(c1 + ('a' - 'A'));
    }
    if (c2 >= 'A' && c2 <= 'Z') {
      c2 = (char)(c2 + ('a' - 'A'));
    }
    if (c1 != c2) {
      return false;
    }
  }
  return true;
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

static_assert(MMDGLTF_SEEK_CUR == SEEK_CUR, "SEEK_CUR mismatch");
static_assert(MMDGLTF_SEEK_END == SEEK_END, "SEEK_END mismatch");
static_assert(MMDGLTF_SEEK_SET == SEEK_SET, "SEEK_SET mismatch");
const mmdgltf_StreamFunc mmdgltf__file_io = {
  .read = (PFN_mmdgltf_read_func)fread,
  .seek = (PFN_mmdgltf_seek_func)fseek,
  .tell = (PFN_mmdgltf_tell_func)ftell,
  .eof = (PFN_mmdgltf_eof_func)feof,
};

MMDGLTF_API mmdgltf_Pmx mmdgltf_load_stream(const mmdgltf_StreamFunc* func, void* src) {
  if (!func || !src || !func->read || !func->seek) {
    return MMDGLTF_ENCODE_ERROR(MMDGLTF_ERROR_INVALID_ARGUMENT);
  }

  u8 magic[4] = {0};
  const size_t rd = func->read(magic, 1, 4, src);
  if (rd < 3) {
    return MMDGLTF_ENCODE_ERROR(MMDGLTF_ERROR_INVALID_SIGNATURE);
  }

  if (func->seek(src, 0, MMDGLTF_SEEK_SET) != 0) {
    return MMDGLTF_ENCODE_ERROR(MMDGLTF_ERROR_IO);
  }

  if (memcmp(magic, "Pmd", 3) == 0) {
    return mmdgltf__pmd_load_stream(func, src);
  } else if (rd >= 4 && memcmp(magic, "PMX ", 4) == 0) {
    return mmdgltf__pmx_load_stream(func, src);
  }

  return MMDGLTF_ENCODE_ERROR(MMDGLTF_ERROR_INVALID_SIGNATURE);
}

MMDGLTF_API mmdgltf_Pmx mmdgltf_load_memory(const void* data, size_t size) {
  if (!data || size == 0) {
    return MMDGLTF_ENCODE_ERROR(MMDGLTF_ERROR_INVALID_ARGUMENT);
  }
  if (size < 4) {
    return MMDGLTF_ENCODE_ERROR(MMDGLTF_ERROR_INVALID_SIGNATURE);
  }
  mmdgltf__MemStream mem = {
    .data = (const u8*)data,
    .size = size,
    .pos = 0,
  };
  return mmdgltf_load_stream(&mmdgltf__mem_io, &mem);
}

MMDGLTF_API mmdgltf_Pmx mmdgltf_load_filen(const char* path, size_t path_len) {
  if (!path || path_len == 0) {
    return MMDGLTF_ENCODE_ERROR(MMDGLTF_ERROR_INVALID_ARGUMENT);
  }

  char* null_path = MMDGLTF_MALLOC(path_len + 1);
  if (!null_path) {
    return MMDGLTF_ENCODE_ERROR(MMDGLTF_ERROR_OUT_OF_MEMORY);
  }
  memcpy(null_path, path, path_len);
  null_path[path_len] = '\0';

  FILE* f = fopen(null_path, "rb");
  MMDGLTF_FREE(null_path, path_len + 1);
  if (!f) {
    return MMDGLTF_ENCODE_ERROR(MMDGLTF_ERROR_IO);
  }

  mmdgltf_Pmx pmx = mmdgltf_load_stream(&mmdgltf__file_io, f);
  fclose(f);
  if (MMDGLTF_CHECK(pmx) == MMDGLTF_OK && pmx) {
    size_t last_slash = (size_t)-1;
    for (size_t i = 0; i < path_len; ++i) {
      if (path[i] == '/' || path[i] == '\\') {
        last_slash = i;
      }
    }
    if (last_slash != (size_t)-1) {
      pmx->base_dir = mmdgltf__arena_strndup(pmx->arena, path, last_slash);
    } else {
      pmx->base_dir = mmdgltf__arena_strndup(pmx->arena, ".", 1);
    }
  }
  return pmx;
}

MMDGLTF_API mmdgltf_Pmx mmdgltf_load_file(const char* path) {
  if (!path) {
    return MMDGLTF_ENCODE_ERROR(MMDGLTF_ERROR_INVALID_ARGUMENT);
  }
  return mmdgltf_load_filen(path, strlen(path));
}
