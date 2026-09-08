cbuffer TransformBuffer : register(b0)
{
	matrix worldViewProjection;
	matrix boneMatrices[256];
	int4 skinningFlags;
};

Texture2D diffuseTexture : register(t0);
SamplerState diffuseSampler : register(s0);

struct VS_INPUT
{
	float3 position : POSITION;
	float3 normal : NORMAL;
	float2 uv : TEXCOORD;
	uint4 boneIndices : BLENDINDICES;
	float4 boneWeights : BLENDWEIGHT;
};

struct PS_INPUT
{
	float4 position : SV_POSITION;
	float3 normal : NORMAL;
	float2 uv : TEXCOORD;
};

PS_INPUT VSMain(VS_INPUT input)
{
	PS_INPUT output;
	float4 localPosition = float4(input.position, 1.0f);
	float3 localNormal = input.normal;

	if (skinningFlags.x != 0)
	{
		uint4 boneIndex = min(input.boneIndices, uint4(255, 255, 255, 255));
		matrix skinMatrix =
			boneMatrices[boneIndex.x] * input.boneWeights.x +
			boneMatrices[boneIndex.y] * input.boneWeights.y +
			boneMatrices[boneIndex.z] * input.boneWeights.z +
			boneMatrices[boneIndex.w] * input.boneWeights.w;

		localPosition = mul(localPosition, skinMatrix);
		localNormal = normalize(mul(float4(localNormal, 0.0f), skinMatrix).xyz);
	}

	output.position = mul(localPosition, worldViewProjection);
	output.normal = normalize(localNormal);
	output.uv = input.uv;
	return output;
}

float4 PSMain(PS_INPUT input) : SV_TARGET
{
	float light = saturate(dot(normalize(input.normal), normalize(float3(0.3f, 0.8f, -0.5f))));
	float4 baseColor = diffuseTexture.Sample(diffuseSampler, input.uv);
	return baseColor * float4(0.35f + light * 0.55f, 0.38f + light * 0.45f, 0.42f + light * 0.35f, 1.0f);
}
