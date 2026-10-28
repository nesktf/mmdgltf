#include "mmdgltf_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32)
#include <dirent.h>
#include <strings.h>
#include <sys/stat.h>
#endif

static void* stb_malloc(size_t sz) {
  return MMDGLTF_MALLOC(sz);
}

static void* stb_realloc(void* p, size_t newsz) {
  return MMDGLTF_REALLOC(p, 0, newsz);
}

static void stb_free(void* p) {
  MMDGLTF_FREE(p, 0);
}

#define STBI_MALLOC(sz)        stb_malloc(sz)
#define STBI_REALLOC(p, newsz) stb_realloc(p, newsz)
#define STBI_FREE(p)           stb_free(p)
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#define STBIW_MALLOC(sz)        stb_malloc(sz)
#define STBIW_REALLOC(p, newsz) stb_realloc(p, newsz)
#define STBIW_FREE(p)           stb_free(p)
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

#if !defined(_WIN32)
static bool resolve_path_ci(const char* base, const char* rel, char* out, size_t out_sz) {
  assert(rel);
  assert(out);
  assert(out_sz != 0);

  char current[4096];
  if (base && base[0] != '\0') {
    snprintf(current, sizeof(current), "%s", base);
  } else {
    current[0] = '.';
    current[1] = '\0';
  }

  char rel_copy[4096];
  snprintf(rel_copy, sizeof(rel_copy), "%s", rel);

  char* rest = rel_copy;
  char* token = NULL;
  while ((token = strtok_r(rest, "/", &rest)) != NULL) {
    if (strcmp(token, ".") == 0) {
      continue;
    }
    if (strcmp(token, "..") == 0) {
      char next[4096];
      snprintf(next, sizeof(next), "%s/..", current);
      snprintf(current, sizeof(current), "%s", next);
      continue;
    }

    DIR* dir = opendir(current);
    if (!dir) {
      return false;
    }

    struct dirent* entry;
    bool found = false;
    char matched_name[256];
    while ((entry = readdir(dir)) != NULL) {
      if (strcasecmp(entry->d_name, token) == 0) {
        snprintf(matched_name, sizeof(matched_name), "%s", entry->d_name);
        found = true;
        break;
      }
    }
    closedir(dir);

    if (!found) {
      return false;
    }

    char next[4096];
    snprintf(next, sizeof(next), "%s/%s", current, matched_name);
    snprintf(current, sizeof(current), "%s", next);
  }

  snprintf(out, out_sz, "%s", current);
  return true;
}
#endif

static FILE* open_texture_file(const char* base_dir, const char* rel_path) {
  char path_buf[4096];
  if (base_dir && base_dir[0] != '\0' && strcmp(base_dir, ".") != 0) {
    snprintf(path_buf, sizeof(path_buf), "%s/%s", base_dir, rel_path);
  } else {
    snprintf(path_buf, sizeof(path_buf), "%s", rel_path);
  }

  for (size_t i = 0; path_buf[i] != '\0'; ++i) {
    if (path_buf[i] == '\\') {
      path_buf[i] = '/';
    }
  }

  FILE* f = fopen(path_buf, "rb");
  if (f) {
    return f;
  }

#if !defined(_WIN32)
  if (resolve_path_ci(base_dir, rel_path, path_buf, sizeof(path_buf))) {
    return fopen(path_buf, "rb");
  }
#endif

  return NULL;
}

mmdgltf_Status mmdgltf__image_load(const char* base_dir, const char* rel_path,
                                   mmdgltf_Arena* arena, mmdgltf_Image* out_image) {
  assert(rel_path);
  assert(arena);
  assert(out_image);

  memset(out_image, 0, sizeof(*out_image));
  out_image->path = mmdgltf__arena_strndup(arena, rel_path, strlen(rel_path));

  FILE* f = open_texture_file(base_dir, rel_path);
  if (!f) {
    return MMDGLTF_ERROR_IO;
  }

  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    return MMDGLTF_ERROR_IO;
  }
  const long file_size_long = ftell(f);
  if (file_size_long <= 0) {
    fclose(f);
    return MMDGLTF_ERROR_CORRUPT_DATA;
  }
  const size_t file_size = (size_t)file_size_long;
  if (fseek(f, 0, SEEK_SET) != 0) {
    fclose(f);
    return MMDGLTF_ERROR_IO;
  }

  u8* raw_bytes = MMDGLTF_MALLOC(file_size);
  if (!raw_bytes) {
    fclose(f);
    return MMDGLTF_ERROR_OUT_OF_MEMORY;
  }

  const size_t bytes_read = fread(raw_bytes, 1, file_size, f);
  fclose(f);
  if (bytes_read != file_size) {
    MMDGLTF_FREE(raw_bytes, file_size);
    return MMDGLTF_ERROR_IO;
  }

  const bool is_png = (file_size >= 8 && memcmp(raw_bytes, "\x89PNG\r\n\x1a\n", 8) == 0);
  const bool is_jpeg =
    (file_size >= 3 && raw_bytes[0] == 0xFF && raw_bytes[1] == 0xD8 && raw_bytes[2] == 0xFF);

  if (is_png || is_jpeg) {
    int w = 0;
    int h = 0;
    int comp = 0;
    if (!stbi_info_from_memory(raw_bytes, (int)file_size, &w, &h, &comp)) {
      MMDGLTF_FREE(raw_bytes, file_size);
      return MMDGLTF_ERROR_CORRUPT_DATA;
    }

    u8* arena_data = mmdgltf__arena_alloc(arena, file_size);
    if (!arena_data) {
      MMDGLTF_FREE(raw_bytes, file_size);
      return MMDGLTF_ERROR_OUT_OF_MEMORY;
    }
    memcpy(arena_data, raw_bytes, file_size);
    MMDGLTF_FREE(raw_bytes, file_size);

    out_image->data = arena_data;
    out_image->size = file_size;
    out_image->mime_type = is_png ? "image/png" : "image/jpeg";
    out_image->width = w;
    out_image->height = h;
    out_image->channels = comp;
    return MMDGLTF_OK;
  }

  int w = 0;
  int h = 0;
  int comp = 0;
  const int req_comp = 4;
  u8* pixels = stbi_load_from_memory(raw_bytes, (int)file_size, &w, &h, &comp, req_comp);
  MMDGLTF_FREE(raw_bytes, file_size);

  if (!pixels) {
    return MMDGLTF_ERROR_CORRUPT_DATA;
  }

  int png_len = 0;
  u8* png_data = stbi_write_png_to_mem(pixels, w * req_comp, w, h, req_comp, &png_len);
  stbi_image_free(pixels);

  if (!png_data || png_len <= 0) {
    return MMDGLTF_ERROR_CORRUPT_DATA;
  }

  u8* arena_data = mmdgltf__arena_alloc(arena, (size_t)png_len);
  if (!arena_data) {
    stb_free(png_data);
    return MMDGLTF_ERROR_OUT_OF_MEMORY;
  }
  memcpy(arena_data, png_data, (size_t)png_len);
  stb_free(png_data);

  out_image->data = arena_data;
  out_image->size = (size_t)png_len;
  out_image->mime_type = "image/png";
  out_image->width = w;
  out_image->height = h;
  out_image->channels = req_comp;

  return MMDGLTF_OK;
}
