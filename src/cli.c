#include "mmdgltf/mmdgltf.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CLI_VERSION "0.1.0"

static double get_time_sec(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static void print_usage(const char* prog) {
  printf("Usage: %s <input.pmx> [output] [options]\n\n", prog);
  printf("Fast MMD (PMX) to glTF 2.0 / GLB converter.\n\n");
  printf("Arguments:\n");
  printf("  <input.pmx>         Path to source PMX model file\n");
  printf("  [output]            Optional destination path (.glb or .gltf)\n\n");
  printf("Options:\n");
  printf("  --gltf              Emit separate text .gltf and .bin files (default: .glb)\n");
  printf("  --unlit             Enable KHR_materials_unlit extension\n");
  printf("  --scale <float>     Scale factor for model geometry (default: 1.0)\n");
  printf("  --bone-names <jp|en> Select bone naming language (default: jp)\n");
  printf("  --english-bone-names Use English bone names (fallback to JP)\n");
  printf("  --verbose           Display detailed model statistics and timing\n");
  printf("  -h, --help          Show this help message and exit\n");
  printf("  -v, --version       Show program version\n");
}

static char* derive_output_path(const char* input_path, bool is_gltf) {
  const size_t len = strlen(input_path);
  size_t dot_idx = len;
  for (size_t i = len; i > 0; --i) {
    if (input_path[i - 1] == '.') {
      dot_idx = i - 1;
      break;
    }
    if (input_path[i - 1] == '/' || input_path[i - 1] == '\\') {
      break;
    }
  }

  const char* ext = is_gltf ? ".gltf" : ".glb";
  const size_t ext_len = strlen(ext);
  char* out = malloc(dot_idx + ext_len + 1);
  if (!out) {
    return NULL;
  }
  memcpy(out, input_path, dot_idx);
  memcpy(out + dot_idx, ext, ext_len + 1);
  return out;
}

static long get_file_size(const char* path) {
  FILE* f = fopen(path, "rb");
  if (!f) {
    return -1;
  }
  fseek(f, 0, SEEK_END);
  long sz = ftell(f);
  fclose(f);
  return sz;
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    print_usage(argv[0]);
    return 1;
  }

  const char* input_file = NULL;
  const char* output_file = NULL;
  bool is_gltf = false;
  bool verbose = false;
  mmdgltf_ExportConfig config;
  mmdgltf_export_config_default(&config);

  for (int i = 1; i < argc; ++i) {
    const char* arg = argv[i];
    if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
      print_usage(argv[0]);
      return 0;
    }
    if (strcmp(arg, "-v") == 0 || strcmp(arg, "--version") == 0) {
      printf("mmdgltf-cli %s\n", CLI_VERSION);
      return 0;
    }
    if (strcmp(arg, "--gltf") == 0) {
      is_gltf = true;
    } else if (strcmp(arg, "--unlit") == 0) {
      config.unlit = 1;
    } else if (strcmp(arg, "--verbose") == 0) {
      verbose = true;
    } else if (strcmp(arg, "--english-bone-names") == 0 || strcmp(arg, "--en-bones") == 0) {
      config.english_bone_names = 1;
    } else if (strcmp(arg, "--bone-names") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "Error: --bone-names requires 'jp' or 'en'.\n");
        return 1;
      }
      const char* lang = argv[++i];
      if (strcmp(lang, "en") == 0) {
        config.english_bone_names = 1;
      } else if (strcmp(lang, "jp") == 0) {
        config.english_bone_names = 0;
      } else {
        fprintf(stderr, "Error: invalid bone name language '%s'. Must be 'jp' or 'en'.\n", lang);
        return 1;
      }
    } else if (strcmp(arg, "--scale") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "Error: --scale requires a floating point value.\n");
        return 1;
      }
      char* end = NULL;
      float s = strtof(argv[++i], &end);
      if (end == argv[i] || s <= 0.0f) {
        fprintf(stderr, "Error: invalid scale factor '%s'. Must be positive.\n", argv[i]);
        return 1;
      }
      config.scale = s;
    } else if (arg[0] == '-') {
      fprintf(stderr, "Error: unknown option '%s'. Use --help for usage.\n", arg);
      return 1;
    } else {
      if (!input_file) {
        input_file = arg;
      } else if (!output_file) {
        output_file = arg;
      } else {
        fprintf(stderr, "Error: unexpected positional argument '%s'.\n", arg);
        return 1;
      }
    }
  }

  if (!input_file) {
    fprintf(stderr, "Error: missing input PMX file.\n");
    return 1;
  }

  char* allocated_output = NULL;
  if (!output_file) {
    allocated_output = derive_output_path(input_file, is_gltf);
    if (!allocated_output) {
      fprintf(stderr, "Error: failed to allocate output path.\n");
      return 1;
    }
    output_file = allocated_output;
  } else {
    const size_t out_len = strlen(output_file);
    if (out_len >= 5 && strcmp(output_file + out_len - 5, ".gltf") == 0) {
      is_gltf = true;
    }
  }

  if (verbose) {
    printf("Converting '%s' -> '%s' (format: %s, scale: %.2f, unlit: %s, bone names: %s)...\n",
           input_file, output_file, is_gltf ? "glTF (separate .bin)" : "GLB (binary)",
           config.scale, config.unlit ? "yes" : "no", config.english_bone_names ? "en" : "jp");
  }

  const double t0 = get_time_sec();
  mmdgltf_Status st;
  if (is_gltf) {
    st = mmdgltf_convert_pmx_to_gltf_file(input_file, output_file, &config);
  } else {
    st = mmdgltf_convert_pmx_file(input_file, output_file, &config);
  }
  const double t1 = get_time_sec();

  if (st != MMDGLTF_OK) {
    fprintf(stderr, "Conversion failed with error code %d\n", (int)st);
    free(allocated_output);
    return 1;
  }

  const long out_sz = get_file_size(output_file);
  if (verbose) {
    printf("Conversion successful in %.4f seconds.\n", t1 - t0);
    if (out_sz >= 0) {
      printf("Output size: %ld bytes (%.2f MB)\n", out_sz, (double)out_sz / (1024.0 * 1024.0));
    }
  } else {
    printf("Saved '%s' (%.2f MB in %.3fs)\n", output_file,
           out_sz >= 0 ? ((double)out_sz / (1024.0 * 1024.0)) : 0.0, t1 - t0);
  }

  free(allocated_output);
  return 0;
}
