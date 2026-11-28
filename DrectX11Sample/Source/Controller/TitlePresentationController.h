#pragma once

#include "Core/Renderer.h"
#include "Core/World.h"
#include "Data/AttackData.h"
#include <string>
#include <vector>

struct CharacterData;

struct PreparedTitlePresentation
{
	int waitFrames = 300;
	std::vector<AttackData> attacks;
};

/// <summary>中央固定モデルの周回カメラと、待機・攻撃の順次演出を管理する。</summary>
class TitlePresentationController
{
  public:
	static PreparedTitlePresentation Prepare();
	void Enter(World &world, const PreparedTitlePresentation &prepared, std::shared_ptr<const CharacterData> character);

	void Update(World &world);

	void Draw(World &world, Renderer &renderer, int width, int height) const;

  private:
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
	std::vector<AttackData> attacks;
};
