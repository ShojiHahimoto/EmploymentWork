Texture2D albedo : register(t0);
SamplerState surfaceSampler : register(s0);

#include "SurfaceLighting.hlsli"

float4 PSMain(SurfaceVertex input) : SV_TARGET
{
	const float3 towardLight = normalize(float3(0.0f, 0.8f, -0.5f));
	const float3 normal = normalize(input.normal);
	const float diffuse = saturate(dot(normal, towardLight));
	float falloff;
	const float3 towardLocal = DirectionToLight(input.worldPosition, falloff);
	const float localDiffuse = saturate(dot(normal, towardLocal)) * falloff;
	const float intensity = lightingStrength.x + 0.17f * diffuse + lightingStrength.y * localDiffuse;
	return float4(albedo.Sample(surfaceSampler, input.uv).rgb * input.tint.rgb * intensity, 1.0f);
}
