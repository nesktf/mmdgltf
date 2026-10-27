#include "mmdgltf_internal.h"

#include <string.h>

void mmdgltf__stream_read_bytes(mmdgltf_Stream* s, void* dst, size_t count) {
  if (s->status != MMDGLTF_OK || count == 0) {
    return;
  }
  const size_t read_bytes = s->func.read(dst, 1, count, s->src);
  if (read_bytes != count) {
    s->status = MMDGLTF_ERROR_BUFFER_TOO_SMALL;
  }
}

u8 mmdgltf__stream_read_u8(mmdgltf_Stream* s) {
  u8 v = 0;
  mmdgltf__stream_read_bytes(s, &v, 1);
  return v;
}

i8 mmdgltf__stream_read_i8(mmdgltf_Stream* s) {
  return (i8)mmdgltf__stream_read_u8(s);
}

u16 mmdgltf__stream_read_u16(mmdgltf_Stream* s) {
  u8 b[2];
  mmdgltf__stream_read_bytes(s, b, 2);
  if (s->status != MMDGLTF_OK) {
    return 0;
  }
  return (u16)b[0] | ((u16)b[1] << 8);
}

i16 mmdgltf__stream_read_i16(mmdgltf_Stream* s) {
  return (i16)mmdgltf__stream_read_u16(s);
}

u32 mmdgltf__stream_read_u32(mmdgltf_Stream* s) {
  u8 b[4];
  mmdgltf__stream_read_bytes(s, b, 4);
  if (s->status != MMDGLTF_OK) {
    return 0;
  }
  return (u32)b[0] | ((u32)b[1] << 8) | ((u32)b[2] << 16) | ((u32)b[3] << 24);
}

i32 mmdgltf__stream_read_i32(mmdgltf_Stream* s) {
  return (i32)mmdgltf__stream_read_u32(s);
}

f32 mmdgltf__stream_read_f32(mmdgltf_Stream* s) {
  const u32 u = mmdgltf__stream_read_u32(s);
  f32 f = 0.0f;
  memcpy(&f, &u, sizeof(f));
  return f;
}

mmdgltf_Vec2f mmdgltf__stream_read_vec2(mmdgltf_Stream* s) {
  const f32 x = mmdgltf__stream_read_f32(s);
  const f32 y = mmdgltf__stream_read_f32(s);
  return (mmdgltf_Vec2f){.x = x, .y = y};
}

mmdgltf_Vec3f mmdgltf__stream_read_vec3(mmdgltf_Stream* s) {
  const f32 x = mmdgltf__stream_read_f32(s);
  const f32 y = mmdgltf__stream_read_f32(s);
  const f32 z = mmdgltf__stream_read_f32(s);
  return (mmdgltf_Vec3f){.x = x, .y = y, .z = z};
}

mmdgltf_Vec4f mmdgltf__stream_read_vec4(mmdgltf_Stream* s) {
  const f32 x = mmdgltf__stream_read_f32(s);
  const f32 y = mmdgltf__stream_read_f32(s);
  const f32 z = mmdgltf__stream_read_f32(s);
  const f32 w = mmdgltf__stream_read_f32(s);
  return (mmdgltf_Vec4f){.x = x, .y = y, .z = z, .w = w};
}

mmdgltf_Color3f mmdgltf__stream_read_color3(mmdgltf_Stream* s) {
  const f32 r = mmdgltf__stream_read_f32(s);
  const f32 g = mmdgltf__stream_read_f32(s);
  const f32 b = mmdgltf__stream_read_f32(s);
  return (mmdgltf_Color3f){.r = r, .g = g, .b = b};
}

mmdgltf_Color4f mmdgltf__stream_read_color4(mmdgltf_Stream* s) {
  const f32 r = mmdgltf__stream_read_f32(s);
  const f32 g = mmdgltf__stream_read_f32(s);
  const f32 b = mmdgltf__stream_read_f32(s);
  const f32 a = mmdgltf__stream_read_f32(s);
  return (mmdgltf_Color4f){.r = r, .g = g, .b = b, .a = a};
}

mmdgltf_Quatf mmdgltf__stream_read_quat(mmdgltf_Stream* s) {
  const f32 x = mmdgltf__stream_read_f32(s);
  const f32 y = mmdgltf__stream_read_f32(s);
  const f32 z = mmdgltf__stream_read_f32(s);
  const f32 w = mmdgltf__stream_read_f32(s);
  return (mmdgltf_Quatf){.x = x, .y = y, .z = z, .w = w};
}

u32 mmdgltf__stream_read_vertex_index(mmdgltf_Stream* s, u8 size) {
  if (size == 1) {
    return (u32)mmdgltf__stream_read_u8(s);
  } else if (size == 2) {
    return (u32)mmdgltf__stream_read_u16(s);
  } else if (size == 4) {
    return mmdgltf__stream_read_u32(s);
  }
  s->status = MMDGLTF_ERROR_CORRUPT_DATA;
  return 0;
}

i32 mmdgltf__stream_read_index(mmdgltf_Stream* s, u8 size) {
  if (size == 1) {
    return (i32)mmdgltf__stream_read_i8(s);
  } else if (size == 2) {
    return (i32)mmdgltf__stream_read_i16(s);
  } else if (size == 4) {
    return mmdgltf__stream_read_i32(s);
  }
  s->status = MMDGLTF_ERROR_CORRUPT_DATA;
  return -1;
}

static char* decode_utf16le(mmdgltf_Arena* arena, const u8* src, size_t src_bytes) {
  const size_t num_units = src_bytes / 2;
  const size_t max_utf8_len = num_units * 3 + 1;
  char* dst = mmdgltf__arena_alloc(arena, max_utf8_len);
  if (!dst) {
    return NULL;
  }

  size_t out_idx = 0;
  for (size_t i = 0; i < num_units; ++i) {
    u32 cp = (u32)src[i * 2] | ((u32)src[i * 2 + 1] << 8);
    if (cp >= 0xD800 && cp <= 0xDBFF) {
      if (i + 1 < num_units) {
        const u32 low = (u32)src[(i + 1) * 2] | ((u32)src[(i + 1) * 2 + 1] << 8);
        if (low >= 0xDC00 && low <= 0xDFFF) {
          cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
          ++i;
        } else {
          cp = 0xFFFD;
        }
      } else {
        cp = 0xFFFD;
      }
    } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
      cp = 0xFFFD;
    }

    if (cp < 0x80) {
      dst[out_idx++] = (char)cp;
    } else if (cp < 0x800) {
      dst[out_idx++] = (char)(0xC0 | (cp >> 6));
      dst[out_idx++] = (char)(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
      dst[out_idx++] = (char)(0xE0 | (cp >> 12));
      dst[out_idx++] = (char)(0x80 | ((cp >> 6) & 0x3F));
      dst[out_idx++] = (char)(0x80 | (cp & 0x3F));
    } else {
      dst[out_idx++] = (char)(0xF0 | (cp >> 18));
      dst[out_idx++] = (char)(0x80 | ((cp >> 12) & 0x3F));
      dst[out_idx++] = (char)(0x80 | ((cp >> 6) & 0x3F));
      dst[out_idx++] = (char)(0x80 | (cp & 0x3F));
    }
  }
  dst[out_idx] = '\0';
  return dst;
}

char* mmdgltf__stream_read_string(mmdgltf_Stream* s, mmdgltf_Arena* arena,
                                  mmdgltf_TextEncoding encoding) {
  const i32 len = mmdgltf__stream_read_i32(s);
  if (s->status != MMDGLTF_OK) {
    return NULL;
  }
  if (len < 0) {
    s->status = MMDGLTF_ERROR_CORRUPT_DATA;
    return NULL;
  }
  if (len == 0) {
    return mmdgltf__arena_strndup(arena, "", 0);
  }

  u8* str_bytes = mmdgltf__arena_alloc(arena, (size_t)len);
  if (!str_bytes) {
    s->status = MMDGLTF_ERROR_OUT_OF_MEMORY;
    return NULL;
  }
  mmdgltf__stream_read_bytes(s, str_bytes, (size_t)len);
  if (s->status != MMDGLTF_OK) {
    return NULL;
  }

  if (encoding == MMDGLTF_ENCODING_UTF8) {
    return mmdgltf__arena_strndup(arena, (const char*)str_bytes, (size_t)len);
  }
  return decode_utf16le(arena, str_bytes, (size_t)len);
}

size_t mmdgltf__mem_read(void* buff, size_t sz, size_t nmemb, mmdgltf__MemStream* m) {
  if (!m || !buff || sz == 0 || nmemb == 0) {
    return 0;
  }
  const size_t total = sz * nmemb;
  const size_t available = (m->pos < m->size) ? (m->size - m->pos) : 0;
  const size_t to_copy = total < available ? total : available;
  if (to_copy > 0) {
    memcpy(buff, m->data + m->pos, to_copy);
    m->pos += to_copy;
  }
  return to_copy / sz;
}

int mmdgltf__mem_seek(mmdgltf__MemStream* m, long offset, int origin) {
  if (!m) {
    return -1;
  }
  long new_pos = 0;
  if (origin == MMDGLTF_SEEK_SET) {
    new_pos = offset;
  } else if (origin == MMDGLTF_SEEK_CUR) {
    new_pos = (long)m->pos + offset;
  } else if (origin == MMDGLTF_SEEK_END) {
    new_pos = (long)m->size + offset;
  } else {
    return -1;
  }
  if (new_pos < 0 || (size_t)new_pos > m->size) {
    return -1;
  }
  m->pos = (size_t)new_pos;
  return 0;
}

long mmdgltf__mem_tell(mmdgltf__MemStream* m) {
  return m ? (long)m->pos : -1;
}

int mmdgltf__mem_eof(mmdgltf__MemStream* m) {
  return (!m || m->pos >= m->size) ? 1 : 0;
}

const mmdgltf_StreamFunc mmdgltf__mem_io = {
  .read = (PFN_mmdgltf_read_func)mmdgltf__mem_read,
  .seek = (PFN_mmdgltf_seek_func)mmdgltf__mem_seek,
  .tell = (PFN_mmdgltf_tell_func)mmdgltf__mem_tell,
  .eof = (PFN_mmdgltf_eof_func)mmdgltf__mem_eof,
};
