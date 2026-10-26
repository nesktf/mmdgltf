#include "tests.h"

static void add_tests(TestData test, MunitTest* tests, size_t* pos) {
  size_t i = 0;
  for (; i < test.len; ++i) {
    tests[*pos + i] = test.tests[i];
  }
  *pos += i;
}

int main(int argc, char* argv[]) {
  size_t len = 1; // One extra for the null terminator
  len += load_tests.len;

  MunitTest* tests = malloc(len * sizeof(*tests));
  memset(tests, 0x00, len * sizeof(*tests));
  size_t pos = 0;
  add_tests(load_tests, tests, &pos);

  const MunitSuite suite = {
    .prefix = "/mmdgltf-tests",
    .tests = tests,
    .suites = NULL,
    .iterations = 1,
    .options = MUNIT_SUITE_OPTION_NONE,
  };

  int ret = munit_suite_main(&suite, NULL, argc, argv);
  free(tests);
  return ret;
}
