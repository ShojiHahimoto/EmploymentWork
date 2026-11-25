cbuffer Surface : register(b0)
{
	matrix viewProjection;
	float4 origin;
	float4 edgeU;
	float4 edgeV;
	float4 normal;
	float4 tint;
};

#include "SurfaceLighting.hlsli"

SurfaceVertex VSMain(uint id : SV_VertexID)
{
	const float2 corners[6] = {
		float2(0, 0), float2(0, 1), float2(1, 0),
		float2(1, 0), float2(0, 1), float2(1, 1)
	};
	const float2 corner = corners[id];
	const float3 position = origin.xyz + corner.x * edgeU.xyz + corner.y * edgeV.xyz;

	SurfaceVertex output;
	output.position = mul(float4(position, 1.0f), viewProjection);
	output.uv = corner * float2(edgeU.w, edgeV.w);
	output.normal = normal.xyz;
	output.tint = tint;
	output.worldPosition = position;
	return output;
}
