#include "ModelSkinning.hlsli"

Texture2D albedo : register(t0);
SamplerState materialSampler : register(s0);

struct Surface
{
	float4 position : SV_POSITION;
	float3 normal : NORMAL;
	float2 uv : TEXCOORD;
	float3 viewPosition : TEXCOORD1;
	float3 viewNormal : TEXCOORD2;
};

float4 PSMain(Surface input) : SV_TARGET
{
	float3 n = normalize(input.viewNormal);
	float3 v = normalize(-input.viewPosition);
	float3 reflected = reflect(-v, n);
	// Broad studio strips reflected in a satin finish rather than baked into the albedo.
	float key = pow(saturate(dot(reflected, normalize(float3(-0.42, 0.72, 0.55)))), 9);
	float fill = pow(saturate(dot(reflected, normalize(float3(0.84, 0.36, -0.3)))), 5);
	float diffuse = saturate(dot(n, normalize(float3(-0.25, 0.7, 0.66))));
	float4 base = albedo.Sample(materialSampler, input.uv);
	float3 rgb = base.rgb * (0.47 + 0.48 * diffuse) + (0.49 * key + 0.24 * fill);
	return float4(rgb * modelColor.rgb, base.a * modelColor.a);
}
