#include "ModelSkinning.hlsli"

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
		matrix skinMatrix = GetSkinMatrix(input);

		localPosition = mul(localPosition, skinMatrix);
		localNormal = normalize(mul(float4(localNormal, 0.0f), skinMatrix).xyz);
	}

	output.position = mul(localPosition, worldViewProjection);
	output.normal = normalize(localNormal);
	output.uv = input.uv;
	return output;
}
