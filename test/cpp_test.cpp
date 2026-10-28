#include "mmdgltf/mmdgltf.hpp"
#include "paths.h"

#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

static const char* model_path = MMDGLTF_TEST_NITORI_PMX;

static void test_expected_special_members() {
  mmdgltf::Expected<int> e1;
  assert(e1.has_value());
  assert(*e1 == 0);

  int val = 42;
  mmdgltf::Expected<int> e2(val);
  assert(e2.has_value());
  assert(*e2 == 42);

  mmdgltf::Expected<std::string> e3(std::string("funny_string"));
  assert(e3.has_value());
  assert(*e3 == "funny_string");

  mmdgltf::Expected<std::string> e4(e3);
  assert(e4.has_value());
  assert(*e4 == "funny_string");

  mmdgltf::Expected<std::string> e5(std::move(e4));
  assert(e5.has_value());
  assert(*e5 == "funny_string");

  mmdgltf::Expected<std::string> e6;
  e6 = e5;
  assert(e6.has_value());
  assert(*e6 == "funny_string");

  mmdgltf::Expected<std::string> e7;
  e7 = std::move(e6);
  assert(e7.has_value());
  assert(*e7 == "funny_string");

  e7 = std::string("another_funny_string");
  assert(*e7 == "another_funny_string");

  mmdgltf::Expected<std::string> err_e(mmdgltf::unexpect, MMDGLTF_ERROR_INVALID_ARGUMENT);
  assert(err_e.has_error());
  assert(err_e.code() == MMDGLTF_ERROR_INVALID_ARGUMENT);

  e7 = mmdgltf::Error(MMDGLTF_ERROR_IO);
  assert(e7.has_error());
  assert(e7.code() == MMDGLTF_ERROR_IO);

  e7 = std::string("even_funnier_string");
  assert(e7.has_value());
  assert(*e7 == "even_funnier_string");
}

static void test_expected_monadic_operations() {
  mmdgltf::Expected<int> e(10);
  auto e_trans = e.transform([](int x) { return x * 2; });
  assert(e_trans.has_value());
  assert(*e_trans == 20);

  auto e_then = e.and_then([](int x) -> mmdgltf::Expected<float> {
    return {mmdgltf::in_place, static_cast<float>(x) + 0.5f};
  });
  assert(e_then.has_value());
  assert(*e_then == 10.5f);

  mmdgltf::Expected<int> err(mmdgltf::unexpect, MMDGLTF_ERROR_INVALID_ARGUMENT);
  auto err_then = err.and_then(
    [](int x) -> mmdgltf::Expected<float> { return {mmdgltf::in_place, static_cast<float>(x)}; });
  assert(err_then.has_error());
  assert(err_then.code() == MMDGLTF_ERROR_INVALID_ARGUMENT);

  auto err_trans = err.transform([](int x) { return x * 2; });
  assert(err_trans.has_error());
  assert(err_trans.code() == MMDGLTF_ERROR_INVALID_ARGUMENT);

  auto recovered = err.or_else([](mmdgltf::Status s) -> mmdgltf::Expected<int> {
    assert(s == MMDGLTF_ERROR_INVALID_ARGUMENT);
    return {mmdgltf::in_place, 999};
  });
  assert(recovered.has_value());
  assert(*recovered == 999);

  auto recovered2 = err.or_else(
    [](const mmdgltf::Error&) -> mmdgltf::Expected<int> { return {mmdgltf::in_place, 888}; });
  assert(recovered2.has_value());
  assert(*recovered2 == 888);

  auto not_called = e.or_else([](auto) -> mmdgltf::Expected<int> {
    assert(false && "Should not be called");
    return {mmdgltf::in_place, 0};
  });
  assert(not_called.has_value());
  assert(*not_called == 10);

  bool logged = false;
  auto logged_res = err.or_else([&](auto) { logged = true; });
  (void)logged_res;
  assert(logged);
}

static void test_pmx_model_loading_and_getters() {
  auto exp_model = mmdgltf::PmxModel::load_file(model_path);
  assert(exp_model.has_value());
  assert(!exp_model->empty());
  assert(exp_model->raw() != nullptr);

  auto info = exp_model->model_info();
  assert(info.has_value());
  assert(std::string(info->name_jp) == "河城にとり");

  auto header = exp_model->header();
  assert(header.has_value());
  assert(header->version == 2.0f);

  auto vertices = exp_model->vertices();
  assert(vertices.has_value());
  assert(vertices->size() == 29685);
  assert(!vertices->empty());
  assert(vertices->data() != nullptr);
  assert((*vertices)[0].weight_type == MMDGLTF_WEIGHT_BDEF1);

  size_t count = 0;
  for (const auto& v : *vertices) {
    (void)v;
    count++;
  }
  assert(count == 29685);

  auto indices = exp_model->indices();
  assert(indices.has_value());
  assert(indices->size() == 131748);

  auto textures = exp_model->textures();
  assert(textures.has_value());
  assert(textures->size() == 27);
  assert(std::string((*textures)[0].path) == "Texture/tume.png");

  auto materials = exp_model->materials();
  assert(materials.has_value());
  assert(materials->size() == 47);
  assert(std::string((*materials)[0].name_jp) == "ツメ");
  assert((*materials)[0].diffuse.a == 1.0f);

  auto bones = exp_model->bones();
  assert(bones.has_value());
  assert(bones->size() == 301);
  assert(std::string((*bones)[0].name_jp) == "全ての親");

  auto morphs = exp_model->morphs();
  assert(morphs.has_value());
  assert(morphs->size() == 168);
  assert(std::string((*morphs)[0].name_jp) == "スフィア暗");

  auto display_items = exp_model->display_items();
  assert(display_items.has_value());
  assert(display_items->size() == 16);
  assert(std::string((*display_items)[0].name_jp) == "Root");

  auto rigid_bodies = exp_model->rigid_bodies();
  assert(rigid_bodies.has_value());
  assert(rigid_bodies->size() == 81);
  assert(std::string((*rigid_bodies)[0].name_jp) == "左胸親");

  auto joints = exp_model->joints();
  assert(joints.has_value());
  assert(joints->size() == 39);
  assert(std::string((*joints)[0].name_jp) == "左胸上");

  mmdgltf::PmxModel moved_model(std::move(*exp_model));
  assert(!moved_model.empty());
  assert(exp_model->empty());

  mmdgltf::PmxModel assigned_model;
  assert(assigned_model.empty());
  assigned_model = std::move(moved_model);
  assert(!assigned_model.empty());
  assert(moved_model.empty());

  mmdgltf::PmxModel throw_model(model_path);
  assert(!throw_model.empty());
  bool threw = false;
  try {
    mmdgltf::PmxModel bad_model("models/nonexistent.pmx");
  } catch (const mmdgltf::Error& err) {
    threw = true;
    assert(err.code() == MMDGLTF_ERROR_IO);
    assert(std::string(err.what()) == "I/O error");
  }
  assert(threw);

  std::string path_str(model_path);
  auto exp_filen = mmdgltf::PmxModel::load_filen(path_str.data(), path_str.size());
  assert(exp_filen.has_value());

  FILE* f = fopen(model_path, "rb");
  assert(f != nullptr);
  fseek(f, 0, SEEK_END);
  long sz = ftell(f);
  fseek(f, 0, SEEK_SET);
  std::vector<char> buf(sz);
  assert(fread(buf.data(), 1, sz, f) == (size_t)sz);
  fclose(f);

  auto exp_mem = mmdgltf::PmxModel::load_memory(buf.data(), buf.size());
  assert(exp_mem.has_value());

  f = fopen(model_path, "rb");
  mmdgltf_StreamFunc sfunc = {
    [](void* b, size_t s, size_t n, void* src) { return fread(b, s, n, (FILE*)src); },
    [](void* src, long off, int org) { return fseek((FILE*)src, off, org); },
    [](void* src) { return ftell((FILE*)src); },
    [](void* src) { return feof((FILE*)src); },
  };
  auto exp_stream = mmdgltf::PmxModel::load_stream(&sfunc, f);
  fclose(f);
  assert(exp_stream.has_value());
}

static void test_gpu_model_runtime() {
  auto exp_pmx = mmdgltf::PmxModel::load_file(model_path);
  assert(exp_pmx.has_value());

  mmdgltf_ModelConfig cfg;
  mmdgltf_model_config_default(&cfg);
  cfg.scale = 1.0f;
  cfg.coordinate_system = MMDGLTF_COORDS_GLTF_RIGHT_HANDED;

  auto exp_model = mmdgltf::model_create(*exp_pmx, &cfg);
  assert(exp_model.has_value());
  assert(!exp_model->empty());

  auto exp_model2 = mmdgltf::model_create(exp_pmx->raw(), &cfg);
  assert(exp_model2.has_value());
  assert(!exp_model2->empty());

  // verify model data independence
  exp_pmx->reset();

  auto vbufs = exp_model->vertex_buffers();
  assert(vbufs.has_value());
  assert(vbufs->vertex_count == 29685);
  assert(vbufs->positions != nullptr);
  assert(vbufs->normals != nullptr);
  assert(vbufs->uvs != nullptr);
  assert(vbufs->joints != nullptr);
  assert(vbufs->weights != nullptr);

  auto indices = exp_model->indices();
  assert(indices.has_value());
  assert(indices->size() == 131748);

  auto materials = exp_model->materials();
  assert(materials.has_value());
  assert(materials->size() == 47);

  auto bones = exp_model->bones();
  assert(bones.has_value());
  assert(bones->size() == 301);

  auto morphs = exp_model->morphs();
  assert(morphs.has_value());
  assert(morphs->size() == 168);

  auto rbs = exp_model->rigid_bodies();
  assert(rbs.has_value());
  assert(rbs->size() == 81);

  auto joints = exp_model->joints();
  assert(joints.has_value());
  assert(joints->size() == 39);

  auto images = exp_model->images();
  assert(images.has_value());
  assert(images->size() == 27);
  assert((*images)[0].data != nullptr);
  assert((*images)[0].size > 0);
  assert(std::string((*images)[0].mime_type) == "image/png");

  mmdgltf::Model moved_model(std::move(*exp_model));
  assert(!moved_model.empty());
  assert(exp_model->empty());

  auto exp_glb = moved_model.export_glb_memory();
  assert(exp_glb.has_value());
  assert(exp_glb->size() > 20);
  assert((*exp_glb)[0] == 'g' && (*exp_glb)[1] == 'l' && (*exp_glb)[2] == 'T' &&
         (*exp_glb)[3] == 'F');
}

static void test_fastpath_cpp() {
  const char* out_file = "/tmp/test_cpp_fastpath.glb";
  auto status = mmdgltf::convert_pmx_file(model_path, out_file);
  assert(status == MMDGLTF_OK);

  FILE* f = fopen(model_path, "rb");
  assert(f != nullptr);
  fseek(f, 0, SEEK_END);
  long fsz = ftell(f);
  fseek(f, 0, SEEK_SET);
  std::vector<uint8_t> pmx_bytes(fsz);
  size_t rd = fread(pmx_bytes.data(), 1, fsz, f);
  assert(rd == static_cast<size_t>(fsz));
  fclose(f);

  auto glb_res = mmdgltf::convert_pmx_memory(pmx_bytes.data(), pmx_bytes.size());
  assert(glb_res.has_value());
  assert(glb_res->size() > 20);
  assert((*glb_res)[0] == 'g' && (*glb_res)[1] == 'l' && (*glb_res)[2] == 'T' &&
         (*glb_res)[3] == 'F');
}

int main() {
  test_expected_special_members();
  test_expected_monadic_operations();
  test_pmx_model_loading_and_getters();
  test_gpu_model_runtime();
  test_fastpath_cpp();
  printf("All mmdgltf C++ tests passed!\n");
  return 0;
}
