#pragma once

#include "Core/Renderer.h"
#include "Core/World.h"
#include "Data/AttackData.h"
#include <string>
#include <vector>

/// <summary>中央固定モデルの周回カメラと、待機・攻撃の順次演出を管理する。</summary>
class TitlePresentationController
{
  public:
	/// <summary>設定を読み込み、中央にモデルを生成して待機再生を開始する。</summary>
	/// <param name="world">展示用World。</param>
	void Enter(World &world);

	/// <summary>固定60fpsで演出を更新する。対戦入力・物理・攻撃移動は実行しない。</summary>
	/// <param name="world">展示用World。</param>
	void Update(World &world);

	/// <summary>中央固定の本体行列に、汎用モーションの見た目オフセットだけを加えて描く。</summary>
	/// <param name="world">展示用World。</param>
	/// <param name="renderer">描画先。</param>
	/// <param name="width">画面幅。</param>
	/// <param name="height">画面高さ。</param>
	void Draw(World &world, Renderer &renderer, int width, int height) const;

  private:
	/// <summary>秒単位の待機間隔と、順次再生する攻撃データIDをJSONから読む。</summary>
	void LoadSettings();
	/// <summary>モーションを0Fから開始する。攻撃は単発、待機は保存されたループ設定に従う。</summary>
	/// <param name="world">展示用World。</param>
	/// <param name="motionId">再生するモーションID。</param>
	/// <returns>モーションを取得できた場合true。</returns>
	bool StartMotion(World &world, const std::string &motionId);

	static constexpr float OrbitRadiansPerFrame = 0.0025f;
	static constexpr float OrbitRadius = 23.0f;
	static constexpr float CameraHeightAboveTarget = 7.0f;
	static constexpr float TargetHeight = 5.0f;
	float angle = 0.0f;
	int waitFrames = 120;
	int elapsedWaitFrames = 0;
	int attackFramesRemaining = 0;
	size_t nextAttackIndex = 0;
	bool attackPlaying = false;
	AttackData currentAttack;
	std::vector<std::string> attackDataIds;
};
