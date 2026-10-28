#include "mmdgltf_internal.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static bool is_knee_bone(const char* name) {
  if (!name) {
    return false;
  }
  const size_t len = strlen(name);
  if (len >= 6 && strcmp(name + len - 6, "\xE3\x81\xB2\xE3\x81\x96") == 0) {
    return true;
  }
  return false;
}

static mmdgltf_Vec3f vec3_normalize(mmdgltf_Vec3f v) {
  const f32 len_sq = v.x * v.x + v.y * v.y + v.z * v.z;
  if (len_sq > 1e-12f) {
    const f32 inv_len = 1.0f / sqrtf(len_sq);
    return (mmdgltf_Vec3f){v.x * inv_len, v.y * inv_len, v.z * inv_len};
  }
  return (mmdgltf_Vec3f){0.0f, 0.0f, 0.0f};
}

typedef struct PmdRawBone {
  char* name_jp;
  u16 parent;
  u16 tail_bone;
  u8 type;
  i16 ik_bone;
  mmdgltf_Vec3f position;
} PmdRawBone;

typedef struct PmdRawMorphItem {
  u32 index;
  mmdgltf_Vec3f offset;
} PmdRawMorphItem;

typedef struct PmdRawMorph {
  char* name_jp;
  u32 data_size;
  u8 type;
  PmdRawMorphItem* data;
} PmdRawMorph;

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

  const u32 material_count = mmdgltf__stream_read_u32(&stream);
  if (stream.status != MMDGLTF_OK) {
    RET_ERR(stream.status);
  }

  mmdgltf__TextureList texture_list = {0};
  pmx->material_count = (size_t)material_count;
  if (material_count > 0) {
    pmx->materials =
      mmdgltf__arena_alloc(arena, sizeof(mmdgltf_PmxMaterial) * (size_t)material_count);
    if (!pmx->materials) {
      RET_ERR(MMDGLTF_ERROR_OUT_OF_MEMORY);
    }
    memset(pmx->materials, 0, sizeof(mmdgltf_PmxMaterial) * (size_t)material_count);

    for (size_t i = 0; i < (size_t)material_count; ++i) {
      mmdgltf_PmxMaterial* m = &pmx->materials[i];
      char name_buf[32];
      snprintf(name_buf, sizeof(name_buf), "Material%zu", i + 1);
      m->name_jp = mmdgltf__arena_strndup(arena, name_buf, strlen(name_buf));
      m->name_en = m->name_jp;
      m->comment = "";

      m->diffuse = mmdgltf__stream_read_color4(&stream);
      m->shininess = mmdgltf__stream_read_f32(&stream);
      m->specular = mmdgltf__stream_read_color3(&stream);
      m->ambient = mmdgltf__stream_read_color3(&stream);
      const i8 toon_idx = mmdgltf__stream_read_i8(&stream);
      const u8 edge_flag = mmdgltf__stream_read_u8(&stream);
      m->index_count = mmdgltf__stream_read_u32(&stream);

      char* raw_tex = mmdgltf__stream_read_string_cp932(&stream, arena, 20);

      m->draw_flags = 0;
      if (m->diffuse.a < 1.0f) {
        m->draw_flags |= MMDGLTF_MATERIAL_DOUBLE_SIDED;
      }
      if (edge_flag != 0) {
        m->draw_flags |= MMDGLTF_MATERIAL_EDGE;
      }
      if (m->diffuse.a > 0.0f) {
        m->draw_flags |= MMDGLTF_MATERIAL_GROUND_SHADOW | MMDGLTF_MATERIAL_SELF_SHADOW_MAP |
                         MMDGLTF_MATERIAL_SELF_SHADOW;
      }

      m->edge_color = (mmdgltf_Color4f){0.0f, 0.0f, 0.0f, 1.0f};
      m->edge_size = 1.0f;
      m->texture_index = -1;
      m->sphere_texture_index = -1;
      m->sphere_mode = MMDGLTF_SPHERE_NONE;

      if (raw_tex && raw_tex[0] != '\0') {
        for (size_t k = 0; raw_tex[k] != '\0'; ++k) {
          if (raw_tex[k] == '\\') {
            raw_tex[k] = '/';
          }
        }
        char* star = strchr(raw_tex, '*');
        const char* diffuse_path = NULL;
        const char* sphere_path = NULL;
        mmdgltf_SphereMode sp_mode = MMDGLTF_SPHERE_NONE;

        if (star) {
          *star = '\0';
          char* part1 = raw_tex;
          char* part2 = star + 1;
          if (!mmdgltf__ends_with_ci(part1, ".sph") && !mmdgltf__ends_with_ci(part1, ".spa")) {
            diffuse_path = part1;
          } else {
            sphere_path = part1;
            sp_mode =
              mmdgltf__ends_with_ci(part1, ".spa") ? MMDGLTF_SPHERE_ADD : MMDGLTF_SPHERE_MUL;
          }
          if (part2[0] != '\0') {
            sphere_path = part2;
            sp_mode =
              mmdgltf__ends_with_ci(part2, ".spa") ? MMDGLTF_SPHERE_ADD : MMDGLTF_SPHERE_MUL;
          }
        } else {
          if (mmdgltf__ends_with_ci(raw_tex, ".sph") || mmdgltf__ends_with_ci(raw_tex, ".spa")) {
            sphere_path = raw_tex;
            sp_mode =
              mmdgltf__ends_with_ci(raw_tex, ".spa") ? MMDGLTF_SPHERE_ADD : MMDGLTF_SPHERE_MUL;
          } else {
            diffuse_path = raw_tex;
          }
        }

        if (diffuse_path && diffuse_path[0] != '\0') {
          m->texture_index = mmdgltf__texture_list_get_or_add(&texture_list, arena, diffuse_path);
        }
        if (sphere_path && sphere_path[0] != '\0') {
          m->sphere_texture_index =
            mmdgltf__texture_list_get_or_add(&texture_list, arena, sphere_path);
          m->sphere_mode = sp_mode;
        }
      }

      if (toon_idx >= 0 && toon_idx <= 9) {
        m->toon_sharing_flag = 1;
        m->toon_index = toon_idx;
      } else {
        m->toon_sharing_flag = 0;
        m->toon_index = -1;
      }
    }
    if (stream.status != MMDGLTF_OK) {
      RET_ERR(stream.status);
    }
  }

  const u16 bone_count = mmdgltf__stream_read_u16(&stream);
  if (stream.status != MMDGLTF_OK) {
    RET_ERR(stream.status);
  }

  pmx->bone_count = (size_t)bone_count;
  PmdRawBone* raw_bones = NULL;
  if (bone_count > 0) {
    pmx->bones = mmdgltf__arena_alloc(arena, sizeof(mmdgltf_PmxBone) * (size_t)bone_count);
    raw_bones = mmdgltf__arena_alloc(arena, sizeof(PmdRawBone) * (size_t)bone_count);
    if (!pmx->bones || !raw_bones) {
      RET_ERR(MMDGLTF_ERROR_OUT_OF_MEMORY);
    }
    memset(pmx->bones, 0, sizeof(mmdgltf_PmxBone) * (size_t)bone_count);

    for (size_t i = 0; i < (size_t)bone_count; ++i) {
      raw_bones[i].name_jp = mmdgltf__stream_read_string_cp932(&stream, arena, 20);
      raw_bones[i].parent = mmdgltf__stream_read_u16(&stream);
      raw_bones[i].tail_bone = mmdgltf__stream_read_u16(&stream);
      raw_bones[i].type = mmdgltf__stream_read_u8(&stream);
      raw_bones[i].ik_bone = (raw_bones[i].type == 9) ? mmdgltf__stream_read_i16(&stream)
                                                      : (i16)mmdgltf__stream_read_u16(&stream);
      raw_bones[i].position = mmdgltf__stream_read_vec3(&stream);
    }
    if (stream.status != MMDGLTF_OK) {
      RET_ERR(stream.status);
    }

    for (size_t i = 0; i < (size_t)bone_count; ++i) {
      mmdgltf_PmxBone* b = &pmx->bones[i];
      const PmdRawBone* rb = &raw_bones[i];

      b->name_jp = rb->name_jp;
      b->name_en = "";
      b->position = rb->position;
      b->parent_index = (rb->parent == 0xFFFF) ? -1 : (i32)rb->parent;
      b->connection_bone_index = -1;
      b->inherit_bone_index = -1;
      b->ik_target_index = -1;

      uint16_t flags = MMDGLTF_BONE_ROTATABLE | MMDGLTF_BONE_CONTROLLABLE;
      if (rb->type == 1) {
        flags |= MMDGLTF_BONE_MOVABLE;
      }
      if (rb->type != 6 && rb->type != 7 && rb->type != 9) {
        flags |= MMDGLTF_BONE_VISIBLE;
      }

      if (rb->tail_bone != 0xFFFF && (size_t)rb->tail_bone < pmx->bone_count && rb->type != 8 &&
          rb->type != 9) {
        flags |= MMDGLTF_BONE_CONNECTION_TARGET_BONE;
        b->connection_bone_index = (i32)rb->tail_bone;
      }

      if (rb->type == 2) {
        b->transform_order = 1;
      } else if (rb->type == 5) {
        b->transform_order = 2;
        flags |= MMDGLTF_BONE_APPEND_ROTATION;
        b->inherit_bone_index = (i32)rb->ik_bone;
        b->inherit_weight = 1.0f;
      } else if (rb->type == 8) {
        flags |= MMDGLTF_BONE_FIXED_AXIS;
        if (rb->tail_bone != 0xFFFF && (size_t)rb->tail_bone < pmx->bone_count) {
          const mmdgltf_Vec3f tail_pos = raw_bones[rb->tail_bone].position;
          const mmdgltf_Vec3f dir = (mmdgltf_Vec3f){
            tail_pos.x - rb->position.x, tail_pos.y - rb->position.y, tail_pos.z - rb->position.z};
          b->fixed_axis = vec3_normalize(dir);
        }
      } else if (rb->type == 9) {
        flags |= MMDGLTF_BONE_APPEND_ROTATION;
        b->inherit_bone_index = (rb->tail_bone == 0xFFFF) ? -1 : (i32)rb->tail_bone;
        b->inherit_weight = (f32)rb->ik_bone / 100.0f;
      }

      b->flags = flags;
    }

    for (size_t i = 0; i < (size_t)bone_count; ++i) {
      const i32 parent = pmx->bones[i].parent_index;
      if (parent >= 0 && (size_t)parent < pmx->bone_count) {
        if (pmx->bones[i].transform_order < pmx->bones[parent].transform_order) {
          pmx->bones[i].transform_order = pmx->bones[parent].transform_order;
        }
      }
    }
  }

  const u16 ik_count = mmdgltf__stream_read_u16(&stream);
  if (stream.status != MMDGLTF_OK) {
    RET_ERR(stream.status);
  }

  for (size_t i = 0; i < (size_t)ik_count; ++i) {
    const u16 ik_bone = mmdgltf__stream_read_u16(&stream);
    const u16 target_bone = mmdgltf__stream_read_u16(&stream);
    const u8 ik_chain = mmdgltf__stream_read_u8(&stream);
    const u16 iterations = mmdgltf__stream_read_u16(&stream);
    const f32 control_weight = mmdgltf__stream_read_f32(&stream);

    u16* child_bones = mmdgltf__arena_alloc(arena, sizeof(u16) * ik_chain);
    for (size_t j = 0; j < (size_t)ik_chain; ++j) {
      child_bones[j] = mmdgltf__stream_read_u16(&stream);
    }
    if (stream.status != MMDGLTF_OK) {
      RET_ERR(stream.status);
    }

    if ((size_t)ik_bone < pmx->bone_count) {
      mmdgltf_PmxBone* b = &pmx->bones[ik_bone];
      b->flags |= MMDGLTF_BONE_IK;
      b->ik_target_index = (i32)target_bone;
      b->ik_loop_count = (i32)iterations;
      b->ik_limit_angle = control_weight * 4.0f;
      b->ik_link_count = (u32)ik_chain;

      mmdgltf_PmxIkLink* links =
        mmdgltf__arena_alloc(arena, sizeof(mmdgltf_PmxIkLink) * (size_t)ik_chain);
      if (!links) {
        RET_ERR(MMDGLTF_ERROR_OUT_OF_MEMORY);
      }
      for (size_t j = 0; j < (size_t)ik_chain; ++j) {
        const u16 cb = child_bones[j];
        links[j].bone_index = (i32)cb;
        if ((size_t)cb < pmx->bone_count && is_knee_bone(pmx->bones[cb].name_jp)) {
          links[j].has_limits = 1;
          links[j].min_angle = (mmdgltf_Vec3f){-3.14159265f, 0.0f, 0.0f};
          links[j].max_angle = (mmdgltf_Vec3f){-0.00872665f, 0.0f, 0.0f};
        } else {
          links[j].has_limits = 0;
          links[j].min_angle = (mmdgltf_Vec3f){0.0f, 0.0f, 0.0f};
          links[j].max_angle = (mmdgltf_Vec3f){0.0f, 0.0f, 0.0f};
        }
      }
      b->ik_links = links;
    }
  }

  const u16 morph_count = mmdgltf__stream_read_u16(&stream);
  if (stream.status != MMDGLTF_OK) {
    RET_ERR(stream.status);
  }

  PmdRawMorph* raw_morphs = NULL;
  if (morph_count > 0) {
    raw_morphs = mmdgltf__arena_alloc(arena, sizeof(PmdRawMorph) * (size_t)morph_count);
    if (!raw_morphs) {
      RET_ERR(MMDGLTF_ERROR_OUT_OF_MEMORY);
    }
    for (size_t i = 0; i < (size_t)morph_count; ++i) {
      raw_morphs[i].name_jp = mmdgltf__stream_read_string_cp932(&stream, arena, 20);
      raw_morphs[i].data_size = mmdgltf__stream_read_u32(&stream);
      raw_morphs[i].type = mmdgltf__stream_read_u8(&stream);
      if (raw_morphs[i].data_size > 0) {
        raw_morphs[i].data =
          mmdgltf__arena_alloc(arena, sizeof(PmdRawMorphItem) * (size_t)raw_morphs[i].data_size);
        if (!raw_morphs[i].data) {
          RET_ERR(MMDGLTF_ERROR_OUT_OF_MEMORY);
        }
        for (size_t j = 0; j < (size_t)raw_morphs[i].data_size; ++j) {
          raw_morphs[i].data[j].index = mmdgltf__stream_read_u32(&stream);
          raw_morphs[i].data[j].offset = mmdgltf__stream_read_vec3(&stream);
        }
      }
    }
    if (stream.status != MMDGLTF_OK) {
      RET_ERR(stream.status);
    }

    const PmdRawMorph* base_morph = NULL;
    for (size_t i = 0; i < (size_t)morph_count; ++i) {
      if (raw_morphs[i].type == 0) {
        base_morph = &raw_morphs[i];
        break;
      }
    }

    if (base_morph && morph_count > 1) {
      pmx->morph_count = (size_t)(morph_count - 1);
      pmx->morphs =
        mmdgltf__arena_alloc(arena, sizeof(mmdgltf_PmxMorph) * (size_t)(morph_count - 1));
      if (!pmx->morphs) {
        RET_ERR(MMDGLTF_ERROR_OUT_OF_MEMORY);
      }
      memset(pmx->morphs, 0, sizeof(mmdgltf_PmxMorph) * (size_t)(morph_count - 1));

      size_t out_m = 0;
      for (size_t i = 0; i < (size_t)morph_count; ++i) {
        if (raw_morphs[i].type == 0) {
          continue;
        }
        mmdgltf_PmxMorph* m = &pmx->morphs[out_m++];
        m->name_jp = raw_morphs[i].name_jp;
        m->name_en = "";
        m->type = MMDGLTF_MORPH_VERTEX;
        m->category = (mmdgltf_MorphCategory)raw_morphs[i].type;
        m->offset_count = raw_morphs[i].data_size;

        if (m->offset_count > 0) {
          mmdgltf_PmxVertexMorphOffset* voffsets = mmdgltf__arena_alloc(
            arena, sizeof(mmdgltf_PmxVertexMorphOffset) * (size_t)m->offset_count);
          if (!voffsets) {
            RET_ERR(MMDGLTF_ERROR_OUT_OF_MEMORY);
          }
          for (size_t j = 0; j < (size_t)m->offset_count; ++j) {
            const u32 base_idx = raw_morphs[i].data[j].index;
            if (base_idx < base_morph->data_size) {
              voffsets[j].vertex_index = base_morph->data[base_idx].index;
              voffsets[j].position_offset = raw_morphs[i].data[j].offset;
            } else {
              voffsets[j].vertex_index = 0;
              voffsets[j].position_offset = (mmdgltf_Vec3f){0.0f, 0.0f, 0.0f};
            }
          }
          m->vertex_offsets = voffsets;
        }
      }
    }
  }

  const u8 facial_count = mmdgltf__stream_read_u8(&stream);
  for (size_t i = 0; i < (size_t)facial_count; ++i) {
    mmdgltf__stream_read_u16(&stream);
  }

  const u8 bone_disp_count = mmdgltf__stream_read_u8(&stream);
  char** bone_disp_names = NULL;
  if (bone_disp_count > 0) {
    bone_disp_names = mmdgltf__arena_alloc(arena, sizeof(char*) * (size_t)bone_disp_count);
    for (size_t i = 0; i < (size_t)bone_disp_count; ++i) {
      bone_disp_names[i] = mmdgltf__stream_read_string_cp932(&stream, arena, 50);
    }
  }

  const u32 bone_elem_count = mmdgltf__stream_read_u32(&stream);
  for (size_t i = 0; i < (size_t)bone_elem_count; ++i) {
    mmdgltf__stream_read_u16(&stream);
    mmdgltf__stream_read_u8(&stream);
  }

  if (stream.status != MMDGLTF_OK) {
    RET_ERR(stream.status);
  }

  u8 eng_flag = 0;
  if (stream.func.read(&eng_flag, 1, 1, stream.src) == 1) {
    if (eng_flag == 1) {
      pmx->model_info.name_en = mmdgltf__stream_read_string_cp932(&stream, arena, 20);
      pmx->model_info.comment_en = mmdgltf__stream_read_string_cp932(&stream, arena, 256);

      for (size_t i = 0; i < pmx->bone_count; ++i) {
        pmx->bones[i].name_en = mmdgltf__stream_read_string_cp932(&stream, arena, 20);
      }

      for (size_t i = 0; i < pmx->morph_count; ++i) {
        pmx->morphs[i].name_en = mmdgltf__stream_read_string_cp932(&stream, arena, 20);
      }

      for (size_t i = 0; i < (size_t)bone_disp_count; ++i) {
        mmdgltf__stream_read_string_cp932(&stream, arena, 50);
      }
    }

    for (size_t i = 0; i < 10; ++i) {
      mmdgltf__stream_read_string_cp932(&stream, arena, 100);
    }

    u32 rigid_count = 0;
    if (stream.func.read(&rigid_count, 1, 4, stream.src) == 4) {
      pmx->rigid_body_count = (size_t)rigid_count;
      if (rigid_count > 0) {
        pmx->rigid_bodies =
          mmdgltf__arena_alloc(arena, sizeof(mmdgltf_PmxRigidBody) * (size_t)rigid_count);
        if (!pmx->rigid_bodies) {
          RET_ERR(MMDGLTF_ERROR_OUT_OF_MEMORY);
        }
        memset(pmx->rigid_bodies, 0, sizeof(mmdgltf_PmxRigidBody) * (size_t)rigid_count);

        for (size_t i = 0; i < (size_t)rigid_count; ++i) {
          mmdgltf_PmxRigidBody* r = &pmx->rigid_bodies[i];
          r->name_jp = mmdgltf__stream_read_string_cp932(&stream, arena, 20);
          r->name_en = "";
          const u16 rb_bone = mmdgltf__stream_read_u16(&stream);
          r->bone_index = (rb_bone == 0xFFFF) ? -1 : (i32)rb_bone;
          r->collision_group = mmdgltf__stream_read_u8(&stream);
          r->collision_mask = mmdgltf__stream_read_u16(&stream);
          r->shape_type = (mmdgltf_RigidShapeType)mmdgltf__stream_read_u8(&stream);
          r->size = mmdgltf__stream_read_vec3(&stream);
          r->position = mmdgltf__stream_read_vec3(&stream);
          r->rotation = mmdgltf__stream_read_vec3(&stream);
          r->mass = mmdgltf__stream_read_f32(&stream);
          r->linear_damping = mmdgltf__stream_read_f32(&stream);
          r->angular_damping = mmdgltf__stream_read_f32(&stream);
          r->restitution = mmdgltf__stream_read_f32(&stream);
          r->friction = mmdgltf__stream_read_f32(&stream);
          r->physics_mode = (mmdgltf_RigidPhysicsMode)mmdgltf__stream_read_u8(&stream);

          if (r->bone_index >= 0 && (size_t)r->bone_index < pmx->bone_count) {
            const mmdgltf_Vec3f bpos = pmx->bones[r->bone_index].position;
            r->position.x += bpos.x;
            r->position.y += bpos.y;
            r->position.z += bpos.z;
          }
        }
      }

      u32 joint_count = 0;
      if (stream.func.read(&joint_count, 1, 4, stream.src) == 4) {
        pmx->joint_count = (size_t)joint_count;
        if (joint_count > 0) {
          pmx->joints =
            mmdgltf__arena_alloc(arena, sizeof(mmdgltf_PmxJoint) * (size_t)joint_count);
          if (!pmx->joints) {
            RET_ERR(MMDGLTF_ERROR_OUT_OF_MEMORY);
          }
          memset(pmx->joints, 0, sizeof(mmdgltf_PmxJoint) * (size_t)joint_count);

          for (size_t i = 0; i < (size_t)joint_count; ++i) {
            mmdgltf_PmxJoint* j = &pmx->joints[i];
            j->name_jp = mmdgltf__stream_read_string_cp932(&stream, arena, 20);
            j->name_en = "";
            j->type = 0;
            j->src_rigid = (i32)mmdgltf__stream_read_u32(&stream);
            j->dest_rigid = (i32)mmdgltf__stream_read_u32(&stream);
            j->position = mmdgltf__stream_read_vec3(&stream);
            j->rotation = mmdgltf__stream_read_vec3(&stream);
            j->min_position = mmdgltf__stream_read_vec3(&stream);
            j->max_position = mmdgltf__stream_read_vec3(&stream);
            j->min_rotation = mmdgltf__stream_read_vec3(&stream);
            j->max_rotation = mmdgltf__stream_read_vec3(&stream);
            j->spring_position = mmdgltf__stream_read_vec3(&stream);
            j->spring_rotation = mmdgltf__stream_read_vec3(&stream);
          }
        }
      }
    }
  }

  pmx->texture_count = texture_list.count;
  pmx->textures = texture_list.items;

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
