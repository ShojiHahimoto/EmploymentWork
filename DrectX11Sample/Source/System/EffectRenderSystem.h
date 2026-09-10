#pragma once

struct CameraComponent;
class World;

class EffectRenderSystem
{
public:
	/// <summary>
	/// World 内の EffectComponent を、指定カメラの 3D 空間へ描画する。
	/// </summary>
	/// <param name="world">EffectComponent を持つ GameObject を保持する World。</param>
	/// <param name="camera">描画に使うカメラ行列。</param>
	static void Draw(World& world, const CameraComponent& camera);

	/// <summary>
	/// EffectRenderSystem が保持する DirectX11 リソースを解放する。
	/// </summary>
	static void ReleaseResources();
};
