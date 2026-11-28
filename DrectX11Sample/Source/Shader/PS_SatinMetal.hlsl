#include "ModelSkinning.hlsli"

Texture2D albedo : register(t0);
Texture2D normalMap : register(t1);
SamplerState materialSampler : register(s0);

struct Surface
{
	float4 position : SV_POSITION;
	float3 normal : NORMAL;
	float2 uv : TEXCOORD;
	float3 viewPosition : TEXCOORD1;
	float3 viewNormal : TEXCOORD2;
};

// Derive a tangent frame from the skinned surface, including mirrored UVs.
float3 SurfaceNormal(Surface input)
{
	const float3 n = normalize(input.viewNormal);
	const float3 dpdx = ddx(input.viewPosition);
	const float3 dpdy = ddy(input.viewPosition);
	const float2 duvdx = ddx(input.uv);
	const float2 duvdy = ddy(input.uv);
	const float determinant = duvdx.x * duvdy.y - duvdx.y * duvdy.x;
	const float3 encoded = normalMap.Sample(materialSampler, input.uv).rgb;
	if (!any(encoded) || abs(determinant) < 1e-12)
	{
		return n;
	}

	const float3 tangent = (dpdx * duvdy.y - dpdy * duvdx.y) * sign(determinant);
	const float3 bitangent = (dpdy * duvdx.x - dpdx * duvdy.x) * sign(determinant);
	const float3 projected = tangent - n * dot(n, tangent);
	if (dot(projected, projected) < 1e-12)
	{
		return n;
	}

	const float3 t = normalize(projected);
	const float3 perpendicular = cross(n, t);
	const float3 b = perpendicular * (dot(perpendicular, bitangent) < 0 ? -1 : 1);
	const float3 mapped = encoded * 2 - 1;
	return normalize(t * mapped.x + b * mapped.y + n * mapped.z);
}

float4 PSMain(Surface input) : SV_TARGET
{
	float3 n = SurfaceNormal(input);
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
