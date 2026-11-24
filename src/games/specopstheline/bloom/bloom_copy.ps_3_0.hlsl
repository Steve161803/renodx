sampler2D bloom_texture : register(s0);

float4 main(float2 uv : TEXCOORD0) : COLOR0 {
  return float4(tex2Dlod(bloom_texture, float4(uv, 0, 0)).rgb, 0);
}
