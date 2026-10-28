#include "paths.h"
#include "tests.h"
#include "tinygltf/tiny_gltf_v3.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static MunitResult test_export_glb_memory_default(const MunitParameter* params, void* data) {
  UNUSED(params);
  UNUSED(data);

  mmdgltf_Pmx pmx = mmdgltf_load_file(MMDGLTF_TEST_NITORI_PMX);
  munit_assert_ptr_not_null(pmx);

  mmdgltf_Model model = mmdgltf_model_create(pmx, NULL);
  mmdgltf_pmx_destroy(pmx);
  munit_assert_ptr_not_null(model);
  munit_assert_int(MMDGLTF_CHECK(model), ==, MMDGLTF_OK);

  u8* glb_data = NULL;
  size_t glb_size = 0;
  mmdgltf_Status st = mmdgltf_export_memory(model, &glb_data, &glb_size, NULL);
  munit_assert_int(st, ==, MMDGLTF_OK);
  munit_assert_ptr_not_null(glb_data);
  munit_assert_size(glb_size, >, 20);

  const u32 magic = *(const u32*)(glb_data + 0);
  const u32 version = *(const u32*)(glb_data + 4);
  const u32 length = *(const u32*)(glb_data + 8);
  const u32 chunk0_type = *(const u32*)(glb_data + 16);

  munit_assert_uint32(magic, ==, 0x46546C67);
  munit_assert_uint32(version, ==, 2);
  munit_assert_size(length, ==, glb_size);
  munit_assert_uint32(chunk0_type, ==, 0x4E4F534A);

  tg3_model parsed;
  memset(&parsed, 0, sizeof(parsed));
  tg3_error_stack es;
  tg3_error_stack_init(&es);
  tg3_parse_options opts;
  tg3_parse_options_init(&opts);

  tg3_error_code perr = tg3_parse_glb(&parsed, &es, glb_data, glb_size, NULL, 0, &opts);
  munit_assert_int(perr, ==, TG3_OK);
  munit_assert_uint32(parsed.meshes_count, ==, 1);
  munit_assert_uint32(parsed.meshes[0].primitives_count, ==, 47);
  munit_assert_uint32(parsed.materials_count, ==, 47);
  munit_assert_uint32(parsed.skins_count, ==, 1);
  munit_assert_uint32(parsed.skins[0].joints_count, ==, 301);
  munit_assert_uint32(parsed.scenes_count, ==, 1);

  tg3_error_stack_free(&es);
  tg3_model_free(&parsed);

  mmdgltf_export_free(glb_data, glb_size);
  mmdgltf_model_destroy(model);

  return MUNIT_OK;
}

static MunitResult test_export_glb_file_unlit(const MunitParameter* params, void* data) {
  UNUSED(params);
  UNUSED(data);

  mmdgltf_Pmx pmx = mmdgltf_load_file(MMDGLTF_TEST_NITORI_PMX);
  munit_assert_ptr_not_null(pmx);

  mmdgltf_Model model = mmdgltf_model_create(pmx, NULL);
  mmdgltf_pmx_destroy(pmx);
  munit_assert_ptr_not_null(model);

  mmdgltf_ExportConfig cfg;
  mmdgltf_export_config_default(&cfg);
  cfg.unlit = 1;

  const char* out_file = "/tmp/test_nitori_export.glb";
  mmdgltf_Status st = mmdgltf_export_file(model, out_file, &cfg);
  munit_assert_int(st, ==, MMDGLTF_OK);

  tg3_model parsed;
  memset(&parsed, 0, sizeof(parsed));
  tg3_error_stack es;
  tg3_error_stack_init(&es);
  tg3_parse_options opts;
  tg3_parse_options_init(&opts);

  tg3_error_code perr = tg3_parse_file(&parsed, &es, out_file, (u32)strlen(out_file), &opts);
  munit_assert_int(perr, ==, TG3_OK);

  munit_assert_size(parsed.extensions_used_count, >=, 1);
  munit_assert_int(tg3_str_equals_cstr(parsed.extensions_used[0], "KHR_materials_unlit"), ==, 1);
  munit_assert_uint32(parsed.materials[0].ext.extensions_count, ==, 1);
  munit_assert_int(
    tg3_str_equals_cstr(parsed.materials[0].ext.extensions[0].name, "KHR_materials_unlit"), ==, 1);

  tg3_error_stack_free(&es);
  tg3_model_free(&parsed);
  remove(out_file);

  mmdgltf_model_destroy(model);
  return MUNIT_OK;
}

static MunitResult test_export_gltf_file(const MunitParameter* params, void* data) {
  UNUSED(params);
  UNUSED(data);

  mmdgltf_Pmx pmx = mmdgltf_load_file(MMDGLTF_TEST_NITORI_PMX);
  munit_assert_ptr_not_null(pmx);

  mmdgltf_Model model = mmdgltf_model_create(pmx, NULL);
  mmdgltf_pmx_destroy(pmx);
  munit_assert_ptr_not_null(model);

  const char* out_file = "/tmp/test_nitori_export.gltf";
  mmdgltf_ExportConfig cfg;
  mmdgltf_export_config_default(&cfg);
  cfg.format = MMDGLTF_FORMAT_GLTF;
  mmdgltf_Status st = mmdgltf_export_file(model, out_file, &cfg);
  munit_assert_int(st, ==, MMDGLTF_OK);

  FILE* fbin = fopen("/tmp/test_nitori_export.bin", "rb");
  munit_assert_ptr_not_null(fbin);
  fclose(fbin);

  tg3_model parsed;
  memset(&parsed, 0, sizeof(parsed));
  tg3_error_stack es;
  tg3_error_stack_init(&es);
  tg3_parse_options opts;
  tg3_parse_options_init(&opts);

  tg3_error_code perr = tg3_parse_file(&parsed, &es, out_file, (u32)strlen(out_file), &opts);
  munit_assert_int(perr, ==, TG3_OK);
  munit_assert_uint32(parsed.meshes_count, ==, 1);
  munit_assert_uint32(parsed.meshes[0].primitives_count, ==, 47);

  tg3_error_stack_free(&es);
  tg3_model_free(&parsed);

  remove("/tmp/test_nitori_export.gltf");
  remove("/tmp/test_nitori_export.bin");

  mmdgltf_model_destroy(model);
  return MUNIT_OK;
}

static MunitResult test_export_errors(const MunitParameter* params, void* data) {
  UNUSED(params);
  UNUSED(data);

  munit_assert_int(mmdgltf_export_file(NULL, "out.glb", NULL), ==, MMDGLTF_ERROR_INVALID_ARGUMENT);
  munit_assert_int(mmdgltf_export_filen(NULL, "out.glb", 7, NULL), ==,
                   MMDGLTF_ERROR_INVALID_ARGUMENT);
  munit_assert_int(mmdgltf_export_memory(NULL, NULL, NULL, NULL), ==,
                   MMDGLTF_ERROR_INVALID_ARGUMENT);

  mmdgltf_Pmx pmx = mmdgltf_load_file(MMDGLTF_TEST_NITORI_PMX);
  mmdgltf_Model model = mmdgltf_model_create(pmx, NULL);
  mmdgltf_pmx_destroy(pmx);

  munit_assert_int(mmdgltf_export_file(model, NULL, NULL), ==, MMDGLTF_ERROR_INVALID_ARGUMENT);
  munit_assert_int(mmdgltf_export_filen(model, NULL, 0, NULL), ==, MMDGLTF_ERROR_INVALID_ARGUMENT);
  munit_assert_int(mmdgltf_export_memory(model, NULL, NULL, NULL), ==,
                   MMDGLTF_ERROR_INVALID_ARGUMENT);

  mmdgltf_model_destroy(model);
  return MUNIT_OK;
}

static MunitResult test_export_glb_pmd(const MunitParameter* params, void* data) {
  UNUSED(params);
  UNUSED(data);

  const mmdgltf_Pmx pmx = mmdgltf_load_file(MMDGLTF_TEST_MIKU_PMD);
  munit_assert_ptr_not_null(pmx);

  const mmdgltf_Model model = mmdgltf_model_create(pmx, NULL);
  mmdgltf_pmx_destroy(pmx);
  munit_assert_ptr_not_null(model);
  munit_assert_int(MMDGLTF_CHECK(model), ==, MMDGLTF_OK);

  u8* glb_data = NULL;
  size_t glb_size = 0;
  mmdgltf_Status st = mmdgltf_export_memory(model, &glb_data, &glb_size, NULL);
  munit_assert_int(st, ==, MMDGLTF_OK);
  munit_assert_ptr_not_null(glb_data);
  munit_assert_size(glb_size, >, 20);

  tg3_model parsed;
  memset(&parsed, 0, sizeof(parsed));
  tg3_error_stack es;
  tg3_error_stack_init(&es);
  tg3_parse_options opts;
  tg3_parse_options_init(&opts);

  tg3_error_code perr = tg3_parse_glb(&parsed, &es, glb_data, glb_size, NULL, 0, &opts);
  munit_assert_int(perr, ==, TG3_OK);
  munit_assert_uint32(parsed.meshes_count, ==, 1);
  munit_assert_uint32(parsed.meshes[0].primitives_count, ==, 17);
  munit_assert_uint32(parsed.materials_count, ==, 17);
  munit_assert_uint32(parsed.skins_count, ==, 1);
  munit_assert_uint32(parsed.skins[0].joints_count, ==, 122);
  munit_assert_uint32(parsed.scenes_count, ==, 1);

  tg3_error_stack_free(&es);
  tg3_model_free(&parsed);

  mmdgltf_export_free(glb_data, glb_size);
  mmdgltf_model_destroy(model);

  return MUNIT_OK;
}

static MunitTest tests[] = {
  {"/export/glb-memory-default", test_export_glb_memory_default, NULL, NULL,
   MUNIT_TEST_OPTION_NONE, NULL},
  {"/export/glb-pmd", test_export_glb_pmd, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/export/glb-file-unlit", test_export_glb_file_unlit, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/export/gltf-file", test_export_gltf_file, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/export/errors", test_export_errors, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

TestData export_tests = {
  .tests = tests,
  .len = ARRSZ(tests),
};
