#include "tests.h"

#include <stdio.h>

static const char* model_path = "models/nitori/河城にとり.pmx"; // add a model fixture later

static MunitResult test_load_pmx_file(const MunitParameter* params, void* data) {
  UNUSED(params);
  UNUSED(data);

  mmdgltf_Pmx pmx = mmdgltf_pmx_load_file(model_path);
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
  munit_assert_uint8(header.bone_index_size, ==, 2);
  munit_assert_uint8(header.morph_index_size, ==, 2);
  munit_assert_uint8(header.rigid_index_size, ==, 1);

  mmdgltf_PmxModelInfo info;
  ret = mmdgltf_pmx_get_model_info(pmx, &info);
  munit_assert_int(ret, ==, MMDGLTF_OK);
  munit_assert_string_equal(info.name_jp, "河城にとり");
  munit_assert_string_equal(info.name_en, "+");
  munit_assert_ptr_not_null(info.comment_jp);
  munit_assert_ptr_not_null(info.comment_en);

  const mmdgltf_PmxVertex* vertices = NULL;
  size_t vertex_count = 0;
  ret = mmdgltf_pmx_vertices(pmx, &vertices, &vertex_count);
  munit_assert_int(ret, ==, MMDGLTF_OK);
  munit_assert_size(vertex_count, ==, 29685);
  munit_assert_ptr_not_null(vertices);
  munit_assert_int(vertices[0].weight_type, ==, MMDGLTF_WEIGHT_BDEF1);
  munit_assert_int32(vertices[0].bones[0], >=, 0);
  munit_assert_float(vertices[0].weights[0], ==, 1.0f);

  const uint32_t* indices = NULL;
  size_t index_count = 0;
  ret = mmdgltf_pmx_indices(pmx, &indices, &index_count);
  munit_assert_int(ret, ==, MMDGLTF_OK);
  munit_assert_size(index_count, ==, 131748);
  munit_assert_ptr_not_null(indices);
  for (size_t i = 0; i < 100; ++i) {
    munit_assert_uint32(indices[i], <, 29685);
  }

  const mmdgltf_PmxTexture* textures = NULL;
  size_t texture_count = 0;
  ret = mmdgltf_pmx_textures(pmx, &textures, &texture_count);
  munit_assert_int(ret, ==, MMDGLTF_OK);
  munit_assert_size(texture_count, ==, 27);
  munit_assert_ptr_not_null(textures);
  munit_assert_string_equal(textures[0].path, "Texture/tume.png");
  munit_assert_string_equal(textures[1].path, "Sphere/s_takane_01.png");
  for (size_t i = 0; i < texture_count; ++i) {
    munit_assert_ptr_not_null(textures[i].path);
    munit_assert_null(strchr(textures[i].path, '\\'));
  }

  const mmdgltf_PmxMaterial* materials = NULL;
  size_t material_count = 0;
  ret = mmdgltf_pmx_materials(pmx, &materials, &material_count);
  munit_assert_int(ret, ==, MMDGLTF_OK);
  munit_assert_size(material_count, ==, 47);
  munit_assert_ptr_not_null(materials);
  munit_assert_string_equal(materials[0].name_jp, "ツメ");
  munit_assert_uint32(materials[0].index_count, ==, 390);
  munit_assert_int32(materials[0].texture_index, ==, 0);
  munit_assert_int32(materials[0].sphere_texture_index, ==, 1);
  munit_assert_int(materials[0].sphere_mode, ==, MMDGLTF_SPHERE_ADD);

  size_t total_mat_indices = 0;
  for (size_t i = 0; i < material_count; ++i) {
    total_mat_indices += materials[i].index_count;
  }
  munit_assert_size(total_mat_indices, ==, 131748);

  mmdgltf_pmx_destroy(pmx);

  // same thing with non null terminated string
  pmx = mmdgltf_pmx_load_filen(model_path, strlen(model_path));
  munit_assert_ptr_not_null(pmx);
  munit_assert_int(MMDGLTF_CHECK(pmx), ==, MMDGLTF_OK);

  munit_assert_int(mmdgltf_pmx_get_model_info(pmx, &info), ==, MMDGLTF_OK);
  munit_assert_string_equal(info.name_jp, "河城にとり");

  mmdgltf_pmx_destroy(pmx);

  return MUNIT_OK;
}

static size_t test_io_read(void* buff, size_t sz, size_t nmemb, void* src) {
  return fread(buff, sz, nmemb, (FILE*)src);
}

static int test_io_seek(void* src, long offset, int origin) {
  return fseek((FILE*)src, offset, origin);
}

static long test_io_tell(void* src) {
  return ftell((FILE*)src);
}

static int test_io_eof(void* src) {
  return feof((FILE*)src);
}

static MunitResult test_load_pmx_stream(const MunitParameter* params, void* data) {
  UNUSED(params);
  UNUSED(data);

  FILE* f = fopen(model_path, "rb");
  munit_assert_ptr_not_null(f);

  const mmdgltf_StreamFunc func = {
    .read = test_io_read,
    .seek = test_io_seek,
    .tell = test_io_tell,
    .eof = test_io_eof,
  };

  const mmdgltf_Pmx pmx = mmdgltf_pmx_load_stream(&func, f);
  fclose(f);

  munit_assert_ptr_not_null(pmx);
  munit_assert_int(MMDGLTF_CHECK(pmx), ==, MMDGLTF_OK);

  mmdgltf_PmxModelInfo info;
  munit_assert_int(mmdgltf_pmx_get_model_info(pmx, &info), ==, MMDGLTF_OK);
  munit_assert_string_equal(info.name_jp, "河城にとり");

  mmdgltf_pmx_destroy(pmx);
  return MUNIT_OK;
}

static MunitResult test_load_pmx_memory(const MunitParameter* params, void* data) {
  UNUSED(params);
  UNUSED(data);

  FILE* const f = fopen(model_path, "rb");
  munit_assert_ptr_not_null(f);

  fseek(f, 0, SEEK_END);
  const long sz = ftell(f);
  fseek(f, 0, SEEK_SET);

  void* const buf = malloc((size_t)sz);
  munit_assert_ptr_not_null(buf);
  munit_assert_size(fread(buf, 1, (size_t)sz, f), ==, (size_t)sz);
  fclose(f);

  const mmdgltf_Pmx pmx = mmdgltf_pmx_load_memory(buf, (size_t)sz);
  free(buf);

  munit_assert_ptr_not_null(pmx);
  munit_assert_int(MMDGLTF_CHECK(pmx), ==, MMDGLTF_OK);

  mmdgltf_PmxModelInfo info;
  munit_assert_int(mmdgltf_pmx_get_model_info(pmx, &info), ==, MMDGLTF_OK);
  munit_assert_string_equal(info.name_jp, "河城にとり");

  mmdgltf_pmx_destroy(pmx);
  return MUNIT_OK;
}

static MunitResult test_load_pmx_errors(const MunitParameter* params, void* data) {
  UNUSED(params);
  UNUSED(data);

  const mmdgltf_Pmx null_pmx = mmdgltf_pmx_load_file(NULL);
  munit_assert_int(MMDGLTF_CHECK(null_pmx), ==, MMDGLTF_ERROR_INVALID_ARGUMENT);

  const mmdgltf_Pmx null_stream = mmdgltf_pmx_load_stream(NULL, NULL);
  munit_assert_int(MMDGLTF_CHECK(null_stream), ==, MMDGLTF_ERROR_INVALID_ARGUMENT);

  const mmdgltf_Pmx noent_pmx = mmdgltf_pmx_load_file("models/nonexistent.pmx");
  munit_assert_int(MMDGLTF_CHECK(noent_pmx), ==, MMDGLTF_ERROR_IO);

  const char bad_size[27] = "funny random data hehehaha";
  mmdgltf_Pmx bad_pmx = mmdgltf_pmx_load_memory(bad_size, sizeof(bad_size));
  munit_assert_int(MMDGLTF_CHECK(bad_pmx), ==, MMDGLTF_ERROR_INVALID_ARGUMENT);

  const char bad_sig[33] = "funny random data again hehehaha";
  bad_pmx = mmdgltf_pmx_load_memory(bad_sig, sizeof(bad_sig));
  munit_assert_int(MMDGLTF_CHECK(bad_pmx), ==, MMDGLTF_ERROR_INVALID_SIGNATURE);

  munit_assert_int(mmdgltf_pmx_vertices(NULL, NULL, NULL), ==, MMDGLTF_ERROR_INVALID_ARGUMENT);
  munit_assert_int(mmdgltf_pmx_indices(NULL, NULL, NULL), ==, MMDGLTF_ERROR_INVALID_ARGUMENT);
  munit_assert_int(mmdgltf_pmx_textures(NULL, NULL, NULL), ==, MMDGLTF_ERROR_INVALID_ARGUMENT);
  munit_assert_int(mmdgltf_pmx_materials(NULL, NULL, NULL), ==, MMDGLTF_ERROR_INVALID_ARGUMENT);

  return MUNIT_OK;
}

static MunitTest tests[] = {
  {"/load/pmx-file", test_load_pmx_file, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/load/pmx-stream", test_load_pmx_stream, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/load/pmx-memory", test_load_pmx_memory, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/load/pmx-errors", test_load_pmx_errors, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

TestData load_tests = {
  .tests = tests,
  .len = ARRSZ(tests),
};
