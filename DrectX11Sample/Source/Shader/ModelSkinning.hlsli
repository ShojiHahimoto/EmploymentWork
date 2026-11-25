cbuffer TransformBuffer : register(b0)
{
	matrix worldViewProjection;
	matrix boneMatrices[256];
	int4 skinningFlags;
	float4 modelColor;
};

struct VS_INPUT
{
	float3 position : POSITION;
	float3 normal : NORMAL;
	float2 uv : TEXCOORD;
	uint4 boneIndices : BLENDINDICES;
	float4 boneWeights : BLENDWEIGHT;
};

matrix GetSkinMatrix(VS_INPUT input)
{
	uint4 indices = min(input.boneIndices, uint4(255, 255, 255, 255));
	return boneMatrices[indices.x] * input.boneWeights.x +
		boneMatrices[indices.y] * input.boneWeights.y +
		boneMatrices[indices.z] * input.boneWeights.z +
		boneMatrices[indices.w] * input.boneWeights.w;
}
