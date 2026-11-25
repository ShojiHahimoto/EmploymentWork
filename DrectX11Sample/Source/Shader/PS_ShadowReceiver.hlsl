#include "ShadowReceiver.hlsli"

Texture2D<float> shadowMap : register(t0);
SamplerComparisonState shadowSampler : register(s0);

float4 PSMain(ReceiverVertex input) : SV_TARGET
{
	const float3 projected = input.lightPosition.xyz / input.lightPosition.w;
	const float2 uv = projected.xy * float2(0.5f, -0.5f) + 0.5f;
	float visibility = 1.0f;

	if (all(uv >= 0.0f) && all(uv <= 1.0f) && projected.z >= 0.0f && projected.z <= 1.0f)
	{
		visibility = shadowMap.SampleCmpLevelZero(shadowSampler, uv, projected.z - shadowSettings.x);
	}

	return float4(0.0f, 0.0f, 0.0f, (1.0f - shadowSettings.y) * (1.0f - visibility));
}
