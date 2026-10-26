#ifndef MMDGLTF_H
#define MMDGLTF_H

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


#ifdef __cplusplus
extern "C" {
#endif

typedef void*(PFN_mmdgltf_alloc_func)(size_t size, void* user);
typedef void*(PFN_mmdgltf_realloc_func)(void* ptr, size_t oldsz, size_t newsz, void* user);
typedef void(PFN_mmdgltf_dealloc_func)(void* ptr, size_t size, void* user);

typedef struct mmdgltf_Alloc{
  PFN_mmdgltf_alloc_func alloc;
  PFN_mmdgltf_realloc_func realloc;
  PFN_mmdgltf_dealloc_func dealloc;
  void* user;
} mmdgltf_Alloc;

MMDGLTF_API void mmdgltf_set_alloc(const mmdgltf_Alloc* alloc);

typedef struct mmdgltf_Model_T* mmdgltf_Model;

MMDGLTF_API mmdgltf_Model mmdgltf_load_model(const char* path);

#ifdef __cplusplus
}
#endif

#endif // MMDGLTF_H
