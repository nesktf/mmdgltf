#ifndef MMDGLTF_INTERNAL_H
#define MMDGLTF_INTERNAL_H

#define MMDGLTF__INTERNAL
#include "../include/mmdgltf/mmdgltf.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#define UNUSED(thing) (void)(thing)
#define ARRSZ(arr)    (sizeof(arr) / sizeof((arr)[0]))

#if defined(_MSC_VER)
#define MMDGLTF_INLINE   __forceinline
#define MMDGLTF_NOINLINE __declspec((noinline))
#define MMDGLTF_NORETURN __declspec((noreturn))
#elif defined(__GNUC__) || defined(__MINGW32__)
#define MMDGLTF_INLINE   inline __attribute__((__always_inline__))
#define MMDGLTF_NOINLINE __attribute__((__noinline__))
#define MMDGLTF_NORETURN __attribute__((__noreturn__))
#elif defined(__clang__)
#define MMDGLTF_INLINE   __forceinline__
#define MMDGLTF_NOINLINE __noinline__
#define MMDGLTF_NORETURN __noreturn__
#else
#define MMDGLTF_INLINE inline
#define MMDGLTF_NOINLINE
#endif

extern _Thread_local mmdgltf_Alloc mmdgltf__alloc;
#define MMDGLTF_MALLOC(_sz)     mmdgltf__alloc.alloc((_sz), mmdgltf__alloc.user)
#define MMDGLTF_CALLOC(_sz, _n) MMDGLTF_MALLOC((_sz) * (_n))
#define MMDGLTF_REALLOC(_ptr, _oldsz, _newsz) \
  mmdgltf__alloc.realloc((_ptr), (_oldsz), (_newsz), mmdgltf__alloc.user)
#define MMDGLTF_FREE(_ptr, _sz) mmdgltf__alloc.dealloc((_ptr), (_sz), mmdgltf__alloc.user)

typedef uint8_t u8;
typedef int8_t i8;
typedef uint16_t u16;
typedef int16_t i16;
typedef uint32_t u32;
typedef int32_t i32;
typedef uint64_t u64;
typedef int64_t i64;

typedef float f32;
static_assert(sizeof(f32) == 4, "float size mismatch");
typedef double f64;
static_assert(sizeof(f64) == 8, "double size mismatch");

typedef struct mmdgltf_ArenaBlock {
  u8* base;
  size_t used;
  size_t capacity;
  struct mmdgltf_ArenaBlock* next;
} mmdgltf_ArenaBlock;

typedef struct mmdgltf_Arena {
  mmdgltf_ArenaBlock* head;
  mmdgltf_ArenaBlock* current;
  size_t total_allocated;
  size_t block_size;
} mmdgltf_Arena;

mmdgltf_Arena* mmdgltf__arena_create(size_t block_size);
void* mmdgltf__arena_alloc(mmdgltf_Arena* arena, size_t size);
char* mmdgltf__arena_strndup(mmdgltf_Arena* arena, const char* str, size_t len);
void mmdgltf__arena_destroy(mmdgltf_Arena* arena);

typedef struct mmdgltf_Stream {
  mmdgltf_StreamFunc func;
  void* src;
  mmdgltf_Status status;
} mmdgltf_Stream;

u8 mmdgltf__stream_read_u8(mmdgltf_Stream* s);
i8 mmdgltf__stream_read_i8(mmdgltf_Stream* s);
u16 mmdgltf__stream_read_u16(mmdgltf_Stream* s);
i16 mmdgltf__stream_read_i16(mmdgltf_Stream* s);
u32 mmdgltf__stream_read_u32(mmdgltf_Stream* s);
i32 mmdgltf__stream_read_i32(mmdgltf_Stream* s);
f32 mmdgltf__stream_read_f32(mmdgltf_Stream* s);
mmdgltf_Vec2f mmdgltf__stream_read_vec2(mmdgltf_Stream* s);
mmdgltf_Vec3f mmdgltf__stream_read_vec3(mmdgltf_Stream* s);
mmdgltf_Vec4f mmdgltf__stream_read_vec4(mmdgltf_Stream* s);
mmdgltf_Color3f mmdgltf__stream_read_color3(mmdgltf_Stream* s);
mmdgltf_Color4f mmdgltf__stream_read_color4(mmdgltf_Stream* s);
mmdgltf_Quatf mmdgltf__stream_read_quat(mmdgltf_Stream* s);
u32 mmdgltf__stream_read_vertex_index(mmdgltf_Stream* s, u8 size);
i32 mmdgltf__stream_read_index(mmdgltf_Stream* s, u8 size);
void mmdgltf__stream_read_bytes(mmdgltf_Stream* s, void* dst, size_t count);
char* mmdgltf__stream_read_string(mmdgltf_Stream* s, mmdgltf_Arena* arena,
                                  mmdgltf_TextEncoding encoding);

struct mmdgltf_Pmx_T {
  mmdgltf_Arena* arena;
  mmdgltf_PmxHeader header;
  mmdgltf_PmxModelInfo model_info;
  mmdgltf_PmxVertex* vertices;
  size_t vertex_count;
  u32* indices;
  size_t index_count;
  mmdgltf_PmxTexture* textures;
  size_t texture_count;
  mmdgltf_PmxMaterial* materials;
  size_t material_count;
  mmdgltf_PmxBone* bones;
  size_t bone_count;
  mmdgltf_PmxMorph* morphs;
  size_t morph_count;
  mmdgltf_PmxDisplayItem* display_items;
  size_t display_item_count;
  mmdgltf_PmxRigidBody* rigid_bodies;
  size_t rigid_body_count;
  mmdgltf_PmxJoint* joints;
  size_t joint_count;
};

#endif // MMDGLTF_INTERNAL_H
