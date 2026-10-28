#include "paths.h"
#include "tests.h"
#include "tinygltf/tiny_gltf_v3.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double get_time_sec(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static MunitResult test_fastpath_memory(const MunitParameter* params, void* data) {
  UNUSED(params);
  UNUSED(data);

  FILE* f = fopen(MMDGLTF_TEST_NITORI_PMX, "rb");
  munit_assert_ptr_not_null(f);
  fseek(f, 0, SEEK_END);
  long fsz = ftell(f);
  fseek(f, 0, SEEK_SET);
  munit_assert_long(fsz, >, 0);

  u8* pmx_data = malloc((size_t)fsz);
  munit_assert_ptr_not_null(pmx_data);
  munit_assert_size(fread(pmx_data, 1, (size_t)fsz, f), ==, (size_t)fsz);
  fclose(f);

  u8* glb_data = NULL;
  size_t glb_size = 0;
  mmdgltf_Status st =
    mmdgltf_convert_pmx_memory(pmx_data, (size_t)fsz, &glb_data, &glb_size, NULL);
  free(pmx_data);

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

  const i32 prim0_indices_acc = parsed.meshes[0].primitives[0].indices;
  munit_assert_int(prim0_indices_acc, >=, 0);
  munit_assert_int(parsed.accessors[prim0_indices_acc].component_type, ==,
                   TG3_COMPONENT_TYPE_UNSIGNED_SHORT);

  tg3_error_stack_free(&es);
  tg3_model_free(&parsed);
  mmdgltf_export_free(glb_data, glb_size);

  return MUNIT_OK;
}

static MunitResult test_fastpath_file(const MunitParameter* params, void* data) {
  UNUSED(params);
  UNUSED(data);

  const char* out_file = "/tmp/test_nitori_fastpath.glb";
  mmdgltf_Status st = mmdgltf_convert_pmx_file(MMDGLTF_TEST_NITORI_PMX, out_file, NULL);
  munit_assert_int(st, ==, MMDGLTF_OK);

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
  munit_assert_uint32(parsed.materials_count, ==, 47);
  munit_assert_uint32(parsed.skins_count, ==, 1);
  munit_assert_uint32(parsed.skins[0].joints_count, ==, 301);

  tg3_error_stack_free(&es);
  tg3_model_free(&parsed);

  return MUNIT_OK;
}

static MunitResult test_fastpath_gltf_file(const MunitParameter* params, void* data) {
  UNUSED(params);
  UNUSED(data);

  const char* out_file = "/tmp/test_nitori_fastpath.gltf";
  mmdgltf_Status st = mmdgltf_convert_pmx_to_gltf_file(MMDGLTF_TEST_NITORI_PMX, out_file, NULL);
  munit_assert_int(st, ==, MMDGLTF_OK);

  FILE* fbin = fopen("/tmp/test_nitori_fastpath.bin", "rb");
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
  munit_assert_uint32(parsed.materials_count, ==, 47);
  munit_assert_uint32(parsed.skins_count, ==, 1);
  munit_assert_uint32(parsed.skins[0].joints_count, ==, 301);

  tg3_error_stack_free(&es);
  tg3_model_free(&parsed);

  return MUNIT_OK;
}

static MunitResult test_fastpath_bone_names(const MunitParameter* params, void* data) {
  UNUSED(params);
  UNUSED(data);

  mmdgltf_ExportConfig cfg_jp;
  mmdgltf_export_config_default(&cfg_jp);
  cfg_jp.english_bone_names = 0;

  mmdgltf_ExportConfig cfg_en;
  mmdgltf_export_config_default(&cfg_en);
  cfg_en.english_bone_names = 1;

  const char* out_jp = "/tmp/test_nitori_jp.glb";
  const char* out_en = "/tmp/test_nitori_en.glb";

  mmdgltf_Status st_jp = mmdgltf_convert_pmx_file(MMDGLTF_TEST_NITORI_PMX, out_jp, &cfg_jp);
  munit_assert_int(st_jp, ==, MMDGLTF_OK);

  mmdgltf_Status st_en = mmdgltf_convert_pmx_file(MMDGLTF_TEST_NITORI_PMX, out_en, &cfg_en);
  munit_assert_int(st_en, ==, MMDGLTF_OK);

  tg3_model parsed_jp, parsed_en;
  memset(&parsed_jp, 0, sizeof(parsed_jp));
  memset(&parsed_en, 0, sizeof(parsed_en));
  tg3_error_stack es;
  tg3_error_stack_init(&es);
  tg3_parse_options opts;
  tg3_parse_options_init(&opts);

  tg3_error_code perr = tg3_parse_file(&parsed_jp, &es, out_jp, (u32)strlen(out_jp), &opts);
  munit_assert_int(perr, ==, TG3_OK);
  perr = tg3_parse_file(&parsed_en, &es, out_en, (u32)strlen(out_en), &opts);
  munit_assert_int(perr, ==, TG3_OK);

  munit_assert_uint32(parsed_jp.skins_count, ==, 1);
  munit_assert_uint32(parsed_en.skins_count, ==, 1);
  munit_assert_uint32(parsed_jp.skins[0].joints_count, ==, parsed_en.skins[0].joints_count);

  int differences = 0;
  for (u32 i = 0; i < parsed_jp.skins[0].joints_count; ++i) {
    i32 j_jp = parsed_jp.skins[0].joints[i];
    i32 j_en = parsed_en.skins[0].joints[i];
    if (j_jp >= 0 && j_en >= 0) {
      tg3_str name_jp = parsed_jp.nodes[j_jp].name;
      tg3_str name_en = parsed_en.nodes[j_en].name;
      if (name_jp.data && name_en.data && !tg3_str_equals(name_jp, name_en)) {
        differences++;
      }
    }
  }
  munit_assert_int(differences, >, 0);

  tg3_error_stack_free(&es);
  tg3_model_free(&parsed_jp);
  tg3_model_free(&parsed_en);
  remove(out_jp);
  remove(out_en);

  return MUNIT_OK;
}

static MunitResult test_fastpath_parity_and_benchmark(const MunitParameter* params, void* data) {
  UNUSED(params);
  UNUSED(data);

  const double t0_std = get_time_sec();
  mmdgltf_Pmx pmx = mmdgltf_pmx_load_file(MMDGLTF_TEST_NITORI_PMX);
  munit_assert_ptr_not_null(pmx);

  mmdgltf_Model model = mmdgltf_model_create(pmx, NULL);
  mmdgltf_pmx_destroy(pmx);
  munit_assert_ptr_not_null(model);

  u8* std_glb_data = NULL;
  size_t std_glb_size = 0;
  mmdgltf_Status st_std = mmdgltf_export_glb_memory(model, &std_glb_data, &std_glb_size, NULL);
  munit_assert_int(st_std, ==, MMDGLTF_OK);
  const double t1_std = get_time_sec();
  const double std_elapsed = t1_std - t0_std;

  tg3_model std_parsed;
  memset(&std_parsed, 0, sizeof(std_parsed));
  tg3_error_stack es_std;
  tg3_error_stack_init(&es_std);
  tg3_parse_options opts;
  tg3_parse_options_init(&opts);
  tg3_error_code perr_std =
    tg3_parse_glb(&std_parsed, &es_std, std_glb_data, std_glb_size, NULL, 0, &opts);
  munit_assert_int(perr_std, ==, TG3_OK);

  const double t0_fast = get_time_sec();
  u8* fast_glb_data = NULL;
  size_t fast_glb_size = 0;

  FILE* f = fopen(MMDGLTF_TEST_NITORI_PMX, "rb");
  munit_assert_ptr_not_null(f);
  fseek(f, 0, SEEK_END);
  long fsz = ftell(f);
  fseek(f, 0, SEEK_SET);
  u8* pmx_bytes = malloc((size_t)fsz);
  munit_assert_size(fread(pmx_bytes, 1, (size_t)fsz, f), ==, (size_t)fsz);
  fclose(f);

  mmdgltf_Status st_fast =
    mmdgltf_convert_pmx_memory(pmx_bytes, (size_t)fsz, &fast_glb_data, &fast_glb_size, NULL);
  free(pmx_bytes);
  munit_assert_int(st_fast, ==, MMDGLTF_OK);
  const double t1_fast = get_time_sec();
  const double fast_elapsed = t1_fast - t0_fast;

  tg3_model fast_parsed;
  memset(&fast_parsed, 0, sizeof(fast_parsed));
  tg3_error_stack es_fast;
  tg3_error_stack_init(&es_fast);
  tg3_error_code perr_fast =
    tg3_parse_glb(&fast_parsed, &es_fast, fast_glb_data, fast_glb_size, NULL, 0, &opts);
  munit_assert_int(perr_fast, ==, TG3_OK);

  munit_assert_uint32(fast_parsed.meshes_count, ==, std_parsed.meshes_count);
  munit_assert_uint32(fast_parsed.meshes[0].primitives_count, ==,
                      std_parsed.meshes[0].primitives_count);
  munit_assert_uint32(fast_parsed.materials_count, ==, std_parsed.materials_count);
  munit_assert_uint32(fast_parsed.skins_count, ==, std_parsed.skins_count);
  munit_assert_uint32(fast_parsed.skins[0].joints_count, ==, std_parsed.skins[0].joints_count);
  munit_assert_uint32(fast_parsed.scenes_count, ==, std_parsed.scenes_count);
  munit_assert_uint32(fast_parsed.nodes_count, ==, std_parsed.nodes_count);

  printf("\n[Benchmark] Standard Path: %.4fs (GLB: %zu bytes) | Fast Path: %.4fs (GLB: %zu bytes) "
         "| Speedup: %.2fx\n",
         std_elapsed, std_glb_size, fast_elapsed, fast_glb_size,
         fast_elapsed > 0.0 ? (std_elapsed / fast_elapsed) : 1.0);

  tg3_error_stack_free(&es_std);
  tg3_model_free(&std_parsed);
  tg3_error_stack_free(&es_fast);
  tg3_model_free(&fast_parsed);

  mmdgltf_export_free(std_glb_data, std_glb_size);
  mmdgltf_export_free(fast_glb_data, fast_glb_size);
  mmdgltf_model_destroy(model);

  return MUNIT_OK;
}

static MunitResult test_fastpath_errors(const MunitParameter* params, void* data) {
  UNUSED(params);
  UNUSED(data);

  munit_assert_int(mmdgltf_convert_pmx_file(NULL, "/tmp/out.glb", NULL), ==,
                   MMDGLTF_ERROR_INVALID_ARGUMENT);
  munit_assert_int(mmdgltf_convert_pmx_file(MMDGLTF_TEST_NITORI_PMX, NULL, NULL), ==,
                   MMDGLTF_ERROR_INVALID_ARGUMENT);

  u8* out_data = NULL;
  size_t out_size = 0;
  munit_assert_int(mmdgltf_convert_pmx_memory(NULL, 100, &out_data, &out_size, NULL), ==,
                   MMDGLTF_ERROR_INVALID_ARGUMENT);

  u8 bad_buf[64] = {0};
  munit_assert_int(
    mmdgltf_convert_pmx_memory(bad_buf, sizeof(bad_buf), &out_data, &out_size, NULL), ==,
    MMDGLTF_ERROR_INVALID_SIGNATURE);

  return MUNIT_OK;
}

static MunitTest tests[] = {
  {
    .name = "/fastpath/memory",
    .test = test_fastpath_memory,
    .setup = NULL,
    .tear_down = NULL,
    .options = MUNIT_TEST_OPTION_NONE,
    .parameters = NULL,
  },
  {
    .name = "/fastpath/file",
    .test = test_fastpath_file,
    .setup = NULL,
    .tear_down = NULL,
    .options = MUNIT_TEST_OPTION_NONE,
    .parameters = NULL,
  },
  {
    .name = "/fastpath/gltf-file",
    .test = test_fastpath_gltf_file,
    .setup = NULL,
    .tear_down = NULL,
    .options = MUNIT_TEST_OPTION_NONE,
    .parameters = NULL,
  },
  {
    .name = "/fastpath/bone-names",
    .test = test_fastpath_bone_names,
    .setup = NULL,
    .tear_down = NULL,
    .options = MUNIT_TEST_OPTION_NONE,
    .parameters = NULL,
  },
  {
    .name = "/fastpath/parity-benchmark",
    .test = test_fastpath_parity_and_benchmark,
    .setup = NULL,
    .tear_down = NULL,
    .options = MUNIT_TEST_OPTION_NONE,
    .parameters = NULL,
  },
  {
    .name = "/fastpath/errors",
    .test = test_fastpath_errors,
    .setup = NULL,
    .tear_down = NULL,
    .options = MUNIT_TEST_OPTION_NONE,
    .parameters = NULL,
  },
};

TestData fastpath_tests = {
  .tests = tests,
  .len = sizeof(tests) / sizeof(tests[0]),
};
