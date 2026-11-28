cbuffer TransformBuffer : register(b0)
{
	matrix worldViewProjection;
	matrix boneMatrices[256];
	int4 skinningFlags;
	float4 modelColor;
};

Texture2D diffuseTexture : register(t0);
SamplerState diffuseSampler : register(s0);

struct PS_INPUT
{
	float4 position : SV_POSITION;
	float3 normal : NORMAL;
	float2 uv : TEXCOORD;
};

float4 PSMain(PS_INPUT input) : SV_TARGET
{
	float light = saturate(dot(normalize(input.normal), normalize(float3(0.3f, 0.8f, -0.5f))));
	float4 baseColor = diffuseTexture.Sample(diffuseSampler, input.uv);
	return baseColor * modelColor * float4(0.35f + light * 0.55f, 0.38f + light * 0.45f, 0.42f + light * 0.35f, 1.0f);
}
