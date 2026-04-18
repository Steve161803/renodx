#include "./common.hlsli"

float OcclusionPercentage : register( c0 );
float4 UniformPixelVector_0 : register( c4 );

struct PS_IN
{
	float2 texcoord : TEXCOORD;
	float4 texcoord1 : TEXCOORD1;
	float4 texcoord4 : TEXCOORD4;
};

float4 main(PS_IN i) : COLOR
{
	float4 o;

	float4 r0;
	float r1;
	
	r0.xy = 1 + -i.texcoord.xy;
	r0.xy = r0.xy * i.texcoord.xy;
	r0.x = dot(r0.xy, 2) + 0;
	r1.x = pow(abs(r0.x), 16);
	r0.x = abs(r0.x) + -9.99999997e-007;
	r0.yzw = (i.texcoord1.xxyz * i.texcoord1.w).yzw;
	r0.yzw = (r0.xyzw * r1.x).yzw;
	r0.yzw = (r0.xyzw * OcclusionPercentage.x).yzw;
	r0.yzw = (r0.xyzw * 10).yzw;
	r0.xyz = (r0.x >= 0) ? r0.yzw : 0;
	r0.xyz = r0.xyz + UniformPixelVector_0.xyz;
	o.xyz = r0.xyz * i.texcoord4.w;
	if (RENODX_TONE_MAP_TYPE > 0) {
	  o.rgb = ApplyHDRBoost(o.rgb);
	}
	o.w = 0;
	return o;
}
