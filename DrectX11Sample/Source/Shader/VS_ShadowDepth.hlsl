#include "ModelSkinning.hlsli"

float4 VSMain(VS_INPUT input) : SV_POSITION
{
	float4 position = float4(input.position, 1.0f);
	if (skinningFlags.x != 0)
	{
		position = mul(position, GetSkinMatrix(input));
	}

	return mul(position, worldViewProjection);
}
