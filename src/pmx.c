#include "mmdgltf_internal.h"

#include <stdio.h>
#include <string.h>

#define PMX_MIN_VALID_HEADER_SIZE 33

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

  mmdgltf_Pmx pmx = mmdgltf__arena_alloc(arena, sizeof(struct mmdgltf_Pmx_T));
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
    pmx->vertices = mmdgltf__arena_alloc(arena, sizeof(mmdgltf_PmxVertex) * (size_t)vertex_count);
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

    v->position = mmdgltf__stream_read_vec3(&stream);
    v->normal = mmdgltf__stream_read_vec3(&stream);
    v->uv = mmdgltf__stream_read_vec2(&stream);

    for (u8 u = 0; u < extra_uvs; ++u) {
      const mmdgltf_Vec4f extra_uv = mmdgltf__stream_read_vec4(&stream);
      if (u < MMDGLTF_MAX_EXTRA_UVS) {
        v->additional_uvs[u] = extra_uv;
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

      v->sdef_c = mmdgltf__stream_read_vec3(&stream);
      v->sdef_r0 = mmdgltf__stream_read_vec3(&stream);
      v->sdef_r1 = mmdgltf__stream_read_vec3(&stream);
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
    pmx->indices = mmdgltf__arena_alloc(arena, sizeof(u32) * (size_t)surface_count);
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
    pmx->textures =
      mmdgltf__arena_alloc(arena, sizeof(mmdgltf_PmxTexture) * (size_t)texture_count);
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
    pmx->materials =
      mmdgltf__arena_alloc(arena, sizeof(mmdgltf_PmxMaterial) * (size_t)material_count);
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

    m->diffuse = mmdgltf__stream_read_color4(&stream);
    m->specular = mmdgltf__stream_read_color3(&stream);
    m->shininess = mmdgltf__stream_read_f32(&stream);
    m->ambient = mmdgltf__stream_read_color3(&stream);
    m->draw_flags = mmdgltf__stream_read_u8(&stream);
    m->edge_color = mmdgltf__stream_read_color4(&stream);
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

  const u8 morph_idx_size = pmx->header.morph_index_size;
  const u8 rigid_idx_size = pmx->header.rigid_index_size;
  const u8 mat_idx_size = pmx->header.material_index_size;

  const i32 bone_count = mmdgltf__stream_read_i32(&stream);
  if (stream.status != MMDGLTF_OK) {
    RET_ERR(stream.status);
  }
  if (bone_count < 0) {
    RET_ERR(MMDGLTF_ERROR_CORRUPT_DATA);
  }

  pmx->bone_count = (size_t)bone_count;
  if (bone_count > 0) {
    pmx->bones = mmdgltf__arena_alloc(arena, sizeof(mmdgltf_PmxBone) * (size_t)bone_count);
    if (!pmx->bones) {
      RET_ERR(MMDGLTF_ERROR_OUT_OF_MEMORY);
    }
  }

  for (i32 i = 0; i < bone_count; ++i) {
    mmdgltf_PmxBone* b = &pmx->bones[i];
    memset(b, 0, sizeof(*b));
    b->parent_index = -1;
    b->connection_bone_index = -1;
    b->inherit_bone_index = -1;
    b->ik_target_index = -1;

    b->name_jp = mmdgltf__stream_read_string(&stream, arena, pmx->header.encoding);
    b->name_en = mmdgltf__stream_read_string(&stream, arena, pmx->header.encoding);

    b->position = mmdgltf__stream_read_vec3(&stream);

    b->parent_index = mmdgltf__stream_read_index(&stream, bone_idx_size);
    b->transform_order = mmdgltf__stream_read_i32(&stream);
    b->flags = mmdgltf__stream_read_u16(&stream);

    if (b->flags & MMDGLTF_BONE_CONNECTION_TARGET_BONE) {
      b->connection_bone_index = mmdgltf__stream_read_index(&stream, bone_idx_size);
    } else {
      b->offset = mmdgltf__stream_read_vec3(&stream);
    }

    if (b->flags & (MMDGLTF_BONE_APPEND_ROTATION | MMDGLTF_BONE_APPEND_TRANSLATION)) {
      b->inherit_bone_index = mmdgltf__stream_read_index(&stream, bone_idx_size);
      b->inherit_weight = mmdgltf__stream_read_f32(&stream);
    }

    if (b->flags & MMDGLTF_BONE_FIXED_AXIS) {
      b->fixed_axis = mmdgltf__stream_read_vec3(&stream);
    }

    if (b->flags & MMDGLTF_BONE_LOCAL_AXIS) {
      b->local_x = mmdgltf__stream_read_vec3(&stream);
      b->local_z = mmdgltf__stream_read_vec3(&stream);
    }

    if (b->flags & MMDGLTF_BONE_EXTERNAL_PARENT) {
      b->external_parent_key = mmdgltf__stream_read_i32(&stream);
    }

    if (b->flags & MMDGLTF_BONE_IK) {
      b->ik_target_index = mmdgltf__stream_read_index(&stream, bone_idx_size);
      b->ik_loop_count = mmdgltf__stream_read_i32(&stream);
      b->ik_limit_angle = mmdgltf__stream_read_f32(&stream);

      const i32 link_count = mmdgltf__stream_read_i32(&stream);
      if (link_count < 0) {
        RET_ERR(MMDGLTF_ERROR_CORRUPT_DATA);
      }
      b->ik_link_count = (u32)link_count;
      if (link_count > 0) {
        mmdgltf_PmxIkLink* links =
          mmdgltf__arena_alloc(arena, sizeof(mmdgltf_PmxIkLink) * (size_t)link_count);
        if (!links) {
          RET_ERR(MMDGLTF_ERROR_OUT_OF_MEMORY);
        }
        for (i32 j = 0; j < link_count; ++j) {
          links[j].bone_index = mmdgltf__stream_read_index(&stream, bone_idx_size);
          links[j].has_limits = mmdgltf__stream_read_u8(&stream);
          if (links[j].has_limits == 1) {
            links[j].min_angle = mmdgltf__stream_read_vec3(&stream);
            links[j].max_angle = mmdgltf__stream_read_vec3(&stream);
          } else if (links[j].has_limits != 0) {
            RET_ERR(MMDGLTF_ERROR_CORRUPT_DATA);
          }
        }
        b->ik_links = links;
      }
    }

    if (stream.status != MMDGLTF_OK) {
      RET_ERR(stream.status);
    }
  }

  for (size_t i = 0; i < pmx->bone_count; ++i) {
    const mmdgltf_PmxBone* b = &pmx->bones[i];
    if (b->parent_index != -1 &&
        (b->parent_index < 0 || (size_t)b->parent_index >= pmx->bone_count)) {
      RET_ERR(MMDGLTF_ERROR_CORRUPT_DATA);
    }
    if ((b->flags & MMDGLTF_BONE_CONNECTION_TARGET_BONE) && b->connection_bone_index != -1 &&
        (b->connection_bone_index < 0 || (size_t)b->connection_bone_index >= pmx->bone_count)) {
      RET_ERR(MMDGLTF_ERROR_CORRUPT_DATA);
    }
    if ((b->flags & (MMDGLTF_BONE_APPEND_ROTATION | MMDGLTF_BONE_APPEND_TRANSLATION)) &&
        b->inherit_bone_index != -1 &&
        (b->inherit_bone_index < 0 || (size_t)b->inherit_bone_index >= pmx->bone_count)) {
      RET_ERR(MMDGLTF_ERROR_CORRUPT_DATA);
    }
    if (b->flags & MMDGLTF_BONE_IK) {
      if (b->ik_target_index != -1 &&
          (b->ik_target_index < 0 || (size_t)b->ik_target_index >= pmx->bone_count)) {
        RET_ERR(MMDGLTF_ERROR_CORRUPT_DATA);
      }
      for (u32 j = 0; j < b->ik_link_count; ++j) {
        if (b->ik_links[j].bone_index < 0 ||
            (size_t)b->ik_links[j].bone_index >= pmx->bone_count) {
          RET_ERR(MMDGLTF_ERROR_CORRUPT_DATA);
        }
      }
    }
  }

  const i32 morph_count = mmdgltf__stream_read_i32(&stream);
  if (stream.status != MMDGLTF_OK) {
    RET_ERR(stream.status);
  }
  if (morph_count < 0) {
    RET_ERR(MMDGLTF_ERROR_CORRUPT_DATA);
  }

  pmx->morph_count = (size_t)morph_count;
  if (morph_count > 0) {
    pmx->morphs = mmdgltf__arena_alloc(arena, sizeof(mmdgltf_PmxMorph) * (size_t)morph_count);
    if (!pmx->morphs) {
      RET_ERR(MMDGLTF_ERROR_OUT_OF_MEMORY);
    }
  }

  for (i32 i = 0; i < morph_count; ++i) {
    mmdgltf_PmxMorph* m = &pmx->morphs[i];
    memset(m, 0, sizeof(*m));

    m->name_jp = mmdgltf__stream_read_string(&stream, arena, pmx->header.encoding);
    m->name_en = mmdgltf__stream_read_string(&stream, arena, pmx->header.encoding);
    m->category = (mmdgltf_MorphCategory)mmdgltf__stream_read_i8(&stream);
    m->type = (mmdgltf_MorphType)mmdgltf__stream_read_i8(&stream);

    const i32 offset_count = mmdgltf__stream_read_i32(&stream);
    if (offset_count < 0) {
      RET_ERR(MMDGLTF_ERROR_CORRUPT_DATA);
    }
    m->offset_count = (u32)offset_count;

    if (m->type == MMDGLTF_MORPH_VERTEX) {
      mmdgltf_PmxVertexMorphOffset* offsets =
        mmdgltf__arena_alloc(arena, sizeof(mmdgltf_PmxVertexMorphOffset) * (size_t)offset_count);
      if (offset_count > 0 && !offsets) {
        RET_ERR(MMDGLTF_ERROR_OUT_OF_MEMORY);
      }
      for (i32 j = 0; j < offset_count; ++j) {
        offsets[j].vertex_index = mmdgltf__stream_read_vertex_index(&stream, vtx_idx_size);
        offsets[j].position_offset = mmdgltf__stream_read_vec3(&stream);
        if (offsets[j].vertex_index >= (u32)pmx->vertex_count) {
          RET_ERR(MMDGLTF_ERROR_CORRUPT_DATA);
        }
      }
      m->vertex_offsets = offsets;
    } else if (m->type == MMDGLTF_MORPH_UV ||
               (m->type >= MMDGLTF_MORPH_EXTRA_UV1 && m->type <= MMDGLTF_MORPH_EXTRA_UV4)) {
      mmdgltf_PmxUvMorphOffset* offsets =
        mmdgltf__arena_alloc(arena, sizeof(mmdgltf_PmxUvMorphOffset) * (size_t)offset_count);
      if (offset_count > 0 && !offsets) {
        RET_ERR(MMDGLTF_ERROR_OUT_OF_MEMORY);
      }
      for (i32 j = 0; j < offset_count; ++j) {
        offsets[j].vertex_index = mmdgltf__stream_read_vertex_index(&stream, vtx_idx_size);
        offsets[j].uv_offset = mmdgltf__stream_read_vec4(&stream);
        if (offsets[j].vertex_index >= (u32)pmx->vertex_count) {
          RET_ERR(MMDGLTF_ERROR_CORRUPT_DATA);
        }
      }
      m->uv_offsets = offsets;
    } else if (m->type == MMDGLTF_MORPH_BONE) {
      mmdgltf_PmxBoneMorphOffset* offsets =
        mmdgltf__arena_alloc(arena, sizeof(mmdgltf_PmxBoneMorphOffset) * (size_t)offset_count);
      if (offset_count > 0 && !offsets) {
        RET_ERR(MMDGLTF_ERROR_OUT_OF_MEMORY);
      }
      for (i32 j = 0; j < offset_count; ++j) {
        offsets[j].bone_index = mmdgltf__stream_read_index(&stream, bone_idx_size);
        offsets[j].translation_offset = mmdgltf__stream_read_vec3(&stream);
        offsets[j].rotation_offset = mmdgltf__stream_read_quat(&stream);
        if (offsets[j].bone_index < 0 || (size_t)offsets[j].bone_index >= pmx->bone_count) {
          RET_ERR(MMDGLTF_ERROR_CORRUPT_DATA);
        }
      }
      m->bone_offsets = offsets;
    } else if (m->type == MMDGLTF_MORPH_MATERIAL) {
      mmdgltf_PmxMaterialMorphOffset* offsets =
        mmdgltf__arena_alloc(arena, sizeof(mmdgltf_PmxMaterialMorphOffset) * (size_t)offset_count);
      if (offset_count > 0 && !offsets) {
        RET_ERR(MMDGLTF_ERROR_OUT_OF_MEMORY);
      }
      for (i32 j = 0; j < offset_count; ++j) {
        offsets[j].material_index = mmdgltf__stream_read_index(&stream, mat_idx_size);
        offsets[j].offset_type = mmdgltf__stream_read_u8(&stream);
        offsets[j].diffuse = mmdgltf__stream_read_color4(&stream);
        offsets[j].specular = mmdgltf__stream_read_color3(&stream);
        offsets[j].shininess = mmdgltf__stream_read_f32(&stream);
        offsets[j].ambient = mmdgltf__stream_read_color3(&stream);
        offsets[j].edge_color = mmdgltf__stream_read_color4(&stream);
        offsets[j].edge_size = mmdgltf__stream_read_f32(&stream);
        offsets[j].texture_factor = mmdgltf__stream_read_color4(&stream);
        offsets[j].sphere_texture_factor = mmdgltf__stream_read_color4(&stream);
        offsets[j].toon_texture_factor = mmdgltf__stream_read_color4(&stream);
        if (offsets[j].material_index != -1 &&
            (offsets[j].material_index < 0 ||
             (size_t)offsets[j].material_index >= pmx->material_count)) {
          RET_ERR(MMDGLTF_ERROR_CORRUPT_DATA);
        }
      }
      m->material_offsets = offsets;
    } else if (m->type == MMDGLTF_MORPH_GROUP) {
      mmdgltf_PmxGroupMorphOffset* offsets =
        mmdgltf__arena_alloc(arena, sizeof(mmdgltf_PmxGroupMorphOffset) * (size_t)offset_count);
      if (offset_count > 0 && !offsets) {
        RET_ERR(MMDGLTF_ERROR_OUT_OF_MEMORY);
      }
      for (i32 j = 0; j < offset_count; ++j) {
        offsets[j].morph_index = mmdgltf__stream_read_index(&stream, morph_idx_size);
        offsets[j].factor = mmdgltf__stream_read_f32(&stream);
      }
      m->group_offsets = offsets;
    } else {
      RET_ERR(MMDGLTF_ERROR_CORRUPT_DATA);
    }

    if (stream.status != MMDGLTF_OK) {
      RET_ERR(stream.status);
    }
  }

  for (size_t i = 0; i < pmx->morph_count; ++i) {
    const mmdgltf_PmxMorph* m = &pmx->morphs[i];
    if (m->type == MMDGLTF_MORPH_GROUP) {
      for (u32 j = 0; j < m->offset_count; ++j) {
        if (m->group_offsets[j].morph_index < 0 ||
            (size_t)m->group_offsets[j].morph_index >= pmx->morph_count) {
          RET_ERR(MMDGLTF_ERROR_CORRUPT_DATA);
        }
      }
    }
  }

  const i32 disp_count = mmdgltf__stream_read_i32(&stream);
  if (stream.status != MMDGLTF_OK) {
    RET_ERR(stream.status);
  }
  if (disp_count < 0) {
    RET_ERR(MMDGLTF_ERROR_CORRUPT_DATA);
  }

  pmx->display_item_count = (size_t)disp_count;
  if (disp_count > 0) {
    pmx->display_items =
      mmdgltf__arena_alloc(arena, sizeof(mmdgltf_PmxDisplayItem) * (size_t)disp_count);
    if (!pmx->display_items) {
      RET_ERR(MMDGLTF_ERROR_OUT_OF_MEMORY);
    }
  }

  for (i32 i = 0; i < disp_count; ++i) {
    mmdgltf_PmxDisplayItem* d = &pmx->display_items[i];
    memset(d, 0, sizeof(*d));

    d->name_jp = mmdgltf__stream_read_string(&stream, arena, pmx->header.encoding);
    d->name_en = mmdgltf__stream_read_string(&stream, arena, pmx->header.encoding);
    d->is_special = mmdgltf__stream_read_u8(&stream);

    const i32 elem_count = mmdgltf__stream_read_i32(&stream);
    if (elem_count < 0) {
      RET_ERR(MMDGLTF_ERROR_CORRUPT_DATA);
    }
    d->element_count = (u32)elem_count;
    if (elem_count > 0) {
      mmdgltf_PmxDisplayElement* elems =
        mmdgltf__arena_alloc(arena, sizeof(mmdgltf_PmxDisplayElement) * (size_t)elem_count);
      if (!elems) {
        RET_ERR(MMDGLTF_ERROR_OUT_OF_MEMORY);
      }
      for (i32 j = 0; j < elem_count; ++j) {
        elems[j].type = mmdgltf__stream_read_u8(&stream);
        if (elems[j].type == 0) {
          elems[j].index = mmdgltf__stream_read_index(&stream, bone_idx_size);
          if (elems[j].index < 0 || (size_t)elems[j].index >= pmx->bone_count) {
            RET_ERR(MMDGLTF_ERROR_CORRUPT_DATA);
          }
        } else if (elems[j].type == 1) {
          elems[j].index = mmdgltf__stream_read_index(&stream, morph_idx_size);
          if (elems[j].index < 0 || (size_t)elems[j].index >= pmx->morph_count) {
            RET_ERR(MMDGLTF_ERROR_CORRUPT_DATA);
          }
        } else {
          RET_ERR(MMDGLTF_ERROR_CORRUPT_DATA);
        }
      }
      d->elements = elems;
    }

    if (stream.status != MMDGLTF_OK) {
      RET_ERR(stream.status);
    }
  }

  const i32 rigid_count = mmdgltf__stream_read_i32(&stream);
  if (stream.status != MMDGLTF_OK) {
    RET_ERR(stream.status);
  }
  if (rigid_count < 0) {
    RET_ERR(MMDGLTF_ERROR_CORRUPT_DATA);
  }

  pmx->rigid_body_count = (size_t)rigid_count;
  if (rigid_count > 0) {
    pmx->rigid_bodies =
      mmdgltf__arena_alloc(arena, sizeof(mmdgltf_PmxRigidBody) * (size_t)rigid_count);
    if (!pmx->rigid_bodies) {
      RET_ERR(MMDGLTF_ERROR_OUT_OF_MEMORY);
    }
  }

  for (i32 i = 0; i < rigid_count; ++i) {
    mmdgltf_PmxRigidBody* r = &pmx->rigid_bodies[i];
    memset(r, 0, sizeof(*r));

    r->name_jp = mmdgltf__stream_read_string(&stream, arena, pmx->header.encoding);
    r->name_en = mmdgltf__stream_read_string(&stream, arena, pmx->header.encoding);

    r->bone_index = mmdgltf__stream_read_index(&stream, bone_idx_size);
    if (r->bone_index != -1 && (r->bone_index < 0 || (size_t)r->bone_index >= pmx->bone_count)) {
      RET_ERR(MMDGLTF_ERROR_CORRUPT_DATA);
    }

    r->collision_group = mmdgltf__stream_read_u8(&stream);
    r->collision_mask = mmdgltf__stream_read_u16(&stream);

    const u8 shape_type = mmdgltf__stream_read_u8(&stream);
    if (shape_type > 2) {
      RET_ERR(MMDGLTF_ERROR_CORRUPT_DATA);
    }
    r->shape_type = (mmdgltf_RigidShapeType)shape_type;

    r->size = mmdgltf__stream_read_vec3(&stream);
    r->position = mmdgltf__stream_read_vec3(&stream);
    r->rotation = mmdgltf__stream_read_vec3(&stream);

    r->mass = mmdgltf__stream_read_f32(&stream);
    r->linear_damping = mmdgltf__stream_read_f32(&stream);
    r->angular_damping = mmdgltf__stream_read_f32(&stream);
    r->restitution = mmdgltf__stream_read_f32(&stream);
    r->friction = mmdgltf__stream_read_f32(&stream);

    const u8 physics_mode = mmdgltf__stream_read_u8(&stream);
    if (physics_mode > 2) {
      RET_ERR(MMDGLTF_ERROR_CORRUPT_DATA);
    }
    r->physics_mode = (mmdgltf_RigidPhysicsMode)physics_mode;

    if (stream.status != MMDGLTF_OK) {
      RET_ERR(stream.status);
    }
  }

  const i32 joint_count = mmdgltf__stream_read_i32(&stream);
  if (stream.status != MMDGLTF_OK) {
    RET_ERR(stream.status);
  }
  if (joint_count < 0) {
    RET_ERR(MMDGLTF_ERROR_CORRUPT_DATA);
  }

  pmx->joint_count = (size_t)joint_count;
  if (joint_count > 0) {
    pmx->joints = mmdgltf__arena_alloc(arena, sizeof(mmdgltf_PmxJoint) * (size_t)joint_count);
    if (!pmx->joints) {
      RET_ERR(MMDGLTF_ERROR_OUT_OF_MEMORY);
    }
  }

  for (i32 i = 0; i < joint_count; ++i) {
    mmdgltf_PmxJoint* j = &pmx->joints[i];
    memset(j, 0, sizeof(*j));

    j->name_jp = mmdgltf__stream_read_string(&stream, arena, pmx->header.encoding);
    j->name_en = mmdgltf__stream_read_string(&stream, arena, pmx->header.encoding);

    j->type = mmdgltf__stream_read_u8(&stream);

    j->src_rigid = mmdgltf__stream_read_index(&stream, rigid_idx_size);
    j->dest_rigid = mmdgltf__stream_read_index(&stream, rigid_idx_size);

    if (j->src_rigid != -1 &&
        (j->src_rigid < 0 || (size_t)j->src_rigid >= pmx->rigid_body_count)) {
      RET_ERR(MMDGLTF_ERROR_CORRUPT_DATA);
    }
    if (j->dest_rigid != -1 &&
        (j->dest_rigid < 0 || (size_t)j->dest_rigid >= pmx->rigid_body_count)) {
      RET_ERR(MMDGLTF_ERROR_CORRUPT_DATA);
    }

    j->position = mmdgltf__stream_read_vec3(&stream);
    j->rotation = mmdgltf__stream_read_vec3(&stream);
    j->min_position = mmdgltf__stream_read_vec3(&stream);
    j->max_position = mmdgltf__stream_read_vec3(&stream);
    j->min_rotation = mmdgltf__stream_read_vec3(&stream);
    j->max_rotation = mmdgltf__stream_read_vec3(&stream);
    j->spring_position = mmdgltf__stream_read_vec3(&stream);
    j->spring_rotation = mmdgltf__stream_read_vec3(&stream);

    if (stream.status != MMDGLTF_OK) {
      RET_ERR(stream.status);
    }
  }

  return pmx;

on_load_error:
  if (arena) {
    mmdgltf__arena_destroy(arena);
  }
  return MMDGLTF_ENCODE_ERROR(err);
}

MMDGLTF_API mmdgltf_Pmx mmdgltf_pmx_load_memory(const void* data, size_t size) {
  if (!data || size < PMX_MIN_VALID_HEADER_SIZE) {
    return MMDGLTF_ENCODE_ERROR(MMDGLTF_ERROR_INVALID_ARGUMENT);
  }
  mmdgltf__MemStream mem = {
    .data = (const u8*)data,
    .size = size,
    .pos = 0,
  };
  return mmdgltf_pmx_load_stream(&mmdgltf__mem_io, &mem);
}

static_assert(MMDGLTF_SEEK_CUR == SEEK_CUR, "SEEK_CUR mismatch");
static_assert(MMDGLTF_SEEK_END == SEEK_END, "SEEK_END mismatch");
static_assert(MMDGLTF_SEEK_SET == SEEK_SET, "SEEK_SET mismatch");
static const mmdgltf_StreamFunc file_io = {
  .read = (PFN_mmdgltf_read_func)fread,
  .seek = (PFN_mmdgltf_seek_func)fseek,
  .tell = (PFN_mmdgltf_tell_func)ftell,
  .eof = (PFN_mmdgltf_eof_func)feof,
};

MMDGLTF_API mmdgltf_Pmx mmdgltf_pmx_load_filen(const char* path, size_t path_len) {
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

  mmdgltf_Pmx pmx = mmdgltf_pmx_load_stream(&file_io, f);
  fclose(f);
  return pmx;
}

MMDGLTF_API mmdgltf_Pmx mmdgltf_pmx_load_file_n(const char* path, size_t path_len) {
  return mmdgltf_pmx_load_filen(path, path_len);
}

MMDGLTF_API mmdgltf_Pmx mmdgltf_pmx_load_file(const char* path) {
  if (!path) {
    return MMDGLTF_ENCODE_ERROR(MMDGLTF_ERROR_INVALID_ARGUMENT);
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

MMDGLTF_API mmdgltf_Status mmdgltf_pmx_bones(mmdgltf_Pmx pmx, const mmdgltf_PmxBone** out_bones,
                                             size_t* out_count) {
  if (!pmx || !out_bones || !out_count) {
    return MMDGLTF_ERROR_INVALID_ARGUMENT;
  }
  const mmdgltf_Status err = MMDGLTF_CHECK(pmx);
  if (err != MMDGLTF_OK) {
    return err;
  }
  *out_bones = pmx->bones;
  *out_count = pmx->bone_count;
  return MMDGLTF_OK;
}

MMDGLTF_API mmdgltf_Status mmdgltf_pmx_morphs(mmdgltf_Pmx pmx, const mmdgltf_PmxMorph** out_morphs,
                                              size_t* out_count) {
  if (!pmx || !out_morphs || !out_count) {
    return MMDGLTF_ERROR_INVALID_ARGUMENT;
  }
  const mmdgltf_Status err = MMDGLTF_CHECK(pmx);
  if (err != MMDGLTF_OK) {
    return err;
  }
  *out_morphs = pmx->morphs;
  *out_count = pmx->morph_count;
  return MMDGLTF_OK;
}

MMDGLTF_API mmdgltf_Status mmdgltf_pmx_display_items(
  mmdgltf_Pmx pmx, const mmdgltf_PmxDisplayItem** out_display_items, size_t* out_count) {
  if (!pmx || !out_display_items || !out_count) {
    return MMDGLTF_ERROR_INVALID_ARGUMENT;
  }
  const mmdgltf_Status err = MMDGLTF_CHECK(pmx);
  if (err != MMDGLTF_OK) {
    return err;
  }
  *out_display_items = pmx->display_items;
  *out_count = pmx->display_item_count;
  return MMDGLTF_OK;
}

MMDGLTF_API mmdgltf_Status mmdgltf_pmx_rigid_bodies(mmdgltf_Pmx pmx,
                                                    const mmdgltf_PmxRigidBody** out_rigid_bodies,
                                                    size_t* out_count) {
  if (!pmx || !out_rigid_bodies || !out_count) {
    return MMDGLTF_ERROR_INVALID_ARGUMENT;
  }
  const mmdgltf_Status err = MMDGLTF_CHECK(pmx);
  if (err != MMDGLTF_OK) {
    return err;
  }
  *out_rigid_bodies = pmx->rigid_bodies;
  *out_count = pmx->rigid_body_count;
  return MMDGLTF_OK;
}

MMDGLTF_API mmdgltf_Status mmdgltf_pmx_joints(mmdgltf_Pmx pmx, const mmdgltf_PmxJoint** out_joints,
                                              size_t* out_count) {
  if (!pmx || !out_joints || !out_count) {
    return MMDGLTF_ERROR_INVALID_ARGUMENT;
  }
  const mmdgltf_Status err = MMDGLTF_CHECK(pmx);
  if (err != MMDGLTF_OK) {
    return err;
  }
  *out_joints = pmx->joints;
  *out_count = pmx->joint_count;
  return MMDGLTF_OK;
}
