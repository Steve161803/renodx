sampler2D bloom_texture : register(s0);
// xy: source mip texel size, z: source mip level.
float4 bloom_metrics : register(c0);

float4 main(float2 uv : TEXCOORD0) : COLOR0 {
  float2 offset = bloom_metrics.xy * 0.5;
  float3 color = tex2Dlod(bloom_texture, float4(uv + offset, 0, bloom_metrics.z)).rgb;
  color += tex2Dlod(bloom_texture, float4(uv - offset, 0, bloom_metrics.z)).rgb;
  color += tex2Dlod(bloom_texture, float4(uv + float2(offset.x, -offset.y), 0, bloom_metrics.z)).rgb;
  color += tex2Dlod(bloom_texture, float4(uv + float2(-offset.x, offset.y), 0, bloom_metrics.z)).rgb;
  return float4(color * 0.25, 0);
}
