#include "paths.h"
#include "tests.h"

static MunitResult test_model_create_default(const MunitParameter* params, void* data) {
  UNUSED(params);
  UNUSED(data);

  mmdgltf_Pmx pmx = mmdgltf_load_file(MMDGLTF_TEST_NITORI_PMX);
  munit_assert_ptr_not_null(pmx);
  munit_assert_int(MMDGLTF_CHECK(pmx), ==, MMDGLTF_OK);

  mmdgltf_Model model = mmdgltf_model_create(pmx, NULL);
  munit_assert_ptr_not_null(model);
  munit_assert_int(MMDGLTF_CHECK(model), ==, MMDGLTF_OK);

  // destroy pmx to ensure the data is self-contained
  mmdgltf_pmx_destroy(pmx);

  mmdgltf_VertexBuffers bufs;
  mmdgltf_Status ret = mmdgltf_model_vertex_buffers(model, &bufs);
  munit_assert_int(ret, ==, MMDGLTF_OK);
  munit_assert_size(bufs.vertex_count, ==, 29685);
  munit_assert_ptr_not_null(bufs.positions);
  munit_assert_ptr_not_null(bufs.normals);
  munit_assert_ptr_not_null(bufs.uvs);
  munit_assert_ptr_not_null(bufs.joints);
  munit_assert_ptr_not_null(bufs.weights);

  const uint32_t* indices = NULL;
  size_t index_count = 0;
  ret = mmdgltf_model_indices(model, &indices, &index_count);
  munit_assert_int(ret, ==, MMDGLTF_OK);
  munit_assert_size(index_count, ==, 131748);
  munit_assert_ptr_not_null(indices);

  const mmdgltf_PmxMaterial* materials = NULL;
  size_t material_count = 0;
  ret = mmdgltf_model_materials(model, &materials, &material_count);
  munit_assert_int(ret, ==, MMDGLTF_OK);
  munit_assert_size(material_count, ==, 47);
  munit_assert_ptr_not_null(materials);
  munit_assert_string_equal(materials[0].name_jp, "ツメ");

  const mmdgltf_PmxBone* bones = NULL;
  size_t bone_count = 0;
  ret = mmdgltf_model_bones(model, &bones, &bone_count);
  munit_assert_int(ret, ==, MMDGLTF_OK);
  munit_assert_size(bone_count, ==, 301);
  munit_assert_ptr_not_null(bones);
  munit_assert_string_equal(bones[0].name_jp, "全ての親");

  const mmdgltf_PmxMorph* morphs = NULL;
  size_t morph_count = 0;
  ret = mmdgltf_model_morphs(model, &morphs, &morph_count);
  munit_assert_int(ret, ==, MMDGLTF_OK);
  munit_assert_size(morph_count, ==, 168);
  munit_assert_ptr_not_null(morphs);

  const mmdgltf_PmxRigidBody* rigid_bodies = NULL;
  size_t rigid_body_count = 0;
  ret = mmdgltf_model_rigid_bodies(model, &rigid_bodies, &rigid_body_count);
  munit_assert_int(ret, ==, MMDGLTF_OK);
  munit_assert_size(rigid_body_count, ==, 81);
  munit_assert_ptr_not_null(rigid_bodies);
  munit_assert_string_equal(rigid_bodies[0].name_jp, "左胸親");

  const mmdgltf_PmxJoint* joints = NULL;
  size_t joint_count = 0;
  ret = mmdgltf_model_joints(model, &joints, &joint_count);
  munit_assert_int(ret, ==, MMDGLTF_OK);
  munit_assert_size(joint_count, ==, 39);
  munit_assert_ptr_not_null(joints);
  munit_assert_string_equal(joints[0].name_jp, "左胸上");

  const mmdgltf_Image* images = NULL;
  size_t image_count = 0;
  ret = mmdgltf_model_images(model, &images, &image_count);
  munit_assert_int(ret, ==, MMDGLTF_OK);
  munit_assert_size(image_count, ==, 27);
  munit_assert_ptr_not_null(images);
  for (size_t i = 0; i < image_count; ++i) {
    munit_assert_ptr_not_null(images[i].data);
    munit_assert_size(images[i].size, >, 0);
    munit_assert_ptr_not_null(images[i].mime_type);
    munit_assert_int(images[i].width, >, 0);
    munit_assert_int(images[i].height, >, 0);
    munit_assert_int(images[i].channels, >, 0);
    munit_assert_ptr_not_null(images[i].path);
    const bool valid_mime = (strcmp(images[i].mime_type, "image/png") == 0 ||
                             strcmp(images[i].mime_type, "image/jpeg") == 0);
    munit_assert_true(valid_mime);
  }

  mmdgltf_model_destroy(model);
  return MUNIT_OK;
}

static MunitResult test_model_coordinate_transforms(const MunitParameter* params, void* data) {
  UNUSED(params);
  UNUSED(data);

  mmdgltf_Pmx pmx = mmdgltf_load_file(MMDGLTF_TEST_NITORI_PMX);
  munit_assert_ptr_not_null(pmx);

  mmdgltf_ModelConfig cfg_rh;
  mmdgltf_model_config_default(&cfg_rh);
  cfg_rh.coordinate_system = MMDGLTF_COORDS_GLTF_RIGHT_HANDED;
  cfg_rh.scale = 2.0f;

  mmdgltf_ModelConfig cfg_lh;
  mmdgltf_model_config_default(&cfg_lh);
  cfg_lh.coordinate_system = MMDGLTF_COORDS_MMD_LEFT_HANDED;
  cfg_lh.scale = 1.0f;

  mmdgltf_Model model_rh = mmdgltf_model_create(pmx, &cfg_rh);
  mmdgltf_Model model_lh = mmdgltf_model_create(pmx, &cfg_lh);
  munit_assert_ptr_not_null(model_rh);
  munit_assert_ptr_not_null(model_lh);

  mmdgltf_VertexBuffers bufs_rh;
  mmdgltf_VertexBuffers bufs_lh;
  munit_assert_int(mmdgltf_model_vertex_buffers(model_rh, &bufs_rh), ==, MMDGLTF_OK);
  munit_assert_int(mmdgltf_model_vertex_buffers(model_lh, &bufs_lh), ==, MMDGLTF_OK);

  for (size_t i = 0; i < 50; ++i) {
    const size_t i3 = i * 3;
    munit_assert_float(bufs_rh.positions[i3 + 0], ==, bufs_lh.positions[i3 + 0] * 2.0f);
    munit_assert_float(bufs_rh.positions[i3 + 1], ==, bufs_lh.positions[i3 + 1] * 2.0f);
    munit_assert_float(bufs_rh.positions[i3 + 2], ==, -bufs_lh.positions[i3 + 2] * 2.0f);

    munit_assert_float(bufs_rh.normals[i3 + 0], ==, bufs_lh.normals[i3 + 0]);
    munit_assert_float(bufs_rh.normals[i3 + 1], ==, bufs_lh.normals[i3 + 1]);
    munit_assert_float(bufs_rh.normals[i3 + 2], ==, -bufs_lh.normals[i3 + 2]);
  }

  const uint32_t* idx_rh = NULL;
  const uint32_t* idx_lh = NULL;
  size_t count_rh = 0;
  size_t count_lh = 0;
  munit_assert_int(mmdgltf_model_indices(model_rh, &idx_rh, &count_rh), ==, MMDGLTF_OK);
  munit_assert_int(mmdgltf_model_indices(model_lh, &idx_lh, &count_lh), ==, MMDGLTF_OK);
  munit_assert_size(count_rh, ==, count_lh);

  for (size_t t = 0; t + 2 < 300; t += 3) {
    munit_assert_uint32(idx_rh[t + 0], ==, idx_lh[t + 0]);
    munit_assert_uint32(idx_rh[t + 1], ==, idx_lh[t + 2]);
    munit_assert_uint32(idx_rh[t + 2], ==, idx_lh[t + 1]);
  }

  const mmdgltf_PmxBone* bones_rh = NULL;
  const mmdgltf_PmxBone* bones_lh = NULL;
  size_t bcount_rh = 0;
  size_t bcount_lh = 0;
  munit_assert_int(mmdgltf_model_bones(model_rh, &bones_rh, &bcount_rh), ==, MMDGLTF_OK);
  munit_assert_int(mmdgltf_model_bones(model_lh, &bones_lh, &bcount_lh), ==, MMDGLTF_OK);
  for (size_t i = 0; i < 20; ++i) {
    munit_assert_float(bones_rh[i].position.x, ==, bones_lh[i].position.x * 2.0f);
    munit_assert_float(bones_rh[i].position.y, ==, bones_lh[i].position.y * 2.0f);
    munit_assert_float(bones_rh[i].position.z, ==, -bones_lh[i].position.z * 2.0f);
  }

  mmdgltf_model_destroy(model_rh);
  mmdgltf_model_destroy(model_lh);
  mmdgltf_pmx_destroy(pmx);

  return MUNIT_OK;
}

static MunitResult test_model_selective_loading(const MunitParameter* params, void* data) {
  UNUSED(params);
  UNUSED(data);

  mmdgltf_Pmx pmx = mmdgltf_load_file(MMDGLTF_TEST_NITORI_PMX);
  munit_assert_ptr_not_null(pmx);

  mmdgltf_ModelConfig cfg;
  mmdgltf_model_config_default(&cfg);
  cfg.load_flags = MMDGLTF_LOAD_VERTICES;

  mmdgltf_Model model_vert_only = mmdgltf_model_create(pmx, &cfg);
  munit_assert_ptr_not_null(model_vert_only);

  mmdgltf_VertexBuffers bufs;
  munit_assert_int(mmdgltf_model_vertex_buffers(model_vert_only, &bufs), ==, MMDGLTF_OK);
  munit_assert_size(bufs.vertex_count, ==, 29685);
  munit_assert_ptr_not_null(bufs.positions);

  const mmdgltf_PmxMaterial* mats = NULL;
  size_t mat_count = 999;
  munit_assert_int(mmdgltf_model_materials(model_vert_only, &mats, &mat_count), ==, MMDGLTF_OK);
  munit_assert_size(mat_count, ==, 0);
  munit_assert_null(mats);

  const mmdgltf_PmxBone* bones = NULL;
  size_t bone_count = 999;
  munit_assert_int(mmdgltf_model_bones(model_vert_only, &bones, &bone_count), ==, MMDGLTF_OK);
  munit_assert_size(bone_count, ==, 0);
  munit_assert_null(bones);

  mmdgltf_model_destroy(model_vert_only);

  cfg.load_flags = MMDGLTF_LOAD_BONES | MMDGLTF_LOAD_PHYSICS;
  mmdgltf_Model model_bones_phys = mmdgltf_model_create(pmx, &cfg);
  munit_assert_ptr_not_null(model_bones_phys);

  munit_assert_int(mmdgltf_model_vertex_buffers(model_bones_phys, &bufs), ==, MMDGLTF_OK);
  munit_assert_size(bufs.vertex_count, ==, 0);
  munit_assert_null(bufs.positions);

  munit_assert_int(mmdgltf_model_bones(model_bones_phys, &bones, &bone_count), ==, MMDGLTF_OK);
  munit_assert_size(bone_count, ==, 301);
  munit_assert_ptr_not_null(bones);

  const mmdgltf_PmxRigidBody* rbs = NULL;
  size_t rb_count = 0;
  munit_assert_int(mmdgltf_model_rigid_bodies(model_bones_phys, &rbs, &rb_count), ==, MMDGLTF_OK);
  munit_assert_size(rb_count, ==, 81);
  munit_assert_ptr_not_null(rbs);

  mmdgltf_model_destroy(model_bones_phys);
  mmdgltf_pmx_destroy(pmx);

  return MUNIT_OK;
}

static MunitResult test_model_textures_format_transcode(const MunitParameter* params, void* data) {
  UNUSED(params);
  UNUSED(data);

  mmdgltf_Pmx pmx = mmdgltf_load_file(MMDGLTF_TEST_NITORI_PMX);
  munit_assert_ptr_not_null(pmx);

  mmdgltf_ModelConfig cfg;
  mmdgltf_model_config_default(&cfg);
  cfg.load_flags = MMDGLTF_LOAD_TEXTURES;

  mmdgltf_Model model = mmdgltf_model_create(pmx, &cfg);
  mmdgltf_pmx_destroy(pmx);

  munit_assert_ptr_not_null(model);
  munit_assert_int(MMDGLTF_CHECK(model), ==, MMDGLTF_OK);

  const mmdgltf_Image* images = NULL;
  size_t count = 0;
  munit_assert_int(mmdgltf_model_images(model, &images, &count), ==, MMDGLTF_OK);
  munit_assert_size(count, ==, 27);

  const uint8_t png_magic[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};

  bool found_bmp = false;
  bool found_tga = false;
  bool found_spa = false;
  bool found_png = false;

  for (size_t i = 0; i < count; ++i) {
    const mmdgltf_Image* img = &images[i];
    munit_assert_ptr_not_null(img->data);
    munit_assert_size(img->size, >, 0);
    munit_assert_int(img->width, >, 0);
    munit_assert_int(img->height, >, 0);
    munit_assert_int(img->channels, >, 0);
    munit_assert_string_equal(img->mime_type, "image/png");
    munit_assert_memory_equal(8, img->data, png_magic);

    if (strstr(img->path, ".bmp") || strstr(img->path, ".BMP")) {
      found_bmp = true;
    }
    if (strstr(img->path, ".tga") || strstr(img->path, ".TGA")) {
      found_tga = true;
    }
    if (strstr(img->path, ".spa") || strstr(img->path, ".SPA")) {
      found_spa = true;
    }
    if (strstr(img->path, ".png") || strstr(img->path, ".PNG")) {
      found_png = true;
    }
  }

  munit_assert_true(found_bmp);
  munit_assert_true(found_tga);
  munit_assert_true(found_spa);
  munit_assert_true(found_png);

  mmdgltf_model_destroy(model);
  return MUNIT_OK;
}

static MunitResult test_model_textures_missing_strict_vs_lenient(const MunitParameter* params,
                                                                 void* data) {
  UNUSED(params);
  UNUSED(data);

  mmdgltf_Pmx pmx = mmdgltf_load_file(MMDGLTF_TEST_NITORI_PMX);
  munit_assert_ptr_not_null(pmx);

  mmdgltf_ModelConfig cfg_lenient;
  mmdgltf_model_config_default(&cfg_lenient);
  cfg_lenient.load_flags = MMDGLTF_LOAD_TEXTURES;
  cfg_lenient.base_dir = "nonexistent_dir_12345";
  cfg_lenient.strict_textures = 0;

  mmdgltf_Model model_lenient = mmdgltf_model_create(pmx, &cfg_lenient);
  munit_assert_ptr_not_null(model_lenient);
  munit_assert_int(MMDGLTF_CHECK(model_lenient), ==, MMDGLTF_OK);

  const mmdgltf_Image* images = NULL;
  size_t count = 0;
  munit_assert_int(mmdgltf_model_images(model_lenient, &images, &count), ==, MMDGLTF_OK);
  munit_assert_size(count, ==, 27);
  for (size_t i = 0; i < count; ++i) {
    munit_assert_null(images[i].data);
    munit_assert_size(images[i].size, ==, 0);
    munit_assert_ptr_not_null(images[i].path);
  }
  mmdgltf_model_destroy(model_lenient);

  mmdgltf_ModelConfig cfg_strict;
  mmdgltf_model_config_default(&cfg_strict);
  cfg_strict.load_flags = MMDGLTF_LOAD_TEXTURES;
  cfg_strict.base_dir = "nonexistent_dir_12345";
  cfg_strict.strict_textures = 1;

  mmdgltf_Model model_strict = mmdgltf_model_create(pmx, &cfg_strict);
  munit_assert_int(MMDGLTF_CHECK(model_strict), ==, MMDGLTF_ERROR_IO);

  mmdgltf_pmx_destroy(pmx);
  return MUNIT_OK;
}

static MunitResult test_model_errors(const MunitParameter* params, void* data) {
  UNUSED(params);
  UNUSED(data);

  const mmdgltf_Model null_model = mmdgltf_model_create(NULL, NULL);
  munit_assert_int(MMDGLTF_CHECK(null_model), ==, MMDGLTF_ERROR_INVALID_ARGUMENT);

  munit_assert_int(mmdgltf_model_vertex_buffers(NULL, NULL), ==, MMDGLTF_ERROR_INVALID_ARGUMENT);
  munit_assert_int(mmdgltf_model_indices(NULL, NULL, NULL), ==, MMDGLTF_ERROR_INVALID_ARGUMENT);
  munit_assert_int(mmdgltf_model_materials(NULL, NULL, NULL), ==, MMDGLTF_ERROR_INVALID_ARGUMENT);
  munit_assert_int(mmdgltf_model_images(NULL, NULL, NULL), ==, MMDGLTF_ERROR_INVALID_ARGUMENT);
  munit_assert_int(mmdgltf_model_bones(NULL, NULL, NULL), ==, MMDGLTF_ERROR_INVALID_ARGUMENT);
  munit_assert_int(mmdgltf_model_morphs(NULL, NULL, NULL), ==, MMDGLTF_ERROR_INVALID_ARGUMENT);
  munit_assert_int(mmdgltf_model_rigid_bodies(NULL, NULL, NULL), ==,
                   MMDGLTF_ERROR_INVALID_ARGUMENT);
  munit_assert_int(mmdgltf_model_joints(NULL, NULL, NULL), ==, MMDGLTF_ERROR_INVALID_ARGUMENT);

  return MUNIT_OK;
}

static MunitResult test_model_create_pmd(const MunitParameter* params, void* data) {
  UNUSED(params);
  UNUSED(data);

  const mmdgltf_Pmx pmx = mmdgltf_load_file(MMDGLTF_TEST_MIKU_PMD);
  munit_assert_ptr_not_null(pmx);
  munit_assert_int(MMDGLTF_CHECK(pmx), ==, MMDGLTF_OK);

  const mmdgltf_Model model = mmdgltf_model_create(pmx, NULL);
  munit_assert_ptr_not_null(model);
  munit_assert_int(MMDGLTF_CHECK(model), ==, MMDGLTF_OK);

  mmdgltf_pmx_destroy(pmx);

  mmdgltf_VertexBuffers bufs;
  mmdgltf_Status ret = mmdgltf_model_vertex_buffers(model, &bufs);
  munit_assert_int(ret, ==, MMDGLTF_OK);
  munit_assert_size(bufs.vertex_count, ==, 9036);
  munit_assert_ptr_not_null(bufs.positions);
  munit_assert_ptr_not_null(bufs.normals);
  munit_assert_ptr_not_null(bufs.uvs);
  munit_assert_ptr_not_null(bufs.joints);
  munit_assert_ptr_not_null(bufs.weights);

  const uint32_t* indices = NULL;
  size_t index_count = 0;
  ret = mmdgltf_model_indices(model, &indices, &index_count);
  munit_assert_int(ret, ==, MMDGLTF_OK);
  munit_assert_size(index_count, ==, 44991);

  const mmdgltf_PmxBone* bones = NULL;
  size_t bone_count = 0;
  ret = mmdgltf_model_bones(model, &bones, &bone_count);
  munit_assert_int(ret, ==, MMDGLTF_OK);
  munit_assert_size(bone_count, ==, 122);

  const mmdgltf_PmxMorph* morphs = NULL;
  size_t morph_count = 0;
  ret = mmdgltf_model_morphs(model, &morphs, &morph_count);
  munit_assert_int(ret, ==, MMDGLTF_OK);
  munit_assert_size(morph_count, ==, 15);

  const mmdgltf_PmxRigidBody* rigids = NULL;
  size_t rigid_count = 0;
  ret = mmdgltf_model_rigid_bodies(model, &rigids, &rigid_count);
  munit_assert_int(ret, ==, MMDGLTF_OK);
  munit_assert_size(rigid_count, ==, 45);

  const mmdgltf_PmxJoint* joints = NULL;
  size_t joint_count = 0;
  ret = mmdgltf_model_joints(model, &joints, &joint_count);
  munit_assert_int(ret, ==, MMDGLTF_OK);
  munit_assert_size(joint_count, ==, 27);

  mmdgltf_model_destroy(model);
  return MUNIT_OK;
}

static MunitTest tests[] = {
  {"/model/create-default", test_model_create_default, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/model/create-pmd", test_model_create_pmd, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/model/coordinate-transforms", test_model_coordinate_transforms, NULL, NULL,
   MUNIT_TEST_OPTION_NONE, NULL},
  {"/model/selective-loading", test_model_selective_loading, NULL, NULL, MUNIT_TEST_OPTION_NONE,
   NULL},
  {"/model/textures-transcode", test_model_textures_format_transcode, NULL, NULL,
   MUNIT_TEST_OPTION_NONE, NULL},
  {"/model/textures-missing", test_model_textures_missing_strict_vs_lenient, NULL, NULL,
   MUNIT_TEST_OPTION_NONE, NULL},
  {"/model/errors", test_model_errors, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

TestData model_tests = {
  .tests = tests,
  .len = ARRSZ(tests),
};
