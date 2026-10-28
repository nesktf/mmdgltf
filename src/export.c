#include "mmdgltf_internal.h"
#include "tinygltf/tiny_gltf_v3.h"

#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

MMDGLTF_API void mmdgltf_export_config_default(mmdgltf_ExportConfig* out_config) {
  if (!out_config) {
    return;
  }
  out_config->unlit = 0;
  out_config->export_skinning = 1;
  out_config->export_morphs = 1;
  out_config->export_physics = 1;
}

static tg3_str make_str(mmdgltf_Arena* arena, const char* s) {
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

static tg3_value make_str_value(mmdgltf_Arena* arena, const char* s) {
  tg3_value v;
  memset(&v, 0, sizeof(v));
  v.type = TG3_VALUE_STRING;
  v.string_val = make_str(arena, s);
  return v;
}

static tg3_value make_int_value(i64 val) {
  tg3_value v;
  memset(&v, 0, sizeof(v));
  v.type = TG3_VALUE_INT;
  v.int_val = val;
  return v;
}

static tg3_value make_real_value(f64 val) {
  tg3_value v;
  memset(&v, 0, sizeof(v));
  v.type = TG3_VALUE_REAL;
  v.real_val = val;
  return v;
}

static tg3_kv_pair make_kv(mmdgltf_Arena* arena, const char* key, tg3_value val) {
  tg3_kv_pair p;
  p.key = make_str(arena, key);
  p.value = val;
  return p;
}

static mmdgltf_Status build_and_serialize_glb(mmdgltf_Model model,
                                              const mmdgltf_ExportConfig* config,
                                              const char* filename, u32 filename_len,
                                              u8** out_data, size_t* out_size) {
  if (!model) {
    return MMDGLTF_ERROR_INVALID_ARGUMENT;
  }
  const mmdgltf_Status model_status = MMDGLTF_CHECK(model);
  if (model_status != MMDGLTF_OK) {
    return model_status;
  }

  mmdgltf_ExportConfig cfg;
  if (config) {
    cfg = *config;
  } else {
    mmdgltf_export_config_default(&cfg);
  }

  mmdgltf_VertexBuffers bufs;
  mmdgltf_Status st = mmdgltf_model_vertex_buffers(model, &bufs);
  if (st != MMDGLTF_OK) {
    return st;
  }

  const u32* indices = NULL;
  size_t index_count = 0;
  st = mmdgltf_model_indices(model, &indices, &index_count);
  if (st != MMDGLTF_OK) {
    return st;
  }

  const mmdgltf_PmxMaterial* materials = NULL;
  size_t material_count = 0;
  st = mmdgltf_model_materials(model, &materials, &material_count);
  if (st != MMDGLTF_OK) {
    return st;
  }

  const mmdgltf_PmxBone* bones = NULL;
  size_t bone_count = 0;
  st = mmdgltf_model_bones(model, &bones, &bone_count);
  if (st != MMDGLTF_OK) {
    return st;
  }

  const mmdgltf_PmxMorph* morphs = NULL;
  size_t morph_count = 0;
  st = mmdgltf_model_morphs(model, &morphs, &morph_count);
  if (st != MMDGLTF_OK) {
    return st;
  }

  const mmdgltf_PmxRigidBody* rigid_bodies = NULL;
  size_t rigid_body_count = 0;
  st = mmdgltf_model_rigid_bodies(model, &rigid_bodies, &rigid_body_count);
  if (st != MMDGLTF_OK) {
    return st;
  }

  const mmdgltf_PmxJoint* joints = NULL;
  size_t joint_count = 0;
  st = mmdgltf_model_joints(model, &joints, &joint_count);
  if (st != MMDGLTF_OK) {
    return st;
  }

  const mmdgltf_Image* images = NULL;
  size_t image_count = 0;
  st = mmdgltf_model_images(model, &images, &image_count);
  if (st != MMDGLTF_OK) {
    return st;
  }

  mmdgltf_Arena* arena = mmdgltf__arena_create(256 * 1024);
  if (!arena) {
    return MMDGLTF_ERROR_OUT_OF_MEMORY;
  }

  const size_t vcount = bufs.vertex_count;
  const bool has_skinning = (cfg.export_skinning && bone_count > 0 && bufs.joints && bufs.weights);

  size_t vertex_morph_count = 0;
  size_t* vertex_morph_indices = NULL;
  if (cfg.export_morphs && morph_count > 0) {
    vertex_morph_indices = mmdgltf__arena_alloc(arena, morph_count * sizeof(size_t));
    for (size_t i = 0; i < morph_count; ++i) {
      if (morphs[i].type == MMDGLTF_MORPH_VERTEX) {
        vertex_morph_indices[vertex_morph_count++] = i;
      }
    }
  }

  mmdgltf__ByteVec bw;
  memset(&bw, 0, sizeof(bw));

  const size_t pos_offset = mmdgltf__bytevec_append(&bw, bufs.positions, vcount * 3 * sizeof(f32));
  const size_t norm_offset = mmdgltf__bytevec_append(&bw, bufs.normals, vcount * 3 * sizeof(f32));
  const size_t uv_offset = mmdgltf__bytevec_append(&bw, bufs.uvs, vcount * 2 * sizeof(f32));

  size_t joints_offset = 0;
  size_t weights_offset = 0;
  size_t ibm_offset = 0;
  if (has_skinning) {
    joints_offset =
      mmdgltf__bytevec_append(&bw, bufs.joints, vcount * MMDGLTF_MAX_BONE_WEIGHTS * sizeof(u16));
    weights_offset =
      mmdgltf__bytevec_append(&bw, bufs.weights, vcount * MMDGLTF_MAX_BONE_WEIGHTS * sizeof(f32));

    f32* ibm_data = mmdgltf__arena_alloc(arena, bone_count * 16 * sizeof(f32));
    for (size_t i = 0; i < bone_count; ++i) {
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
    ibm_offset = mmdgltf__bytevec_append(&bw, ibm_data, bone_count * 16 * sizeof(f32));
  }

  const size_t indices_offset = mmdgltf__bytevec_append(&bw, indices, index_count * sizeof(u32));

  size_t* morph_offsets = NULL;
  f32** morph_deltas = NULL;
  f64** morph_mins = NULL;
  f64** morph_maxs = NULL;
  if (vertex_morph_count > 0) {
    morph_offsets = mmdgltf__arena_alloc(arena, vertex_morph_count * sizeof(size_t));
    morph_deltas = mmdgltf__arena_alloc(arena, vertex_morph_count * sizeof(f32*));
    morph_mins = mmdgltf__arena_alloc(arena, vertex_morph_count * sizeof(f64*));
    morph_maxs = mmdgltf__arena_alloc(arena, vertex_morph_count * sizeof(f64*));

    const bool is_rh = (model->config.coordinate_system == MMDGLTF_COORDS_GLTF_RIGHT_HANDED);
    const f32 scale = model->config.scale > 0.0f ? model->config.scale : 1.0f;

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
          const f32 dz =
            is_rh ? (-off->position_offset.z * scale) : (off->position_offset.z * scale);
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

      morph_deltas[m] = deltas;
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
  if (image_count > 0) {
    image_bv_offsets = mmdgltf__arena_alloc(arena, image_count * sizeof(size_t));
    image_to_bv = mmdgltf__arena_alloc(arena, image_count * sizeof(i32));
    for (size_t i = 0; i < image_count; ++i) {
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
  for (size_t i = 0; i < image_count; ++i) {
    if (image_to_bv[i] >= 0) {
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
  }

  const u32 bv_indices = cur_bv++;
  bvs[bv_indices].buffer = 0;
  bvs[bv_indices].byte_offset = indices_offset;
  bvs[bv_indices].byte_length = index_count * sizeof(u32);
  bvs[bv_indices].target = 34963;

  if (has_skinning) {
    bv_ibm = cur_bv++;
    bvs[bv_ibm].buffer = 0;
    bvs[bv_ibm].byte_offset = ibm_offset;
    bvs[bv_ibm].byte_length = bone_count * 16 * sizeof(f32);
    bvs[bv_ibm].target = 0;
  }

  u32* morph_bvs = NULL;
  if (vertex_morph_count > 0) {
    morph_bvs = mmdgltf__arena_alloc(arena, vertex_morph_count * sizeof(u32));
    for (size_t m = 0; m < vertex_morph_count; ++m) {
      morph_bvs[m] = cur_bv++;
      bvs[morph_bvs[m]].buffer = 0;
      bvs[morph_bvs[m]].byte_offset = morph_offsets[m];
      bvs[morph_bvs[m]].byte_length = vcount * 3 * sizeof(f32);
      bvs[morph_bvs[m]].target = 34962;
    }
  }

  if (image_count > 0) {
    for (size_t i = 0; i < image_count; ++i) {
      if (image_to_bv[i] >= 0) {
        const u32 ibv = cur_bv++;
        image_to_bv[i] = (i32)ibv;
        bvs[ibv].buffer = 0;
        bvs[ibv].byte_offset = image_bv_offsets[i];
        bvs[ibv].byte_length = images[i].size;
        bvs[ibv].target = 0;
      }
    }
  }

  size_t num_accessors = 3 + material_count;
  if (has_skinning)
    num_accessors += 3;
  num_accessors += vertex_morph_count;

  tg3_accessor* accs = mmdgltf__arena_alloc(arena, num_accessors * sizeof(tg3_accessor));
  memset(accs, 0, num_accessors * sizeof(tg3_accessor));

  u32 cur_acc = 0;

  f64 pos_min[3] = {DBL_MAX, DBL_MAX, DBL_MAX};
  f64 pos_max[3] = {-DBL_MAX, -DBL_MAX, -DBL_MAX};
  for (size_t i = 0; i < vcount; ++i) {
    const f32 px = bufs.positions[i * 3 + 0];
    const f32 py = bufs.positions[i * 3 + 1];
    const f32 pz = bufs.positions[i * 3 + 2];
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
  }
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
    accs[acc_ibm].count = (u32)bone_count;
    accs[acc_ibm].type = TG3_TYPE_MAT4;
  }

  u32* prim_indices_acc = mmdgltf__arena_alloc(arena, material_count * sizeof(u32));
  size_t running_index_start = 0;
  for (size_t p = 0; p < material_count; ++p) {
    const u32 a_idx = cur_acc++;
    prim_indices_acc[p] = a_idx;
    accs[a_idx].buffer_view = (i32)bv_indices;
    accs[a_idx].byte_offset = running_index_start * sizeof(u32);
    accs[a_idx].component_type = TG3_COMPONENT_TYPE_UNSIGNED_INT;
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
  shared_attribs[0].key = make_str(arena, "POSITION");
  shared_attribs[0].value = (i32)acc_pos;
  shared_attribs[1].key = make_str(arena, "NORMAL");
  shared_attribs[1].value = (i32)acc_norm;
  shared_attribs[2].key = make_str(arena, "TEXCOORD_0");
  shared_attribs[2].value = (i32)acc_uv;
  if (has_skinning) {
    shared_attribs[3].key = make_str(arena, "JOINTS_0");
    shared_attribs[3].value = (i32)acc_joints;
    shared_attribs[4].key = make_str(arena, "WEIGHTS_0");
    shared_attribs[4].value = (i32)acc_weights;
  }

  tg3_str_int_pair** shared_targets = NULL;
  u32* shared_target_counts = NULL;
  if (vertex_morph_count > 0) {
    shared_targets = mmdgltf__arena_alloc(arena, vertex_morph_count * sizeof(tg3_str_int_pair*));
    shared_target_counts = mmdgltf__arena_alloc(arena, vertex_morph_count * sizeof(u32));
    for (size_t m = 0; m < vertex_morph_count; ++m) {
      tg3_str_int_pair* target_pair = mmdgltf__arena_alloc(arena, sizeof(tg3_str_int_pair));
      target_pair->key = make_str(arena, "POSITION");
      target_pair->value = (i32)morph_accs[m];
      shared_targets[m] = target_pair;
      shared_target_counts[m] = 1;
    }
  }

  tg3_primitive* primitives = mmdgltf__arena_alloc(arena, material_count * sizeof(tg3_primitive));
  memset(primitives, 0, material_count * sizeof(tg3_primitive));
  for (size_t p = 0; p < material_count; ++p) {
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
  meshes[0].name = make_str(arena, "ModelMesh");
  meshes[0].primitives = primitives;
  meshes[0].primitives_count = (u32)material_count;
  if (vertex_morph_count > 0) {
    f64* weights = mmdgltf__arena_alloc(arena, vertex_morph_count * sizeof(f64));
    memset(weights, 0, vertex_morph_count * sizeof(f64));
    meshes[0].weights = weights;
    meshes[0].weights_count = (u32)vertex_morph_count;

    tg3_str* target_names = mmdgltf__arena_alloc(arena, vertex_morph_count * sizeof(tg3_str));
    for (size_t m = 0; m < vertex_morph_count; ++m) {
      const mmdgltf_PmxMorph* morph = &morphs[vertex_morph_indices[m]];
      target_names[m] = make_str(arena, morph->name_jp ? morph->name_jp : "morph");
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
    mesh_extras_kv[0] = make_kv(arena, "targetNames", target_names_val);

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
  if (image_count > 0) {
    gltf_images = mmdgltf__arena_alloc(arena, image_count * sizeof(tg3_image));
    gltf_textures = mmdgltf__arena_alloc(arena, image_count * sizeof(tg3_texture));
    memset(gltf_images, 0, image_count * sizeof(tg3_image));
    memset(gltf_textures, 0, image_count * sizeof(tg3_texture));

    gltf_samplers = mmdgltf__arena_alloc(arena, sizeof(tg3_sampler));
    memset(gltf_samplers, 0, sizeof(tg3_sampler));
    gltf_samplers[0].wrap_s = 10497;
    gltf_samplers[0].wrap_t = 10497;

    for (size_t i = 0; i < image_count; ++i) {
      gltf_images[i].buffer_view = image_to_bv[i];
      gltf_images[i].mime_type = make_str(arena, images[i].mime_type);
      gltf_images[i].name = make_str(arena, images[i].path);

      gltf_textures[i].source = (i32)i;
      gltf_textures[i].sampler = 0;
    }
  }

  tg3_material* gltf_materials =
    mmdgltf__arena_alloc(arena, material_count * sizeof(tg3_material));
  memset(gltf_materials, 0, material_count * sizeof(tg3_material));

  tg3_extension unlit_ext;
  memset(&unlit_ext, 0, sizeof(unlit_ext));
  unlit_ext.name = make_str(arena, "KHR_materials_unlit");
  unlit_ext.value.type = TG3_VALUE_OBJECT;
  unlit_ext.value.object_count = 0;
  unlit_ext.value.object_data = NULL;

  for (size_t p = 0; p < material_count; ++p) {
    const mmdgltf_PmxMaterial* mat = &materials[p];
    gltf_materials[p].name = make_str(arena, mat->name_jp ? mat->name_jp : "Material");
    gltf_materials[p].double_sided = (mat->draw_flags & MMDGLTF_MATERIAL_DOUBLE_SIDED) ? 1 : 0;
    gltf_materials[p].alpha_mode =
      mat->diffuse.a < 1.0f ? make_str(arena, "BLEND") : make_str(arena, "OPAQUE");
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

    if (mat->texture_index >= 0 && (size_t)mat->texture_index < image_count &&
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

  const size_t total_nodes = 1 + (has_skinning ? bone_count : 0);
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

  nodes[0].name = make_str(arena, "Model");
  nodes[0].mesh = 0;
  nodes[0].skin = has_skinning ? 0 : -1;

  size_t root_bone_count = 0;
  if (has_skinning) {
    u32* child_counts = mmdgltf__arena_alloc(arena, bone_count * sizeof(u32));
    memset(child_counts, 0, bone_count * sizeof(u32));

    for (size_t b = 0; b < bone_count; ++b) {
      const i32 parent = bones[b].parent_index;
      if (parent >= 0 && (size_t)parent < bone_count) {
        child_counts[parent]++;
      } else {
        root_bone_count++;
      }
    }

    i32** children_arrays = mmdgltf__arena_alloc(arena, bone_count * sizeof(i32*));
    u32* child_cursor = mmdgltf__arena_alloc(arena, bone_count * sizeof(u32));
    memset(child_cursor, 0, bone_count * sizeof(u32));

    for (size_t b = 0; b < bone_count; ++b) {
      if (child_counts[b] > 0) {
        children_arrays[b] = mmdgltf__arena_alloc(arena, child_counts[b] * sizeof(i32));
      } else {
        children_arrays[b] = NULL;
      }
    }

    for (size_t b = 0; b < bone_count; ++b) {
      const i32 parent = bones[b].parent_index;
      if (parent >= 0 && (size_t)parent < bone_count) {
        children_arrays[parent][child_cursor[parent]++] = (i32)(b + 1);
      }
    }

    for (size_t b = 0; b < bone_count; ++b) {
      const size_t n_idx = b + 1;
      nodes[n_idx].name = make_str(arena, bones[b].name_jp ? bones[b].name_jp : "Bone");
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
      if (parent >= 0 && (size_t)parent < bone_count) {
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
    skins[0].name = make_str(arena, "Armature");
    skins[0].inverse_bind_matrices = (i32)acc_ibm;
    skins[0].skeleton = -1;

    i32* joint_nodes = mmdgltf__arena_alloc(arena, bone_count * sizeof(i32));
    for (size_t b = 0; b < bone_count; ++b) {
      joint_nodes[b] = (i32)(b + 1);
    }
    skins[0].joints = joint_nodes;
    skins[0].joints_count = (u32)bone_count;
  }

  const size_t scene_nodes_count = 1 + root_bone_count;
  i32* scene_nodes = mmdgltf__arena_alloc(arena, scene_nodes_count * sizeof(i32));
  scene_nodes[0] = 0;
  size_t sn_idx = 1;
  if (has_skinning) {
    for (size_t b = 0; b < bone_count; ++b) {
      if (bones[b].parent_index < 0 || (size_t)bones[b].parent_index >= bone_count) {
        scene_nodes[sn_idx++] = (i32)(b + 1);
      }
    }
  }

  tg3_scene* scenes = mmdgltf__arena_alloc(arena, sizeof(tg3_scene));
  memset(scenes, 0, sizeof(tg3_scene));
  scenes[0].name = make_str(arena, "Scene");
  scenes[0].nodes = scene_nodes;
  scenes[0].nodes_count = (u32)scene_nodes_count;

  if (cfg.export_physics && (rigid_body_count > 0 || joint_count > 0)) {
    tg3_value mmd_extras;
    memset(&mmd_extras, 0, sizeof(mmd_extras));
    mmd_extras.type = TG3_VALUE_OBJECT;

    tg3_kv_pair* mmd_pairs = mmdgltf__arena_alloc(arena, 2 * sizeof(tg3_kv_pair));
    u32 mmd_pairs_count = 0;

    if (rigid_body_count > 0) {
      tg3_value rb_arr;
      memset(&rb_arr, 0, sizeof(rb_arr));
      rb_arr.type = TG3_VALUE_ARRAY;
      tg3_value* rb_items = mmdgltf__arena_alloc(arena, rigid_body_count * sizeof(tg3_value));

      for (size_t i = 0; i < rigid_body_count; ++i) {
        const mmdgltf_PmxRigidBody* rb = &rigid_bodies[i];
        tg3_kv_pair* rb_kv = mmdgltf__arena_alloc(arena, 6 * sizeof(tg3_kv_pair));
        rb_kv[0] = make_kv(arena, "name", make_str_value(arena, rb->name_jp));
        rb_kv[1] = make_kv(arena, "bone_index", make_int_value(rb->bone_index));
        rb_kv[2] = make_kv(arena, "shape_type", make_int_value(rb->shape_type));
        rb_kv[3] = make_kv(arena, "mass", make_real_value(rb->mass));
        rb_kv[4] = make_kv(arena, "collision_group", make_int_value(rb->collision_group));
        rb_kv[5] = make_kv(arena, "physics_mode", make_int_value(rb->physics_mode));

        rb_items[i].type = TG3_VALUE_OBJECT;
        rb_items[i].object_data = rb_kv;
        rb_items[i].object_count = 6;
      }
      rb_arr.array_data = rb_items;
      rb_arr.array_count = (u32)rigid_body_count;
      mmd_pairs[mmd_pairs_count++] = make_kv(arena, "rigid_bodies", rb_arr);
    }

    if (joint_count > 0) {
      tg3_value j_arr;
      memset(&j_arr, 0, sizeof(j_arr));
      j_arr.type = TG3_VALUE_ARRAY;
      tg3_value* j_items = mmdgltf__arena_alloc(arena, joint_count * sizeof(tg3_value));

      for (size_t i = 0; i < joint_count; ++i) {
        const mmdgltf_PmxJoint* jt = &joints[i];
        tg3_kv_pair* j_kv = mmdgltf__arena_alloc(arena, 4 * sizeof(tg3_kv_pair));
        j_kv[0] = make_kv(arena, "name", make_str_value(arena, jt->name_jp));
        j_kv[1] = make_kv(arena, "src_rigid", make_int_value(jt->src_rigid));
        j_kv[2] = make_kv(arena, "dest_rigid", make_int_value(jt->dest_rigid));
        j_kv[3] = make_kv(arena, "type", make_int_value(jt->type));

        j_items[i].type = TG3_VALUE_OBJECT;
        j_items[i].object_data = j_kv;
        j_items[i].object_count = 4;
      }
      j_arr.array_data = j_items;
      j_arr.array_count = (u32)joint_count;
      mmd_pairs[mmd_pairs_count++] = make_kv(arena, "joints", j_arr);
    }

    mmd_extras.object_data = mmd_pairs;
    mmd_extras.object_count = mmd_pairs_count;

    tg3_kv_pair* scene_extras_kv = mmdgltf__arena_alloc(arena, sizeof(tg3_kv_pair));
    scene_extras_kv[0] = make_kv(arena, "mmd", mmd_extras);

    tg3_value* scene_extras = mmdgltf__arena_alloc(arena, sizeof(tg3_value));
    memset(scene_extras, 0, sizeof(tg3_value));
    scene_extras->type = TG3_VALUE_OBJECT;
    scene_extras->object_data = scene_extras_kv;
    scene_extras->object_count = 1;
    scenes[0].ext.extras = scene_extras;
  }

  tg3_model gltf_model;
  memset(&gltf_model, 0, sizeof(gltf_model));

  gltf_model.asset.version = make_str(arena, "2.0");
  gltf_model.asset.generator = make_str(arena, "mmdgltf");

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
  gltf_model.materials_count = (u32)material_count;

  gltf_model.nodes = nodes;
  gltf_model.nodes_count = (u32)total_nodes;

  gltf_model.skins = skins;
  gltf_model.skins_count = has_skinning ? 1 : 0;

  gltf_model.scenes = scenes;
  gltf_model.scenes_count = 1;
  gltf_model.default_scene = 0;

  gltf_model.images = gltf_images;
  gltf_model.images_count = (u32)image_count;

  gltf_model.textures = gltf_textures;
  gltf_model.textures_count = (u32)image_count;

  gltf_model.samplers = gltf_samplers;
  gltf_model.samplers_count = (image_count > 0) ? 1 : 0;

  if (cfg.unlit) {
    tg3_str* exts_used = mmdgltf__arena_alloc(arena, sizeof(tg3_str));
    exts_used[0] = make_str(arena, "KHR_materials_unlit");
    gltf_model.extensions_used = exts_used;
    gltf_model.extensions_used_count = 1;
  }

  tg3_write_options write_opts;
  tg3_write_options_init(&write_opts);
  write_opts.write_binary = 1;
  write_opts.embed_buffers = 0;
  write_opts.embed_images = 0;

  tg3_error_stack errors;
  tg3_error_stack_init(&errors);

  tg3_error_code err = TG3_OK;
  if (filename) {
    err = tg3_write_to_file(&gltf_model, &errors, filename, filename_len, &write_opts);
  } else if (out_data && out_size) {
    u64 written_size = 0;
    err = tg3_write_to_memory(&gltf_model, &errors, out_data, &written_size, &write_opts);
    *out_size = (size_t)written_size;
  }

  tg3_error_stack_free(&errors);

  if (bw.data) {
    MMDGLTF_FREE(bw.data, bw.capacity);
  }
  mmdgltf__arena_destroy(arena);

  if (err != TG3_OK) {
    return MMDGLTF_ERROR_IO;
  }

  return MMDGLTF_OK;
}

MMDGLTF_API mmdgltf_Status mmdgltf_export_glb_filen(mmdgltf_Model model, const char* filename,
                                                    size_t filename_len,
                                                    const mmdgltf_ExportConfig* config) {
  if (!filename || filename_len == 0) {
    return MMDGLTF_ERROR_INVALID_ARGUMENT;
  }
  return build_and_serialize_glb(model, config, filename, (u32)filename_len, NULL, NULL);
}

MMDGLTF_API mmdgltf_Status mmdgltf_export_glb_file(mmdgltf_Model model, const char* filename,
                                                   const mmdgltf_ExportConfig* config) {
  if (!filename) {
    return MMDGLTF_ERROR_INVALID_ARGUMENT;
  }
  return mmdgltf_export_glb_filen(model, filename, strlen(filename), config);
}

MMDGLTF_API mmdgltf_Status mmdgltf_export_glb_memory(mmdgltf_Model model, uint8_t** out_data,
                                                     size_t* out_size,
                                                     const mmdgltf_ExportConfig* config) {
  if (!out_data || !out_size) {
    return MMDGLTF_ERROR_INVALID_ARGUMENT;
  }
  return build_and_serialize_glb(model, config, NULL, 0, out_data, out_size);
}

MMDGLTF_API void mmdgltf_export_free(uint8_t* data, size_t size) {
  if (!data) {
    return;
  }
  tg3_write_free(data, NULL);
  UNUSED(size);
}
