#include "SurfaceLighting.hlsli"

float4 PSMain(SurfaceVertex input) : SV_TARGET
{
	const float3 normal = normalize(input.normal);
	float falloff;
	const float3 towardLight = DirectionToLight(input.worldPosition, falloff);
	const float3 towardCamera = normalize(cameraPosition.xyz - input.worldPosition);
	const float3 reflected = reflect(-towardLight, normal);
	const float facing = saturate(dot(normal, towardLight));
	const float highlight = pow(saturate(dot(reflected, towardCamera)), lightingStrength.w);
	const float specular = highlight * facing * falloff * lightingStrength.z;
	return float4(specular.xxx, 0.0f);
}
