sampler2D scene_texture : register(s0);
float4 bloom_scale : register(c1);
float4 sample_offsets[8] : register(c2);

float4 main(float2 uv : TEXCOORD0) : COLOR0 {
  // Match the sixteen bright-pass samples in 0xC4EC30F6, without its DOF term.
  float3 color = 0;
  [unroll]
  for (int index = 0; index < 8; ++index) {
    float3 a = tex2Dlod(scene_texture, float4(uv + sample_offsets[index].xy, 0, 0)).rgb;
    float3 b = tex2Dlod(scene_texture, float4(uv + sample_offsets[index].wz, 0, 0)).rgb;
    color += a * saturate((max(a.r, max(a.g, a.b)) - 1.0) * 0.5);
    color += b * saturate((max(b.r, max(b.g, b.b)) - 1.0) * 0.5);
  }
  // The native gather averages sixteen samples, then packs both bloom and DOF
  // at quarter intensity for the later composite's x4 decode.
  return float4(color * (bloom_scale.x / 64.0), 0);
}
