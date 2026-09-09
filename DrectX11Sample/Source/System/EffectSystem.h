#pragma once

class World;

class EffectSystem
{
public:
	/// <summary>
	/// エフェクトの粒子生成、粒子更新、再生終了時の削除リクエストを行う。
	/// </summary>
	/// <param name="world">EffectComponent を持つ GameObject を保持する World。</param>
	static void Update(World& world);
};
