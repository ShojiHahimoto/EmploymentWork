cbuffer FloorBuffer : register(b0)
{
	matrix cameraViewProjection;
	matrix lightViewProjection;
	float4 floorBounds;
	float4 shadowSettings;
};

struct ReceiverVertex
{
	float4 position : SV_POSITION;
	float4 lightPosition : TEXCOORD0;
};
