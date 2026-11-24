sampler2D bloom_texture : register(s0);
float4 bloom_metrics : register(c0);  // w: initialized mip count (at most six).

float4 main(float2 uv : TEXCOORD0) : COLOR0 {
  const float weights[6] = {0.32, 0.25, 0.18, 0.12, 0.08, 0.05};
  float3 color = 0;
  float weight_sum = 0;
  [unroll]
  for (int level = 0; level < 6; ++level) {
    if (level < bloom_metrics.w) {
      color += tex2Dlod(bloom_texture, float4(uv, 0, level)).rgb * weights[level];
      weight_sum += weights[level];
    }
  }
  return float4(color / max(weight_sum, 0.0001), 0);
}
