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

typedef struct mmdgltf_Vec2f {
  float x, y;
} mmdgltf_Vec2f;

typedef struct mmdgltf_Vec3f {
  float x, y, z;
} mmdgltf_Vec3f;

typedef struct mmdgltf_Color3f {
  float r, g, b;
} mmdgltf_Color3f;

typedef struct mmdgltf_Vec4f {
  float x, y, z, w;
} mmdgltf_Vec4f;

typedef struct mmdgltf_Color4f {
  float r, g, b, a;
} mmdgltf_Color4f;

typedef struct mmdgltf_Quatf {
  float x, y, z, w;
} mmdgltf_Quatf;

typedef enum mmdgltf_TextEncoding {
  MMDGLTF_ENCODING_UTF16LE = 0,
  MMDGLTF_ENCODING_UTF8 = 1,
  MMDGLTF_ENCODING_CP932 = 2,
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
  mmdgltf_Vec3f position;
  mmdgltf_Vec3f normal;
  mmdgltf_Vec2f uv;
  mmdgltf_Vec4f additional_uvs[MMDGLTF_MAX_EXTRA_UVS];
  mmdgltf_WeightType weight_type;
  int32_t bones[MMDGLTF_MAX_BONE_WEIGHTS];
  float weights[MMDGLTF_MAX_BONE_WEIGHTS];
  mmdgltf_Vec3f sdef_c;
  mmdgltf_Vec3f sdef_r0;
  mmdgltf_Vec3f sdef_r1;
  float edge_scale;
} mmdgltf_PmxVertex;

typedef struct mmdgltf_PmxTexture {
  const char* path;
} mmdgltf_PmxTexture;

typedef struct mmdgltf_PmxMaterial {
  const char* name_jp;
  const char* name_en;
  const char* comment;
  mmdgltf_Color4f diffuse;
  mmdgltf_Color3f specular;
  float shininess;
  mmdgltf_Color3f ambient;
  uint8_t draw_flags;
  mmdgltf_Color4f edge_color;
  float edge_size;
  int32_t texture_index;
  int32_t sphere_texture_index;
  mmdgltf_SphereMode sphere_mode;
  uint8_t toon_sharing_flag;
  int32_t toon_index;
  uint32_t index_count;
} mmdgltf_PmxMaterial;

#define MMDGLTF_BONE_CONNECTION_TARGET_BONE 0x0001
#define MMDGLTF_BONE_ROTATABLE              0x0002
#define MMDGLTF_BONE_MOVABLE                0x0004
#define MMDGLTF_BONE_VISIBLE                0x0008
#define MMDGLTF_BONE_CONTROLLABLE           0x0010
#define MMDGLTF_BONE_IK                     0x0020
#define MMDGLTF_BONE_LOCAL_APPEND           0x0080
#define MMDGLTF_BONE_APPEND_ROTATION        0x0100
#define MMDGLTF_BONE_APPEND_TRANSLATION     0x0200
#define MMDGLTF_BONE_FIXED_AXIS             0x0400
#define MMDGLTF_BONE_LOCAL_AXIS             0x0800
#define MMDGLTF_BONE_PHYSICS_TRANSFORM      0x1000
#define MMDGLTF_BONE_EXTERNAL_PARENT        0x2000

typedef struct mmdgltf_PmxIkLink {
  int32_t bone_index;
  uint8_t has_limits;
  mmdgltf_Vec3f min_angle;
  mmdgltf_Vec3f max_angle;
} mmdgltf_PmxIkLink;

typedef struct mmdgltf_PmxBone {
  const char* name_jp;
  const char* name_en;
  mmdgltf_Vec3f position;
  int32_t parent_index;
  int32_t transform_order;
  uint16_t flags;
  mmdgltf_Vec3f offset;
  int32_t connection_bone_index;
  int32_t inherit_bone_index;
  float inherit_weight;
  mmdgltf_Vec3f fixed_axis;
  mmdgltf_Vec3f local_x;
  mmdgltf_Vec3f local_z;
  int32_t external_parent_key;
  int32_t ik_target_index;
  int32_t ik_loop_count;
  float ik_limit_angle;
  uint32_t ik_link_count;
  const mmdgltf_PmxIkLink* ik_links;
} mmdgltf_PmxBone;

typedef enum mmdgltf_MorphCategory {
  MMDGLTF_MORPH_CATEGORY_SYSTEM = 0,
  MMDGLTF_MORPH_CATEGORY_EYEBROW = 1,
  MMDGLTF_MORPH_CATEGORY_EYE = 2,
  MMDGLTF_MORPH_CATEGORY_MOUTH = 3,
  MMDGLTF_MORPH_CATEGORY_OTHER = 4,
} mmdgltf_MorphCategory;

typedef enum mmdgltf_MorphType {
  MMDGLTF_MORPH_GROUP = 0,
  MMDGLTF_MORPH_VERTEX = 1,
  MMDGLTF_MORPH_BONE = 2,
  MMDGLTF_MORPH_UV = 3,
  MMDGLTF_MORPH_EXTRA_UV1 = 4,
  MMDGLTF_MORPH_EXTRA_UV2 = 5,
  MMDGLTF_MORPH_EXTRA_UV3 = 6,
  MMDGLTF_MORPH_EXTRA_UV4 = 7,
  MMDGLTF_MORPH_MATERIAL = 8,
  MMDGLTF_MORPH_FLIP = 9,
  MMDGLTF_MORPH_IMPULSE = 10,
} mmdgltf_MorphType;

typedef struct mmdgltf_PmxVertexMorphOffset {
  uint32_t vertex_index;
  mmdgltf_Vec3f position_offset;
} mmdgltf_PmxVertexMorphOffset;

typedef struct mmdgltf_PmxUvMorphOffset {
  uint32_t vertex_index;
  mmdgltf_Vec4f uv_offset;
} mmdgltf_PmxUvMorphOffset;

typedef struct mmdgltf_PmxBoneMorphOffset {
  int32_t bone_index;
  mmdgltf_Vec3f translation_offset;
  mmdgltf_Quatf rotation_offset;
} mmdgltf_PmxBoneMorphOffset;

typedef struct mmdgltf_PmxMaterialMorphOffset {
  int32_t material_index;
  uint8_t offset_type;
  mmdgltf_Color4f diffuse;
  mmdgltf_Color3f specular;
  float shininess;
  mmdgltf_Color3f ambient;
  mmdgltf_Color4f edge_color;
  float edge_size;
  mmdgltf_Color4f texture_factor;
  mmdgltf_Color4f sphere_texture_factor;
  mmdgltf_Color4f toon_texture_factor;
} mmdgltf_PmxMaterialMorphOffset;

typedef struct mmdgltf_PmxGroupMorphOffset {
  int32_t morph_index;
  float factor;
} mmdgltf_PmxGroupMorphOffset;

typedef struct mmdgltf_PmxMorph {
  const char* name_jp;
  const char* name_en;
  mmdgltf_MorphCategory category;
  mmdgltf_MorphType type;
  uint32_t offset_count;

  union {
    const mmdgltf_PmxVertexMorphOffset* vertex_offsets;
    const mmdgltf_PmxUvMorphOffset* uv_offsets;
    const mmdgltf_PmxBoneMorphOffset* bone_offsets;
    const mmdgltf_PmxMaterialMorphOffset* material_offsets;
    const mmdgltf_PmxGroupMorphOffset* group_offsets;
    const void* raw_offsets;
  };
} mmdgltf_PmxMorph;

typedef struct mmdgltf_PmxDisplayElement {
  uint8_t type;
  int32_t index;
} mmdgltf_PmxDisplayElement;

typedef struct mmdgltf_PmxDisplayItem {
  const char* name_jp;
  const char* name_en;
  uint8_t is_special;
  uint32_t element_count;
  const mmdgltf_PmxDisplayElement* elements;
} mmdgltf_PmxDisplayItem;

typedef enum mmdgltf_RigidShapeType {
  MMDGLTF_RIGID_SHAPE_SPHERE = 0,
  MMDGLTF_RIGID_SHAPE_BOX = 1,
  MMDGLTF_RIGID_SHAPE_CAPSULE = 2,
} mmdgltf_RigidShapeType;

typedef enum mmdgltf_RigidPhysicsMode {
  MMDGLTF_RIGID_MODE_STATIC = 0,
  MMDGLTF_RIGID_MODE_DYNAMIC = 1,
  MMDGLTF_RIGID_MODE_DYNAMIC_BONE_ALIGN = 2,
} mmdgltf_RigidPhysicsMode;

typedef struct mmdgltf_PmxRigidBody {
  const char* name_jp;
  const char* name_en;
  int32_t bone_index;
  uint8_t collision_group;
  uint16_t collision_mask;
  mmdgltf_RigidShapeType shape_type;
  mmdgltf_Vec3f size;
  mmdgltf_Vec3f position;
  mmdgltf_Vec3f rotation;
  float mass;
  float linear_damping;
  float angular_damping;
  float restitution;
  float friction;
  mmdgltf_RigidPhysicsMode physics_mode;
} mmdgltf_PmxRigidBody;

typedef struct mmdgltf_PmxJoint {
  const char* name_jp;
  const char* name_en;
  uint8_t type;
  int32_t src_rigid;
  int32_t dest_rigid;
  mmdgltf_Vec3f position;
  mmdgltf_Vec3f rotation;
  mmdgltf_Vec3f min_position;
  mmdgltf_Vec3f max_position;
  mmdgltf_Vec3f min_rotation;
  mmdgltf_Vec3f max_rotation;
  mmdgltf_Vec3f spring_position;
  mmdgltf_Vec3f spring_rotation;
} mmdgltf_PmxJoint;

typedef struct mmdgltf_Pmx_T* mmdgltf_Pmx;

typedef enum mmdgltf_CoordinateSystem {
  MMDGLTF_COORDS_GLTF_RIGHT_HANDED = 0,
  MMDGLTF_COORDS_MMD_LEFT_HANDED = 1,
} mmdgltf_CoordinateSystem;

#define MMDGLTF_LOAD_VERTICES  0x0001
#define MMDGLTF_LOAD_MATERIALS 0x0002
#define MMDGLTF_LOAD_BONES     0x0004
#define MMDGLTF_LOAD_MORPHS    0x0008
#define MMDGLTF_LOAD_PHYSICS   0x0010
#define MMDGLTF_LOAD_TEXTURES  0x0020
#define MMDGLTF_LOAD_ALL       0x003F

typedef struct mmdgltf_Image {
  const uint8_t* data;
  size_t size;
  const char* mime_type;
  int32_t width;
  int32_t height;
  int32_t channels;
  const char* path;
} mmdgltf_Image;

typedef struct mmdgltf_ModelConfig {
  uint32_t load_flags;
  mmdgltf_CoordinateSystem coordinate_system;
  float scale;
  const char* base_dir;
  uint8_t strict_textures;
} mmdgltf_ModelConfig;

typedef struct mmdgltf_VertexBuffers {
  size_t vertex_count;
  const float* positions;
  const float* normals;
  const float* uvs;
  const uint16_t* joints;
  const float* weights;
  const float* sdef_c;
  const float* sdef_r0;
  const float* sdef_r1;
} mmdgltf_VertexBuffers;

typedef struct mmdgltf_Model_T* mmdgltf_Model;

typedef struct mmdgltf_ExportConfig {
  uint8_t unlit;
  uint8_t export_skinning;
  uint8_t export_morphs;
  uint8_t export_physics;
  uint8_t english_bone_names;
  float scale;
} mmdgltf_ExportConfig;

MMDGLTF_API void mmdgltf_set_alloc(const mmdgltf_Alloc* alloc);

MMDGLTF_API void mmdgltf_model_config_default(mmdgltf_ModelConfig* out_config);
MMDGLTF_API mmdgltf_Model mmdgltf_model_create(mmdgltf_Pmx pmx, const mmdgltf_ModelConfig* config);
MMDGLTF_API void mmdgltf_model_destroy(mmdgltf_Model model);

MMDGLTF_API mmdgltf_Status mmdgltf_model_vertex_buffers(mmdgltf_Model model,
                                                        mmdgltf_VertexBuffers* out_buffers);
MMDGLTF_API mmdgltf_Status mmdgltf_model_indices(mmdgltf_Model model, const uint32_t** out_indices,
                                                 size_t* out_count);
MMDGLTF_API mmdgltf_Status mmdgltf_model_materials(mmdgltf_Model model,
                                                   const mmdgltf_PmxMaterial** out_materials,
                                                   size_t* out_count);
MMDGLTF_API mmdgltf_Status mmdgltf_model_images(mmdgltf_Model model,
                                                const mmdgltf_Image** out_images,
                                                size_t* out_count);
MMDGLTF_API mmdgltf_Status mmdgltf_model_bones(mmdgltf_Model model,
                                               const mmdgltf_PmxBone** out_bones,
                                               size_t* out_count);
MMDGLTF_API mmdgltf_Status mmdgltf_model_morphs(mmdgltf_Model model,
                                                const mmdgltf_PmxMorph** out_morphs,
                                                size_t* out_count);
MMDGLTF_API mmdgltf_Status mmdgltf_model_rigid_bodies(
  mmdgltf_Model model, const mmdgltf_PmxRigidBody** out_rigid_bodies, size_t* out_count);
MMDGLTF_API mmdgltf_Status mmdgltf_model_joints(mmdgltf_Model model,
                                                const mmdgltf_PmxJoint** out_joints,
                                                size_t* out_count);

MMDGLTF_API mmdgltf_Pmx mmdgltf_pmx_load_file(const char* path);
MMDGLTF_API mmdgltf_Pmx mmdgltf_pmx_load_filen(const char* path, size_t path_len);
MMDGLTF_API mmdgltf_Pmx mmdgltf_pmx_load_memory(const void* data, size_t size);
MMDGLTF_API mmdgltf_Pmx mmdgltf_pmx_load_stream(const mmdgltf_StreamFunc* func, void* src);
MMDGLTF_API void mmdgltf_pmx_destroy(mmdgltf_Pmx pmx);

MMDGLTF_API mmdgltf_Pmx mmdgltf_pmd_load_file(const char* path);
MMDGLTF_API mmdgltf_Pmx mmdgltf_pmd_load_filen(const char* path, size_t path_len);
MMDGLTF_API mmdgltf_Pmx mmdgltf_pmd_load_memory(const void* data, size_t size);
MMDGLTF_API mmdgltf_Pmx mmdgltf_pmd_load_stream(const mmdgltf_StreamFunc* func, void* src);

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
MMDGLTF_API mmdgltf_Status mmdgltf_pmx_bones(mmdgltf_Pmx pmx, const mmdgltf_PmxBone** out_bones,
                                             size_t* out_count);
MMDGLTF_API mmdgltf_Status mmdgltf_pmx_morphs(mmdgltf_Pmx pmx, const mmdgltf_PmxMorph** out_morphs,
                                              size_t* out_count);
MMDGLTF_API mmdgltf_Status mmdgltf_pmx_display_items(
  mmdgltf_Pmx pmx, const mmdgltf_PmxDisplayItem** out_display_items, size_t* out_count);
MMDGLTF_API mmdgltf_Status mmdgltf_pmx_rigid_bodies(mmdgltf_Pmx pmx,
                                                    const mmdgltf_PmxRigidBody** out_rigid_bodies,
                                                    size_t* out_count);
MMDGLTF_API mmdgltf_Status mmdgltf_pmx_joints(mmdgltf_Pmx pmx, const mmdgltf_PmxJoint** out_joints,
                                              size_t* out_count);

MMDGLTF_API void mmdgltf_export_config_default(mmdgltf_ExportConfig* out_config);
MMDGLTF_API mmdgltf_Status mmdgltf_export_glb_file(mmdgltf_Model model, const char* filename,
                                                   const mmdgltf_ExportConfig* config);
MMDGLTF_API mmdgltf_Status mmdgltf_export_glb_filen(mmdgltf_Model model, const char* filename,
                                                    size_t filename_len,
                                                    const mmdgltf_ExportConfig* config);
MMDGLTF_API mmdgltf_Status mmdgltf_export_gltf_file(mmdgltf_Model model, const char* filename,
                                                    const mmdgltf_ExportConfig* config);
MMDGLTF_API mmdgltf_Status mmdgltf_export_gltf_filen(mmdgltf_Model model, const char* filename,
                                                     size_t filename_len,
                                                     const mmdgltf_ExportConfig* config);
MMDGLTF_API mmdgltf_Status mmdgltf_export_glb_memory(mmdgltf_Model model, uint8_t** out_data,
                                                     size_t* out_size,
                                                     const mmdgltf_ExportConfig* config);
MMDGLTF_API void mmdgltf_export_free(uint8_t* data, size_t size);

MMDGLTF_API mmdgltf_Status mmdgltf_convert_pmx_file(const char* pmx_path, const char* glb_path,
                                                    const mmdgltf_ExportConfig* config);
MMDGLTF_API mmdgltf_Status mmdgltf_convert_pmx_filen(const char* pmx_path, size_t pmx_path_len,
                                                     const char* glb_path, size_t glb_path_len,
                                                     const mmdgltf_ExportConfig* config);
MMDGLTF_API mmdgltf_Status mmdgltf_convert_pmx_to_gltf_file(const char* pmx_path,
                                                            const char* gltf_path,
                                                            const mmdgltf_ExportConfig* config);
MMDGLTF_API mmdgltf_Status mmdgltf_convert_pmx_to_gltf_filen(const char* pmx_path,
                                                             size_t pmx_path_len,
                                                             const char* gltf_path,
                                                             size_t gltf_path_len,
                                                             const mmdgltf_ExportConfig* config);
MMDGLTF_API mmdgltf_Status mmdgltf_convert_pmx_memory(const uint8_t* pmx_data, size_t pmx_size,
                                                      uint8_t** out_glb_data, size_t* out_glb_size,
                                                      const mmdgltf_ExportConfig* config);

#ifdef __cplusplus
}
#endif

#endif // MMDGLTF_H
