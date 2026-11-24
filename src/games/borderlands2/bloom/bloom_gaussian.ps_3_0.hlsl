sampler2D bloom_texture : register(s0);
// xy: blur direction in source mip texels, z: source mip level.
float4 bloom_metrics : register(c0);

float4 main(float2 uv : TEXCOORD0) : COLOR0 {
  // A normalized nine-tap Gaussian, paired into five bilinear fetches.
  float3 color = tex2Dlod(bloom_texture, float4(uv, 0, bloom_metrics.z)).rgb * 0.2270270270;
  float2 offset = bloom_metrics.xy * 1.3846153846;
  color += tex2Dlod(bloom_texture, float4(uv + offset, 0, bloom_metrics.z)).rgb * 0.3162162162;
  color += tex2Dlod(bloom_texture, float4(uv - offset, 0, bloom_metrics.z)).rgb * 0.3162162162;
  offset = bloom_metrics.xy * 3.2307692308;
  color += tex2Dlod(bloom_texture, float4(uv + offset, 0, bloom_metrics.z)).rgb * 0.0702702703;
  color += tex2Dlod(bloom_texture, float4(uv - offset, 0, bloom_metrics.z)).rgb * 0.0702702703;
  return float4(color, 0);
}
