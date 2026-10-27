#include "mmdgltf_internal.h"

#include <string.h>

MMDGLTF_API void mmdgltf_model_config_default(mmdgltf_ModelConfig* config) {
  if (!config) {
    return;
  }
  config->load_flags = MMDGLTF_LOAD_ALL;
  config->coordinate_system = MMDGLTF_COORDS_GLTF_RIGHT_HANDED;
  config->scale = 1.0f;
}

static char* mmdgltf__model_strdup(mmdgltf_Arena* arena, const char* str) {
  if (!str) {
    return NULL;
  }
  const size_t len = strlen(str);
  return mmdgltf__arena_strndup(arena, str, len);
}

MMDGLTF_API mmdgltf_Model mmdgltf_model_create(mmdgltf_Pmx pmx,
                                               const mmdgltf_ModelConfig* config) {
  if (!pmx) {
    return MMDGLTF_ENCODE_ERROR(MMDGLTF_ERROR_INVALID_ARGUMENT);
  }

  mmdgltf_ModelConfig cfg;
  if (config) {
    cfg = *config;
    if (cfg.load_flags == 0) {
      cfg.load_flags = MMDGLTF_LOAD_ALL;
    }
  } else {
    mmdgltf_model_config_default(&cfg);
  }

  const f32 scale = cfg.scale > 0.0f ? cfg.scale : 1.0f;
  const bool is_rh = (cfg.coordinate_system == MMDGLTF_COORDS_GLTF_RIGHT_HANDED);

  mmdgltf_Arena* arena = mmdgltf__arena_create(64 * 1024);
  if (!arena) {
    return MMDGLTF_ENCODE_ERROR(MMDGLTF_ERROR_OUT_OF_MEMORY);
  }

  mmdgltf_Model model = mmdgltf__arena_alloc(arena, sizeof(struct mmdgltf_Model_T));
  if (!model) {
    mmdgltf__arena_destroy(arena);
    return MMDGLTF_ENCODE_ERROR(MMDGLTF_ERROR_OUT_OF_MEMORY);
  }

  memset(model, 0, sizeof(struct mmdgltf_Model_T));
  model->arena = arena;
  model->config = cfg;

  if (cfg.load_flags & MMDGLTF_LOAD_VERTICES) {
    const size_t vcount = pmx->vertex_count;
    model->vertex_count = vcount;
    model->vertex_buffers.vertex_count = vcount;

    if (vcount > 0) {
      bool has_sdef = false;
      for (size_t i = 0; i < vcount; ++i) {
        if (pmx->vertices[i].weight_type == MMDGLTF_WEIGHT_SDEF) {
          has_sdef = true;
          break;
        }
      }

      f32* positions = mmdgltf__arena_alloc(arena, vcount * 3 * sizeof(f32));
      f32* normals = mmdgltf__arena_alloc(arena, vcount * 3 * sizeof(f32));
      f32* uvs = mmdgltf__arena_alloc(arena, vcount * 2 * sizeof(f32));
      u16* joints = mmdgltf__arena_alloc(arena, vcount * MMDGLTF_MAX_BONE_WEIGHTS * sizeof(u16));
      f32* weights = mmdgltf__arena_alloc(arena, vcount * MMDGLTF_MAX_BONE_WEIGHTS * sizeof(f32));

      f32* sdef_c = NULL;
      f32* sdef_r0 = NULL;
      f32* sdef_r1 = NULL;
      if (has_sdef) {
        sdef_c = mmdgltf__arena_alloc(arena, vcount * 3 * sizeof(f32));
        sdef_r0 = mmdgltf__arena_alloc(arena, vcount * 3 * sizeof(f32));
        sdef_r1 = mmdgltf__arena_alloc(arena, vcount * 3 * sizeof(f32));
      }

      for (size_t i = 0; i < vcount; ++i) {
        const mmdgltf_PmxVertex* v = &pmx->vertices[i];
        const size_t i3 = i * 3;
        const size_t i2 = i * 2;
        const size_t iw = i * MMDGLTF_MAX_BONE_WEIGHTS;

        positions[i3 + 0] = v->position.x * scale;
        positions[i3 + 1] = v->position.y * scale;
        positions[i3 + 2] = is_rh ? (-v->position.z * scale) : (v->position.z * scale);

        normals[i3 + 0] = v->normal.x;
        normals[i3 + 1] = v->normal.y;
        normals[i3 + 2] = is_rh ? (-v->normal.z) : v->normal.z;

        uvs[i2 + 0] = v->uv.x;
        uvs[i2 + 1] = v->uv.y;

        for (size_t j = 0; j < MMDGLTF_MAX_BONE_WEIGHTS; ++j) {
          const i32 bone_idx = v->bones[j];
          joints[iw + j] = (bone_idx >= 0) ? (u16)bone_idx : 0;
          weights[iw + j] = v->weights[j];
        }

        if (has_sdef) {
          if (v->weight_type == MMDGLTF_WEIGHT_SDEF) {
            sdef_c[i3 + 0] = v->sdef_c.x * scale;
            sdef_c[i3 + 1] = v->sdef_c.y * scale;
            sdef_c[i3 + 2] = is_rh ? (-v->sdef_c.z * scale) : (v->sdef_c.z * scale);

            sdef_r0[i3 + 0] = v->sdef_r0.x * scale;
            sdef_r0[i3 + 1] = v->sdef_r0.y * scale;
            sdef_r0[i3 + 2] = is_rh ? (-v->sdef_r0.z * scale) : (v->sdef_r0.z * scale);

            sdef_r1[i3 + 0] = v->sdef_r1.x * scale;
            sdef_r1[i3 + 1] = v->sdef_r1.y * scale;
            sdef_r1[i3 + 2] = is_rh ? (-v->sdef_r1.z * scale) : (v->sdef_r1.z * scale);
          } else {
            sdef_c[i3 + 0] = 0.0f;
            sdef_c[i3 + 1] = 0.0f;
            sdef_c[i3 + 2] = 0.0f;

            sdef_r0[i3 + 0] = 0.0f;
            sdef_r0[i3 + 1] = 0.0f;
            sdef_r0[i3 + 2] = 0.0f;

            sdef_r1[i3 + 0] = 0.0f;
            sdef_r1[i3 + 1] = 0.0f;
            sdef_r1[i3 + 2] = 0.0f;
          }
        }
      }

      model->vertex_buffers.positions = positions;
      model->vertex_buffers.normals = normals;
      model->vertex_buffers.uvs = uvs;
      model->vertex_buffers.joints = joints;
      model->vertex_buffers.weights = weights;
      model->vertex_buffers.sdef_c = sdef_c;
      model->vertex_buffers.sdef_r0 = sdef_r0;
      model->vertex_buffers.sdef_r1 = sdef_r1;
    }

    const size_t icount = pmx->index_count;
    model->index_count = icount;
    if (icount > 0) {
      u32* indices = mmdgltf__arena_alloc(arena, icount * sizeof(u32));
      if (is_rh) {
        for (size_t t = 0; t + 2 < icount; t += 3) {
          indices[t + 0] = pmx->indices[t + 0];
          indices[t + 1] = pmx->indices[t + 2];
          indices[t + 2] = pmx->indices[t + 1];
        }
      } else {
        memcpy(indices, pmx->indices, icount * sizeof(u32));
      }
      model->indices = indices;
    }
  }

  if ((cfg.load_flags & MMDGLTF_LOAD_MATERIALS) && pmx->material_count > 0) {
    const size_t mcount = pmx->material_count;
    mmdgltf_PmxMaterial* mats = mmdgltf__arena_alloc(arena, mcount * sizeof(mmdgltf_PmxMaterial));
    for (size_t i = 0; i < mcount; ++i) {
      const mmdgltf_PmxMaterial* src = &pmx->materials[i];
      mmdgltf_PmxMaterial* dst = &mats[i];
      *dst = *src;
      dst->name_jp = mmdgltf__model_strdup(arena, src->name_jp);
      dst->name_en = mmdgltf__model_strdup(arena, src->name_en);
      dst->comment = mmdgltf__model_strdup(arena, src->comment);
    }
    model->materials = mats;
    model->material_count = mcount;
  }

  if ((cfg.load_flags & MMDGLTF_LOAD_BONES) && pmx->bone_count > 0) {
    const size_t bcount = pmx->bone_count;
    mmdgltf_PmxBone* bones = mmdgltf__arena_alloc(arena, bcount * sizeof(mmdgltf_PmxBone));
    for (size_t i = 0; i < bcount; ++i) {
      const mmdgltf_PmxBone* src = &pmx->bones[i];
      mmdgltf_PmxBone* dst = &bones[i];
      *dst = *src;
      dst->name_jp = mmdgltf__model_strdup(arena, src->name_jp);
      dst->name_en = mmdgltf__model_strdup(arena, src->name_en);

      dst->position.x = src->position.x * scale;
      dst->position.y = src->position.y * scale;
      dst->position.z = is_rh ? (-src->position.z * scale) : (src->position.z * scale);

      dst->offset.x = src->offset.x * scale;
      dst->offset.y = src->offset.y * scale;
      dst->offset.z = is_rh ? (-src->offset.z * scale) : (src->offset.z * scale);

      if (is_rh) {
        dst->fixed_axis.x = src->fixed_axis.x;
        dst->fixed_axis.y = src->fixed_axis.y;
        dst->fixed_axis.z = -src->fixed_axis.z;

        dst->local_x.x = src->local_x.x;
        dst->local_x.y = src->local_x.y;
        dst->local_x.z = -src->local_x.z;

        dst->local_z.x = src->local_z.x;
        dst->local_z.y = src->local_z.y;
        dst->local_z.z = -src->local_z.z;
      }

      if (src->ik_link_count > 0 && src->ik_links) {
        mmdgltf_PmxIkLink* links =
          mmdgltf__arena_alloc(arena, src->ik_link_count * sizeof(mmdgltf_PmxIkLink));
        memcpy(links, src->ik_links, src->ik_link_count * sizeof(mmdgltf_PmxIkLink));
        if (is_rh) {
          for (size_t k = 0; k < src->ik_link_count; ++k) {
            if (links[k].has_limits) {
              const f32 min_y = -links[k].max_angle.y;
              const f32 max_y = -links[k].min_angle.y;
              const f32 min_z = -links[k].max_angle.z;
              const f32 max_z = -links[k].min_angle.z;
              links[k].min_angle.y = min_y;
              links[k].max_angle.y = max_y;
              links[k].min_angle.z = min_z;
              links[k].max_angle.z = max_z;
            }
          }
        }
        dst->ik_links = links;
      }
    }
    model->bones = bones;
    model->bone_count = bcount;
  }

  if ((cfg.load_flags & MMDGLTF_LOAD_MORPHS) && pmx->morph_count > 0) {
    const size_t morph_count = pmx->morph_count;
    mmdgltf_PmxMorph* morphs = mmdgltf__arena_alloc(arena, morph_count * sizeof(mmdgltf_PmxMorph));
    for (size_t i = 0; i < morph_count; ++i) {
      const mmdgltf_PmxMorph* src = &pmx->morphs[i];
      mmdgltf_PmxMorph* dst = &morphs[i];
      *dst = *src;
      dst->name_jp = mmdgltf__model_strdup(arena, src->name_jp);
      dst->name_en = mmdgltf__model_strdup(arena, src->name_en);

      if (src->offset_count > 0 && src->raw_offsets) {
        switch (src->type) {
          case MMDGLTF_MORPH_VERTEX: {
            mmdgltf_PmxVertexMorphOffset* voffsets = mmdgltf__arena_alloc(
              arena, src->offset_count * sizeof(mmdgltf_PmxVertexMorphOffset));
            for (size_t k = 0; k < src->offset_count; ++k) {
              const mmdgltf_PmxVertexMorphOffset* s = &src->vertex_offsets[k];
              voffsets[k].vertex_index = s->vertex_index;
              voffsets[k].position_offset.x = s->position_offset.x * scale;
              voffsets[k].position_offset.y = s->position_offset.y * scale;
              voffsets[k].position_offset.z =
                is_rh ? (-s->position_offset.z * scale) : (s->position_offset.z * scale);
            }
            dst->vertex_offsets = voffsets;
            break;
          }
          case MMDGLTF_MORPH_BONE: {
            mmdgltf_PmxBoneMorphOffset* boffsets =
              mmdgltf__arena_alloc(arena, src->offset_count * sizeof(mmdgltf_PmxBoneMorphOffset));
            for (size_t k = 0; k < src->offset_count; ++k) {
              const mmdgltf_PmxBoneMorphOffset* s = &src->bone_offsets[k];
              boffsets[k].bone_index = s->bone_index;
              boffsets[k].translation_offset.x = s->translation_offset.x * scale;
              boffsets[k].translation_offset.y = s->translation_offset.y * scale;
              boffsets[k].translation_offset.z =
                is_rh ? (-s->translation_offset.z * scale) : (s->translation_offset.z * scale);
              if (is_rh) {
                boffsets[k].rotation_offset.x = -s->rotation_offset.x;
                boffsets[k].rotation_offset.y = -s->rotation_offset.y;
                boffsets[k].rotation_offset.z = s->rotation_offset.z;
                boffsets[k].rotation_offset.w = s->rotation_offset.w;
              } else {
                boffsets[k].rotation_offset = s->rotation_offset;
              }
            }
            dst->bone_offsets = boffsets;
            break;
          }
          case MMDGLTF_MORPH_UV:
          case MMDGLTF_MORPH_EXTRA_UV1:
          case MMDGLTF_MORPH_EXTRA_UV2:
          case MMDGLTF_MORPH_EXTRA_UV3:
          case MMDGLTF_MORPH_EXTRA_UV4: {
            mmdgltf_PmxUvMorphOffset* uoffsets =
              mmdgltf__arena_alloc(arena, src->offset_count * sizeof(mmdgltf_PmxUvMorphOffset));
            memcpy(uoffsets, src->uv_offsets,
                   src->offset_count * sizeof(mmdgltf_PmxUvMorphOffset));
            dst->uv_offsets = uoffsets;
            break;
          }
          case MMDGLTF_MORPH_MATERIAL: {
            mmdgltf_PmxMaterialMorphOffset* moffsets = mmdgltf__arena_alloc(
              arena, src->offset_count * sizeof(mmdgltf_PmxMaterialMorphOffset));
            memcpy(moffsets, src->material_offsets,
                   src->offset_count * sizeof(mmdgltf_PmxMaterialMorphOffset));
            dst->material_offsets = moffsets;
            break;
          }
          case MMDGLTF_MORPH_GROUP: {
            mmdgltf_PmxGroupMorphOffset* goffsets =
              mmdgltf__arena_alloc(arena, src->offset_count * sizeof(mmdgltf_PmxGroupMorphOffset));
            memcpy(goffsets, src->group_offsets,
                   src->offset_count * sizeof(mmdgltf_PmxGroupMorphOffset));
            dst->group_offsets = goffsets;
            break;
          }
          default:
            dst->raw_offsets = NULL;
            break;
        }
      }
    }
    model->morphs = morphs;
    model->morph_count = morph_count;
  }

  if ((cfg.load_flags & MMDGLTF_LOAD_PHYSICS)) {
    if (pmx->rigid_body_count > 0) {
      const size_t rcount = pmx->rigid_body_count;
      mmdgltf_PmxRigidBody* rbs =
        mmdgltf__arena_alloc(arena, rcount * sizeof(mmdgltf_PmxRigidBody));
      for (size_t i = 0; i < rcount; ++i) {
        const mmdgltf_PmxRigidBody* src = &pmx->rigid_bodies[i];
        mmdgltf_PmxRigidBody* dst = &rbs[i];
        *dst = *src;
        dst->name_jp = mmdgltf__model_strdup(arena, src->name_jp);
        dst->name_en = mmdgltf__model_strdup(arena, src->name_en);

        dst->position.x = src->position.x * scale;
        dst->position.y = src->position.y * scale;
        dst->position.z = is_rh ? (-src->position.z * scale) : (src->position.z * scale);

        dst->size.x = src->size.x * scale;
        dst->size.y = src->size.y * scale;
        dst->size.z = src->size.z * scale;

        if (is_rh) {
          dst->rotation.x = src->rotation.x;
          dst->rotation.y = -src->rotation.y;
          dst->rotation.z = -src->rotation.z;
        }
      }
      model->rigid_bodies = rbs;
      model->rigid_body_count = rcount;
    }

    if (pmx->joint_count > 0) {
      const size_t jcount = pmx->joint_count;
      mmdgltf_PmxJoint* joints = mmdgltf__arena_alloc(arena, jcount * sizeof(mmdgltf_PmxJoint));
      for (size_t i = 0; i < jcount; ++i) {
        const mmdgltf_PmxJoint* src = &pmx->joints[i];
        mmdgltf_PmxJoint* dst = &joints[i];
        *dst = *src;
        dst->name_jp = mmdgltf__model_strdup(arena, src->name_jp);
        dst->name_en = mmdgltf__model_strdup(arena, src->name_en);

        dst->position.x = src->position.x * scale;
        dst->position.y = src->position.y * scale;
        dst->position.z = is_rh ? (-src->position.z * scale) : (src->position.z * scale);

        if (is_rh) {
          dst->rotation.x = src->rotation.x;
          dst->rotation.y = -src->rotation.y;
          dst->rotation.z = -src->rotation.z;

          dst->min_position.x = src->min_position.x * scale;
          dst->min_position.y = src->min_position.y * scale;
          dst->min_position.z = -src->max_position.z * scale;

          dst->max_position.x = src->max_position.x * scale;
          dst->max_position.y = src->max_position.y * scale;
          dst->max_position.z = -src->min_position.z * scale;

          dst->min_rotation.x = src->min_rotation.x;
          dst->min_rotation.y = -src->max_rotation.y;
          dst->min_rotation.z = -src->max_rotation.z;

          dst->max_rotation.x = src->max_rotation.x;
          dst->max_rotation.y = -src->min_rotation.y;
          dst->max_rotation.z = -src->min_rotation.z;
        } else {
          dst->min_position.x = src->min_position.x * scale;
          dst->min_position.y = src->min_position.y * scale;
          dst->min_position.z = src->min_position.z * scale;

          dst->max_position.x = src->max_position.x * scale;
          dst->max_position.y = src->max_position.y * scale;
          dst->max_position.z = src->max_position.z * scale;
        }
      }
      model->joints = joints;
      model->joint_count = jcount;
    }
  }

  return model;
}

MMDGLTF_API void mmdgltf_model_destroy(mmdgltf_Model model) {
  if (!model) {
    return;
  }
  mmdgltf__arena_destroy(model->arena);
}

MMDGLTF_API mmdgltf_Status mmdgltf_model_vertex_buffers(mmdgltf_Model model,
                                                        mmdgltf_VertexBuffers* out_buffers) {
  if (!model || !out_buffers) {
    return MMDGLTF_ERROR_INVALID_ARGUMENT;
  }
  *out_buffers = model->vertex_buffers;
  return MMDGLTF_OK;
}

MMDGLTF_API mmdgltf_Status mmdgltf_model_indices(mmdgltf_Model model, const uint32_t** out_indices,
                                                 size_t* out_count) {
  if (!model || !out_indices || !out_count) {
    return MMDGLTF_ERROR_INVALID_ARGUMENT;
  }
  *out_indices = model->indices;
  *out_count = model->index_count;
  return MMDGLTF_OK;
}

MMDGLTF_API mmdgltf_Status mmdgltf_model_materials(mmdgltf_Model model,
                                                   const mmdgltf_PmxMaterial** out_materials,
                                                   size_t* out_count) {
  if (!model || !out_materials || !out_count) {
    return MMDGLTF_ERROR_INVALID_ARGUMENT;
  }
  *out_materials = model->materials;
  *out_count = model->material_count;
  return MMDGLTF_OK;
}

MMDGLTF_API mmdgltf_Status mmdgltf_model_bones(mmdgltf_Model model,
                                               const mmdgltf_PmxBone** out_bones,
                                               size_t* out_count) {
  if (!model || !out_bones || !out_count) {
    return MMDGLTF_ERROR_INVALID_ARGUMENT;
  }
  *out_bones = model->bones;
  *out_count = model->bone_count;
  return MMDGLTF_OK;
}

MMDGLTF_API mmdgltf_Status mmdgltf_model_morphs(mmdgltf_Model model,
                                                const mmdgltf_PmxMorph** out_morphs,
                                                size_t* out_count) {
  if (!model || !out_morphs || !out_count) {
    return MMDGLTF_ERROR_INVALID_ARGUMENT;
  }
  *out_morphs = model->morphs;
  *out_count = model->morph_count;
  return MMDGLTF_OK;
}

MMDGLTF_API mmdgltf_Status mmdgltf_model_rigid_bodies(
  mmdgltf_Model model, const mmdgltf_PmxRigidBody** out_rigid_bodies, size_t* out_count) {
  if (!model || !out_rigid_bodies || !out_count) {
    return MMDGLTF_ERROR_INVALID_ARGUMENT;
  }
  *out_rigid_bodies = model->rigid_bodies;
  *out_count = model->rigid_body_count;
  return MMDGLTF_OK;
}

MMDGLTF_API mmdgltf_Status mmdgltf_model_joints(mmdgltf_Model model,
                                                const mmdgltf_PmxJoint** out_joints,
                                                size_t* out_count) {
  if (!model || !out_joints || !out_count) {
    return MMDGLTF_ERROR_INVALID_ARGUMENT;
  }
  *out_joints = model->joints;
  *out_count = model->joint_count;
  return MMDGLTF_OK;
}
