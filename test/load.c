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
