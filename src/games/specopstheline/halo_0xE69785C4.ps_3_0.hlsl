#include "./common.hlsli"

sampler2D Texture2D_0 : register( s0 );
float4 UniformPixelScalars_0 : register( c4 );
float4 UniformPixelVector_0 : register( c0 );

struct PS_IN
{
	float2 texcoord : TEXCOORD;
	float4 texcoord4 : TEXCOORD4;
};

float4 main(PS_IN i) : COLOR
{
	float4 o;

	float4 r0;
	float3 r1;

    if (RENODX_TONE_MAP_TYPE > 0) {
	  clip(-1.0);
	  return float4(0.0, 0.0, 0.0, 0.0);
    } else {			
	  r0 = tex2D(Texture2D_0, i.texcoord);
	  o.w = r0.w * UniformPixelScalars_0.z;
	  r0.w = dot(r0.xyz, float3(0.300000012, 0.589999974, 0.109999999));
	  r1.xyz = lerp(r0.xyz, r0.w, UniformPixelScalars_0.x);
	  r0.y = UniformPixelScalars_0.y;
	  r0.xyz = r0.y * r1.xyz + UniformPixelVector_0.xyz;
	  o.xyz = r0.xyz * i.texcoord4.w + i.texcoord4.xyz;
	  return o;
	}
}
