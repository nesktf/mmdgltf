#include "mmdgltf_internal.h"
#include "tinygltf/tiny_gltf_v3.h"

#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char* extract_dir(mmdgltf_Arena* arena, const char* path, size_t len) {
  if (!path || len == 0) {
    return NULL;
  }
  size_t last_slash = len;
  for (size_t i = len; i > 0; --i) {
    if (path[i - 1] == '/' || path[i - 1] == '\\') {
      last_slash = i - 1;
      break;
    }
  }
  if (last_slash == len) {
    return NULL;
  }
  return mmdgltf__arena_strndup(arena, path, last_slash);
}

static mmdgltf_Status convert_stream(mmdgltf_Stream* stream, const char* base_dir,
                                     const mmdgltf_ExportConfig* config, const char* filename,
                                     u32 filename_len, u8** out_data, size_t* out_size,
                                     bool write_binary) {
  mmdgltf_Status err = MMDGLTF_OK;
  mmdgltf_Arena* arena = NULL;
  mmdgltf__ByteVec bw;
  memset(&bw, 0, sizeof(bw));

#define RET_ERR(_err) \
  err = _err;         \
  goto on_convert_end

  mmdgltf_ExportConfig cfg;
  if (config) {
    cfg = *config;
  } else {
    mmdgltf_export_config_default(&cfg);
  }

  u8 sig[4];
  mmdgltf__stream_read_bytes(stream, sig, 4);
  if (stream->status != MMDGLTF_OK) {
    RET_ERR(stream->status);
  }
  if (sig[0] != 'P' || sig[1] != 'M' || sig[2] != 'X' || sig[3] != ' ') {
    RET_ERR(MMDGLTF_ERROR_INVALID_SIGNATURE);
  }

  const f32 version = mmdgltf__stream_read_f32(stream);
  if (version < 2.0f || version > 2.1f) {
    RET_ERR(MMDGLTF_ERROR_UNSUPPORTED_VERSION);
  }

  const u8 globals_count = mmdgltf__stream_read_u8(stream);
  if (globals_count < 8) {
    RET_ERR(MMDGLTF_ERROR_CORRUPT_DATA);
  }

  u8 globals[8];
  mmdgltf__stream_read_bytes(stream, globals, 8);
  for (u8 i = 8; i < globals_count; ++i) {
    mmdgltf__stream_read_u8(stream);
  }
  if (stream->status != MMDGLTF_OK) {
    RET_ERR(stream->status);
  }

  const mmdgltf_TextEncoding encoding = (mmdgltf_TextEncoding)globals[0];
  const u8 extra_uvs = globals[1];
  const u8 vtx_idx_size = globals[2];
  const u8 tex_idx_size = globals[3];
  const u8 mat_idx_size = globals[4];
  const u8 bone_idx_size = globals[5];
  const u8 morph_idx_size = globals[6];
  const u8 rigid_idx_size = globals[7];

  arena = mmdgltf__arena_create(256 * 1024);
  if (!arena) {
    RET_ERR(MMDGLTF_ERROR_OUT_OF_MEMORY);
  }

  mmdgltf__stream_read_string(stream, arena, encoding);
  mmdgltf__stream_read_string(stream, arena, encoding);
  mmdgltf__stream_read_string(stream, arena, encoding);
  mmdgltf__stream_read_string(stream, arena, encoding);
  if (stream->status != MMDGLTF_OK) {
    RET_ERR(stream->status);
  }

  const i32 vertex_count = mmdgltf__stream_read_i32(stream);
  if (stream->status != MMDGLTF_OK || vertex_count < 0) {
    RET_ERR(stream->status != MMDGLTF_OK ? stream->status : MMDGLTF_ERROR_CORRUPT_DATA);
  }

  const size_t vcount = (size_t)vertex_count;
  const bool has_skinning = cfg.export_skinning != 0;
  const f32 scale = cfg.scale > 0.0f ? cfg.scale : 1.0f;

  const size_t pos_offset = mmdgltf__bytevec_append(&bw, NULL, vcount * 3 * sizeof(f32));
  const size_t norm_offset = mmdgltf__bytevec_append(&bw, NULL, vcount * 3 * sizeof(f32));
  const size_t uv_offset = mmdgltf__bytevec_append(&bw, NULL, vcount * 2 * sizeof(f32));

  size_t joints_offset = 0;
  size_t weights_offset = 0;
  if (has_skinning) {
    joints_offset =
      mmdgltf__bytevec_append(&bw, NULL, vcount * MMDGLTF_MAX_BONE_WEIGHTS * sizeof(u16));
    weights_offset =
      mmdgltf__bytevec_append(&bw, NULL, vcount * MMDGLTF_MAX_BONE_WEIGHTS * sizeof(f32));
  }

  f32* dst_pos = (f32*)(bw.data + pos_offset);
  f32* dst_norm = (f32*)(bw.data + norm_offset);
  f32* dst_uv = (f32*)(bw.data + uv_offset);
  u16* dst_joints = has_skinning ? (u16*)(bw.data + joints_offset) : NULL;
  f32* dst_weights = has_skinning ? (f32*)(bw.data + weights_offset) : NULL;

  f64 pos_min[3] = {DBL_MAX, DBL_MAX, DBL_MAX};
  f64 pos_max[3] = {-DBL_MAX, -DBL_MAX, -DBL_MAX};

  for (size_t i = 0; i < vcount; ++i) {
    const mmdgltf_Vec3f p = mmdgltf__stream_read_vec3(stream);
    const mmdgltf_Vec3f n = mmdgltf__stream_read_vec3(stream);
    const mmdgltf_Vec2f uv = mmdgltf__stream_read_vec2(stream);

    const f32 px = p.x * scale;
    const f32 py = p.y * scale;
    const f32 pz = -p.z * scale;

    dst_pos[i * 3 + 0] = px;
    dst_pos[i * 3 + 1] = py;
    dst_pos[i * 3 + 2] = pz;

    if (px < pos_min[0])
      pos_min[0] = px;
    if (py < pos_min[1])
      pos_min[1] = py;
    if (pz < pos_min[2])
      pos_min[2] = pz;
    if (px > pos_max[0])
      pos_max[0] = px;
    if (py > pos_max[1])
      pos_max[1] = py;
    if (pz > pos_max[2])
      pos_max[2] = pz;

    dst_norm[i * 3 + 0] = n.x;
    dst_norm[i * 3 + 1] = n.y;
    dst_norm[i * 3 + 2] = -n.z;

    dst_uv[i * 2 + 0] = uv.x;
    dst_uv[i * 2 + 1] = uv.y;

    for (u8 u = 0; u < extra_uvs; ++u) {
      mmdgltf__stream_read_vec4(stream);
    }

    const u8 weight_type = mmdgltf__stream_read_u8(stream);
    u16 b_idx[MMDGLTF_MAX_BONE_WEIGHTS] = {0};
    f32 b_wt[MMDGLTF_MAX_BONE_WEIGHTS] = {0.0f};

    if (weight_type == MMDGLTF_WEIGHT_BDEF1) {
      const i32 b0 = mmdgltf__stream_read_index(stream, bone_idx_size);
      b_idx[0] = b0 >= 0 ? (u16)b0 : 0;
      b_wt[0] = 1.0f;
    } else if (weight_type == MMDGLTF_WEIGHT_BDEF2) {
      const i32 b0 = mmdgltf__stream_read_index(stream, bone_idx_size);
      const i32 b1 = mmdgltf__stream_read_index(stream, bone_idx_size);
      const f32 w0 = mmdgltf__stream_read_f32(stream);
      b_idx[0] = b0 >= 0 ? (u16)b0 : 0;
      b_idx[1] = b1 >= 0 ? (u16)b1 : 0;
      b_wt[0] = w0;
      b_wt[1] = 1.0f - w0;
    } else if (weight_type == MMDGLTF_WEIGHT_BDEF4 || weight_type == MMDGLTF_WEIGHT_QDEF) {
      for (size_t b = 0; b < 4; ++b) {
        const i32 bi = mmdgltf__stream_read_index(stream, bone_idx_size);
        if (b < MMDGLTF_MAX_BONE_WEIGHTS) {
          b_idx[b] = bi >= 0 ? (u16)bi : 0;
        }
      }
      for (size_t b = 0; b < 4; ++b) {
        const f32 wi = mmdgltf__stream_read_f32(stream);
        if (b < MMDGLTF_MAX_BONE_WEIGHTS) {
          b_wt[b] = wi;
        }
      }
    } else if (weight_type == MMDGLTF_WEIGHT_SDEF) {
      const i32 b0 = mmdgltf__stream_read_index(stream, bone_idx_size);
      const i32 b1 = mmdgltf__stream_read_index(stream, bone_idx_size);
      const f32 w0 = mmdgltf__stream_read_f32(stream);
      b_idx[0] = b0 >= 0 ? (u16)b0 : 0;
      b_idx[1] = b1 >= 0 ? (u16)b1 : 0;
      b_wt[0] = w0;
      b_wt[1] = 1.0f - w0;

      mmdgltf__stream_read_vec3(stream);
      mmdgltf__stream_read_vec3(stream);
      mmdgltf__stream_read_vec3(stream);
    } else {
      RET_ERR(MMDGLTF_ERROR_CORRUPT_DATA);
    }

    mmdgltf__stream_read_f32(stream); // edge_scale

    if (has_skinning) {
      for (size_t b = 0; b < MMDGLTF_MAX_BONE_WEIGHTS; ++b) {
        dst_joints[i * MMDGLTF_MAX_BONE_WEIGHTS + b] = b_idx[b];
        dst_weights[i * MMDGLTF_MAX_BONE_WEIGHTS + b] = b_wt[b];
      }
    }

    if (stream->status != MMDGLTF_OK) {
      RET_ERR(stream->status);
    }
  }

  const i32 surface_count = mmdgltf__stream_read_i32(stream);
  if (stream->status != MMDGLTF_OK || surface_count < 0 || (surface_count % 3 != 0)) {
    RET_ERR(stream->status != MMDGLTF_OK ? stream->status : MMDGLTF_ERROR_CORRUPT_DATA);
  }

  const size_t index_count = (size_t)surface_count;
  const bool use_u16 = (vcount <= 65535);
  const size_t idx_elem_sz = use_u16 ? sizeof(u16) : sizeof(u32);
  const size_t indices_offset = mmdgltf__bytevec_append(&bw, NULL, index_count * idx_elem_sz);

  u16* dst_idx16 = use_u16 ? (u16*)(bw.data + indices_offset) : NULL;
  u32* dst_idx32 = !use_u16 ? (u32*)(bw.data + indices_offset) : NULL;

  for (size_t k = 0; k < index_count; k += 3) {
    const u32 i0 = mmdgltf__stream_read_vertex_index(stream, vtx_idx_size);
    const u32 i1 = mmdgltf__stream_read_vertex_index(stream, vtx_idx_size);
    const u32 i2 = mmdgltf__stream_read_vertex_index(stream, vtx_idx_size);

    if (i0 >= vcount || i1 >= vcount || i2 >= vcount) {
      RET_ERR(MMDGLTF_ERROR_CORRUPT_DATA);
    }

    if (use_u16) {
      dst_idx16[k + 0] = (u16)i0;
      dst_idx16[k + 1] = (u16)i2;
      dst_idx16[k + 2] = (u16)i1;
    } else {
      dst_idx32[k + 0] = i0;
      dst_idx32[k + 1] = i2;
      dst_idx32[k + 2] = i1;
    }
  }

  if (stream->status != MMDGLTF_OK) {
    RET_ERR(stream->status);
  }

  const i32 texture_count = mmdgltf__stream_read_i32(stream);
  if (stream->status != MMDGLTF_OK || texture_count < 0) {
    RET_ERR(stream->status != MMDGLTF_OK ? stream->status : MMDGLTF_ERROR_CORRUPT_DATA);
  }

  const size_t tex_count = (size_t)texture_count;
  char** tex_paths = mmdgltf__arena_alloc(arena, tex_count * sizeof(char*));
  for (size_t i = 0; i < tex_count; ++i) {
    char* path = mmdgltf__stream_read_string(stream, arena, encoding);
    if (path) {
      for (char* p = path; *p; ++p) {
        if (*p == '\\')
          *p = '/';
      }
    }
    tex_paths[i] = path;
  }

  mmdgltf_Image* images = mmdgltf__arena_alloc(arena, tex_count * sizeof(mmdgltf_Image));
  memset(images, 0, tex_count * sizeof(mmdgltf_Image));
  for (size_t i = 0; i < tex_count; ++i) {
    if (tex_paths[i] && tex_paths[i][0] != '\0') {
      mmdgltf_Image img;
      memset(&img, 0, sizeof(img));
      if (mmdgltf__image_load(base_dir, tex_paths[i], arena, &img) == MMDGLTF_OK) {
        images[i] = img;
      }
    }
  }

  const i32 material_count = mmdgltf__stream_read_i32(stream);
  if (stream->status != MMDGLTF_OK || material_count < 0) {
    RET_ERR(stream->status != MMDGLTF_OK ? stream->status : MMDGLTF_ERROR_CORRUPT_DATA);
  }

  const size_t mat_count = (size_t)material_count;
  mmdgltf_PmxMaterial* materials =
    mmdgltf__arena_alloc(arena, mat_count * sizeof(mmdgltf_PmxMaterial));
  size_t total_mat_indices = 0;

  for (size_t i = 0; i < mat_count; ++i) {
    mmdgltf_PmxMaterial* m = &materials[i];
    memset(m, 0, sizeof(*m));

    m->name_jp = mmdgltf__stream_read_string(stream, arena, encoding);
    m->name_en = mmdgltf__stream_read_string(stream, arena, encoding);
    m->diffuse = mmdgltf__stream_read_color4(stream);
    m->specular = mmdgltf__stream_read_color3(stream);
    m->shininess = mmdgltf__stream_read_f32(stream);
    m->ambient = mmdgltf__stream_read_color3(stream);
    m->draw_flags = mmdgltf__stream_read_u8(stream);
    m->edge_color = mmdgltf__stream_read_color4(stream);
    m->edge_size = mmdgltf__stream_read_f32(stream);
    m->texture_index = mmdgltf__stream_read_index(stream, tex_idx_size);
    m->sphere_texture_index = mmdgltf__stream_read_index(stream, tex_idx_size);
    m->sphere_mode = (mmdgltf_SphereMode)mmdgltf__stream_read_u8(stream);
    m->toon_sharing_flag = mmdgltf__stream_read_u8(stream);
    if (m->toon_sharing_flag == 1) {
      m->toon_index = (i32)mmdgltf__stream_read_u8(stream);
    } else {
      m->toon_index = mmdgltf__stream_read_index(stream, tex_idx_size);
    }
    m->comment = mmdgltf__stream_read_string(stream, arena, encoding);

    const i32 mat_surface_count = mmdgltf__stream_read_i32(stream);
    if (mat_surface_count < 0 || (mat_surface_count % 3 != 0)) {
      RET_ERR(MMDGLTF_ERROR_CORRUPT_DATA);
    }
    m->index_count = (u32)mat_surface_count;
    total_mat_indices += (size_t)mat_surface_count;
  }

  if (stream->status != MMDGLTF_OK || total_mat_indices != index_count) {
    RET_ERR(stream->status != MMDGLTF_OK ? stream->status : MMDGLTF_ERROR_CORRUPT_DATA);
  }

  const i32 bone_count = mmdgltf__stream_read_i32(stream);
  if (stream->status != MMDGLTF_OK || bone_count < 0) {
    RET_ERR(stream->status != MMDGLTF_OK ? stream->status : MMDGLTF_ERROR_CORRUPT_DATA);
  }

  const size_t b_count = (size_t)bone_count;
  mmdgltf_PmxBone* bones = mmdgltf__arena_alloc(arena, b_count * sizeof(mmdgltf_PmxBone));

  for (size_t i = 0; i < b_count; ++i) {
    mmdgltf_PmxBone* b = &bones[i];
    memset(b, 0, sizeof(*b));

    b->name_jp = mmdgltf__stream_read_string(stream, arena, encoding);
    b->name_en = mmdgltf__stream_read_string(stream, arena, encoding);

    const mmdgltf_Vec3f bpos = mmdgltf__stream_read_vec3(stream);
    b->position.x = bpos.x * scale;
    b->position.y = bpos.y * scale;
    b->position.z = -bpos.z * scale;

    b->parent_index = mmdgltf__stream_read_index(stream, bone_idx_size);
    b->transform_order = mmdgltf__stream_read_i32(stream);
    b->flags = mmdgltf__stream_read_u16(stream);

    if (b->flags & MMDGLTF_BONE_CONNECTION_TARGET_BONE) {
      b->connection_bone_index = mmdgltf__stream_read_index(stream, bone_idx_size);
    } else {
      const mmdgltf_Vec3f conn = mmdgltf__stream_read_vec3(stream);
      b->offset.x = conn.x * scale;
      b->offset.y = conn.y * scale;
      b->offset.z = -conn.z * scale;
    }

    if (b->flags & (MMDGLTF_BONE_APPEND_ROTATION | MMDGLTF_BONE_APPEND_TRANSLATION)) {
      b->inherit_bone_index = mmdgltf__stream_read_index(stream, bone_idx_size);
      b->inherit_weight = mmdgltf__stream_read_f32(stream);
    }

    if (b->flags & MMDGLTF_BONE_FIXED_AXIS) {
      b->fixed_axis = mmdgltf__stream_read_vec3(stream);
    }

    if (b->flags & MMDGLTF_BONE_LOCAL_AXIS) {
      b->local_x = mmdgltf__stream_read_vec3(stream);
      b->local_z = mmdgltf__stream_read_vec3(stream);
    }

    if (b->flags & MMDGLTF_BONE_EXTERNAL_PARENT) {
      b->external_parent_key = mmdgltf__stream_read_i32(stream);
    }

    if (b->flags & MMDGLTF_BONE_IK) {
      b->ik_target_index = mmdgltf__stream_read_index(stream, bone_idx_size);
      b->ik_loop_count = mmdgltf__stream_read_i32(stream);
      b->ik_limit_angle = mmdgltf__stream_read_f32(stream);

      const i32 link_count = mmdgltf__stream_read_i32(stream);
      b->ik_link_count = link_count > 0 ? (u32)link_count : 0;
      if (link_count > 0) {
        mmdgltf_PmxIkLink* links =
          mmdgltf__arena_alloc(arena, (size_t)link_count * sizeof(mmdgltf_PmxIkLink));
        for (i32 j = 0; j < link_count; ++j) {
          links[j].bone_index = mmdgltf__stream_read_index(stream, bone_idx_size);
          links[j].has_limits = mmdgltf__stream_read_u8(stream);
          if (links[j].has_limits) {
            links[j].min_angle = mmdgltf__stream_read_vec3(stream);
            links[j].max_angle = mmdgltf__stream_read_vec3(stream);
          }
        }
        b->ik_links = links;
      }
    }
  }

  size_t ibm_offset = 0;
  if (has_skinning && b_count > 0) {
    f32* ibm_data = mmdgltf__arena_alloc(arena, b_count * 16 * sizeof(f32));
    for (size_t i = 0; i < b_count; ++i) {
      f32* m = &ibm_data[i * 16];
      memset(m, 0, 16 * sizeof(f32));
      m[0] = 1.0f;
      m[5] = 1.0f;
      m[10] = 1.0f;
      m[15] = 1.0f;
      m[12] = -bones[i].position.x;
      m[13] = -bones[i].position.y;
      m[14] = -bones[i].position.z;
    }
    ibm_offset = mmdgltf__bytevec_append(&bw, ibm_data, b_count * 16 * sizeof(f32));
  }

  const i32 morph_count = mmdgltf__stream_read_i32(stream);
  if (stream->status != MMDGLTF_OK || morph_count < 0) {
    RET_ERR(stream->status != MMDGLTF_OK ? stream->status : MMDGLTF_ERROR_CORRUPT_DATA);
  }

  const size_t m_count = (size_t)morph_count;
  mmdgltf_PmxMorph* morphs = mmdgltf__arena_alloc(arena, m_count * sizeof(mmdgltf_PmxMorph));

  for (size_t i = 0; i < m_count; ++i) {
    mmdgltf_PmxMorph* m = &morphs[i];
    memset(m, 0, sizeof(*m));

    m->name_jp = mmdgltf__stream_read_string(stream, arena, encoding);
    m->name_en = mmdgltf__stream_read_string(stream, arena, encoding);
    m->category = (mmdgltf_MorphCategory)mmdgltf__stream_read_u8(stream);
    m->type = (mmdgltf_MorphType)mmdgltf__stream_read_u8(stream);

    const i32 offset_count = mmdgltf__stream_read_i32(stream);
    if (offset_count < 0) {
      RET_ERR(MMDGLTF_ERROR_CORRUPT_DATA);
    }
    m->offset_count = (u32)offset_count;

    if (m->type == MMDGLTF_MORPH_VERTEX) {
      mmdgltf_PmxVertexMorphOffset* voffsets =
        mmdgltf__arena_alloc(arena, (size_t)offset_count * sizeof(mmdgltf_PmxVertexMorphOffset));
      for (i32 j = 0; j < offset_count; ++j) {
        voffsets[j].vertex_index = mmdgltf__stream_read_vertex_index(stream, vtx_idx_size);
        voffsets[j].position_offset = mmdgltf__stream_read_vec3(stream);
      }
      m->vertex_offsets = voffsets;
    } else if (m->type >= MMDGLTF_MORPH_UV && m->type <= MMDGLTF_MORPH_EXTRA_UV4) {
      for (i32 j = 0; j < offset_count; ++j) {
        mmdgltf__stream_read_vertex_index(stream, vtx_idx_size);
        mmdgltf__stream_read_vec4(stream);
      }
    } else if (m->type == MMDGLTF_MORPH_BONE) {
      for (i32 j = 0; j < offset_count; ++j) {
        mmdgltf__stream_read_index(stream, bone_idx_size);
        mmdgltf__stream_read_vec3(stream);
        mmdgltf__stream_read_quat(stream);
      }
    } else if (m->type == MMDGLTF_MORPH_MATERIAL) {
      for (i32 j = 0; j < offset_count; ++j) {
        mmdgltf__stream_read_index(stream, mat_idx_size);
        mmdgltf__stream_read_u8(stream);
        mmdgltf__stream_read_color4(stream);
        mmdgltf__stream_read_color3(stream);
        mmdgltf__stream_read_f32(stream);
        mmdgltf__stream_read_color3(stream);
        mmdgltf__stream_read_color4(stream);
        mmdgltf__stream_read_f32(stream);
        mmdgltf__stream_read_color4(stream);
        mmdgltf__stream_read_color4(stream);
        mmdgltf__stream_read_color4(stream);
      }
    } else if (m->type == MMDGLTF_MORPH_GROUP || m->type == MMDGLTF_MORPH_FLIP) {
      for (i32 j = 0; j < offset_count; ++j) {
        mmdgltf__stream_read_index(stream, morph_idx_size);
        mmdgltf__stream_read_f32(stream);
      }
    } else if (m->type == MMDGLTF_MORPH_IMPULSE) {
      for (i32 j = 0; j < offset_count; ++j) {
        mmdgltf__stream_read_index(stream, rigid_idx_size);
        mmdgltf__stream_read_u8(stream);
        mmdgltf__stream_read_vec3(stream);
        mmdgltf__stream_read_vec3(stream);
      }
    }
  }

  const i32 disp_count = mmdgltf__stream_read_i32(stream);
  if (disp_count > 0) {
    for (i32 i = 0; i < disp_count; ++i) {
      mmdgltf__stream_read_string(stream, arena, encoding);
      mmdgltf__stream_read_string(stream, arena, encoding);
      mmdgltf__stream_read_u8(stream);
      const i32 elem_count = mmdgltf__stream_read_i32(stream);
      for (i32 j = 0; j < elem_count; ++j) {
        const u8 etype = mmdgltf__stream_read_u8(stream);
        if (etype == 0) {
          mmdgltf__stream_read_index(stream, bone_idx_size);
        } else {
          mmdgltf__stream_read_index(stream, morph_idx_size);
        }
      }
    }
  }

  const i32 rigid_count = mmdgltf__stream_read_i32(stream);
  const size_t rb_count = (rigid_count > 0) ? (size_t)rigid_count : 0;
  mmdgltf_PmxRigidBody* rigid_bodies =
    rb_count > 0 ? mmdgltf__arena_alloc(arena, rb_count * sizeof(mmdgltf_PmxRigidBody)) : NULL;

  for (size_t i = 0; i < rb_count; ++i) {
    mmdgltf_PmxRigidBody* r = &rigid_bodies[i];
    memset(r, 0, sizeof(*r));

    r->name_jp = mmdgltf__stream_read_string(stream, arena, encoding);
    r->name_en = mmdgltf__stream_read_string(stream, arena, encoding);
    r->bone_index = mmdgltf__stream_read_index(stream, bone_idx_size);
    r->collision_group = mmdgltf__stream_read_u8(stream);
    r->collision_mask = mmdgltf__stream_read_u16(stream);
    r->shape_type = (mmdgltf_RigidShapeType)mmdgltf__stream_read_u8(stream);
    r->size = mmdgltf__stream_read_vec3(stream);
    r->position = mmdgltf__stream_read_vec3(stream);
    r->rotation = mmdgltf__stream_read_vec3(stream);
    r->mass = mmdgltf__stream_read_f32(stream);
    r->linear_damping = mmdgltf__stream_read_f32(stream);
    r->angular_damping = mmdgltf__stream_read_f32(stream);
    r->restitution = mmdgltf__stream_read_f32(stream);
    r->friction = mmdgltf__stream_read_f32(stream);
    r->physics_mode = (mmdgltf_RigidPhysicsMode)mmdgltf__stream_read_u8(stream);
  }

  const i32 joint_count = mmdgltf__stream_read_i32(stream);
  const size_t jt_count = (joint_count > 0) ? (size_t)joint_count : 0;
  mmdgltf_PmxJoint* joints =
    jt_count > 0 ? mmdgltf__arena_alloc(arena, jt_count * sizeof(mmdgltf_PmxJoint)) : NULL;

  for (size_t i = 0; i < jt_count; ++i) {
    mmdgltf_PmxJoint* j = &joints[i];
    memset(j, 0, sizeof(*j));

    j->name_jp = mmdgltf__stream_read_string(stream, arena, encoding);
    j->name_en = mmdgltf__stream_read_string(stream, arena, encoding);
    j->type = mmdgltf__stream_read_u8(stream);
    j->src_rigid = mmdgltf__stream_read_index(stream, rigid_idx_size);
    j->dest_rigid = mmdgltf__stream_read_index(stream, rigid_idx_size);
    j->position = mmdgltf__stream_read_vec3(stream);
    j->rotation = mmdgltf__stream_read_vec3(stream);
    j->min_position = mmdgltf__stream_read_vec3(stream);
    j->max_position = mmdgltf__stream_read_vec3(stream);
    j->min_rotation = mmdgltf__stream_read_vec3(stream);
    j->max_rotation = mmdgltf__stream_read_vec3(stream);
    j->spring_position = mmdgltf__stream_read_vec3(stream);
    j->spring_rotation = mmdgltf__stream_read_vec3(stream);
  }

  size_t vertex_morph_count = 0;
  size_t* vertex_morph_indices = NULL;
  if (cfg.export_morphs && m_count > 0) {
    vertex_morph_indices = mmdgltf__arena_alloc(arena, m_count * sizeof(size_t));
    for (size_t i = 0; i < m_count; ++i) {
      if (morphs[i].type == MMDGLTF_MORPH_VERTEX) {
        vertex_morph_indices[vertex_morph_count++] = i;
      }
    }
  }

  size_t* morph_offsets = NULL;
  f64** morph_mins = NULL;
  f64** morph_maxs = NULL;
  if (vertex_morph_count > 0) {
    morph_offsets = mmdgltf__arena_alloc(arena, vertex_morph_count * sizeof(size_t));
    morph_mins = mmdgltf__arena_alloc(arena, vertex_morph_count * sizeof(f64*));
    morph_maxs = mmdgltf__arena_alloc(arena, vertex_morph_count * sizeof(f64*));

    for (size_t m = 0; m < vertex_morph_count; ++m) {
      const mmdgltf_PmxMorph* morph = &morphs[vertex_morph_indices[m]];
      f32* deltas = mmdgltf__arena_alloc(arena, vcount * 3 * sizeof(f32));
      memset(deltas, 0, vcount * 3 * sizeof(f32));

      f64 min_v[3] = {0.0, 0.0, 0.0};
      f64 max_v[3] = {0.0, 0.0, 0.0};

      for (u32 o = 0; o < morph->offset_count; ++o) {
        const mmdgltf_PmxVertexMorphOffset* off = &morph->vertex_offsets[o];
        if (off->vertex_index < vcount) {
          const size_t vidx = off->vertex_index * 3;
          const f32 dx = off->position_offset.x * scale;
          const f32 dy = off->position_offset.y * scale;
          const f32 dz = -off->position_offset.z * scale;
          deltas[vidx + 0] = dx;
          deltas[vidx + 1] = dy;
          deltas[vidx + 2] = dz;

          if (dx < min_v[0])
            min_v[0] = dx;
          if (dy < min_v[1])
            min_v[1] = dy;
          if (dz < min_v[2])
            min_v[2] = dz;
          if (dx > max_v[0])
            max_v[0] = dx;
          if (dy > max_v[1])
            max_v[1] = dy;
          if (dz > max_v[2])
            max_v[2] = dz;
        }
      }

      morph_offsets[m] = mmdgltf__bytevec_append(&bw, deltas, vcount * 3 * sizeof(f32));
      f64* pmin = mmdgltf__arena_alloc(arena, 3 * sizeof(f64));
      f64* pmax = mmdgltf__arena_alloc(arena, 3 * sizeof(f64));
      memcpy(pmin, min_v, 3 * sizeof(f64));
      memcpy(pmax, max_v, 3 * sizeof(f64));
      morph_mins[m] = pmin;
      morph_maxs[m] = pmax;
    }
  }

  size_t* image_bv_offsets = NULL;
  i32* image_to_bv = NULL;
  if (tex_count > 0) {
    image_bv_offsets = mmdgltf__arena_alloc(arena, tex_count * sizeof(size_t));
    image_to_bv = mmdgltf__arena_alloc(arena, tex_count * sizeof(i32));
    for (size_t i = 0; i < tex_count; ++i) {
      if (images[i].data && images[i].size > 0) {
        image_bv_offsets[i] = mmdgltf__bytevec_append(&bw, images[i].data, images[i].size);
        image_to_bv[i] = 1;
      } else {
        image_to_bv[i] = -1;
      }
    }
  }

  size_t num_bvs = 4;
  if (has_skinning)
    num_bvs += 3;
  num_bvs += vertex_morph_count;
  for (size_t i = 0; i < tex_count; ++i) {
    if (image_to_bv && image_to_bv[i] >= 0) {
      num_bvs++;
    }
  }

  tg3_buffer_view* bvs = mmdgltf__arena_alloc(arena, num_bvs * sizeof(tg3_buffer_view));
  memset(bvs, 0, num_bvs * sizeof(tg3_buffer_view));

  u32 cur_bv = 0;
  const u32 bv_pos = cur_bv++;
  bvs[bv_pos].buffer = 0;
  bvs[bv_pos].byte_offset = pos_offset;
  bvs[bv_pos].byte_length = vcount * 3 * sizeof(f32);
  bvs[bv_pos].target = 34962;

  const u32 bv_norm = cur_bv++;
  bvs[bv_norm].buffer = 0;
  bvs[bv_norm].byte_offset = norm_offset;
  bvs[bv_norm].byte_length = vcount * 3 * sizeof(f32);
  bvs[bv_norm].target = 34962;

  const u32 bv_uv = cur_bv++;
  bvs[bv_uv].buffer = 0;
  bvs[bv_uv].byte_offset = uv_offset;
  bvs[bv_uv].byte_length = vcount * 2 * sizeof(f32);
  bvs[bv_uv].target = 34962;

  u32 bv_joints = 0;
  u32 bv_weights = 0;
  u32 bv_ibm = 0;
  if (has_skinning) {
    bv_joints = cur_bv++;
    bvs[bv_joints].buffer = 0;
    bvs[bv_joints].byte_offset = joints_offset;
    bvs[bv_joints].byte_length = vcount * MMDGLTF_MAX_BONE_WEIGHTS * sizeof(u16);
    bvs[bv_joints].target = 34962;

    bv_weights = cur_bv++;
    bvs[bv_weights].buffer = 0;
    bvs[bv_weights].byte_offset = weights_offset;
    bvs[bv_weights].byte_length = vcount * MMDGLTF_MAX_BONE_WEIGHTS * sizeof(f32);
    bvs[bv_weights].target = 34962;

    bv_ibm = cur_bv++;
    bvs[bv_ibm].buffer = 0;
    bvs[bv_ibm].byte_offset = ibm_offset;
    bvs[bv_ibm].byte_length = b_count * 16 * sizeof(f32);
  }

  const u32 bv_indices = cur_bv++;
  bvs[bv_indices].buffer = 0;
  bvs[bv_indices].byte_offset = indices_offset;
  bvs[bv_indices].byte_length = index_count * idx_elem_sz;
  bvs[bv_indices].target = 34963;

  u32* morph_bvs = NULL;
  if (vertex_morph_count > 0) {
    morph_bvs = mmdgltf__arena_alloc(arena, vertex_morph_count * sizeof(u32));
    for (size_t m = 0; m < vertex_morph_count; ++m) {
      const u32 mbv = cur_bv++;
      morph_bvs[m] = mbv;
      bvs[mbv].buffer = 0;
      bvs[mbv].byte_offset = morph_offsets[m];
      bvs[mbv].byte_length = vcount * 3 * sizeof(f32);
      bvs[mbv].target = 34962;
    }
  }

  for (size_t i = 0; i < tex_count; ++i) {
    if (image_to_bv && image_to_bv[i] >= 0) {
      const u32 img_bv = cur_bv++;
      image_to_bv[i] = (i32)img_bv;
      bvs[img_bv].buffer = 0;
      bvs[img_bv].byte_offset = image_bv_offsets[i];
      bvs[img_bv].byte_length = images[i].size;
    }
  }

  size_t num_accessors = 3 + (has_skinning ? 3 : 0) + mat_count + vertex_morph_count;
  tg3_accessor* accs = mmdgltf__arena_alloc(arena, num_accessors * sizeof(tg3_accessor));
  memset(accs, 0, num_accessors * sizeof(tg3_accessor));

  u32 cur_acc = 0;
  if (vcount == 0) {
    pos_min[0] = pos_min[1] = pos_min[2] = 0.0;
    pos_max[0] = pos_max[1] = pos_max[2] = 0.0;
  }

  f64* pos_min_p = mmdgltf__arena_alloc(arena, 3 * sizeof(f64));
  f64* pos_max_p = mmdgltf__arena_alloc(arena, 3 * sizeof(f64));
  memcpy(pos_min_p, pos_min, 3 * sizeof(f64));
  memcpy(pos_max_p, pos_max, 3 * sizeof(f64));

  const u32 acc_pos = cur_acc++;
  accs[acc_pos].buffer_view = (i32)bv_pos;
  accs[acc_pos].component_type = TG3_COMPONENT_TYPE_FLOAT;
  accs[acc_pos].count = (u32)vcount;
  accs[acc_pos].type = TG3_TYPE_VEC3;
  accs[acc_pos].min_values = pos_min_p;
  accs[acc_pos].min_values_count = 3;
  accs[acc_pos].max_values = pos_max_p;
  accs[acc_pos].max_values_count = 3;

  const u32 acc_norm = cur_acc++;
  accs[acc_norm].buffer_view = (i32)bv_norm;
  accs[acc_norm].component_type = TG3_COMPONENT_TYPE_FLOAT;
  accs[acc_norm].count = (u32)vcount;
  accs[acc_norm].type = TG3_TYPE_VEC3;

  const u32 acc_uv = cur_acc++;
  accs[acc_uv].buffer_view = (i32)bv_uv;
  accs[acc_uv].component_type = TG3_COMPONENT_TYPE_FLOAT;
  accs[acc_uv].count = (u32)vcount;
  accs[acc_uv].type = TG3_TYPE_VEC2;

  u32 acc_joints = 0;
  u32 acc_weights = 0;
  u32 acc_ibm = 0;
  if (has_skinning) {
    acc_joints = cur_acc++;
    accs[acc_joints].buffer_view = (i32)bv_joints;
    accs[acc_joints].component_type = TG3_COMPONENT_TYPE_UNSIGNED_SHORT;
    accs[acc_joints].count = (u32)vcount;
    accs[acc_joints].type = TG3_TYPE_VEC4;

    acc_weights = cur_acc++;
    accs[acc_weights].buffer_view = (i32)bv_weights;
    accs[acc_weights].component_type = TG3_COMPONENT_TYPE_FLOAT;
    accs[acc_weights].count = (u32)vcount;
    accs[acc_weights].type = TG3_TYPE_VEC4;

    acc_ibm = cur_acc++;
    accs[acc_ibm].buffer_view = (i32)bv_ibm;
    accs[acc_ibm].component_type = TG3_COMPONENT_TYPE_FLOAT;
    accs[acc_ibm].count = (u32)b_count;
    accs[acc_ibm].type = TG3_TYPE_MAT4;
  }

  u32* prim_indices_acc = mmdgltf__arena_alloc(arena, mat_count * sizeof(u32));
  size_t running_index_start = 0;
  for (size_t p = 0; p < mat_count; ++p) {
    const u32 a_idx = cur_acc++;
    prim_indices_acc[p] = a_idx;
    accs[a_idx].buffer_view = (i32)bv_indices;
    accs[a_idx].byte_offset = running_index_start * idx_elem_sz;
    accs[a_idx].component_type =
      use_u16 ? TG3_COMPONENT_TYPE_UNSIGNED_SHORT : TG3_COMPONENT_TYPE_UNSIGNED_INT;
    accs[a_idx].count = (u32)materials[p].index_count;
    accs[a_idx].type = TG3_TYPE_SCALAR;
    running_index_start += materials[p].index_count;
  }

  u32* morph_accs = NULL;
  if (vertex_morph_count > 0) {
    morph_accs = mmdgltf__arena_alloc(arena, vertex_morph_count * sizeof(u32));
    for (size_t m = 0; m < vertex_morph_count; ++m) {
      const u32 a_idx = cur_acc++;
      morph_accs[m] = a_idx;
      accs[a_idx].buffer_view = (i32)morph_bvs[m];
      accs[a_idx].component_type = TG3_COMPONENT_TYPE_FLOAT;
      accs[a_idx].count = (u32)vcount;
      accs[a_idx].type = TG3_TYPE_VEC3;
      accs[a_idx].min_values = morph_mins[m];
      accs[a_idx].min_values_count = 3;
      accs[a_idx].max_values = morph_maxs[m];
      accs[a_idx].max_values_count = 3;
    }
  }

  const u32 num_attribs = has_skinning ? 5 : 3;
  tg3_str_int_pair* shared_attribs =
    mmdgltf__arena_alloc(arena, num_attribs * sizeof(tg3_str_int_pair));
  shared_attribs[0].key = mmdgltf__tg3_make_str(arena, "POSITION");
  shared_attribs[0].value = (i32)acc_pos;
  shared_attribs[1].key = mmdgltf__tg3_make_str(arena, "NORMAL");
  shared_attribs[1].value = (i32)acc_norm;
  shared_attribs[2].key = mmdgltf__tg3_make_str(arena, "TEXCOORD_0");
  shared_attribs[2].value = (i32)acc_uv;
  if (has_skinning) {
    shared_attribs[3].key = mmdgltf__tg3_make_str(arena, "JOINTS_0");
    shared_attribs[3].value = (i32)acc_joints;
    shared_attribs[4].key = mmdgltf__tg3_make_str(arena, "WEIGHTS_0");
    shared_attribs[4].value = (i32)acc_weights;
  }

  tg3_str_int_pair** shared_targets = NULL;
  u32* shared_target_counts = NULL;
  if (vertex_morph_count > 0) {
    shared_targets = mmdgltf__arena_alloc(arena, vertex_morph_count * sizeof(tg3_str_int_pair*));
    shared_target_counts = mmdgltf__arena_alloc(arena, vertex_morph_count * sizeof(u32));
    for (size_t m = 0; m < vertex_morph_count; ++m) {
      tg3_str_int_pair* target_pair = mmdgltf__arena_alloc(arena, sizeof(tg3_str_int_pair));
      target_pair->key = mmdgltf__tg3_make_str(arena, "POSITION");
      target_pair->value = (i32)morph_accs[m];
      shared_targets[m] = target_pair;
      shared_target_counts[m] = 1;
    }
  }

  tg3_primitive* primitives = mmdgltf__arena_alloc(arena, mat_count * sizeof(tg3_primitive));
  memset(primitives, 0, mat_count * sizeof(tg3_primitive));
  for (size_t p = 0; p < mat_count; ++p) {
    primitives[p].attributes = shared_attribs;
    primitives[p].attributes_count = num_attribs;
    primitives[p].indices = (i32)prim_indices_acc[p];
    primitives[p].material = (i32)p;
    primitives[p].mode = 4;
    if (vertex_morph_count > 0) {
      primitives[p].targets = (const tg3_str_int_pair* const*)shared_targets;
      primitives[p].target_attribute_counts = shared_target_counts;
      primitives[p].targets_count = (u32)vertex_morph_count;
    }
  }

  tg3_mesh* meshes = mmdgltf__arena_alloc(arena, sizeof(tg3_mesh));
  memset(meshes, 0, sizeof(tg3_mesh));
  meshes[0].name = mmdgltf__tg3_make_str(arena, "ModelMesh");
  meshes[0].primitives = primitives;
  meshes[0].primitives_count = (u32)mat_count;
  if (vertex_morph_count > 0) {
    f64* weights = mmdgltf__arena_alloc(arena, vertex_morph_count * sizeof(f64));
    memset(weights, 0, vertex_morph_count * sizeof(f64));
    meshes[0].weights = weights;
    meshes[0].weights_count = (u32)vertex_morph_count;

    tg3_str* target_names = mmdgltf__arena_alloc(arena, vertex_morph_count * sizeof(tg3_str));
    for (size_t m = 0; m < vertex_morph_count; ++m) {
      const mmdgltf_PmxMorph* morph = &morphs[vertex_morph_indices[m]];
      target_names[m] = mmdgltf__tg3_make_str(arena, morph->name_jp ? morph->name_jp : "morph");
    }
    tg3_value target_names_val;
    memset(&target_names_val, 0, sizeof(target_names_val));
    target_names_val.type = TG3_VALUE_ARRAY;
    tg3_value* target_str_vals =
      mmdgltf__arena_alloc(arena, vertex_morph_count * sizeof(tg3_value));
    for (size_t m = 0; m < vertex_morph_count; ++m) {
      target_str_vals[m].type = TG3_VALUE_STRING;
      target_str_vals[m].string_val = target_names[m];
    }
    target_names_val.array_data = target_str_vals;
    target_names_val.array_count = (u32)vertex_morph_count;

    tg3_kv_pair* mesh_extras_kv = mmdgltf__arena_alloc(arena, sizeof(tg3_kv_pair));
    mesh_extras_kv[0] = mmdgltf__tg3_make_kv(arena, "targetNames", target_names_val);

    tg3_value* mesh_extras = mmdgltf__arena_alloc(arena, sizeof(tg3_value));
    memset(mesh_extras, 0, sizeof(tg3_value));
    mesh_extras->type = TG3_VALUE_OBJECT;
    mesh_extras->object_data = mesh_extras_kv;
    mesh_extras->object_count = 1;
    meshes[0].ext.extras = mesh_extras;
  }

  tg3_image* gltf_images = NULL;
  tg3_texture* gltf_textures = NULL;
  tg3_sampler* gltf_samplers = NULL;
  if (tex_count > 0) {
    gltf_images = mmdgltf__arena_alloc(arena, tex_count * sizeof(tg3_image));
    gltf_textures = mmdgltf__arena_alloc(arena, tex_count * sizeof(tg3_texture));
    memset(gltf_images, 0, tex_count * sizeof(tg3_image));
    memset(gltf_textures, 0, tex_count * sizeof(tg3_texture));

    gltf_samplers = mmdgltf__arena_alloc(arena, sizeof(tg3_sampler));
    memset(gltf_samplers, 0, sizeof(tg3_sampler));
    gltf_samplers[0].wrap_s = 10497;
    gltf_samplers[0].wrap_t = 10497;

    for (size_t i = 0; i < tex_count; ++i) {
      gltf_images[i].buffer_view = image_to_bv[i];
      gltf_images[i].mime_type = mmdgltf__tg3_make_str(arena, images[i].mime_type);
      gltf_images[i].name = mmdgltf__tg3_make_str(arena, images[i].path);

      gltf_textures[i].source = (i32)i;
      gltf_textures[i].sampler = 0;
    }
  }

  tg3_material* gltf_materials = mmdgltf__arena_alloc(arena, mat_count * sizeof(tg3_material));
  memset(gltf_materials, 0, mat_count * sizeof(tg3_material));

  tg3_extension unlit_ext;
  memset(&unlit_ext, 0, sizeof(unlit_ext));
  unlit_ext.name = mmdgltf__tg3_make_str(arena, "KHR_materials_unlit");
  unlit_ext.value.type = TG3_VALUE_OBJECT;
  unlit_ext.value.object_count = 0;
  unlit_ext.value.object_data = NULL;

  for (size_t p = 0; p < mat_count; ++p) {
    const mmdgltf_PmxMaterial* mat = &materials[p];
    gltf_materials[p].name =
      mmdgltf__tg3_make_str(arena, mat->name_jp ? mat->name_jp : "Material");
    gltf_materials[p].double_sided = (mat->draw_flags & MMDGLTF_MATERIAL_DOUBLE_SIDED) ? 1 : 0;
    gltf_materials[p].alpha_mode = mat->diffuse.a < 1.0f ? mmdgltf__tg3_make_str(arena, "BLEND")
                                                         : mmdgltf__tg3_make_str(arena, "OPAQUE");
    gltf_materials[p].alpha_cutoff = 0.5;

    gltf_materials[p].pbr_metallic_roughness.base_color_factor[0] = mat->diffuse.r;
    gltf_materials[p].pbr_metallic_roughness.base_color_factor[1] = mat->diffuse.g;
    gltf_materials[p].pbr_metallic_roughness.base_color_factor[2] = mat->diffuse.b;
    gltf_materials[p].pbr_metallic_roughness.base_color_factor[3] = mat->diffuse.a;
    gltf_materials[p].pbr_metallic_roughness.metallic_factor = 0.0;
    gltf_materials[p].pbr_metallic_roughness.roughness_factor = 0.9;
    gltf_materials[p].pbr_metallic_roughness.base_color_texture.index = -1;
    gltf_materials[p].pbr_metallic_roughness.metallic_roughness_texture.index = -1;
    gltf_materials[p].normal_texture.index = -1;
    gltf_materials[p].occlusion_texture.index = -1;
    gltf_materials[p].emissive_texture.index = -1;

    if (mat->texture_index >= 0 && (size_t)mat->texture_index < tex_count &&
        image_to_bv[mat->texture_index] >= 0) {
      gltf_materials[p].pbr_metallic_roughness.base_color_texture.index = mat->texture_index;
      gltf_materials[p].pbr_metallic_roughness.base_color_texture.tex_coord = 0;
    }

    if (cfg.unlit) {
      tg3_extension* ext_copy = mmdgltf__arena_alloc(arena, sizeof(tg3_extension));
      *ext_copy = unlit_ext;
      gltf_materials[p].ext.extensions = ext_copy;
      gltf_materials[p].ext.extensions_count = 1;
    }
  }

  const size_t total_nodes = 1 + (has_skinning ? b_count : 0);
  tg3_node* nodes = mmdgltf__arena_alloc(arena, total_nodes * sizeof(tg3_node));
  memset(nodes, 0, total_nodes * sizeof(tg3_node));
  for (size_t n = 0; n < total_nodes; ++n) {
    nodes[n].mesh = -1;
    nodes[n].skin = -1;
    nodes[n].camera = -1;
    nodes[n].light = -1;
    nodes[n].emitter = -1;
    nodes[n].scale[0] = nodes[n].scale[1] = nodes[n].scale[2] = 1.0;
    nodes[n].rotation[3] = 1.0;
  }

  nodes[0].name = mmdgltf__tg3_make_str(arena, "Model");
  nodes[0].mesh = 0;
  nodes[0].skin = has_skinning ? 0 : -1;

  size_t root_bone_count = 0;
  if (has_skinning) {
    u32* child_counts = mmdgltf__arena_alloc(arena, b_count * sizeof(u32));
    memset(child_counts, 0, b_count * sizeof(u32));

    for (size_t b = 0; b < b_count; ++b) {
      const i32 parent = bones[b].parent_index;
      if (parent >= 0 && (size_t)parent < b_count) {
        child_counts[parent]++;
      } else {
        root_bone_count++;
      }
    }

    i32** children_arrays = mmdgltf__arena_alloc(arena, b_count * sizeof(i32*));
    u32* child_cursor = mmdgltf__arena_alloc(arena, b_count * sizeof(u32));
    memset(child_cursor, 0, b_count * sizeof(u32));

    for (size_t b = 0; b < b_count; ++b) {
      if (child_counts[b] > 0) {
        children_arrays[b] = mmdgltf__arena_alloc(arena, child_counts[b] * sizeof(i32));
      } else {
        children_arrays[b] = NULL;
      }
    }

    for (size_t b = 0; b < b_count; ++b) {
      const i32 parent = bones[b].parent_index;
      if (parent >= 0 && (size_t)parent < b_count) {
        children_arrays[parent][child_cursor[parent]++] = (i32)(b + 1);
      }
    }

    for (size_t b = 0; b < b_count; ++b) {
      const size_t n_idx = b + 1;
      const char* bname = NULL;
      if (cfg.english_bone_names) {
        bname =
          (bones[b].name_en && bones[b].name_en[0] != '\0') ? bones[b].name_en : bones[b].name_jp;
      } else {
        bname =
          (bones[b].name_jp && bones[b].name_jp[0] != '\0') ? bones[b].name_jp : bones[b].name_en;
      }
      nodes[n_idx].name = mmdgltf__tg3_make_str(arena, bname ? bname : "Bone");
      nodes[n_idx].mesh = -1;
      nodes[n_idx].skin = -1;
      nodes[n_idx].camera = -1;
      nodes[n_idx].light = -1;
      nodes[n_idx].emitter = -1;
      nodes[n_idx].scale[0] = nodes[n_idx].scale[1] = nodes[n_idx].scale[2] = 1.0;
      nodes[n_idx].rotation[3] = 1.0;

      f32 lx = bones[b].position.x;
      f32 ly = bones[b].position.y;
      f32 lz = bones[b].position.z;

      const i32 parent = bones[b].parent_index;
      if (parent >= 0 && (size_t)parent < b_count) {
        lx -= bones[parent].position.x;
        ly -= bones[parent].position.y;
        lz -= bones[parent].position.z;
      }

      nodes[n_idx].translation[0] = lx;
      nodes[n_idx].translation[1] = ly;
      nodes[n_idx].translation[2] = lz;

      nodes[n_idx].children = children_arrays[b];
      nodes[n_idx].children_count = child_counts[b];
    }
  }

  tg3_skin* skins = NULL;
  if (has_skinning) {
    skins = mmdgltf__arena_alloc(arena, sizeof(tg3_skin));
    memset(skins, 0, sizeof(tg3_skin));
    skins[0].name = mmdgltf__tg3_make_str(arena, "Armature");
    skins[0].inverse_bind_matrices = (i32)acc_ibm;
    skins[0].skeleton = -1;

    i32* joint_nodes = mmdgltf__arena_alloc(arena, b_count * sizeof(i32));
    for (size_t b = 0; b < b_count; ++b) {
      joint_nodes[b] = (i32)(b + 1);
    }
    skins[0].joints = joint_nodes;
    skins[0].joints_count = (u32)b_count;
  }

  const size_t scene_nodes_count = 1 + root_bone_count;
  i32* scene_nodes = mmdgltf__arena_alloc(arena, scene_nodes_count * sizeof(i32));
  scene_nodes[0] = 0;
  size_t sn_idx = 1;
  if (has_skinning) {
    for (size_t b = 0; b < b_count; ++b) {
      if (bones[b].parent_index < 0 || (size_t)bones[b].parent_index >= b_count) {
        scene_nodes[sn_idx++] = (i32)(b + 1);
      }
    }
  }

  tg3_scene* scenes = mmdgltf__arena_alloc(arena, sizeof(tg3_scene));
  memset(scenes, 0, sizeof(tg3_scene));
  scenes[0].name = mmdgltf__tg3_make_str(arena, "Scene");
  scenes[0].nodes = scene_nodes;
  scenes[0].nodes_count = (u32)scene_nodes_count;

  if (cfg.export_physics && (rb_count > 0 || jt_count > 0)) {
    tg3_value mmd_extras;
    memset(&mmd_extras, 0, sizeof(mmd_extras));
    mmd_extras.type = TG3_VALUE_OBJECT;

    tg3_kv_pair* mmd_pairs = mmdgltf__arena_alloc(arena, 2 * sizeof(tg3_kv_pair));
    u32 mmd_pairs_count = 0;

    if (rb_count > 0) {
      tg3_value rb_arr;
      memset(&rb_arr, 0, sizeof(rb_arr));
      rb_arr.type = TG3_VALUE_ARRAY;
      tg3_value* rb_items = mmdgltf__arena_alloc(arena, rb_count * sizeof(tg3_value));

      for (size_t i = 0; i < rb_count; ++i) {
        const mmdgltf_PmxRigidBody* rb = &rigid_bodies[i];
        tg3_kv_pair* rb_kv = mmdgltf__arena_alloc(arena, 6 * sizeof(tg3_kv_pair));
        rb_kv[0] =
          mmdgltf__tg3_make_kv(arena, "name", mmdgltf__tg3_make_str_value(arena, rb->name_jp));
        rb_kv[1] =
          mmdgltf__tg3_make_kv(arena, "bone_index", mmdgltf__tg3_make_int_value(rb->bone_index));
        rb_kv[2] =
          mmdgltf__tg3_make_kv(arena, "shape_type", mmdgltf__tg3_make_int_value(rb->shape_type));
        rb_kv[3] = mmdgltf__tg3_make_kv(arena, "mass", mmdgltf__tg3_make_real_value(rb->mass));
        rb_kv[4] = mmdgltf__tg3_make_kv(arena, "collision_group",
                                        mmdgltf__tg3_make_int_value(rb->collision_group));
        rb_kv[5] = mmdgltf__tg3_make_kv(arena, "physics_mode",
                                        mmdgltf__tg3_make_int_value(rb->physics_mode));

        rb_items[i].type = TG3_VALUE_OBJECT;
        rb_items[i].object_data = rb_kv;
        rb_items[i].object_count = 6;
      }
      rb_arr.array_data = rb_items;
      rb_arr.array_count = (u32)rb_count;
      mmd_pairs[mmd_pairs_count++] = mmdgltf__tg3_make_kv(arena, "rigid_bodies", rb_arr);
    }

    if (jt_count > 0) {
      tg3_value j_arr;
      memset(&j_arr, 0, sizeof(j_arr));
      j_arr.type = TG3_VALUE_ARRAY;
      tg3_value* j_items = mmdgltf__arena_alloc(arena, jt_count * sizeof(tg3_value));

      for (size_t i = 0; i < jt_count; ++i) {
        const mmdgltf_PmxJoint* jt = &joints[i];
        tg3_kv_pair* j_kv = mmdgltf__arena_alloc(arena, 4 * sizeof(tg3_kv_pair));
        j_kv[0] =
          mmdgltf__tg3_make_kv(arena, "name", mmdgltf__tg3_make_str_value(arena, jt->name_jp));
        j_kv[1] =
          mmdgltf__tg3_make_kv(arena, "src_rigid", mmdgltf__tg3_make_int_value(jt->src_rigid));
        j_kv[2] =
          mmdgltf__tg3_make_kv(arena, "dest_rigid", mmdgltf__tg3_make_int_value(jt->dest_rigid));
        j_kv[3] = mmdgltf__tg3_make_kv(arena, "type", mmdgltf__tg3_make_int_value(jt->type));

        j_items[i].type = TG3_VALUE_OBJECT;
        j_items[i].object_data = j_kv;
        j_items[i].object_count = 4;
      }
      j_arr.array_data = j_items;
      j_arr.array_count = (u32)jt_count;
      mmd_pairs[mmd_pairs_count++] = mmdgltf__tg3_make_kv(arena, "joints", j_arr);
    }

    mmd_extras.object_data = mmd_pairs;
    mmd_extras.object_count = mmd_pairs_count;

    tg3_kv_pair* scene_extras_kv = mmdgltf__arena_alloc(arena, sizeof(tg3_kv_pair));
    scene_extras_kv[0] = mmdgltf__tg3_make_kv(arena, "mmd", mmd_extras);

    tg3_value* scene_extras = mmdgltf__arena_alloc(arena, sizeof(tg3_value));
    memset(scene_extras, 0, sizeof(tg3_value));
    scene_extras->type = TG3_VALUE_OBJECT;
    scene_extras->object_data = scene_extras_kv;
    scene_extras->object_count = 1;
    scenes[0].ext.extras = scene_extras;
  }

  tg3_model gltf_model;
  memset(&gltf_model, 0, sizeof(gltf_model));

  gltf_model.asset.version = mmdgltf__tg3_make_str(arena, "2.0");
  gltf_model.asset.generator = mmdgltf__tg3_make_str(arena, "mmdgltf-fastpath");

  tg3_buffer gltf_buffer;
  memset(&gltf_buffer, 0, sizeof(gltf_buffer));
  gltf_buffer.byte_length = bw.size;
  gltf_buffer.data.data = bw.data;
  gltf_buffer.data.count = bw.size;

  gltf_model.buffers = &gltf_buffer;
  gltf_model.buffers_count = 1;
  gltf_model.buffer_views = bvs;
  gltf_model.buffer_views_count = (u32)num_bvs;
  gltf_model.accessors = accs;
  gltf_model.accessors_count = (u32)num_accessors;
  gltf_model.meshes = meshes;
  gltf_model.meshes_count = 1;
  gltf_model.materials = gltf_materials;
  gltf_model.materials_count = (u32)mat_count;
  gltf_model.nodes = nodes;
  gltf_model.nodes_count = (u32)total_nodes;
  gltf_model.skins = skins;
  gltf_model.skins_count = has_skinning ? 1 : 0;
  gltf_model.scenes = scenes;
  gltf_model.scenes_count = 1;
  gltf_model.default_scene = 0;
  gltf_model.images = gltf_images;
  gltf_model.images_count = (u32)tex_count;
  gltf_model.textures = gltf_textures;
  gltf_model.textures_count = (u32)tex_count;
  gltf_model.samplers = gltf_samplers;
  gltf_model.samplers_count = (tex_count > 0) ? 1 : 0;

  if (cfg.unlit) {
    tg3_str* exts_used = mmdgltf__arena_alloc(arena, sizeof(tg3_str));
    exts_used[0] = mmdgltf__tg3_make_str(arena, "KHR_materials_unlit");
    gltf_model.extensions_used = exts_used;
    gltf_model.extensions_used_count = 1;
  }

  char* bin_path = NULL;
  char* bin_uri = NULL;
  if (!write_binary && filename) {
    mmdgltf__derive_bin_paths(arena, filename, filename_len, &bin_path, &bin_uri);
    gltf_buffer.uri = mmdgltf__tg3_make_str(arena, bin_uri);
  }

  tg3_write_options write_opts;
  tg3_write_options_init(&write_opts);
  write_opts.write_binary = write_binary ? 1 : 0;
  write_opts.pretty_print = write_binary ? 0 : 1;
  write_opts.embed_buffers = 0;
  write_opts.embed_images = 0;

  tg3_error_stack errors;
  tg3_error_stack_init(&errors);

  tg3_error_code status = TG3_OK;
  if (filename) {
    status = tg3_write_to_file(&gltf_model, &errors, filename, filename_len, &write_opts);
    if (status == TG3_OK && !write_binary && bin_path) {
      FILE* bf = fopen(bin_path, "wb");
      if (!bf) {
        status = TG3_ERR_FILE_WRITE;
      } else {
        if (bw.size > 0 && fwrite(bw.data, 1, bw.size, bf) != bw.size) {
          status = TG3_ERR_FILE_WRITE;
        }
        fclose(bf);
      }
    }
  } else if (out_data && out_size) {
    u64 written_size = 0;
    status = tg3_write_to_memory(&gltf_model, &errors, out_data, &written_size, &write_opts);
    *out_size = (size_t)written_size;
  }

  tg3_error_stack_free(&errors);
  if (status != TG3_OK) {
    RET_ERR(MMDGLTF_ERROR_IO);
  }

on_convert_end:
  if (bw.data) {
    MMDGLTF_FREE(bw.data, bw.capacity);
  }
  if (arena) {
    mmdgltf__arena_destroy(arena);
  }
  return err;
#undef RET_ERR
}

MMDGLTF_API mmdgltf_Status mmdgltf_convert_pmx_filen(const char* pmx_path, size_t pmx_path_len,
                                                     const char* glb_path, size_t glb_path_len,
                                                     const mmdgltf_ExportConfig* config) {
  if (!pmx_path || pmx_path_len == 0 || !glb_path || glb_path_len == 0) {
    return MMDGLTF_ERROR_INVALID_ARGUMENT;
  }

  char* null_pmx = MMDGLTF_MALLOC(pmx_path_len + 1);
  if (!null_pmx) {
    return MMDGLTF_ERROR_OUT_OF_MEMORY;
  }
  memcpy(null_pmx, pmx_path, pmx_path_len);
  null_pmx[pmx_path_len] = '\0';

  FILE* f = fopen(null_pmx, "rb");
  if (!f) {
    MMDGLTF_FREE(null_pmx, pmx_path_len + 1);
    return MMDGLTF_ERROR_IO;
  }

  mmdgltf_Arena* dir_arena = mmdgltf__arena_create(4096);
  char* base_dir = extract_dir(dir_arena, null_pmx, pmx_path_len);
  MMDGLTF_FREE(null_pmx, pmx_path_len + 1);

  mmdgltf_Stream stream;
  stream.func = mmdgltf__file_io;
  stream.src = f;
  stream.status = MMDGLTF_OK;

  const mmdgltf_Status st =
    convert_stream(&stream, base_dir, config, glb_path, (u32)glb_path_len, NULL, NULL, true);

  fclose(f);
  if (dir_arena) {
    mmdgltf__arena_destroy(dir_arena);
  }
  return st;
}

MMDGLTF_API mmdgltf_Status mmdgltf_convert_pmx_file(const char* pmx_path, const char* glb_path,
                                                    const mmdgltf_ExportConfig* config) {
  if (!pmx_path || !glb_path) {
    return MMDGLTF_ERROR_INVALID_ARGUMENT;
  }
  return mmdgltf_convert_pmx_filen(pmx_path, strlen(pmx_path), glb_path, strlen(glb_path), config);
}

MMDGLTF_API mmdgltf_Status mmdgltf_convert_pmx_to_gltf_filen(const char* pmx_path,
                                                             size_t pmx_path_len,
                                                             const char* gltf_path,
                                                             size_t gltf_path_len,
                                                             const mmdgltf_ExportConfig* config) {
  if (!pmx_path || pmx_path_len == 0 || !gltf_path || gltf_path_len == 0) {
    return MMDGLTF_ERROR_INVALID_ARGUMENT;
  }

  char* null_pmx = MMDGLTF_MALLOC(pmx_path_len + 1);
  if (!null_pmx) {
    return MMDGLTF_ERROR_OUT_OF_MEMORY;
  }
  memcpy(null_pmx, pmx_path, pmx_path_len);
  null_pmx[pmx_path_len] = '\0';

  FILE* f = fopen(null_pmx, "rb");
  if (!f) {
    MMDGLTF_FREE(null_pmx, pmx_path_len + 1);
    return MMDGLTF_ERROR_IO;
  }

  mmdgltf_Arena* dir_arena = mmdgltf__arena_create(4096);
  char* base_dir = extract_dir(dir_arena, null_pmx, pmx_path_len);
  MMDGLTF_FREE(null_pmx, pmx_path_len + 1);

  mmdgltf_Stream stream;
  stream.func = mmdgltf__file_io;
  stream.src = f;
  stream.status = MMDGLTF_OK;

  const mmdgltf_Status st =
    convert_stream(&stream, base_dir, config, gltf_path, (u32)gltf_path_len, NULL, NULL, false);

  fclose(f);
  if (dir_arena) {
    mmdgltf__arena_destroy(dir_arena);
  }
  return st;
}

MMDGLTF_API mmdgltf_Status mmdgltf_convert_pmx_to_gltf_file(const char* pmx_path,
                                                            const char* gltf_path,
                                                            const mmdgltf_ExportConfig* config) {
  if (!pmx_path || !gltf_path) {
    return MMDGLTF_ERROR_INVALID_ARGUMENT;
  }
  return mmdgltf_convert_pmx_to_gltf_filen(pmx_path, strlen(pmx_path), gltf_path,
                                           strlen(gltf_path), config);
}

MMDGLTF_API mmdgltf_Status mmdgltf_convert_pmx_memory(const uint8_t* pmx_data, size_t pmx_size,
                                                      uint8_t** out_glb_data, size_t* out_glb_size,
                                                      const mmdgltf_ExportConfig* config) {
  if (!pmx_data || pmx_size < 33 || !out_glb_data || !out_glb_size) {
    return MMDGLTF_ERROR_INVALID_ARGUMENT;
  }

  mmdgltf__MemStream mem = {
    .data = (const u8*)pmx_data,
    .size = pmx_size,
    .pos = 0,
  };

  mmdgltf_Stream stream;
  stream.func = mmdgltf__mem_io;
  stream.src = &mem;
  stream.status = MMDGLTF_OK;

  return convert_stream(&stream, NULL, config, NULL, 0, out_glb_data, out_glb_size, true);
}
