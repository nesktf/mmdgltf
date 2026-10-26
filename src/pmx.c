#include "mmdgltf_internal.h"

#include <stdio.h>
#include <string.h>

#define PMX_MIN_VALID_HEADER_SIZE 33

void mmdgltf__stream_read_bytes(mmdgltf_Stream* s, void* dst, size_t count) {
  if (s->status != MMDGLTF_OK || count == 0) {
    return;
  }
  const size_t read_bytes = s->func.read(dst, 1, count, s->src);
  if (read_bytes != count) {
    s->status = MMDGLTF_ERROR_BUFFER_TOO_SMALL;
  }
}

u8 mmdgltf__stream_read_u8(mmdgltf_Stream* s) {
  u8 v = 0;
  mmdgltf__stream_read_bytes(s, &v, 1);
  return v;
}

i8 mmdgltf__stream_read_i8(mmdgltf_Stream* s) {
  return (i8)mmdgltf__stream_read_u8(s);
}

u16 mmdgltf__stream_read_u16(mmdgltf_Stream* s) {
  u8 b[2];
  mmdgltf__stream_read_bytes(s, b, 2);
  if (s->status != MMDGLTF_OK) {
    return 0;
  }
  return (u16)b[0] | ((u16)b[1] << 8);
}

i16 mmdgltf__stream_read_i16(mmdgltf_Stream* s) {
  return (i16)mmdgltf__stream_read_u16(s);
}

u32 mmdgltf__stream_read_u32(mmdgltf_Stream* s) {
  u8 b[4];
  mmdgltf__stream_read_bytes(s, b, 4);
  if (s->status != MMDGLTF_OK) {
    return 0;
  }
  return (u32)b[0] | ((u32)b[1] << 8) | ((u32)b[2] << 16) | ((u32)b[3] << 24);
}

i32 mmdgltf__stream_read_i32(mmdgltf_Stream* s) {
  return (i32)mmdgltf__stream_read_u32(s);
}

f32 mmdgltf__stream_read_f32(mmdgltf_Stream* s) {
  const u32 u = mmdgltf__stream_read_u32(s);
  f32 f = 0.0f;
  memcpy(&f, &u, sizeof(f));
  return f;
}

u32 mmdgltf__stream_read_vertex_index(mmdgltf_Stream* s, u8 size) {
  if (size == 1) {
    return (u32)mmdgltf__stream_read_u8(s);
  } else if (size == 2) {
    return (u32)mmdgltf__stream_read_u16(s);
  } else if (size == 4) {
    return mmdgltf__stream_read_u32(s);
  }
  s->status = MMDGLTF_ERROR_CORRUPT_DATA;
  return 0;
}

i32 mmdgltf__stream_read_index(mmdgltf_Stream* s, u8 size) {
  if (size == 1) {
    return (i32)mmdgltf__stream_read_i8(s);
  } else if (size == 2) {
    return (i32)mmdgltf__stream_read_i16(s);
  } else if (size == 4) {
    return mmdgltf__stream_read_i32(s);
  }
  s->status = MMDGLTF_ERROR_CORRUPT_DATA;
  return -1;
}

static char* decode_utf16le(mmdgltf_Arena* arena, const u8* src, size_t src_bytes) {
  const size_t num_units = src_bytes / 2;
  const size_t max_utf8_len = num_units * 3 + 1;
  char* dst = (char*)mmdgltf__arena_alloc(arena, max_utf8_len);
  if (!dst) {
    return NULL;
  }

  size_t out_idx = 0;
  for (size_t i = 0; i < num_units; ++i) {
    u32 cp = (u32)src[i * 2] | ((u32)src[i * 2 + 1] << 8);
    if (cp >= 0xD800 && cp <= 0xDBFF) {
      if (i + 1 < num_units) {
        const u32 low = (u32)src[(i + 1) * 2] | ((u32)src[(i + 1) * 2 + 1] << 8);
        if (low >= 0xDC00 && low <= 0xDFFF) {
          cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
          ++i;
        } else {
          cp = 0xFFFD;
        }
      } else {
        cp = 0xFFFD;
      }
    } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
      cp = 0xFFFD;
    }

    if (cp < 0x80) {
      dst[out_idx++] = (char)cp;
    } else if (cp < 0x800) {
      dst[out_idx++] = (char)(0xC0 | (cp >> 6));
      dst[out_idx++] = (char)(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
      dst[out_idx++] = (char)(0xE0 | (cp >> 12));
      dst[out_idx++] = (char)(0x80 | ((cp >> 6) & 0x3F));
      dst[out_idx++] = (char)(0x80 | (cp & 0x3F));
    } else {
      dst[out_idx++] = (char)(0xF0 | (cp >> 18));
      dst[out_idx++] = (char)(0x80 | ((cp >> 12) & 0x3F));
      dst[out_idx++] = (char)(0x80 | ((cp >> 6) & 0x3F));
      dst[out_idx++] = (char)(0x80 | (cp & 0x3F));
    }
  }
  dst[out_idx] = '\0';
  return dst;
}

char* mmdgltf__stream_read_string(mmdgltf_Stream* s, mmdgltf_Arena* arena,
                                  mmdgltf_TextEncoding encoding) {
  const i32 len = mmdgltf__stream_read_i32(s);
  if (s->status != MMDGLTF_OK) {
    return NULL;
  }
  if (len < 0) {
    s->status = MMDGLTF_ERROR_CORRUPT_DATA;
    return NULL;
  }
  if (len == 0) {
    return mmdgltf__arena_strndup(arena, "", 0);
  }

  u8* str_bytes = (u8*)mmdgltf__arena_alloc(arena, (size_t)len);
  if (!str_bytes) {
    s->status = MMDGLTF_ERROR_OUT_OF_MEMORY;
    return NULL;
  }
  mmdgltf__stream_read_bytes(s, str_bytes, (size_t)len);
  if (s->status != MMDGLTF_OK) {
    return NULL;
  }

  if (encoding == MMDGLTF_ENCODING_UTF8) {
    return mmdgltf__arena_strndup(arena, (const char*)str_bytes, (size_t)len);
  }
  return decode_utf16le(arena, str_bytes, (size_t)len);
}

MMDGLTF_API mmdgltf_Pmx mmdgltf_pmx_load_stream(const mmdgltf_StreamFunc* func, void* src) {
  mmdgltf_Status err;
  mmdgltf_Arena* arena = NULL;

#define RET_ERR(_err) \
  err = _err;         \
  goto on_load_error

  if (!func || !func->read) {
    RET_ERR(MMDGLTF_ERROR_INVALID_ARGUMENT);
  }

  mmdgltf_Stream stream;
  stream.func = *func;
  stream.src = src;
  stream.status = MMDGLTF_OK;

  u8 sig[4];
  mmdgltf__stream_read_bytes(&stream, sig, 4);
  if (stream.status != MMDGLTF_OK) {
    RET_ERR(stream.status);
  }
  if (sig[0] != 'P' || sig[1] != 'M' || sig[2] != 'X' || sig[3] != ' ') {
    RET_ERR(MMDGLTF_ERROR_INVALID_SIGNATURE);
  }

  const f32 version = mmdgltf__stream_read_f32(&stream);
  if (version < 2.0f || version > 2.1f) {
    RET_ERR(MMDGLTF_ERROR_UNSUPPORTED_VERSION);
  }

  const u8 globals_count = mmdgltf__stream_read_u8(&stream);
  if (globals_count < 8) {
    RET_ERR(MMDGLTF_ERROR_CORRUPT_DATA);
  }

  u8 globals[8];
  mmdgltf__stream_read_bytes(&stream, globals, 8);
  for (u8 i = 8; i < globals_count; ++i) {
    mmdgltf__stream_read_u8(&stream);
  }

  if (stream.status != MMDGLTF_OK) {
    RET_ERR(stream.status);
  }

  arena = mmdgltf__arena_create(64 * 1024);
  if (!arena) {
    RET_ERR(MMDGLTF_ERROR_OUT_OF_MEMORY);
  }

  mmdgltf_Pmx pmx = (mmdgltf_Pmx)mmdgltf__arena_alloc(arena, sizeof(struct mmdgltf_Pmx_T));
  if (!pmx) {
    RET_ERR(MMDGLTF_ERROR_OUT_OF_MEMORY);
  }

  pmx->arena = arena;
  pmx->header.version = version;
  pmx->header.encoding = (mmdgltf_TextEncoding)globals[0];
  pmx->header.additional_uvs = globals[1];
  pmx->header.vertex_index_size = globals[2];
  pmx->header.texture_index_size = globals[3];
  pmx->header.material_index_size = globals[4];
  pmx->header.bone_index_size = globals[5];
  pmx->header.morph_index_size = globals[6];
  pmx->header.rigid_index_size = globals[7];

  pmx->model_info.name_jp = mmdgltf__stream_read_string(&stream, arena, pmx->header.encoding);
  pmx->model_info.name_en = mmdgltf__stream_read_string(&stream, arena, pmx->header.encoding);
  pmx->model_info.comment_jp = mmdgltf__stream_read_string(&stream, arena, pmx->header.encoding);
  pmx->model_info.comment_en = mmdgltf__stream_read_string(&stream, arena, pmx->header.encoding);

  if (stream.status != MMDGLTF_OK) {
    RET_ERR(stream.status);
  }

  const i32 vertex_count = mmdgltf__stream_read_i32(&stream);
  if (stream.status != MMDGLTF_OK) {
    RET_ERR(stream.status);
  }
  if (vertex_count < 0) {
    RET_ERR(MMDGLTF_ERROR_CORRUPT_DATA);
  }

  pmx->vertex_count = (size_t)vertex_count;
  if (vertex_count > 0) {
    pmx->vertices = (mmdgltf_PmxVertex*)mmdgltf__arena_alloc(arena, sizeof(mmdgltf_PmxVertex) *
                                                                      (size_t)vertex_count);
    if (!pmx->vertices) {
      RET_ERR(MMDGLTF_ERROR_OUT_OF_MEMORY);
    }
  }

  const u8 bone_idx_size = pmx->header.bone_index_size;
  const u8 extra_uvs = pmx->header.additional_uvs;

  for (i32 i = 0; i < vertex_count; ++i) {
    mmdgltf_PmxVertex* v = &pmx->vertices[i];
    memset(v, 0, sizeof(*v));
    for (size_t b = 0; b < MMDGLTF_MAX_BONE_WEIGHTS; ++b) {
      v->bones[b] = -1;
    }

    v->position[0] = mmdgltf__stream_read_f32(&stream);
    v->position[1] = mmdgltf__stream_read_f32(&stream);
    v->position[2] = mmdgltf__stream_read_f32(&stream);

    v->normal[0] = mmdgltf__stream_read_f32(&stream);
    v->normal[1] = mmdgltf__stream_read_f32(&stream);
    v->normal[2] = mmdgltf__stream_read_f32(&stream);

    v->uv[0] = mmdgltf__stream_read_f32(&stream);
    v->uv[1] = mmdgltf__stream_read_f32(&stream);

    for (u8 u = 0; u < extra_uvs; ++u) {
      const f32 uv_x = mmdgltf__stream_read_f32(&stream);
      const f32 uv_y = mmdgltf__stream_read_f32(&stream);
      const f32 uv_z = mmdgltf__stream_read_f32(&stream);
      const f32 uv_w = mmdgltf__stream_read_f32(&stream);
      if (u < MMDGLTF_MAX_EXTRA_UVS) {
        v->additional_uvs[u][0] = uv_x;
        v->additional_uvs[u][1] = uv_y;
        v->additional_uvs[u][2] = uv_z;
        v->additional_uvs[u][3] = uv_w;
      }
    }

    const u8 weight_type = mmdgltf__stream_read_u8(&stream);
    v->weight_type = (mmdgltf_WeightType)weight_type;

    if (weight_type == MMDGLTF_WEIGHT_BDEF1) {
      v->bones[0] = mmdgltf__stream_read_index(&stream, bone_idx_size);
      v->weights[0] = 1.0f;
    } else if (weight_type == MMDGLTF_WEIGHT_BDEF2) {
      v->bones[0] = mmdgltf__stream_read_index(&stream, bone_idx_size);
      v->bones[1] = mmdgltf__stream_read_index(&stream, bone_idx_size);
      const f32 w0 = mmdgltf__stream_read_f32(&stream);
      v->weights[0] = w0;
      v->weights[1] = 1.0f - w0;
    } else if (weight_type == MMDGLTF_WEIGHT_BDEF4 || weight_type == MMDGLTF_WEIGHT_QDEF) {
      for (size_t b = 0; b < 4; ++b) {
        const i32 bone_idx = mmdgltf__stream_read_index(&stream, bone_idx_size);
        if (b < MMDGLTF_MAX_BONE_WEIGHTS) {
          v->bones[b] = bone_idx;
        }
      }
      for (size_t b = 0; b < 4; ++b) {
        const f32 w = mmdgltf__stream_read_f32(&stream);
        if (b < MMDGLTF_MAX_BONE_WEIGHTS) {
          v->weights[b] = w;
        }
      }
    } else if (weight_type == MMDGLTF_WEIGHT_SDEF) {
      v->bones[0] = mmdgltf__stream_read_index(&stream, bone_idx_size);
      v->bones[1] = mmdgltf__stream_read_index(&stream, bone_idx_size);
      const f32 w0 = mmdgltf__stream_read_f32(&stream);
      v->weights[0] = w0;
      v->weights[1] = 1.0f - w0;

      v->sdef_c[0] = mmdgltf__stream_read_f32(&stream);
      v->sdef_c[1] = mmdgltf__stream_read_f32(&stream);
      v->sdef_c[2] = mmdgltf__stream_read_f32(&stream);

      v->sdef_r0[0] = mmdgltf__stream_read_f32(&stream);
      v->sdef_r0[1] = mmdgltf__stream_read_f32(&stream);
      v->sdef_r0[2] = mmdgltf__stream_read_f32(&stream);

      v->sdef_r1[0] = mmdgltf__stream_read_f32(&stream);
      v->sdef_r1[1] = mmdgltf__stream_read_f32(&stream);
      v->sdef_r1[2] = mmdgltf__stream_read_f32(&stream);
    } else {
      RET_ERR(MMDGLTF_ERROR_CORRUPT_DATA);
    }

    v->edge_scale = mmdgltf__stream_read_f32(&stream);

    if (stream.status != MMDGLTF_OK) {
      RET_ERR(stream.status);
    }
  }

  const i32 surface_count = mmdgltf__stream_read_i32(&stream);
  if (stream.status != MMDGLTF_OK) {
    RET_ERR(stream.status);
  }
  if (surface_count < 0 || (surface_count % 3 != 0)) {
    RET_ERR(MMDGLTF_ERROR_CORRUPT_DATA);
  }

  pmx->index_count = (size_t)surface_count;
  if (surface_count > 0) {
    pmx->indices = (u32*)mmdgltf__arena_alloc(arena, sizeof(u32) * (size_t)surface_count);
    if (!pmx->indices) {
      RET_ERR(MMDGLTF_ERROR_OUT_OF_MEMORY);
    }
  }

  const u8 vtx_idx_size = pmx->header.vertex_index_size;
  for (i32 i = 0; i < surface_count; ++i) {
    const u32 idx = mmdgltf__stream_read_vertex_index(&stream, vtx_idx_size);
    if (stream.status != MMDGLTF_OK) {
      RET_ERR(stream.status);
    }
    if (idx >= (u32)vertex_count) {
      RET_ERR(MMDGLTF_ERROR_CORRUPT_DATA);
    }
    pmx->indices[i] = idx;
  }

  const i32 texture_count = mmdgltf__stream_read_i32(&stream);
  if (stream.status != MMDGLTF_OK) {
    RET_ERR(stream.status);
  }
  if (texture_count < 0) {
    RET_ERR(MMDGLTF_ERROR_CORRUPT_DATA);
  }

  pmx->texture_count = (size_t)texture_count;
  if (texture_count > 0) {
    pmx->textures = (mmdgltf_PmxTexture*)mmdgltf__arena_alloc(arena, sizeof(mmdgltf_PmxTexture) *
                                                                       (size_t)texture_count);
    if (!pmx->textures) {
      RET_ERR(MMDGLTF_ERROR_OUT_OF_MEMORY);
    }
  }

  for (i32 i = 0; i < texture_count; ++i) {
    char* path = mmdgltf__stream_read_string(&stream, arena, pmx->header.encoding);
    if (stream.status != MMDGLTF_OK) {
      RET_ERR(stream.status);
    }
    if (path) {
      for (char* p = path; *p; ++p) {
        if (*p == '\\') {
          *p = '/';
        }
      }
    }
    pmx->textures[i].path = path;
  }

  const i32 material_count = mmdgltf__stream_read_i32(&stream);
  if (stream.status != MMDGLTF_OK) {
    RET_ERR(stream.status);
  }
  if (material_count < 0) {
    RET_ERR(MMDGLTF_ERROR_CORRUPT_DATA);
  }

  pmx->material_count = (size_t)material_count;
  if (material_count > 0) {
    pmx->materials = (mmdgltf_PmxMaterial*)mmdgltf__arena_alloc(
      arena, sizeof(mmdgltf_PmxMaterial) * (size_t)material_count);
    if (!pmx->materials) {
      RET_ERR(MMDGLTF_ERROR_OUT_OF_MEMORY);
    }
  }

  const u8 tex_idx_size = pmx->header.texture_index_size;
  u32 total_mat_indices = 0;

  for (i32 i = 0; i < material_count; ++i) {
    mmdgltf_PmxMaterial* m = &pmx->materials[i];
    m->name_jp = mmdgltf__stream_read_string(&stream, arena, pmx->header.encoding);
    m->name_en = mmdgltf__stream_read_string(&stream, arena, pmx->header.encoding);

    m->diffuse[0] = mmdgltf__stream_read_f32(&stream);
    m->diffuse[1] = mmdgltf__stream_read_f32(&stream);
    m->diffuse[2] = mmdgltf__stream_read_f32(&stream);
    m->diffuse[3] = mmdgltf__stream_read_f32(&stream);

    m->specular[0] = mmdgltf__stream_read_f32(&stream);
    m->specular[1] = mmdgltf__stream_read_f32(&stream);
    m->specular[2] = mmdgltf__stream_read_f32(&stream);

    m->shininess = mmdgltf__stream_read_f32(&stream);

    m->ambient[0] = mmdgltf__stream_read_f32(&stream);
    m->ambient[1] = mmdgltf__stream_read_f32(&stream);
    m->ambient[2] = mmdgltf__stream_read_f32(&stream);

    m->draw_flags = mmdgltf__stream_read_u8(&stream);

    m->edge_color[0] = mmdgltf__stream_read_f32(&stream);
    m->edge_color[1] = mmdgltf__stream_read_f32(&stream);
    m->edge_color[2] = mmdgltf__stream_read_f32(&stream);
    m->edge_color[3] = mmdgltf__stream_read_f32(&stream);

    m->edge_size = mmdgltf__stream_read_f32(&stream);

    m->texture_index = mmdgltf__stream_read_index(&stream, tex_idx_size);
    if (m->texture_index != -1 &&
        (m->texture_index < 0 || (size_t)m->texture_index >= pmx->texture_count)) {
      RET_ERR(MMDGLTF_ERROR_CORRUPT_DATA);
    }

    m->sphere_texture_index = mmdgltf__stream_read_index(&stream, tex_idx_size);
    if (m->sphere_texture_index != -1 &&
        (m->sphere_texture_index < 0 || (size_t)m->sphere_texture_index >= pmx->texture_count)) {
      RET_ERR(MMDGLTF_ERROR_CORRUPT_DATA);
    }

    const u8 sphere_mode = mmdgltf__stream_read_u8(&stream);
    if (sphere_mode > 3) {
      RET_ERR(MMDGLTF_ERROR_CORRUPT_DATA);
    }
    m->sphere_mode = (mmdgltf_SphereMode)sphere_mode;

    m->toon_sharing_flag = mmdgltf__stream_read_u8(&stream);
    if (m->toon_sharing_flag == 0) {
      m->toon_index = mmdgltf__stream_read_index(&stream, tex_idx_size);
      if (m->toon_index != -1 &&
          (m->toon_index < 0 || (size_t)m->toon_index >= pmx->texture_count)) {
        RET_ERR(MMDGLTF_ERROR_CORRUPT_DATA);
      }
    } else if (m->toon_sharing_flag == 1) {
      m->toon_index = (i32)mmdgltf__stream_read_u8(&stream);
    } else {
      RET_ERR(MMDGLTF_ERROR_CORRUPT_DATA);
    }

    m->comment = mmdgltf__stream_read_string(&stream, arena, pmx->header.encoding);

    const i32 idx_count = mmdgltf__stream_read_i32(&stream);
    if (idx_count < 0 || (idx_count % 3 != 0)) {
      RET_ERR(MMDGLTF_ERROR_CORRUPT_DATA);
    }
    m->index_count = (u32)idx_count;
    total_mat_indices += (u32)idx_count;
  }

  if (total_mat_indices != (u32)pmx->index_count) {
    RET_ERR(MMDGLTF_ERROR_CORRUPT_DATA);
  }

  return pmx;

on_load_error:
  if (arena) {
    mmdgltf__arena_destroy(arena);
  }
  return (mmdgltf_Pmx)MMDGLTF_ENCODE_ERROR(err);
}

typedef struct mmdgltf__MemStream {
  const u8* data;
  size_t size;
  size_t pos;
} mmdgltf__MemStream;

static size_t mem_read(void* buff, size_t sz, size_t nmemb, mmdgltf__MemStream* m) {
  if (!m || !buff || sz == 0 || nmemb == 0) {
    return 0;
  }
  const size_t total = sz * nmemb;
  const size_t available = (m->pos < m->size) ? (m->size - m->pos) : 0;
  const size_t to_copy = total < available ? total : available;
  if (to_copy > 0) {
    memcpy(buff, m->data + m->pos, to_copy);
    m->pos += to_copy;
  }
  return to_copy / sz;
}

static int mem_seek(mmdgltf__MemStream* m, long offset, int origin) {
  if (!m) {
    return -1;
  }
  long new_pos = 0;
  if (origin == MMDGLTF_SEEK_SET) {
    new_pos = offset;
  } else if (origin == MMDGLTF_SEEK_CUR) {
    new_pos = (long)m->pos + offset;
  } else if (origin == MMDGLTF_SEEK_END) {
    new_pos = (long)m->size + offset;
  } else {
    return -1;
  }
  if (new_pos < 0 || (size_t)new_pos > m->size) {
    return -1;
  }
  m->pos = (size_t)new_pos;
  return 0;
}

static long mem_tell(mmdgltf__MemStream* m) {
  return m ? (long)m->pos : -1;
}

static int mem_eof(mmdgltf__MemStream* m) {
  return (!m || m->pos >= m->size) ? 1 : 0;
}

static const mmdgltf_StreamFunc mem_io = {
  .read = (PFN_mmdgltf_read_func)mem_read,
  .seek = (PFN_mmdgltf_seek_func)mem_seek,
  .tell = (PFN_mmdgltf_tell_func)mem_tell,
  .eof = (PFN_mmdgltf_eof_func)mem_eof,
};

MMDGLTF_API mmdgltf_Pmx mmdgltf_pmx_load_memory(const void* data, size_t size) {
  if (!data || size < PMX_MIN_VALID_HEADER_SIZE) {
    return (mmdgltf_Pmx)MMDGLTF_ENCODE_ERROR(MMDGLTF_ERROR_INVALID_ARGUMENT);
  }
  mmdgltf__MemStream mem = {
    .data = (const u8*)data,
    .size = size,
    .pos = 0,
  };
  return mmdgltf_pmx_load_stream(&mem_io, &mem);
}

static const mmdgltf_StreamFunc file_io = {
  .read = (PFN_mmdgltf_read_func)fread,
  .seek = (PFN_mmdgltf_seek_func)(fseek),
  .tell = (PFN_mmdgltf_tell_func)ftell,
  .eof = (PFN_mmdgltf_eof_func)feof,
};

MMDGLTF_API mmdgltf_Pmx mmdgltf_pmx_load_filen(const char* path, size_t path_len) {
  if (!path || path_len == 0) {
    return (mmdgltf_Pmx)MMDGLTF_ENCODE_ERROR(MMDGLTF_ERROR_INVALID_ARGUMENT);
  }

  char* null_path = (char*)malloc(path_len + 1);
  if (!null_path) {
    return (mmdgltf_Pmx)MMDGLTF_ENCODE_ERROR(MMDGLTF_ERROR_OUT_OF_MEMORY);
  }
  memcpy(null_path, path, path_len);
  null_path[path_len] = '\0';

  FILE* f = fopen(null_path, "rb");
  free(null_path);
  if (!f) {
    return (mmdgltf_Pmx)MMDGLTF_ENCODE_ERROR(MMDGLTF_ERROR_IO);
  }

  mmdgltf_Pmx pmx = mmdgltf_pmx_load_stream(&file_io, f);
  fclose(f);
  return pmx;
}

MMDGLTF_API mmdgltf_Pmx mmdgltf_pmx_load_file_n(const char* path, size_t path_len) {
  return mmdgltf_pmx_load_filen(path, path_len);
}

MMDGLTF_API mmdgltf_Pmx mmdgltf_pmx_load_file(const char* path) {
  if (!path) {
    return (mmdgltf_Pmx)MMDGLTF_ENCODE_ERROR(MMDGLTF_ERROR_INVALID_ARGUMENT);
  }
  return mmdgltf_pmx_load_filen(path, strlen(path));
}

MMDGLTF_API void mmdgltf_pmx_destroy(mmdgltf_Pmx pmx) {
  if (!pmx || MMDGLTF_CHECK(pmx) != MMDGLTF_OK) {
    return;
  }
  mmdgltf__arena_destroy(pmx->arena);
}

MMDGLTF_API mmdgltf_Status mmdgltf_pmx_get_header(mmdgltf_Pmx pmx, mmdgltf_PmxHeader* out_header) {
  if (!pmx || !out_header) {
    return MMDGLTF_ERROR_INVALID_ARGUMENT;
  }
  const mmdgltf_Status err = MMDGLTF_CHECK(pmx);
  if (err != MMDGLTF_OK) {
    return err;
  }
  *out_header = pmx->header;
  return MMDGLTF_OK;
}

MMDGLTF_API mmdgltf_Status mmdgltf_pmx_get_model_info(mmdgltf_Pmx pmx,
                                                      mmdgltf_PmxModelInfo* out_info) {
  if (!pmx || !out_info) {
    return MMDGLTF_ERROR_INVALID_ARGUMENT;
  }
  const mmdgltf_Status err = MMDGLTF_CHECK(pmx);
  if (err != MMDGLTF_OK) {
    return err;
  }
  *out_info = pmx->model_info;
  return MMDGLTF_OK;
}

MMDGLTF_API mmdgltf_Status mmdgltf_pmx_vertices(mmdgltf_Pmx pmx,
                                                const mmdgltf_PmxVertex** out_vertices,
                                                size_t* out_count) {
  if (!pmx || !out_vertices || !out_count) {
    return MMDGLTF_ERROR_INVALID_ARGUMENT;
  }
  const mmdgltf_Status err = MMDGLTF_CHECK(pmx);
  if (err != MMDGLTF_OK) {
    return err;
  }
  *out_vertices = pmx->vertices;
  *out_count = pmx->vertex_count;
  return MMDGLTF_OK;
}

MMDGLTF_API mmdgltf_Status mmdgltf_pmx_indices(mmdgltf_Pmx pmx, const uint32_t** out_indices,
                                               size_t* out_count) {
  if (!pmx || !out_indices || !out_count) {
    return MMDGLTF_ERROR_INVALID_ARGUMENT;
  }
  const mmdgltf_Status err = MMDGLTF_CHECK(pmx);
  if (err != MMDGLTF_OK) {
    return err;
  }
  *out_indices = pmx->indices;
  *out_count = pmx->index_count;
  return MMDGLTF_OK;
}

MMDGLTF_API mmdgltf_Status mmdgltf_pmx_textures(mmdgltf_Pmx pmx,
                                                const mmdgltf_PmxTexture** out_textures,
                                                size_t* out_count) {
  if (!pmx || !out_textures || !out_count) {
    return MMDGLTF_ERROR_INVALID_ARGUMENT;
  }
  const mmdgltf_Status err = MMDGLTF_CHECK(pmx);
  if (err != MMDGLTF_OK) {
    return err;
  }
  *out_textures = pmx->textures;
  *out_count = pmx->texture_count;
  return MMDGLTF_OK;
}

MMDGLTF_API mmdgltf_Status mmdgltf_pmx_materials(mmdgltf_Pmx pmx,
                                                 const mmdgltf_PmxMaterial** out_materials,
                                                 size_t* out_count) {
  if (!pmx || !out_materials || !out_count) {
    return MMDGLTF_ERROR_INVALID_ARGUMENT;
  }
  const mmdgltf_Status err = MMDGLTF_CHECK(pmx);
  if (err != MMDGLTF_OK) {
    return err;
  }
  *out_materials = pmx->materials;
  *out_count = pmx->material_count;
  return MMDGLTF_OK;
}
