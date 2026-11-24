/*
 * Copyright (C) 2026 RenoDX contributors
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <d3d9.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <utility>

#include <embed/shaders.h>
#include <include/reshade.hpp>

#include "../../../utils/resource.hpp"

namespace {
namespace bloom {

using Microsoft::WRL::ComPtr;

static float mode = 1.f;
static constexpr UINT MAX_LEVELS = 6;

struct MipChain {
  ComPtr<IDirect3DTexture9> texture;
  std::array<ComPtr<IDirect3DSurface9>, MAX_LEVELS> surfaces;
};

struct Resources {
  std::array<ComPtr<IDirect3DPixelShader9>, 4> shaders;
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

struct __declspec(uuid("79b73c94-396b-4bd9-bc39-e05de615d6f6")) DeviceData {
  Resources resources;
  bool filtering = false;
  bool ready = false;
  bool logged_active = false;
  bool logged_failure = false;
};

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
    data->filtering = false;
    data->ready = false;
    data->logged_active = false;
    data->logged_failure = false;
  }
}

static void OnPresent(reshade::api::command_queue* queue, reshade::api::swapchain* swapchain,
                      const reshade::api::rect*, const reshade::api::rect*, uint32_t, const reshade::api::rect*) {
  if (queue->get_device() != swapchain->get_device()) return;
  if (auto* data = queue->get_device()->get_private_data<DeviceData>(); data != nullptr) {
    data->filtering = false;
    data->ready = false;
  }
}

static bool OnGather(reshade::api::command_list* cmd_list) {
  if (auto* data = cmd_list->get_device()->get_private_data<DeviceData>(); data != nullptr) {
    // Replace bloom blurs until the tonemap draw. Borderlands 2 can use several
    // horizontal/vertical pairs; the same shaders also serve depth of field.
    data->filtering = true;
    data->ready = false;
  }
  // This only arms the following blur passes. A post-draw callback would replay
  // the game's DrawIndexedPrimitiveUP without its transient vertex/index data.
  return true;
}

static bool OnBlur(reshade::api::command_list* cmd_list, bool hdr_enabled) {
  auto* data = cmd_list->get_device()->get_private_data<DeviceData>();
  if (data == nullptr || !data->filtering) return true;
  if (mode == 0.f || !hdr_enabled) {
    data->filtering = false;
    return true;
  }
  auto* device = reinterpret_cast<IDirect3DDevice9*>(cmd_list->get_device()->get_native());
  auto& resources = data->resources;
  ComPtr<IDirect3DBaseTexture9> bound_texture;
  ComPtr<IDirect3DTexture9> source;
  ComPtr<IDirect3DSurface9> target;
  D3DSURFACE_DESC source_desc = {};
  D3DSURFACE_DESC target_desc = {};
  bool success = SUCCEEDED(device->GetTexture(0, &bound_texture)) && bound_texture != nullptr
                 && SUCCEEDED(bound_texture.As(&source))
                 && SUCCEEDED(device->GetRenderTarget(0, &target))
                 && SUCCEEDED(target->GetDesc(&target_desc));
  if (success) {
    renodx::utils::resource::GetResourceInfo({reinterpret_cast<uint64_t>(source.Get())}, [&](const auto& info) {
      if (info.clone_enabled && info.clone.handle != 0) {
        source = reinterpret_cast<IDirect3DTexture9*>(info.clone.handle);
      }
    });
    success = SUCCEEDED(source->GetLevelDesc(0, &source_desc))
              && source_desc.Format == D3DFMT_A16B16G16R16F
              && source_desc.Width == target_desc.Width && source_desc.Height == target_desc.Height;
  }

  if (success && (resources.width != source_desc.Width || resources.height != source_desc.Height)) {
    resources = {};
    data->ready = false;
    Resources next;
    next.width = source_desc.Width;
    next.height = source_desc.Height;
    next.levels = 1;
    for (UINT size = std::min(next.width, next.height); next.levels < MAX_LEVELS && size > 1; size >>= 1) {
      ++next.levels;
    }
    const std::array<std::span<const uint8_t>, 4> shaders = {
        __bloom_copy, __bloom_downsample, __bloom_gaussian, __bloom_combine};
    for (size_t index = 0; index < shaders.size() && success; ++index) {
      success = SUCCEEDED(device->CreatePixelShader(reinterpret_cast<const DWORD*>(shaders[index].data()), &next.shaders[index]));
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
    if (success && !data->ready) {
      UINT previous_width = resources.width;
      UINT previous_height = resources.height;
      for (UINT level = 0; level < resources.levels && success; ++level) {
        const UINT width = std::max(1u, resources.width >> level);
        const UINT height = std::max(1u, resources.height >> level);
        const float mip = static_cast<float>(level);
        const D3DVIEWPORT9 viewport = {0, 0, width, height, 0.f, 1.f};
        if (level == 0) {
          success = draw_pass(0, resources.downsample.surfaces[0].Get(), source.Get(), viewport, {}, {0, 0, 1, 1});
        } else {
          success = draw_pass(1, resources.downsample.surfaces[level].Get(), resources.vertical.texture.Get(), viewport,
                              {1.f / previous_width, 1.f / previous_height, mip - 1.f, 0.f}, {0, 0, 1, 1});
        }
        success = success
                  && draw_pass(2, resources.horizontal.surfaces[level].Get(), resources.downsample.texture.Get(), viewport,
                               {1.f / width, 0.f, mip, 0.f}, {0, 0, 1, 1})
                  && draw_pass(2, resources.vertical.surfaces[level].Get(), resources.horizontal.texture.Get(), viewport,
                               {0.f, 1.f / height, mip, 0.f}, {0, 0, 1, 1});
        previous_width = width;
        previous_height = height;
      }
      const D3DVIEWPORT9 viewport = {0, 0, resources.width, resources.height, 0.f, 1.f};
      success = success && draw_pass(3, resources.combined_surface.Get(), resources.vertical.texture.Get(), viewport, {0.f, 0.f, 0.f, static_cast<float>(resources.levels)}, {0, 0, 1, 1});
      data->ready = success;
    }
    if (success) {
      const float width = static_cast<float>(target_desc.Width);
      const float height = static_cast<float>(target_desc.Height);
      const std::array<float, 4> uv_rect = {
          saved.viewport.X / width, saved.viewport.Y / height,
          (saved.viewport.X + saved.viewport.Width) / width, (saved.viewport.Y + saved.viewport.Height) / height};
      // Update both the game target and its float clone. RenoDX's following copy
      // draws may read either one, depending on resource-cloning state.
      success = draw_pass(0, target.Get(), resources.combined.Get(), saved.viewport, {}, uv_rect);
      ComPtr<IDirect3DTexture9> target_texture;
      ComPtr<IDirect3DTexture9> clone;
      if (SUCCEEDED(target->GetContainer(IID_PPV_ARGS(&target_texture)))) {
        renodx::utils::resource::GetResourceInfo({reinterpret_cast<uint64_t>(target_texture.Get())}, [&](const auto& info) {
          if (info.clone_enabled && info.clone.handle != 0) {
            clone = reinterpret_cast<IDirect3DTexture9*>(info.clone.handle);
          }
        });
      }
      if (success && clone != nullptr) {
        ComPtr<IDirect3DSurface9> clone_surface;
        success = SUCCEEDED(clone->GetSurfaceLevel(0, &clone_surface))
                  && draw_pass(0, clone_surface.Get(), resources.combined.Get(), saved.viewport, {}, uv_rect);
      }
    }
  }
  if (!success) {
    data->filtering = false;
    data->ready = false;
    if (!data->logged_failure) {
      data->logged_failure = true;
      reshade::log::message(reshade::log::level::warning, "[Borderlands 2 Bloom] Mip Gaussian unavailable; using the game blur.");
    }
    return true;
  }
  if (!data->logged_active) {
    data->logged_active = true;
    reshade::log::message(reshade::log::level::info, "[Borderlands 2 Bloom] Separable Gaussian mip bloom active (RGBA16F, up to six levels).");
  }
  return false;
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
