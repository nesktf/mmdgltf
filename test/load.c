#include "paths.h"
#include "tests.h"

#include <stdio.h>

static MunitResult test_load_pmx_file(const MunitParameter* params, void* data) {
  UNUSED(params);
  UNUSED(data);

  mmdgltf_Pmx pmx = mmdgltf_load_file(MMDGLTF_TEST_KYOUKO_PMX);
  munit_assert_ptr_not_null(pmx);
  munit_assert_int(MMDGLTF_CHECK(pmx), ==, MMDGLTF_OK);

  mmdgltf_PmxHeader header;
  mmdgltf_Status ret;
  ret = mmdgltf_pmx_get_header(pmx, &header);
  munit_assert_int(ret, ==, MMDGLTF_OK);
  munit_assert_double_equal(header.version, 2.0, 5);
  munit_assert_int(header.encoding, ==, MMDGLTF_ENCODING_UTF16LE);
  munit_assert_uint8(header.additional_uvs, ==, 0);
  munit_assert_uint8(header.vertex_index_size, ==, 2);
  munit_assert_uint8(header.texture_index_size, ==, 1);
  munit_assert_uint8(header.material_index_size, ==, 1);
  munit_assert_uint8(header.bone_index_size, ==, 1);
  munit_assert_uint8(header.morph_index_size, ==, 1);
  munit_assert_uint8(header.rigid_index_size, ==, 1);

  mmdgltf_PmxModelInfo info;
  ret = mmdgltf_pmx_get_model_info(pmx, &info);
  munit_assert_int(ret, ==, MMDGLTF_OK);
  munit_assert_string_equal(info.name_jp, "ぎゃてコラ風幽谷響子");
  munit_assert_string_equal(info.name_en, "");
  munit_assert_ptr_not_null(info.comment_jp);
  munit_assert_ptr_not_null(info.comment_en);

  const mmdgltf_PmxVertex* vertices = NULL;
  size_t vertex_count = 0;
  ret = mmdgltf_pmx_vertices(pmx, &vertices, &vertex_count);
  munit_assert_int(ret, ==, MMDGLTF_OK);
  munit_assert_size(vertex_count, ==, 4434);
  munit_assert_ptr_not_null(vertices);
  munit_assert_int(vertices[0].weight_type, ==, MMDGLTF_WEIGHT_BDEF1);
  munit_assert_int32(vertices[0].bones[0], >=, 0);
  munit_assert_float(vertices[0].weights[0], ==, 1.0f);
  munit_assert_float(vertices[0].uv.x, >=, 0.0f);

  const uint32_t* indices = NULL;
  size_t index_count = 0;
  ret = mmdgltf_pmx_indices(pmx, &indices, &index_count);
  munit_assert_int(ret, ==, MMDGLTF_OK);
  munit_assert_size(index_count, ==, 22143);
  munit_assert_ptr_not_null(indices);
  for (size_t i = 0; i < 100; ++i) {
    munit_assert_uint32(indices[i], <, 4434);
  }

  const mmdgltf_PmxTexture* textures = NULL;
  size_t texture_count = 0;
  ret = mmdgltf_pmx_textures(pmx, &textures, &texture_count);
  munit_assert_int(ret, ==, MMDGLTF_OK);
  munit_assert_size(texture_count, ==, 5);
  munit_assert_ptr_not_null(textures);
  munit_assert_string_equal(textures[0].path, "tx1.png");
  munit_assert_string_equal(textures[1].path, "tx2.png");
  for (size_t i = 0; i < texture_count; ++i) {
    munit_assert_ptr_not_null(textures[i].path);
    munit_assert_null(strchr(textures[i].path, '\\'));
  }

  const mmdgltf_PmxMaterial* materials = NULL;
  size_t material_count = 0;
  ret = mmdgltf_pmx_materials(pmx, &materials, &material_count);
  munit_assert_int(ret, ==, MMDGLTF_OK);
  munit_assert_size(material_count, ==, 5);
  munit_assert_ptr_not_null(materials);
  munit_assert_string_equal(materials[0].name_jp, "通常");
  munit_assert_float(materials[0].diffuse.a, ==, 1.0f);
  munit_assert_uint32(materials[0].index_count, ==, 16401);
  munit_assert_int32(materials[0].texture_index, ==, 0);

  size_t total_mat_indices = 0;
  for (size_t i = 0; i < material_count; ++i) {
    total_mat_indices += materials[i].index_count;
  }
  munit_assert_size(total_mat_indices, ==, 22143);

  const mmdgltf_PmxBone* bones = NULL;
  size_t bone_count = 0;
  ret = mmdgltf_pmx_bones(pmx, &bones, &bone_count);
  munit_assert_int(ret, ==, MMDGLTF_OK);
  munit_assert_size(bone_count, ==, 100);
  munit_assert_ptr_not_null(bones);
  munit_assert_string_equal(bones[0].name_jp, "操作中心");
  munit_assert_int32(bones[0].parent_index, ==, -1);
  munit_assert_float(bones[0].position.x, ==, 0.0f);

  const mmdgltf_PmxMorph* morphs = NULL;
  size_t morph_count = 0;
  ret = mmdgltf_pmx_morphs(pmx, &morphs, &morph_count);
  munit_assert_int(ret, ==, MMDGLTF_OK);
  munit_assert_size(morph_count, ==, 52);
  munit_assert_ptr_not_null(morphs);
  munit_assert_string_equal(morphs[0].name_jp, "眉上げ");
  munit_assert_int(morphs[0].type, ==, MMDGLTF_MORPH_VERTEX);
  munit_assert_uint32(morphs[0].offset_count, ==, 100);
  munit_assert_ptr_not_null(morphs[0].vertex_offsets);

  const mmdgltf_PmxDisplayItem* display_items = NULL;
  size_t display_item_count = 0;
  ret = mmdgltf_pmx_display_items(pmx, &display_items, &display_item_count);
  munit_assert_int(ret, ==, MMDGLTF_OK);
  munit_assert_size(display_item_count, ==, 10);
  munit_assert_ptr_not_null(display_items);
  munit_assert_string_equal(display_items[0].name_jp, "Root");
  munit_assert_uint8(display_items[0].is_special, ==, 1);
  munit_assert_uint32(display_items[0].element_count, ==, 1);

  const mmdgltf_PmxRigidBody* rigid_bodies = NULL;
  size_t rigid_body_count = 0;
  ret = mmdgltf_pmx_rigid_bodies(pmx, &rigid_bodies, &rigid_body_count);
  munit_assert_int(ret, ==, MMDGLTF_OK);
  munit_assert_size(rigid_body_count, ==, 30);
  munit_assert_ptr_not_null(rigid_bodies);
  munit_assert_string_equal(rigid_bodies[0].name_jp, "頭");
  munit_assert_int32(rigid_bodies[0].bone_index, ==, 8);
  munit_assert_int(rigid_bodies[0].shape_type, ==, MMDGLTF_RIGID_SHAPE_SPHERE);
  munit_assert_int(rigid_bodies[0].physics_mode, ==, MMDGLTF_RIGID_MODE_STATIC);
  munit_assert_float(rigid_bodies[0].size.x, >, 0.0f);

  const mmdgltf_PmxJoint* joints = NULL;
  size_t joint_count = 0;
  ret = mmdgltf_pmx_joints(pmx, &joints, &joint_count);
  munit_assert_int(ret, ==, MMDGLTF_OK);
  munit_assert_size(joint_count, ==, 24);
  munit_assert_ptr_not_null(joints);
  munit_assert_string_equal(joints[0].name_jp, "前髪１");
  munit_assert_int32(joints[0].src_rigid, ==, 0);
  munit_assert_int32(joints[0].dest_rigid, ==, 1);
  munit_assert_float(joints[0].position.y, >, 0.0f);

  mmdgltf_pmx_destroy(pmx);

  // same thing with non null terminated string
  pmx = mmdgltf_load_filen(MMDGLTF_TEST_KYOUKO_PMX, strlen(MMDGLTF_TEST_KYOUKO_PMX));
  munit_assert_ptr_not_null(pmx);
  munit_assert_int(MMDGLTF_CHECK(pmx), ==, MMDGLTF_OK);

  munit_assert_int(mmdgltf_pmx_get_model_info(pmx, &info), ==, MMDGLTF_OK);
  munit_assert_string_equal(info.name_jp, "ぎゃてコラ風幽谷響子");

  mmdgltf_pmx_destroy(pmx);

  return MUNIT_OK;
}

static_assert(MMDGLTF_SEEK_CUR == SEEK_CUR, "SEEK_CUR mismatch");
static_assert(MMDGLTF_SEEK_END == SEEK_END, "SEEK_END mismatch");
static_assert(MMDGLTF_SEEK_SET == SEEK_SET, "SEEK_SET mismatch");

static MunitResult test_load_pmx_stream(const MunitParameter* params, void* data) {
  UNUSED(params);
  UNUSED(data);

  FILE* f = fopen(MMDGLTF_TEST_KYOUKO_PMX, "rb");
  munit_assert_ptr_not_null(f);

  const mmdgltf_StreamFunc func = {
    .read = (PFN_mmdgltf_read_func)fread,
    .seek = (PFN_mmdgltf_seek_func)fseek,
    .tell = (PFN_mmdgltf_tell_func)ftell,
    .eof = (PFN_mmdgltf_eof_func)feof,
  };

  const mmdgltf_Pmx pmx = mmdgltf_load_stream(&func, f);
  fclose(f);

  munit_assert_ptr_not_null(pmx);
  munit_assert_int(MMDGLTF_CHECK(pmx), ==, MMDGLTF_OK);

  mmdgltf_PmxModelInfo info;
  munit_assert_int(mmdgltf_pmx_get_model_info(pmx, &info), ==, MMDGLTF_OK);
  munit_assert_string_equal(info.name_jp, "ぎゃてコラ風幽谷響子");

  mmdgltf_pmx_destroy(pmx);
  return MUNIT_OK;
}

static MunitResult test_load_pmx_memory(const MunitParameter* params, void* data) {
  UNUSED(params);
  UNUSED(data);

  FILE* f = fopen(MMDGLTF_TEST_KYOUKO_PMX, "rb");
  munit_assert_ptr_not_null(f);

  fseek(f, 0, SEEK_END);
  const long sz = ftell(f);
  fseek(f, 0, SEEK_SET);

  void* buf = malloc((size_t)sz);
  munit_assert_ptr_not_null(buf);
  munit_assert_size(fread(buf, 1, (size_t)sz, f), ==, (size_t)sz);
  fclose(f);

  const mmdgltf_Pmx pmx = mmdgltf_load_memory(buf, (size_t)sz);
  free(buf);

  munit_assert_ptr_not_null(pmx);
  munit_assert_int(MMDGLTF_CHECK(pmx), ==, MMDGLTF_OK);

  mmdgltf_PmxModelInfo info;
  munit_assert_int(mmdgltf_pmx_get_model_info(pmx, &info), ==, MMDGLTF_OK);
  munit_assert_string_equal(info.name_jp, "ぎゃてコラ風幽谷響子");

  mmdgltf_pmx_destroy(pmx);
  return MUNIT_OK;
}

static MunitResult test_load_pmx_errors(const MunitParameter* params, void* data) {
  UNUSED(params);
  UNUSED(data);

  const mmdgltf_Pmx null_pmx = mmdgltf_load_file(NULL);
  munit_assert_int(MMDGLTF_CHECK(null_pmx), ==, MMDGLTF_ERROR_INVALID_ARGUMENT);

  const mmdgltf_Pmx null_stream = mmdgltf_load_stream(NULL, NULL);
  munit_assert_int(MMDGLTF_CHECK(null_stream), ==, MMDGLTF_ERROR_INVALID_ARGUMENT);

  const mmdgltf_Pmx noent_pmx = mmdgltf_load_file("models/nonexistent.pmx");
  munit_assert_int(MMDGLTF_CHECK(noent_pmx), ==, MMDGLTF_ERROR_IO);

  mmdgltf_Pmx bad_pmx = mmdgltf_load_memory(NULL, 0);
  munit_assert_int(MMDGLTF_CHECK(bad_pmx), ==, MMDGLTF_ERROR_INVALID_ARGUMENT);

  const char bad_sig[33] = "funny random data again hehehaha";
  bad_pmx = mmdgltf_load_memory(bad_sig, sizeof(bad_sig));
  munit_assert_int(MMDGLTF_CHECK(bad_pmx), ==, MMDGLTF_ERROR_INVALID_SIGNATURE);

  munit_assert_int(mmdgltf_pmx_vertices(NULL, NULL, NULL), ==, MMDGLTF_ERROR_INVALID_ARGUMENT);
  munit_assert_int(mmdgltf_pmx_indices(NULL, NULL, NULL), ==, MMDGLTF_ERROR_INVALID_ARGUMENT);
  munit_assert_int(mmdgltf_pmx_textures(NULL, NULL, NULL), ==, MMDGLTF_ERROR_INVALID_ARGUMENT);
  munit_assert_int(mmdgltf_pmx_materials(NULL, NULL, NULL), ==, MMDGLTF_ERROR_INVALID_ARGUMENT);
  munit_assert_int(mmdgltf_pmx_bones(NULL, NULL, NULL), ==, MMDGLTF_ERROR_INVALID_ARGUMENT);
  munit_assert_int(mmdgltf_pmx_morphs(NULL, NULL, NULL), ==, MMDGLTF_ERROR_INVALID_ARGUMENT);
  munit_assert_int(mmdgltf_pmx_display_items(NULL, NULL, NULL), ==,
                   MMDGLTF_ERROR_INVALID_ARGUMENT);
  munit_assert_int(mmdgltf_pmx_rigid_bodies(NULL, NULL, NULL), ==, MMDGLTF_ERROR_INVALID_ARGUMENT);
  munit_assert_int(mmdgltf_pmx_joints(NULL, NULL, NULL), ==, MMDGLTF_ERROR_INVALID_ARGUMENT);

  return MUNIT_OK;
}

static MunitResult test_load_pmd_file(const MunitParameter* params, void* data) {
  UNUSED(params);
  UNUSED(data);

  const mmdgltf_Pmx pmx = mmdgltf_load_file(MMDGLTF_TEST_MIKU_PMD);
  munit_assert_ptr_not_null(pmx);
  munit_assert_int(MMDGLTF_CHECK(pmx), ==, MMDGLTF_OK);

  mmdgltf_PmxHeader header;
  mmdgltf_Status ret = mmdgltf_pmx_get_header(pmx, &header);
  munit_assert_int(ret, ==, MMDGLTF_OK);
  munit_assert_double_equal(header.version, 2.0, 5);
  munit_assert_int(header.encoding, ==, MMDGLTF_ENCODING_UTF8);

  mmdgltf_PmxModelInfo info;
  ret = mmdgltf_pmx_get_model_info(pmx, &info);
  munit_assert_int(ret, ==, MMDGLTF_OK);
  munit_assert_string_equal(info.name_jp, "初音ミク");
  munit_assert_ptr_not_null(info.comment_jp);
  munit_assert_ptr_not_null(strstr(info.comment_jp, "初音ミク"));

  const mmdgltf_PmxVertex* vertices = NULL;
  size_t vertex_count = 0;
  ret = mmdgltf_pmx_vertices(pmx, &vertices, &vertex_count);
  munit_assert_int(ret, ==, MMDGLTF_OK);
  munit_assert_size(vertex_count, ==, 9036);
  munit_assert_ptr_not_null(vertices);
  munit_assert_int(vertices[0].weight_type, ==, MMDGLTF_WEIGHT_BDEF2);
  munit_assert_int32(vertices[0].bones[0], ==, 3);
  munit_assert_int32(vertices[0].bones[1], ==, 0);
  munit_assert_float(vertices[0].weights[0], ==, 1.0f);
  munit_assert_float(vertices[0].weights[1], ==, 0.0f);

  const uint32_t* indices = NULL;
  size_t index_count = 0;
  ret = mmdgltf_pmx_indices(pmx, &indices, &index_count);
  munit_assert_int(ret, ==, MMDGLTF_OK);
  munit_assert_size(index_count, ==, 44991);
  munit_assert_ptr_not_null(indices);
  for (size_t i = 0; i < 100; ++i) {
    munit_assert_uint32(indices[i], <, 9036);
  }

  const mmdgltf_PmxMaterial* materials = NULL;
  size_t material_count = 0;
  ret = mmdgltf_pmx_materials(pmx, &materials, &material_count);
  munit_assert_int(ret, ==, MMDGLTF_OK);
  munit_assert_size(material_count, ==, 17);
  munit_assert_ptr_not_null(materials);
  size_t total_mat_indices = 0;
  for (size_t i = 0; i < material_count; ++i) {
    total_mat_indices += materials[i].index_count;
  }
  munit_assert_size(total_mat_indices, ==, 44991);
  munit_assert_int32(materials[5].texture_index, >=, 0);

  const mmdgltf_PmxTexture* textures = NULL;
  size_t texture_count = 0;
  ret = mmdgltf_pmx_textures(pmx, &textures, &texture_count);
  munit_assert_int(ret, ==, MMDGLTF_OK);
  munit_assert_size(texture_count, >=, 1);
  munit_assert_ptr_not_null(textures);
  munit_assert_string_equal(textures[materials[5].texture_index].path, "eye2.bmp");

  const mmdgltf_PmxBone* bones = NULL;
  size_t bone_count = 0;
  ret = mmdgltf_pmx_bones(pmx, &bones, &bone_count);
  munit_assert_int(ret, ==, MMDGLTF_OK);
  munit_assert_size(bone_count, ==, 122);
  munit_assert_ptr_not_null(bones);
  munit_assert_int32(bones[83].ik_target_index, ==, 40);
  munit_assert_uint32(bones[83].ik_link_count, ==, 2);
  munit_assert_ptr_not_null(bones[83].ik_links);
  munit_assert_int(bones[83].ik_links[0].has_limits, ==, 1);
  munit_assert_float(bones[83].ik_links[0].min_angle.x, <, -3.14f);

  const mmdgltf_PmxMorph* morphs = NULL;
  size_t morph_count = 0;
  ret = mmdgltf_pmx_morphs(pmx, &morphs, &morph_count);
  munit_assert_int(ret, ==, MMDGLTF_OK);
  munit_assert_size(morph_count, ==, 15);
  munit_assert_ptr_not_null(morphs);
  munit_assert_string_equal(morphs[0].name_jp, "真面目");
  munit_assert_int(morphs[0].category, ==, MMDGLTF_MORPH_CATEGORY_EYEBROW);
  munit_assert_int(morphs[0].type, ==, MMDGLTF_MORPH_VERTEX);
  munit_assert_uint32(morphs[0].offset_count, ==, 34);
  munit_assert_ptr_not_null(morphs[0].vertex_offsets);
  munit_assert_uint32(morphs[0].vertex_offsets[0].vertex_index, <, 9036);

  const mmdgltf_PmxRigidBody* rigids = NULL;
  size_t rigid_count = 0;
  ret = mmdgltf_pmx_rigid_bodies(pmx, &rigids, &rigid_count);
  munit_assert_int(ret, ==, MMDGLTF_OK);
  munit_assert_size(rigid_count, ==, 45);

  const mmdgltf_PmxJoint* joints = NULL;
  size_t joint_count = 0;
  ret = mmdgltf_pmx_joints(pmx, &joints, &joint_count);
  munit_assert_int(ret, ==, MMDGLTF_OK);
  munit_assert_size(joint_count, ==, 27);

  mmdgltf_pmx_destroy(pmx);
  return MUNIT_OK;
}

static MunitResult test_load_pmd_sphere_maps(const MunitParameter* params, void* data) {
  UNUSED(params);
  UNUSED(data);

  const mmdgltf_Pmx metal_pmx = mmdgltf_load_file(MMDGLTF_TEST_METAL_PMD);
  munit_assert_ptr_not_null(metal_pmx);
  munit_assert_int(MMDGLTF_CHECK(metal_pmx), ==, MMDGLTF_OK);

  const mmdgltf_PmxMaterial* materials = NULL;
  size_t material_count = 0;
  mmdgltf_Status ret = mmdgltf_pmx_materials(metal_pmx, &materials, &material_count);
  munit_assert_int(ret, ==, MMDGLTF_OK);
  munit_assert_size(material_count, >=, 1);
  munit_assert_int(materials[0].sphere_mode, ==, MMDGLTF_SPHERE_MUL);
  munit_assert_int32(materials[0].sphere_texture_index, >=, 0);

  const mmdgltf_PmxTexture* textures = NULL;
  size_t texture_count = 0;
  ret = mmdgltf_pmx_textures(metal_pmx, &textures, &texture_count);
  munit_assert_int(ret, ==, MMDGLTF_OK);
  munit_assert_string_equal(textures[materials[0].sphere_texture_index].path, "metal.sph");

  mmdgltf_pmx_destroy(metal_pmx);

  const mmdgltf_Pmx luka_pmx = mmdgltf_load_file(MMDGLTF_TEST_LUKA_PMD);
  munit_assert_ptr_not_null(luka_pmx);
  munit_assert_int(MMDGLTF_CHECK(luka_pmx), ==, MMDGLTF_OK);

  ret = mmdgltf_pmx_materials(luka_pmx, &materials, &material_count);
  munit_assert_int(ret, ==, MMDGLTF_OK);
  munit_assert_size(material_count, ==, 31);
  munit_assert_int(materials[1].sphere_mode, ==, MMDGLTF_SPHERE_ADD);
  munit_assert_int32(materials[1].sphere_texture_index, >=, 0);

  ret = mmdgltf_pmx_textures(luka_pmx, &textures, &texture_count);
  munit_assert_int(ret, ==, MMDGLTF_OK);
  munit_assert_string_equal(textures[materials[1].sphere_texture_index].path, "a2.spa");

  mmdgltf_pmx_destroy(luka_pmx);
  return MUNIT_OK;
}

static MunitResult test_load_pmd_dammy(const MunitParameter* params, void* data) {
  UNUSED(params);
  UNUSED(data);

  const mmdgltf_Pmx pmx = mmdgltf_load_file(MMDGLTF_TEST_DAMMY_PMD);
  munit_assert_ptr_not_null(pmx);
  munit_assert_int(MMDGLTF_CHECK(pmx), ==, MMDGLTF_OK);

  mmdgltf_PmxModelInfo info;
  mmdgltf_Status ret = mmdgltf_pmx_get_model_info(pmx, &info);
  munit_assert_int(ret, ==, MMDGLTF_OK);
  munit_assert_string_equal(info.name_jp, "ダミーボーン");

  const mmdgltf_PmxVertex* vertices = NULL;
  size_t vertex_count = 0;
  ret = mmdgltf_pmx_vertices(pmx, &vertices, &vertex_count);
  munit_assert_int(ret, ==, MMDGLTF_OK);
  munit_assert_size(vertex_count, ==, 0);

  const uint32_t* indices = NULL;
  size_t index_count = 0;
  ret = mmdgltf_pmx_indices(pmx, &indices, &index_count);
  munit_assert_int(ret, ==, MMDGLTF_OK);
  munit_assert_size(index_count, ==, 0);

  mmdgltf_pmx_destroy(pmx);
  return MUNIT_OK;
}

static MunitResult test_load_pmd_errors(const MunitParameter* params, void* data) {
  UNUSED(params);
  UNUSED(data);

  const mmdgltf_Pmx null_pmd = mmdgltf_load_file(NULL);
  munit_assert_int(MMDGLTF_CHECK(null_pmd), ==, MMDGLTF_ERROR_INVALID_ARGUMENT);

  const mmdgltf_Pmx null_stream = mmdgltf_load_stream(NULL, NULL);
  munit_assert_int(MMDGLTF_CHECK(null_stream), ==, MMDGLTF_ERROR_INVALID_ARGUMENT);

  const mmdgltf_Pmx noent_pmd = mmdgltf_load_file("models/nonexistent.pmd");
  munit_assert_int(MMDGLTF_CHECK(noent_pmd), ==, MMDGLTF_ERROR_IO);

  const char bad_sig[20] = "XYZ\x00\x00\x80\x3f";
  mmdgltf_Pmx bad_pmd = mmdgltf_load_memory(bad_sig, sizeof(bad_sig));
  munit_assert_int(MMDGLTF_CHECK(bad_pmd), ==, MMDGLTF_ERROR_INVALID_SIGNATURE);

  const char bad_ver[20] = "Pmd\x00\x00\x00\x40";
  bad_pmd = mmdgltf_load_memory(bad_ver, sizeof(bad_ver));
  munit_assert_int(MMDGLTF_CHECK(bad_pmd), ==, MMDGLTF_ERROR_UNSUPPORTED_VERSION);

  return MUNIT_OK;
}

static MunitTest tests[] = {
  {"/load/pmx-file", test_load_pmx_file, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/load/pmx-stream", test_load_pmx_stream, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/load/pmx-memory", test_load_pmx_memory, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/load/pmx-errors", test_load_pmx_errors, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/load/pmd-file", test_load_pmd_file, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/load/pmd-dammy", test_load_pmd_dammy, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/load/pmd-sphere-maps", test_load_pmd_sphere_maps, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/load/pmd-errors", test_load_pmd_errors, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

TestData load_tests = {
  .tests = tests,
  .len = ARRSZ(tests),
};
