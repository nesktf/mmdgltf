#include "tests.h"

static MunitResult load_pmx_test(const MunitParameter* params, void* data) {
  UNUSED(params);
  UNUSED(data);
  return MUNIT_OK;
}

static MunitTest tests[] = {
  {"/load/load-pmx", load_pmx_test, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

TestData load_tests = {
  .tests = tests,
  .len = ARRSZ(tests),
};
