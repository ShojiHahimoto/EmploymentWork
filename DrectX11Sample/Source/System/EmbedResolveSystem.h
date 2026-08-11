#pragma once

class World;

class EmbedResolveSystem
{
public:
	static constexpr float StageMinX = -30.0f;
	static constexpr float StageMaxX = 30.0f;

	/// <summary>
	/// 移動後の地面、壁、プレイヤー同士のめり込みを補正する。
	/// </summary>
	/// <param name="world">補正対象の GameObject と Component を保持する World。</param>
	static void Update(World& world);

private:
	static void ResolveWallBounds(World& world);
	static void ResolveCameraViewBounds(World& world);
	static void ResolvePlayerPushBoxes(World& world);
	static void ResolveTemporaryGround(World& world);
};
