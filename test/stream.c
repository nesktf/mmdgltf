#include "tests.h"

#include <string.h>

static mmdgltf_Stream init_mem_stream(mmdgltf__MemStream* src, const void* data, size_t size) {
  src->data = data;
  src->size = size;
  src->pos = 0;
  return (mmdgltf_Stream){
    .func = mmdgltf__mem_io,
    .src = src,
    .status = MMDGLTF_OK,
  };
}

// clang-format off
static const u8 raw_primitive_data[] = {
  0xAB,                   // u8: 171
  0xCE,                   // i8: -50
  0x34, 0x12,             // u16: 0x1234
  0x18, 0xFC,             // i16: -1000 (0xFC18)
  0x78, 0x56, 0x34, 0x12, // u32: 0x12345678
  0x60, 0xFB, 0xFE, 0xFF, // i32: -66720 (0xFFFEFB60)
  0x00, 0x00, 0xA0, 0x3F, // f32: 1.25f (0x3FA00000)
  0x00, 0x00, 0x60, 0xC0, // f32: -3.5f (0xC0600000)
  'P',  'M',  'X',  ' ',  // bytes[4]
};
// clang-format on

static MunitResult test_stream_primitives(const MunitParameter* params, void* data) {
  UNUSED(params);
  UNUSED(data);

  mmdgltf__MemStream src;
  mmdgltf_Stream s = init_mem_stream(&src, raw_primitive_data, sizeof(raw_primitive_data));

  const u8 u8_val = mmdgltf__stream_read_u8(&s);
  munit_assert_uint8(u8_val, ==, 0xAB);
  munit_assert_int(s.status, ==, MMDGLTF_OK);

  const i8 i8_val = mmdgltf__stream_read_i8(&s);
  munit_assert_int8(i8_val, ==, -50);
  munit_assert_int(s.status, ==, MMDGLTF_OK);

  const u16 u16_val = mmdgltf__stream_read_u16(&s);
  munit_assert_uint16(u16_val, ==, 0x1234);
  munit_assert_int(s.status, ==, MMDGLTF_OK);

  const i16 i16_val = mmdgltf__stream_read_i16(&s);
  munit_assert_int16(i16_val, ==, -1000);
  munit_assert_int(s.status, ==, MMDGLTF_OK);

  const u32 u32_val = mmdgltf__stream_read_u32(&s);
  munit_assert_uint32(u32_val, ==, 0x12345678);
  munit_assert_int(s.status, ==, MMDGLTF_OK);

  const i32 i32_val = mmdgltf__stream_read_i32(&s);
  munit_assert_int32(i32_val, ==, -66720);
  munit_assert_int(s.status, ==, MMDGLTF_OK);

  const f32 f1 = mmdgltf__stream_read_f32(&s);
  munit_assert_float(f1, ==, 1.25f);
  munit_assert_int(s.status, ==, MMDGLTF_OK);

  const f32 f2 = mmdgltf__stream_read_f32(&s);
  munit_assert_float(f2, ==, -3.5f);
  munit_assert_int(s.status, ==, MMDGLTF_OK);

  char buf[5] = {0};
  mmdgltf__stream_read_bytes(&s, buf, 4);
  munit_assert_string_equal(buf, "PMX ");
  munit_assert_int(s.status, ==, MMDGLTF_OK);

  munit_assert_size(src.pos, ==, sizeof(raw_primitive_data));
  return MUNIT_OK;
}

// clang-format off
static const u8 raw_vector_data[] = {
  // Vec2: 1.0f, -2.0f
  0x00, 0x00, 0x80, 0x3F,
  0x00, 0x00, 0x00, 0xC0,
  // Vec3: 10.0f, 20.0f, 30.0f
  0x00, 0x00, 0x20, 0x41,
  0x00, 0x00, 0xA0, 0x41,
  0x00, 0x00, 0xF0, 0x41,
  // Vec4: -1.0f, 2.0f, -3.0f, 4.0f
  0x00, 0x00, 0x80, 0xBF,
  0x00, 0x00, 0x00, 0x40,
  0x00, 0x00,0x40, 0xC0,
  0x00, 0x00, 0x80, 0x40,
  // Color3: 0.25f, 0.5f, 0.75f
  0x00, 0x00, 0x80, 0x3E,
  0x00, 0x00, 0x00, 0x3F,
  0x00, 0x00, 0x40, 0x3F,
  // Color4: 0.125f, 0.25f, 0.5f, 1.0f
  0x00, 0x00, 0x00, 0x3E,
  0x00, 0x00, 0x80, 0x3E,
  0x00, 0x00, 0x00, 0x3F,
  0x00, 0x00, 0x80, 0x3F,
  // Quat: 0.0f, 0.0f, 0.0f, 1.0f (identity; x, y, z, w)
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x80, 0x3F,
};
// clang-format on

static MunitResult test_stream_vectors(const MunitParameter* params, void* data) {
  UNUSED(params);
  UNUSED(data);

  mmdgltf__MemStream src;
  mmdgltf_Stream s = init_mem_stream(&src, raw_vector_data, sizeof(raw_vector_data));

  const mmdgltf_Vec2f v2 = mmdgltf__stream_read_vec2(&s);
  munit_assert_float(v2.x, ==, 1.0f);
  munit_assert_float(v2.y, ==, -2.0f);

  const mmdgltf_Vec3f v3 = mmdgltf__stream_read_vec3(&s);
  munit_assert_float(v3.x, ==, 10.0f);
  munit_assert_float(v3.y, ==, 20.0f);
  munit_assert_float(v3.z, ==, 30.0f);

  const mmdgltf_Vec4f v4 = mmdgltf__stream_read_vec4(&s);
  munit_assert_float(v4.x, ==, -1.0f);
  munit_assert_float(v4.y, ==, 2.0f);
  munit_assert_float(v4.z, ==, -3.0f);
  munit_assert_float(v4.w, ==, 4.0f);

  const mmdgltf_Color3f c3 = mmdgltf__stream_read_color3(&s);
  munit_assert_float(c3.r, ==, 0.25f);
  munit_assert_float(c3.g, ==, 0.5f);
  munit_assert_float(c3.b, ==, 0.75f);

  const mmdgltf_Color4f c4 = mmdgltf__stream_read_color4(&s);
  munit_assert_float(c4.r, ==, 0.125f);
  munit_assert_float(c4.g, ==, 0.25f);
  munit_assert_float(c4.b, ==, 0.5f);
  munit_assert_float(c4.a, ==, 1.0f);

  const mmdgltf_Quatf q = mmdgltf__stream_read_quat(&s);
  munit_assert_float(q.x, ==, 0.0f);
  munit_assert_float(q.y, ==, 0.0f);
  munit_assert_float(q.z, ==, 0.0f);
  munit_assert_float(q.w, ==, 1.0f);

  munit_assert_int(s.status, ==, MMDGLTF_OK);
  munit_assert_size(src.pos, ==, sizeof(raw_vector_data));
  return MUNIT_OK;
}

// clang-format off
static const u8 raw_index_data[] = {
  // unsigned vertex indices size 1 (0x55), size 2 (0x1234), size 4 (0x00010000)
  0x55, 0x34, 0x12, 0x00, 0x00, 0x01, 0x00,
  // signed size 1: positive 42, negative -1 (0xFF)
  0x2A, 0xFF,
  // signed size 2: positive 1000 (0x03E8), negative -1 (0xFFFF)
  0xE8, 0x03, 0xFF, 0xFF,
  // signed size 4: positive 70000 (0x00011170), negative -1 (0xFFFFFFFF)
  0x70, 0x11, 0x01, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
};
// clang-format on

static MunitResult test_stream_indices(const MunitParameter* params, void* data) {
  UNUSED(params);
  UNUSED(data);

  mmdgltf__MemStream src;
  mmdgltf_Stream s = init_mem_stream(&src, raw_index_data, sizeof(raw_index_data));

  // vertex indices
  const u32 vi1 = mmdgltf__stream_read_vertex_index(&s, 1);
  munit_assert_uint32(vi1, ==, 0x55);
  const u32 vi2 = mmdgltf__stream_read_vertex_index(&s, 2);
  munit_assert_uint32(vi2, ==, 0x1234);
  const u32 vi4 = mmdgltf__stream_read_vertex_index(&s, 4);
  munit_assert_uint32(vi4, ==, 0x00010000);

  // signed indices size 1
  const i32 i1_pos = mmdgltf__stream_read_index(&s, 1);
  munit_assert_int32(i1_pos, ==, 42);
  const i32 i1_neg = mmdgltf__stream_read_index(&s, 1);
  munit_assert_int32(i1_neg, ==, -1);

  // signed indices size 2
  const i32 i2_pos = mmdgltf__stream_read_index(&s, 2);
  munit_assert_int32(i2_pos, ==, 1000);
  const i32 i2_neg = mmdgltf__stream_read_index(&s, 2);
  munit_assert_int32(i2_neg, ==, -1);

  // signed indices size 4
  const i32 i4_pos = mmdgltf__stream_read_index(&s, 4);
  munit_assert_int32(i4_pos, ==, 70000);
  const i32 i4_neg = mmdgltf__stream_read_index(&s, 4);
  munit_assert_int32(i4_neg, ==, -1);

  munit_assert_int(s.status, ==, MMDGLTF_OK);
  munit_assert_size(src.pos, ==, sizeof(raw_index_data));

  // invalid size
  mmdgltf_Stream s_bad1 = init_mem_stream(&src, raw_index_data, sizeof(raw_index_data));
  const u32 bad_vi = mmdgltf__stream_read_vertex_index(&s_bad1, 3);
  munit_assert_uint32(bad_vi, ==, 0);
  munit_assert_int(s_bad1.status, ==, MMDGLTF_ERROR_CORRUPT_DATA);

  mmdgltf_Stream s_bad2 = init_mem_stream(&src, raw_index_data, sizeof(raw_index_data));
  const i32 bad_idx = mmdgltf__stream_read_index(&s_bad2, 5);
  munit_assert_int32(bad_idx, ==, -1);
  munit_assert_int(s_bad2.status, ==, MMDGLTF_ERROR_CORRUPT_DATA);

  return MUNIT_OK;
}

// clang-format off
static const u8 raw_string_data[] = {
  // UTF-8 string: len = 5 ("Hello")
  0x05, 0x00, 0x00, 0x00, 'H', 'e', 'l', 'l', 'o',
  // UTF-8 empty string: len = 0
  0x00, 0x00, 0x00, 0x00,
  // UTF-16LE string: len = 8 ("Miku" in UTF-16LE)
  0x08, 0x00, 0x00, 0x00, 'M', 0x00, 'i', 0x00, 'k', 0x00, 'u', 0x00,
  // UTF-16LE Japanese string: len = 6 ("あいう")
  // U+3042 (0x42, 0x30), U+3044 (0x44, 0x30), U+3046 (0x46, 0x30)
  0x06, 0x00, 0x00, 0x00, 0x42, 0x30, 0x44, 0x30, 0x46, 0x30,
  // UTF-16LE empty string: len = 0
  0x00, 0x00, 0x00, 0x00,
};
// clang-format on

static MunitResult test_stream_strings(const MunitParameter* params, void* data) {
  UNUSED(params);
  UNUSED(data);

  mmdgltf_Arena* arena = mmdgltf__arena_create(1024);
  munit_assert_ptr_not_null(arena);

  mmdgltf__MemStream src;
  mmdgltf_Stream s = init_mem_stream(&src, raw_string_data, sizeof(raw_string_data));

  // UTF-8
  char* str1 = mmdgltf__stream_read_string(&s, arena, MMDGLTF_ENCODING_UTF8);
  munit_assert_int(s.status, ==, MMDGLTF_OK);
  munit_assert_ptr_not_null(str1);
  munit_assert_string_equal(str1, "Hello");

  // UTF-8 empty
  char* str2 = mmdgltf__stream_read_string(&s, arena, MMDGLTF_ENCODING_UTF8);
  munit_assert_int(s.status, ==, MMDGLTF_OK);
  munit_assert_ptr_not_null(str2);
  munit_assert_string_equal(str2, "");

  // UTF-16LE ASCII
  char* str3 = mmdgltf__stream_read_string(&s, arena, MMDGLTF_ENCODING_UTF16LE);
  munit_assert_int(s.status, ==, MMDGLTF_OK);
  munit_assert_ptr_not_null(str3);
  munit_assert_string_equal(str3, "Miku");

  // UTF-16LE Japanese
  char* str4 = mmdgltf__stream_read_string(&s, arena, MMDGLTF_ENCODING_UTF16LE);
  munit_assert_int(s.status, ==, MMDGLTF_OK);
  munit_assert_ptr_not_null(str4);
  munit_assert_string_equal(str4, "あいう");

  // UTF-16LE empty
  char* str5 = mmdgltf__stream_read_string(&s, arena, MMDGLTF_ENCODING_UTF16LE);
  munit_assert_int(s.status, ==, MMDGLTF_OK);
  munit_assert_ptr_not_null(str5);
  munit_assert_string_equal(str5, "");

  munit_assert_size(src.pos, ==, sizeof(raw_string_data));
  mmdgltf__arena_destroy(arena);
  return MUNIT_OK;
}

static MunitResult test_stream_errors(const MunitParameter* params, void* data) {
  UNUSED(params);
  UNUSED(data);

  // truncated, 2 bytes buffer when reading u32
  const u8 short_buf[2] = {0x12, 0x34};
  mmdgltf__MemStream src;
  mmdgltf_Stream s = init_mem_stream(&src, short_buf, sizeof(short_buf));

  const u32 val = mmdgltf__stream_read_u32(&s);
  UNUSED(val);
  munit_assert_int(s.status, ==, MMDGLTF_ERROR_BUFFER_TOO_SMALL);

  // should short-circuit and remain in error state
  const u8 val2 = mmdgltf__stream_read_u8(&s);
  munit_assert_uint8(val2, ==, 0);
  munit_assert_int(s.status, ==, MMDGLTF_ERROR_BUFFER_TOO_SMALL);

  // negative length string
  const u8 neg_len_buf[4] = {0xFF, 0xFF, 0xFF, 0xFF}; // len = -1
  mmdgltf_Stream s_neg = init_mem_stream(&src, neg_len_buf, sizeof(neg_len_buf));
  mmdgltf_Arena* arena = mmdgltf__arena_create(256);
  char* neg_str = mmdgltf__stream_read_string(&s_neg, arena, MMDGLTF_ENCODING_UTF8);
  munit_assert_null(neg_str);
  munit_assert_int(s_neg.status, ==, MMDGLTF_ERROR_CORRUPT_DATA);

  // length exceeds remaining buffer
  const u8 trunc_str_buf[6] = {
    0x0A, 0x00, 0x00, 0x00, // len = 10
    'a',  'b',
  };
  mmdgltf_Stream s_trunc = init_mem_stream(&src, trunc_str_buf, sizeof(trunc_str_buf));
  char* trunc_str = mmdgltf__stream_read_string(&s_trunc, arena, MMDGLTF_ENCODING_UTF8);
  munit_assert_null(trunc_str);
  munit_assert_int(s_trunc.status, ==, MMDGLTF_ERROR_BUFFER_TOO_SMALL);

  mmdgltf__arena_destroy(arena);
  return MUNIT_OK;
}

static MunitTest tests[] = {
  {"/stream/primitives", test_stream_primitives, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/stream/vectors", test_stream_vectors, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/stream/indices", test_stream_indices, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/stream/strings", test_stream_strings, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
  {"/stream/errors", test_stream_errors, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

TestData stream_tests = {
  .tests = tests,
  .len = ARRSZ(tests),
};
