#include "./common.hlsl"

// ---- Created with 3Dmigoto v1.4.1 on Sun Apr  5 11:46:40 2026

cbuffer _Globals : register(b0)
{
  float4x4 view : packoffset(c0);
  float4x4 projection : packoffset(c4);
  float4x4 viewprojection : packoffset(c8);
  float4 eyeDirectionWS : packoffset(c12);
  float4 eyePositionWS : packoffset(c13);
  float4 eyePositionOS : packoffset(c14);
  float4 cameraNear : packoffset(c15);
  float4x4 modelIT : packoffset(c16);
  float4x4 model : packoffset(c20);
  float4x4 modelViewProj : packoffset(c24);
  float cameraNearTimesFar : packoffset(c28);
  float cameraFarMinusNear : packoffset(c28.y);
  float4 cameraNearFar : packoffset(c29);
  float Orient : packoffset(c30);
  float4 CenterPos : packoffset(c31);
  float Bias : packoffset(c32);
  float Alpha : packoffset(c32.y) = {1};
}

SamplerState PointClampSampler_s : register(s8);
SamplerState LinearClampSampler_s : register(s9);
Texture2D<float4> tex : register(t0);


// 3Dmigoto declarations
#define cmp -


void main(
  float4 v0 : SV_POSITION0,
  float2 v1 : TEXCOORD0,
  out float4 o0 : SV_TARGET0)
{
  float4 r0;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = tex.Sample(LinearClampSampler_s, v1.xy).yz;
  o0.yz = r0.xy;
  r0.xy = tex.Sample(PointClampSampler_s, v1.xy).wx;
  r0.xy = r0.xy;
  r0.xy = r0.xy;
  o0.w = Alpha * r0.x * CUSTOM_VIGNETTE;
  o0.x = r0.y;
  return;
}