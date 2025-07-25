#include "./shared.h"

// ---- Created with 3Dmigoto v1.3.16 on Tue Jul 15 15:59:17 2025

cbuffer _Globals : register(b0) {
  float4 PackedParameters : packoffset(c0);
  float4 InputTextureSize : packoffset(c1);
  float4 MinMaxBlurClamp : packoffset(c2);
  float4 DOFKernelParams : packoffset(c3);
  float4 RenderTargetClampParameter : packoffset(c4);
  float4 MotionBlurMaskScaleAndBias : packoffset(c5);
  float4x4 ScreenToWorld : packoffset(c6);
  float4x4 PrevViewProjMatrix : packoffset(c10);
  float4 StaticVelocityParameters : packoffset(c14) = { 0.5, -0.5, 0.0125000002, 0.0222222228 };
  float4 DynamicVelocityParameters : packoffset(c15) = { 0.0250000004, -0.0444444455, -0.0500000007, 0.088888891 };
  float StepOffsetsOpaque[5] : packoffset(c16);
  float StepWeightsOpaque[5] : packoffset(c21);
  float StepOffsetsTranslucent[5] : packoffset(c26);
  float StepWeightsTranslucent[5] : packoffset(c31);
  float4 BloomTintAndScreenBlendThreshold : packoffset(c36);
  float4 HalfResMaskRect : packoffset(c37);
  float4 GammaColorScaleAndInverse : packoffset(c38);
  float4 GammaOverlayColor : packoffset(c39);
  float4 NoiseTextureOffset : packoffset(c40);
  float FilmGrain_Scale : packoffset(c41);
  float4 ScreenUVScaleBias : packoffset(c42);
}

cbuffer PSOffsetConstants : register(b2) {
  float4 ScreenPositionScaleBias : packoffset(c0);
  float4 MinZ_MaxZRatio : packoffset(c1);
  float4 DynamicScale : packoffset(c2);
}

SamplerState SceneColorTextureSampler_s : register(s0);
SamplerState DOFTextureSampler_s : register(s1);
SamplerState DOFBlurredNearSampler_s : register(s2);
SamplerState DOFBlurredFarSampler_s : register(s3);
SamplerState BlurredImageSeperateBloomSampler_s : register(s4);
SamplerState ColorGradingLUTSampler_s : register(s5);
SamplerState NoiseTextureSampler_s : register(s6);
SamplerState smpFilmicLUTSampler_s : register(s7);
Texture2D<float4> SceneColorTexture : register(t0);
Texture2D<float4> DOFTexture : register(t1);
Texture2D<float4> DOFBlurredNear : register(t2);
Texture2D<float4> DOFBlurredFar : register(t3);
Texture2D<float4> BlurredImageSeperateBloom : register(t4);
Texture2D<float4> ColorGradingLUT : register(t5);
Texture2D<float4> NoiseTexture : register(t6);
Texture2D<float4> smpFilmicLUT : register(t7);

// 3Dmigoto declarations
#define cmp -

void main(
    float4 v0: TEXCOORD0,
    float2 v1: TEXCOORD1,
    out float4 o0: SV_Target0,
    out float o1: SV_Target1) {
  float4 r0, r1, r2, r3, r4;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = DynamicScale.xy * v0.zw;
  r1.xyz = SceneColorTexture.Sample(SceneColorTextureSampler_s, r0.xy).xyz;
  r0.zw = cmp(float2(0, 0) < MinMaxBlurClamp.xy);
  r1.w = (int)r0.w | (int)r0.z;
  if (r1.w != 0) {
    r1.w = DOFTexture.Sample(DOFTextureSampler_s, r0.xy).x;
    r1.w = saturate(r1.w);
    r1.w = r1.w * MinZ_MaxZRatio.z + -MinZ_MaxZRatio.w;
    r1.w = max(1.00000001e-07, r1.w);
    r1.w = 1 / r1.w;
    r2.xyzw = DOFBlurredNear.Sample(DOFBlurredNearSampler_s, r0.xy).xyzw;
    r2.xyzw = r0.zzzz ? r2.xyzw : 0;
    r3.xyzw = DOFBlurredFar.Sample(DOFBlurredFarSampler_s, r0.xy).xyzw;
    r3.xyzw = r0.wwww ? r3.xyzw : 0;
    r0.z = cmp(0 < PackedParameters.w);
    r0.w = -PackedParameters.x + r1.w;
    r4.x = saturate(-PackedParameters.y * r0.w);
    r4.x = max(9.99999975e-05, r4.x);
    r4.x = log2(r4.x);
    r4.x = PackedParameters.w * r4.x;
    r4.x = exp2(r4.x);
    r1.w = PackedParameters.x + -r1.w;
    r4.yz = float2(300, 500) * PackedParameters.zz;
    r4.yz = max(float2(9.99999975e-05, 9.99999975e-05), r4.yz);
    r1.w = saturate(r1.w / r4.y);
    r4.x = r0.z ? r4.x : r1.w;
    r1.w = saturate(PackedParameters.y * r0.w);
    r1.w = max(9.99999975e-05, r1.w);
    r1.w = log2(r1.w);
    r1.w = PackedParameters.w * r1.w;
    r1.w = exp2(r1.w);
    r0.w = -PackedParameters.y + r0.w;
    r0.w = saturate(r0.w / r4.z);
    r4.y = r0.z ? r1.w : r0.w;
    r4.xy = MinMaxBlurClamp.xy * r4.xy;
    r4.xy = r4.xy * r4.xy;
    r0.w = max(r4.x, r2.w);
    r1.w = min(r4.y, r3.w);
    r2.w = saturate(r0.w);
    r3.w = r2.w * -2 + 3;
    r2.w = r2.w * r2.w;
    r4.y = r3.w * r2.w;
    r4.z = saturate(DOFKernelParams.y * r1.w);
    r4.x = saturate(DOFKernelParams.x * r0.w);
    r0.zw = r0.zz ? r4.yz : r4.xz;
    r3.xyz = r3.xyz + -r1.xyz;
    r3.xyz = r0.www * r3.xyz + r1.xyz;
    r2.xyz = -r3.xyz + r2.xyz;
    r1.xyz = r0.zzz * r2.xyz + r3.xyz;
  }
  r0.xyz = BlurredImageSeperateBloom.Sample(BlurredImageSeperateBloomSampler_s, r0.xy).xyz;
  r0.xyz = BloomTintAndScreenBlendThreshold.xyz * r0.xyz;
  r0.w = dot(r1.xyz, float3(0.298999995, 0.587000012, 0.114));
  r0.xyzw = float4(4, 4, 4, -3) * r0.xyzw;
  r0.w = exp2(r0.w);
  r0.w = saturate(BloomTintAndScreenBlendThreshold.w * r0.w) * CUSTOM_BLOOM;

  r0.xyz = r0.xyz * r0.www + r1.xyz;

  float3 untonemapped = r0.xyz;

  // TODO: Improve LUT sampling
  r0.xyz = float3(0.0616082214, 0.0616082214, 0.0616082214) * r0.xyz;
  r0.x = smpFilmicLUT.Sample(smpFilmicLUTSampler_s, r0.xx).x;
  r0.y = smpFilmicLUT.Sample(smpFilmicLUTSampler_s, r0.yy).x;
  r0.z = smpFilmicLUT.Sample(smpFilmicLUTSampler_s, r0.zz).x;
  r0.xw = float2(0.05859375, 15) * r0.xz;
  r0.w = floor(r0.w);
  r0.z = r0.z * 15 + -r0.w;
  r1.x = r0.w * 0.0625 + r0.x;
  r1.y = 0.9375 * r0.y;
  r1.xyzw = float4(0.001953125, 0.03125, 0.064453125, 0.03125) + r1.xyxy;
  r0.xyw = ColorGradingLUT.Sample(ColorGradingLUTSampler_s, r1.xy).xyz;
  r1.xyz = ColorGradingLUT.Sample(ColorGradingLUTSampler_s, r1.zw).xyz;
  r1.xyz = r1.xyz + -r0.xyw;
  r0.xyz = r0.zzz * r1.xyz + r0.xyw;
  r0.xyz = GammaOverlayColor.xyz + r0.xyz;
  if (RENODX_TONE_MAP_TYPE != 0.f) {
    r0.xyz = (GammaColorScaleAndInverse.xyz * r0.xyz);
    r0.xyz = renodx::math::SignPow(r0.xyz, GammaColorScaleAndInverse.w);
    r0.xyz = renodx::color::gamma::DecodeSafe(r0.xyz);
    float3 tonemapped = renodx::draw::ToneMapPass(untonemapped, r0.xyz);
    r0.xyz = tonemapped;
    // Scale and Encode later with film grain
  } else {
    r0.xyz = saturate(GammaColorScaleAndInverse.xyz * r0.xyz);
    r0.xyz = max(float3(9.99999975e-05, 9.99999975e-05, 9.99999975e-05), r0.xyz);
    r0.xyz = log2(r0.xyz);
    r0.xyz = GammaColorScaleAndInverse.www * r0.xyz;
    r0.xyz = exp2(r0.xyz);
  }
  r1.xy = v0.zw * ScreenUVScaleBias.xy + ScreenUVScaleBias.zw;
  r1.xy = float2(-0.5, -0.5) + r1.xy;
  r1.xy = float2(0.832050323, 0.554700196) * r1.xy;
  r0.w = dot(r1.xy, r1.xy);
  r0.w = -0.0500000007 + r0.w;
  r0.w = saturate(4 * r0.w);
  r1.x = r0.w * -2 + 3;
  r0.w = r0.w * r0.w;
  r1.xyz = -r1.xxx * r0.www + float3(1.01036298, 1.00000572, 1.16309249);
  r1.xyz = lerp(1.f, r1.xyz, CUSTOM_VIGNETTE);
  if (RENODX_TONE_MAP_TYPE != 0.f) {
    if (FilmGrain_Scale > 0 && CUSTOM_FILM_GRAIN > 0.f) {
      r0.xyz = renodx::effects::ApplyFilmGrain(
          r0.xyz,
          v0.zw,
          CUSTOM_RANDOM,
          FilmGrain_Scale / 0.06 * CUSTOM_FILM_GRAIN * 0.03f,
          1.f);
    }
    r0.xyz *= RENODX_DIFFUSE_WHITE_NITS / RENODX_GRAPHICS_WHITE_NITS;
    r0.xyz = renodx::color::gamma::EncodeSafe(r0.rgb, 2.2f);
    // vignette in gamma
    r0.xyz = r0.xyz * r1.xyz;

    o0.w = dot(r0.xyz, float3(0.298999995, 0.587000012, 0.114));
  } else {
    r2.xy = v0.zw * NoiseTextureOffset.xy + NoiseTextureOffset.zw;
    r0.w = NoiseTexture.Sample(NoiseTextureSampler_s, r2.xy).x;
    r0.w = -0.5 + r0.w;
    r0.w = FilmGrain_Scale * r0.w;
    r0.xyz = r0.xyz * r1.xyz + r0.www;
    o0.w = dot(r0.xyz, float3(0.298999995, 0.587000012, 0.114));
    r0.xyz = saturate(r0.xyz);
  }
  r0.w = dot(r0.xyz, float3(0.212670997, 0.715160012, 0.0721689984));
  r0.w = r0.w * 15 + 1;
  r0.w = log2(r0.w);
  o1.x = 0.25 * r0.w;
  o0.xyz = r0.xyz;
  return;
}
