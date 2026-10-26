#ifndef MMDGLTF_INTERNAL_H
#define MMDGLTF_INTERNAL_H

#include "../include/mmdgltf/mmdgltf.h"

#include <assert.h>
#include <stdint.h>

#define UNUSED(thing) (void)thing
#define ARRSZ(arr) sizeof(arr) / sizeof(arr[0])

extern _Thread_local mmdgltf_Alloc mmdgltf__alloc;
#define MMDGLTF_MALLOC(_sz) mmdgltf__alloc.alloc(_sz, mmdgltf__alloc.user)
#define MMDGLTF_CALLOC(_sz, _n) MMDGLTF_MALLOC(_sz*_n)
#define MMDGLTF_REALLOC(_ptr, _oldsz, _newsz) \
  mmdgltf__alloc(_ptr, _oldsz, _newsz, mmdgltf__alloc.user)
#define MMDGLTF_FREE(_ptr, _sz) mmdgltf__alloc.dealloc(_ptr, _sz, mmdgltf__alloc.user)

typedef int8_t i8;
typedef uint16_t u16;
typedef int16_t i16;
typedef uint32_t u32;
typedef int32_t i32;
typedef int64_t i64;

typedef float f32;
static_assert(sizeof(f32) == 4, "float size mismatch");
typedef double f64;
static_assert(sizeof(f64) == 8, "double size mismatch");



#endif // MMDGLTF_INTERNAL_H
