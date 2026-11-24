sampler2D AmbientOcclusionTexture : register(s0);

struct PS_IN
{
	float2 texcoord : TEXCOORD;
};

float4 main(PS_IN i) : COLOR
{
	float4 o;
	float4 r0;

	r0 = tex2D(AmbientOcclusionTexture, i.texcoord);
	o.x = saturate(r0.x);
	o.yzw = 0;
	return o;
}
