#include "mmdgltf_internal.h"

#include <stdio.h>
#include <string.h>

MMDGLTF_API mmdgltf_Pmx mmdgltf_pmd_load_stream(const mmdgltf_StreamFunc* func, void* src) {
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

  u8 sig[3];
  mmdgltf__stream_read_bytes(&stream, sig, 3);
  if (stream.status != MMDGLTF_OK) {
    RET_ERR(stream.status);
  }
  if (sig[0] != 'P' || sig[1] != 'm' || sig[2] != 'd') {
    RET_ERR(MMDGLTF_ERROR_INVALID_SIGNATURE);
  }

  const f32 version = mmdgltf__stream_read_f32(&stream);
  if (version != 1.0f) {
    RET_ERR(MMDGLTF_ERROR_UNSUPPORTED_VERSION);
  }

  arena = mmdgltf__arena_create(64 * 1024);
  if (!arena) {
    RET_ERR(MMDGLTF_ERROR_OUT_OF_MEMORY);
  }

  mmdgltf_Pmx pmx = mmdgltf__arena_alloc(arena, sizeof(struct mmdgltf_Pmx_T));
  if (!pmx) {
    RET_ERR(MMDGLTF_ERROR_OUT_OF_MEMORY);
  }
  memset(pmx, 0, sizeof(struct mmdgltf_Pmx_T));

  pmx->arena = arena;
  pmx->header.version = 2.0f;
  pmx->header.encoding = MMDGLTF_ENCODING_UTF8;
  pmx->header.additional_uvs = 0;
  pmx->header.vertex_index_size = 4;
  pmx->header.texture_index_size = 2;
  pmx->header.material_index_size = 2;
  pmx->header.bone_index_size = 2;
  pmx->header.morph_index_size = 2;
  pmx->header.rigid_index_size = 2;

  pmx->model_info.name_jp = mmdgltf__stream_read_string_cp932(&stream, arena, 20);
  pmx->model_info.name_en = "";
  pmx->model_info.comment_jp = mmdgltf__stream_read_string_cp932(&stream, arena, 256);
  pmx->model_info.comment_en = "";

  if (stream.status != MMDGLTF_OK) {
    RET_ERR(stream.status);
  }

  const u32 vertex_count = mmdgltf__stream_read_u32(&stream);
  if (stream.status != MMDGLTF_OK) {
    RET_ERR(stream.status);
  }

  pmx->vertex_count = (size_t)vertex_count;
  if (vertex_count > 0) {
    pmx->vertices = mmdgltf__arena_alloc(arena, sizeof(mmdgltf_PmxVertex) * (size_t)vertex_count);
    if (!pmx->vertices) {
      RET_ERR(MMDGLTF_ERROR_OUT_OF_MEMORY);
    }
    memset(pmx->vertices, 0, sizeof(mmdgltf_PmxVertex) * (size_t)vertex_count);

    for (size_t i = 0; i < (size_t)vertex_count; ++i) {
      mmdgltf_PmxVertex* v = &pmx->vertices[i];
      v->position = mmdgltf__stream_read_vec3(&stream);
      v->normal = mmdgltf__stream_read_vec3(&stream);
      v->uv = mmdgltf__stream_read_vec2(&stream);
      const u16 b0 = mmdgltf__stream_read_u16(&stream);
      const u16 b1 = mmdgltf__stream_read_u16(&stream);
      const u8 weight = mmdgltf__stream_read_u8(&stream);
      const u8 edge_flag = mmdgltf__stream_read_u8(&stream);

      v->edge_scale = (edge_flag == 0) ? 1.0f : 0.0f;
      if (b0 == b1) {
        v->weight_type = MMDGLTF_WEIGHT_BDEF1;
        v->bones[0] = (i32)b0;
        v->weights[0] = 1.0f;
      } else {
        v->weight_type = MMDGLTF_WEIGHT_BDEF2;
        v->bones[0] = (i32)b0;
        v->bones[1] = (i32)b1;
        v->weights[0] = (f32)weight / 100.0f;
        v->weights[1] = 1.0f - v->weights[0];
      }
    }
    if (stream.status != MMDGLTF_OK) {
      RET_ERR(stream.status);
    }
  }

  const u32 face_vert_count = mmdgltf__stream_read_u32(&stream);
  if (stream.status != MMDGLTF_OK) {
    RET_ERR(stream.status);
  }

  pmx->index_count = (size_t)face_vert_count;
  if (face_vert_count > 0) {
    pmx->indices = mmdgltf__arena_alloc(arena, sizeof(u32) * (size_t)face_vert_count);
    if (!pmx->indices) {
      RET_ERR(MMDGLTF_ERROR_OUT_OF_MEMORY);
    }
    for (size_t i = 0; i < (size_t)face_vert_count; ++i) {
      pmx->indices[i] = (u32)mmdgltf__stream_read_u16(&stream);
    }
    if (stream.status != MMDGLTF_OK) {
      RET_ERR(stream.status);
    }
  }

  // TODO: parse materials, bones, IKs, morphs, display frames, and physics.

  return pmx;

on_load_error:
  if (arena) {
    mmdgltf__arena_destroy(arena);
  }
  return MMDGLTF_ENCODE_ERROR(err);
#undef RET_ERR
}

MMDGLTF_API mmdgltf_Pmx mmdgltf_pmd_load_memory(const void* data, size_t size) {
  if (!data || size == 0) {
    return MMDGLTF_ENCODE_ERROR(MMDGLTF_ERROR_INVALID_ARGUMENT);
  }
  mmdgltf__MemStream mem = {
    .data = (const u8*)data,
    .size = size,
    .pos = 0,
  };
  return mmdgltf_pmd_load_stream(&mmdgltf__mem_io, &mem);
}

MMDGLTF_API mmdgltf_Pmx mmdgltf_pmd_load_filen(const char* path, size_t path_len) {
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

  mmdgltf_Pmx pmx = mmdgltf_pmd_load_stream(&mmdgltf__file_io, f);
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

MMDGLTF_API mmdgltf_Pmx mmdgltf_pmd_load_file(const char* path) {
  if (!path) {
    return MMDGLTF_ENCODE_ERROR(MMDGLTF_ERROR_INVALID_ARGUMENT);
  }
  return mmdgltf_pmd_load_filen(path, strlen(path));
}
