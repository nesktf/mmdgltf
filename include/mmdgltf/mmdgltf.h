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

#ifndef MMDGLTF_MAX_BONE_WEIGHTS
#define MMDGLTF_MAX_BONE_WEIGHTS 4
#endif

#ifndef MMDGLTF_MAX_EXTRA_UVS
#define MMDGLTF_MAX_EXTRA_UVS 4
#endif

#define MMDGLTF_ENCODE_ERROR(status) ((void*)(((uintptr_t)(status)) << 48))
#define MMDGLTF_CHECK(ptr)           ((mmdgltf_Status)(((uintptr_t)(ptr)) >> 48))

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

typedef enum mmdgltf_WeightType {
  MMDGLTF_WEIGHT_BDEF1 = 0,
  MMDGLTF_WEIGHT_BDEF2 = 1,
  MMDGLTF_WEIGHT_BDEF4 = 2,
  MMDGLTF_WEIGHT_SDEF = 3,
  MMDGLTF_WEIGHT_QDEF = 4,
} mmdgltf_WeightType;

typedef enum mmdgltf_SphereMode {
  MMDGLTF_SPHERE_NONE = 0,
  MMDGLTF_SPHERE_MUL = 1,
  MMDGLTF_SPHERE_ADD = 2,
  MMDGLTF_SPHERE_SUBTEXTURE = 3,
} mmdgltf_SphereMode;

#define MMDGLTF_MATERIAL_DOUBLE_SIDED    0x01
#define MMDGLTF_MATERIAL_GROUND_SHADOW   0x02
#define MMDGLTF_MATERIAL_SELF_SHADOW_MAP 0x04
#define MMDGLTF_MATERIAL_SELF_SHADOW     0x08
#define MMDGLTF_MATERIAL_EDGE            0x10
#define MMDGLTF_MATERIAL_VERTEX_COLOR    0x20
#define MMDGLTF_MATERIAL_POINT_DRAWING   0x40
#define MMDGLTF_MATERIAL_LINE_DRAWING    0x80

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

typedef struct mmdgltf_PmxVertex {
  float position[3];
  float normal[3];
  float uv[2];
  float additional_uvs[MMDGLTF_MAX_EXTRA_UVS][4];
  mmdgltf_WeightType weight_type;
  int32_t bones[MMDGLTF_MAX_BONE_WEIGHTS];
  float weights[MMDGLTF_MAX_BONE_WEIGHTS];
  float sdef_c[3];
  float sdef_r0[3];
  float sdef_r1[3];
  float edge_scale;
} mmdgltf_PmxVertex;

typedef struct mmdgltf_PmxTexture {
  const char* path;
} mmdgltf_PmxTexture;

typedef struct mmdgltf_PmxMaterial {
  const char* name_jp;
  const char* name_en;
  const char* comment;
  float diffuse[4];
  float specular[3];
  float shininess;
  float ambient[3];
  uint8_t draw_flags;
  float edge_color[4];
  float edge_size;
  int32_t texture_index;
  int32_t sphere_texture_index;
  mmdgltf_SphereMode sphere_mode;
  uint8_t toon_sharing_flag;
  int32_t toon_index;
  uint32_t index_count;
} mmdgltf_PmxMaterial;

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
MMDGLTF_API mmdgltf_Status mmdgltf_pmx_vertices(mmdgltf_Pmx pmx,
                                                const mmdgltf_PmxVertex** out_vertices,
                                                size_t* out_count);
MMDGLTF_API mmdgltf_Status mmdgltf_pmx_indices(mmdgltf_Pmx pmx, const uint32_t** out_indices,
                                               size_t* out_count);
MMDGLTF_API mmdgltf_Status mmdgltf_pmx_textures(mmdgltf_Pmx pmx,
                                                const mmdgltf_PmxTexture** out_textures,
                                                size_t* out_count);
MMDGLTF_API mmdgltf_Status mmdgltf_pmx_materials(mmdgltf_Pmx pmx,
                                                 const mmdgltf_PmxMaterial** out_materials,
                                                 size_t* out_count);

#ifdef __cplusplus
}
#endif

#endif // MMDGLTF_H
