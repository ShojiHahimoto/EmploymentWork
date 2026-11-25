struct SurfaceVertex
{
	float4 position : SV_POSITION;
	float2 uv : TEXCOORD0;
	float3 normal : NORMAL;
	float4 tint : COLOR;
	float3 worldPosition : TEXCOORD1;
};

cbuffer SurfaceLighting : register(b1)
{
	float4 localLight;
	float4 cameraPosition;
	float4 lightingStrength;
};

float3 DirectionToLight(float3 worldPosition, out float falloff)
{
	const float3 offset = localLight.xyz - worldPosition;
	const float distanceSquared = dot(offset, offset);
	falloff = rcp(1.0f + distanceSquared / (localLight.w * localLight.w));
	return offset * rsqrt(max(distanceSquared, 0.0001f));
}
