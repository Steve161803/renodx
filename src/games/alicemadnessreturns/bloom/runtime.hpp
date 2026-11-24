/*
 * Copyright (C) 2026 RenoDX contributors
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <d3d9.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

#include <embed/shaders.h>
#include <include/reshade.hpp>

#include "../../../utils/resource.hpp"

namespace {
namespace bloom {

using Microsoft::WRL::ComPtr;

static float mode = 1.f;
static constexpr UINT MAX_LEVELS = 6;
static constexpr std::array<D3DSAMPLERSTATETYPE, 6> COMPOSITE_SAMPLER_STATES = {
    D3DSAMP_ADDRESSU, D3DSAMP_ADDRESSV, D3DSAMP_MINFILTER,
    D3DSAMP_MAGFILTER, D3DSAMP_MIPFILTER, D3DSAMP_SRGBTEXTURE};

struct MipChain {
  ComPtr<IDirect3DTexture9> texture;
  std::array<ComPtr<IDirect3DSurface9>, MAX_LEVELS> surfaces;
};

struct Resources {
  std::array<ComPtr<IDirect3DPixelShader9>, 4> shaders;
  ComPtr<IDirect3DPixelShader9> dof_gather;
  ComPtr<IDirect3DStateBlock9> state;
  // Distinct textures avoid sampling a resource while rendering to another mip of it.
  MipChain downsample;
  MipChain horizontal;
  MipChain vertical;
  ComPtr<IDirect3DTexture9> combined;
  ComPtr<IDirect3DSurface9> combined_surface;
  UINT width = 0;
  UINT height = 0;
  UINT levels = 0;
};

struct __declspec(uuid("b2b22420-1da4-4ea1-99f8-f283f05240b0")) DeviceData {
  Resources resources;
  std::vector<DWORD> gather_code;
  ComPtr<IDirect3DPixelShader9> vanilla_gather;
  std::array<float, 4> vanilla_bloom_scale = {};
  bool ready = false;
  bool logged_active = false;
  bool logged_failure = false;
  ComPtr<IDirect3DBaseTexture9> composite_texture;
  std::array<DWORD, COMPOSITE_SAMPLER_STATES.size()> composite_sampler = {};
  bool restore_composite = false;
};

static void RestoreComposite(IDirect3DDevice9* device, DeviceData* data) {
  if (!data->restore_composite) return;
  device->SetTexture(2, data->composite_texture.Get());
  for (size_t index = 0; index < COMPOSITE_SAMPLER_STATES.size(); ++index) {
    device->SetSamplerState(2, COMPOSITE_SAMPLER_STATES[index], data->composite_sampler[index]);
  }
  data->composite_texture.Reset();
  data->restore_composite = false;
}

struct BloomDrawState {
  IDirect3DDevice9* device;
  IDirect3DStateBlock9* state;
  std::array<ComPtr<IDirect3DSurface9>, 4> targets;
  ComPtr<IDirect3DSurface9> depth;
  D3DVIEWPORT9 viewport = {};
  bool valid = false;

  BloomDrawState(IDirect3DDevice9* device, IDirect3DStateBlock9* state) : device(device), state(state) {
    if (FAILED(state->Capture()) || FAILED(device->GetRenderTarget(0, &targets[0]))
        || FAILED(device->GetViewport(&viewport))) return;
    for (DWORD index = 1; index < targets.size(); ++index) {
      device->GetRenderTarget(index, &targets[index]);
    }
    device->GetDepthStencilSurface(&depth);
    valid = true;
    for (DWORD index = 1; index < targets.size(); ++index) {
      device->SetRenderTarget(index, nullptr);
    }
    device->SetDepthStencilSurface(nullptr);
    device->SetVertexShader(nullptr);
    device->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);
    device->SetRenderState(D3DRS_ZENABLE, FALSE);
    device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
    device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
    device->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
    device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
    device->SetRenderState(D3DRS_FOGENABLE, FALSE);
    device->SetRenderState(D3DRS_CLIPPLANEENABLE, 0);
    device->SetStreamSourceFreq(0, 1);
    for (DWORD slot = 0; slot < 16; ++slot) {
      device->SetTexture(slot, nullptr);
    }
    device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_POINT);
    device->SetSamplerState(0, D3DSAMP_MAXMIPLEVEL, 0);
    device->SetSamplerState(0, D3DSAMP_MIPMAPLODBIAS, 0);
    device->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, FALSE);
  }

  ~BloomDrawState() {
    if (!valid) return;
    device->SetTexture(0, nullptr);
    for (DWORD index = 0; index < targets.size(); ++index) {
      device->SetRenderTarget(index, targets[index].Get());
    }
    device->SetDepthStencilSurface(depth.Get());
    state->Apply();
  }
};

static void OnInitDevice(reshade::api::device* device) {
  if (device->get_api() == reshade::api::device_api::d3d9) {
    device->create_private_data<DeviceData>();
  }
}

static void OnDestroyDevice(reshade::api::device* device) {
  if (device->get_api() == reshade::api::device_api::d3d9) {
    device->destroy_private_data<DeviceData>();
  }
}

static void OnDestroySwapchain(reshade::api::swapchain* swapchain, bool) {
  if (auto* data = swapchain->get_device()->get_private_data<DeviceData>(); data != nullptr) {
    // Release every default-pool reference before a D3D9 reset.
    data->resources = {};
    data->composite_texture.Reset();
    data->restore_composite = false;
    data->ready = false;
    data->logged_active = false;
    data->logged_failure = false;
  }
}

static void OnPresent(reshade::api::command_queue* queue, reshade::api::swapchain* swapchain,
                      const reshade::api::rect*, const reshade::api::rect*, uint32_t, const reshade::api::rect*) {
  if (queue->get_device() != swapchain->get_device()) return;
  if (auto* data = queue->get_device()->get_private_data<DeviceData>(); data != nullptr) {
    RestoreComposite(reinterpret_cast<IDirect3DDevice9*>(queue->get_device()->get_native()), data);
    data->ready = false;
  }
}

static bool OnGather(reshade::api::command_list* cmd_list, bool hdr_enabled, float bloom_strength) {
  auto* data = cmd_list->get_device()->get_private_data<DeviceData>();
  if (data == nullptr) return true;
  data->ready = false;
  const bool gaussian_enabled = mode != 0.f && hdr_enabled;
  if (!gaussian_enabled && bloom_strength == 1.f) return true;
  auto* device = reinterpret_cast<IDirect3DDevice9*>(cmd_list->get_device()->get_native());
  if (data->gather_code.empty()) {
    ComPtr<IDirect3DPixelShader9> original;
    UINT size = 0;
    if (FAILED(device->GetPixelShader(&original)) || original == nullptr
        || FAILED(original->GetFunction(nullptr, &size)) || size < sizeof(DWORD)
        || size % sizeof(DWORD) != 0) return true;
    std::vector<DWORD> code(size / sizeof(DWORD));
    if (FAILED(original->GetFunction(code.data(), &size)) || code[0] != D3DPS_VERSION(3, 0)) return true;
    data->gather_code = std::move(code);
  }
  const auto create_gather = [&](const std::array<float, 4>& scale, IDirect3DPixelShader9** shader) {
    auto code = data->gather_code;
    // c0 is solely BloomScale in 0xC4EC30F6. A local definition changes
    // only bloom, preserving native DOF and the game's external constants.
    code.insert(code.begin() + 1, {D3DSIO_DEF | (5u << D3DSI_INSTLENGTH_SHIFT),
                                   0x80000000u | (D3DSPR_CONST << D3DSP_REGTYPE_SHIFT) | D3DSP_WRITEMASK_ALL,
                                   std::bit_cast<DWORD>(scale[0]), std::bit_cast<DWORD>(scale[1]),
                                   std::bit_cast<DWORD>(scale[2]), std::bit_cast<DWORD>(scale[3])});
    return device->CreatePixelShader(code.data(), shader);
  };
  const auto apply_vanilla_bloom = [&]() {
    if (bloom_strength == 1.f) return;
    std::array<float, 4> scale = {};
    if (FAILED(device->GetPixelShaderConstantF(0, scale.data(), 1))) return;
    scale[0] *= bloom_strength;
    if (data->vanilla_gather == nullptr || data->vanilla_bloom_scale != scale) {
      ComPtr<IDirect3DPixelShader9> next;
      if (FAILED(create_gather(scale, &next))) return;
      data->vanilla_gather = std::move(next);
      data->vanilla_bloom_scale = scale;
    }
    device->SetPixelShader(data->vanilla_gather.Get());
  };
  if (!gaussian_enabled) {
    apply_vanilla_bloom();
    return true;
  }
  auto& resources = data->resources;
  ComPtr<IDirect3DBaseTexture9> bound_texture;
  ComPtr<IDirect3DTexture9> source;
  ComPtr<IDirect3DSurface9> target;
  D3DSURFACE_DESC source_desc = {};
  D3DSURFACE_DESC target_desc = {};
  D3DVIEWPORT9 gather_viewport = {};
  std::array<float, 4> bloom_scale = {};
  std::array<float, 32> sample_offsets = {};
  bool success = SUCCEEDED(device->GetTexture(0, &bound_texture)) && bound_texture != nullptr
                 && SUCCEEDED(bound_texture.As(&source))
                 && SUCCEEDED(device->GetRenderTarget(0, &target))
                 && SUCCEEDED(target->GetDesc(&target_desc))
                 && SUCCEEDED(device->GetViewport(&gather_viewport))
                 && SUCCEEDED(device->GetPixelShaderConstantF(0, bloom_scale.data(), 1))
                 && SUCCEEDED(device->GetVertexShaderConstantF(0, sample_offsets.data(), 8));
  if (success) {
    renodx::utils::resource::GetResourceInfo({reinterpret_cast<uint64_t>(source.Get())}, [&](const auto& info) {
      if (info.clone_enabled && info.clone.handle != 0) {
        source = reinterpret_cast<IDirect3DTexture9*>(info.clone.handle);
      }
    });
    success = SUCCEEDED(source->GetLevelDesc(0, &source_desc))
              && source_desc.Format == D3DFMT_A16B16G16R16F;
  }

  if (success && (resources.width != target_desc.Width || resources.height != target_desc.Height)) {
    resources = {};
    data->ready = false;
    Resources next;
    next.width = target_desc.Width;
    next.height = target_desc.Height;
    next.levels = 1;
    for (UINT size = std::min(next.width, next.height); next.levels < MAX_LEVELS && size > 1; size >>= 1) {
      ++next.levels;
    }
    const std::array<std::span<const uint8_t>, 4> shaders = {
        __bloom_downsample, __bloom_gaussian, __bloom_combine, __bloom_prefilter};
    for (size_t index = 0; index < shaders.size() && success; ++index) {
      success = SUCCEEDED(device->CreatePixelShader(reinterpret_cast<const DWORD*>(shaders[index].data()), &next.shaders[index]));
    }
    if (success) {
      success = SUCCEEDED(create_gather({}, &next.dof_gather));
    }
    for (auto* chain : {&next.downsample, &next.horizontal, &next.vertical}) {
      if (!success) break;
      success = SUCCEEDED(device->CreateTexture(next.width, next.height, next.levels, D3DUSAGE_RENDERTARGET,
                                                D3DFMT_A16B16G16R16F, D3DPOOL_DEFAULT, &chain->texture, nullptr));
      for (UINT level = 0; level < next.levels && success; ++level) {
        success = SUCCEEDED(chain->texture->GetSurfaceLevel(level, &chain->surfaces[level]));
      }
    }
    if (success) {
      success = SUCCEEDED(device->CreateTexture(next.width, next.height, 1, D3DUSAGE_RENDERTARGET,
                                                D3DFMT_A16B16G16R16F, D3DPOOL_DEFAULT, &next.combined, nullptr))
                && SUCCEEDED(next.combined->GetSurfaceLevel(0, &next.combined_surface))
                && SUCCEEDED(device->CreateStateBlock(D3DSBT_ALL, &next.state));
    }
    if (success) {
      resources = std::move(next);
    }
  }

  if (success) {
    BloomDrawState saved(device, resources.state.Get());
    success = saved.valid;
    const auto draw_pass = [&](UINT shader, IDirect3DSurface9* output, IDirect3DTexture9* input,
                               const D3DVIEWPORT9& viewport, const std::array<float, 4>& metrics,
                               const std::array<float, 4>& uv_rect) {
      struct Vertex {
        float x, y, z, w, u, v;
      };
      const float left = static_cast<float>(viewport.X) - 0.5f;
      const float top = static_cast<float>(viewport.Y) - 0.5f;
      const float right = left + static_cast<float>(viewport.Width);
      const float bottom = top + static_cast<float>(viewport.Height);
      const std::array<Vertex, 4> vertices = {{
          {left, top, 0.f, 1.f, uv_rect[0], uv_rect[1]},
          {right, top, 0.f, 1.f, uv_rect[2], uv_rect[1]},
          {left, bottom, 0.f, 1.f, uv_rect[0], uv_rect[3]},
          {right, bottom, 0.f, 1.f, uv_rect[2], uv_rect[3]},
      }};
      return SUCCEEDED(device->SetTexture(0, nullptr))
             && SUCCEEDED(device->SetRenderTarget(0, output))
             && SUCCEEDED(device->SetViewport(&viewport))
             && SUCCEEDED(device->SetPixelShader(resources.shaders[shader].Get()))
             && SUCCEEDED(device->SetPixelShaderConstantF(0, metrics.data(), 1))
             && SUCCEEDED(device->SetTexture(0, input))
             && SUCCEEDED(device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices.data(), sizeof(Vertex)));
    };
    if (success) {
      const D3DVIEWPORT9 full_viewport = {0, 0, resources.width, resources.height, 0.f, 1.f};
      success = SUCCEEDED(device->SetRenderTarget(0, resources.downsample.surfaces[0].Get()))
                && SUCCEEDED(device->SetViewport(&full_viewport))
                && SUCCEEDED(device->Clear(0, nullptr, D3DCLEAR_TARGET, 0, 1.f, 0))
                && SUCCEEDED(device->SetPixelShaderConstantF(1, bloom_scale.data(), 1))
                && SUCCEEDED(device->SetPixelShaderConstantF(2, sample_offsets.data(), 8))
                && draw_pass(3, resources.downsample.surfaces[0].Get(), source.Get(), gather_viewport, {}, {0, 0, 1, 1});
      UINT previous_width = resources.width;
      UINT previous_height = resources.height;
      for (UINT level = 0; level < resources.levels && success; ++level) {
        const UINT width = std::max(1u, resources.width >> level);
        const UINT height = std::max(1u, resources.height >> level);
        const float mip = static_cast<float>(level);
        const D3DVIEWPORT9 viewport = {0, 0, width, height, 0.f, 1.f};
        if (level != 0) {
          success = draw_pass(0, resources.downsample.surfaces[level].Get(), resources.vertical.texture.Get(), viewport,
                              {1.f / previous_width, 1.f / previous_height, mip - 1.f, 0.f}, {0, 0, 1, 1});
        }
        success = success
                  && draw_pass(1, resources.horizontal.surfaces[level].Get(), resources.downsample.texture.Get(), viewport,
                               {1.f / width, 0.f, mip, 0.f}, {0, 0, 1, 1})
                  && draw_pass(1, resources.vertical.surfaces[level].Get(), resources.horizontal.texture.Get(), viewport,
                               {0.f, 1.f / height, mip, 0.f}, {0, 0, 1, 1});
        previous_width = width;
        previous_height = height;
      }
      const D3DVIEWPORT9 viewport = {0, 0, resources.width, resources.height, 0.f, 1.f};
      success = success && draw_pass(2, resources.combined_surface.Get(), resources.vertical.texture.Get(), viewport, {0.f, 0.f, 0.f, static_cast<float>(resources.levels)}, {0, 0, 1, 1});
    }
  }
  // Bind after restoring the captured game state. Let the original draw execute;
  // a post-draw callback would replay DrawIndexedPrimitiveUP without its data.
  success = success && SUCCEEDED(device->SetPixelShader(resources.dof_gather.Get()));
  data->ready = success;
  if (!success) {
    data->ready = false;
    apply_vanilla_bloom();
    if (!data->logged_failure) {
      data->logged_failure = true;
      reshade::log::message(reshade::log::level::warning, "[Alice Bloom] Mip Gaussian unavailable; using the game bloom/DOF path.");
    }
    return true;
  }
  if (!data->logged_active) {
    data->logged_active = true;
    reshade::log::message(reshade::log::level::info, "[Alice Bloom] Separable Gaussian mip bloom active (RGBA16F, up to six levels; native DOF preserved).");
  }
  return true;
}

static void Use(DWORD reason) {
  switch (reason) {
    case DLL_PROCESS_ATTACH:
      reshade::register_event<reshade::addon_event::init_device>(OnInitDevice);
      reshade::register_event<reshade::addon_event::destroy_device>(OnDestroyDevice);
      reshade::register_event<reshade::addon_event::destroy_swapchain>(OnDestroySwapchain);
      reshade::register_event<reshade::addon_event::present>(OnPresent);
      break;
    case DLL_PROCESS_DETACH:
      reshade::unregister_event<reshade::addon_event::present>(OnPresent);
      reshade::unregister_event<reshade::addon_event::destroy_swapchain>(OnDestroySwapchain);
      reshade::unregister_event<reshade::addon_event::destroy_device>(OnDestroyDevice);
      reshade::unregister_event<reshade::addon_event::init_device>(OnInitDevice);
      break;
  }
}

}  // namespace bloom
}  // namespace
