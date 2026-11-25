#include "ShadowReceiver.hlsli"

ReceiverVertex VSMain(uint id : SV_VertexID)
{
	const float2 corners[6] = {
		float2(0, 0), float2(0, 1), float2(1, 0),
		float2(1, 0), float2(0, 1), float2(1, 1)
	};
	const float2 xz = lerp(floorBounds.xy, floorBounds.zw, corners[id]);
	const float4 worldPosition = float4(xz.x, -0.01f, xz.y, 1.0f);

	ReceiverVertex output;
	output.position = mul(worldPosition, cameraViewProjection);
	output.lightPosition = mul(worldPosition, lightViewProjection);
	return output;
}
