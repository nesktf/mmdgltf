#ifndef MMDGLTF_H
#define MMDGLTF_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#if defined MMDGLTF_STATIC
#define MMDGLTF_API
#elif defined _WIN32 || defined __CYGWIN__
#if defined MMDGLTF__INTERNAL
#define MMDGLTF_API __declspec(dllexport)
#else
#define MMDGLTF_API __declspec(dllimport)
#endif
#elif defined MMDGLTF__INTERNAL && defined __GNUC__
#define MMDGLTF_API __attribute__((visibility("default")))
#else
#define MMDGLTF_API
#endif

#define MMDGLTF_SEEK_SET 0
#define MMDGLTF_SEEK_CUR 1
#define MMDGLTF_SEEK_END 2

#ifdef __cplusplus
extern "C" {
#endif

typedef enum mmdgltf_Status {
  MMDGLTF_OK = 0,
  MMDGLTF_ERROR_INVALID_ARGUMENT = 1,
  MMDGLTF_ERROR_OUT_OF_MEMORY = 2,
  MMDGLTF_ERROR_IO = 3,
  MMDGLTF_ERROR_INVALID_SIGNATURE = 4,
  MMDGLTF_ERROR_UNSUPPORTED_VERSION = 5,
  MMDGLTF_ERROR_BUFFER_TOO_SMALL = 6,
  MMDGLTF_ERROR_CORRUPT_DATA = 7,
} mmdgltf_Status;

#define MMDGLTF_ENCODE_ERROR(status) ((void*)(((uintptr_t)(status)) << 48))
#define MMDGLTF_CHECK(ptr)           ((mmdgltf_Status)(((uintptr_t)(ptr)) >> 48))

typedef void* (*PFN_mmdgltf_alloc_func)(size_t size, void* user);
typedef void* (*PFN_mmdgltf_realloc_func)(void* ptr, size_t oldsz, size_t newsz, void* user);
typedef void (*PFN_mmdgltf_dealloc_func)(void* ptr, size_t size, void* user);

typedef struct mmdgltf_Alloc {
  PFN_mmdgltf_alloc_func alloc;
  PFN_mmdgltf_realloc_func realloc;
  PFN_mmdgltf_dealloc_func dealloc;
  void* user;
} mmdgltf_Alloc;

typedef size_t (*PFN_mmdgltf_read_func)(void* buff, size_t sz, size_t nmemb, void* src);
typedef int (*PFN_mmdgltf_seek_func)(void* src, long offset, int origin);
typedef long (*PFN_mmdgltf_tell_func)(void* src);
typedef int (*PFN_mmdgltf_eof_func)(void* src);

typedef struct mmdgltf_StreamFunc {
  PFN_mmdgltf_read_func read;
  PFN_mmdgltf_seek_func seek;
  PFN_mmdgltf_tell_func tell;
  PFN_mmdgltf_eof_func eof;
} mmdgltf_StreamFunc;

typedef enum mmdgltf_TextEncoding {
  MMDGLTF_ENCODING_UTF16LE = 0,
  MMDGLTF_ENCODING_UTF8 = 1,
} mmdgltf_TextEncoding;

typedef struct mmdgltf_PmxHeader {
  float version;
  mmdgltf_TextEncoding encoding;
  uint8_t additional_uvs;
  uint8_t vertex_index_size;
  uint8_t texture_index_size;
  uint8_t material_index_size;
  uint8_t bone_index_size;
  uint8_t morph_index_size;
  uint8_t rigid_index_size;
} mmdgltf_PmxHeader;

typedef struct mmdgltf_PmxModelInfo {
  const char* name_jp;
  const char* name_en;
  const char* comment_jp;
  const char* comment_en;
} mmdgltf_PmxModelInfo;

typedef struct mmdgltf_Pmx_T* mmdgltf_Pmx;

MMDGLTF_API void mmdgltf_set_alloc(const mmdgltf_Alloc* alloc);

MMDGLTF_API mmdgltf_Pmx mmdgltf_pmx_load_file(const char* path);
MMDGLTF_API mmdgltf_Pmx mmdgltf_pmx_load_filen(const char* path, size_t path_len);
MMDGLTF_API mmdgltf_Pmx mmdgltf_pmx_load_memory(const void* data, size_t size);
MMDGLTF_API mmdgltf_Pmx mmdgltf_pmx_load_stream(const mmdgltf_StreamFunc* func, void* src);
MMDGLTF_API void mmdgltf_pmx_destroy(mmdgltf_Pmx pmx);

MMDGLTF_API mmdgltf_Status mmdgltf_pmx_get_header(mmdgltf_Pmx pmx, mmdgltf_PmxHeader* out_header);
MMDGLTF_API mmdgltf_Status mmdgltf_pmx_get_model_info(mmdgltf_Pmx pmx,
                                                      mmdgltf_PmxModelInfo* out_info);

#ifdef __cplusplus
}
#endif

#endif // MMDGLTF_H
