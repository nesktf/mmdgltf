#ifndef MMDGLTF_TESTS_H
#define MMDGLTF_TESTS_H

#include "mmdgltf_internal.h"
#include <munit.h>

typedef struct TestData {
  MunitTest* tests;
  size_t len;
} TestData;

extern TestData load_tests;
extern TestData stream_tests;
extern TestData model_tests;

#endif // MMDGLTF_TESTS_H
