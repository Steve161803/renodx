#include "./common.hlsli"

Texture2D t0 : register(t0);
SamplerState s0 : register(s0);
float4 main(float4 vpos: SV_POSITION, float2 uv: TEXCOORD0)
    : SV_TARGET {
  float4 color = t0.Sample(s0, uv);

  color.a = saturate(color.a);
  color.rgb = renodx::color::gamma::DecodeSafe(color.rgb);
  color.rgb = DisplayMap(color.rgb, uv);
  color.rgb *= RENODX_DIFFUSE_WHITE_NITS / RENODX_GRAPHICS_WHITE_NITS;
  color.rgb = renodx::color::gamma::EncodeSafe(color.rgb);

  float a = color.a;
  float3 rgb = renodx::draw::SwapChainPass(color.rgb);
  return float4(rgb, a);
}
