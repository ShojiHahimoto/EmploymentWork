#pragma once

#include "Component/Component.h"
#include "Core/GameObject.h"
#include "Data/EffectData.h"

#include <SimpleMath.h>

#include <string>
#include <vector>

/// <summary>
/// 実行中の 1 粒子分の状態を保持する。
/// </summary>
struct EffectParticleRuntime
{
	DirectX::SimpleMath::Vector3 position = DirectX::SimpleMath::Vector3::Zero;
	DirectX::SimpleMath::Vector2 velocity = DirectX::SimpleMath::Vector2::Zero;
	DirectX::SimpleMath::Vector2 acceleration = DirectX::SimpleMath::Vector2::Zero;
	float startScale = 1.0f;
	float endScale = 1.0f;
	float rotationDegrees = 0.0f;
	float angularVelocityDegrees = 0.0f;
	int ageFrames = 0;
	int lifeFrames = 1;
	DirectX::SimpleMath::Color startColor = DirectX::SimpleMath::Color(1.0f, 1.0f, 1.0f, 1.0f);
	DirectX::SimpleMath::Color endColor = DirectX::SimpleMath::Color(1.0f, 1.0f, 1.0f, 0.0f);
	std::string texturePath;
	EffectBlendMode blendMode = EffectBlendMode::Additive;
	bool depthEnabled = false;
	bool alive = true;
};

/// <summary>
/// World 上で再生中のエフェクト状態を保持する Component。
/// </summary>
struct EffectComponent : public Component
{
	// 再生元のエフェクト定義。生成時に JSON からコピーされ、以後はランタイム状態として扱う。
	EffectData effectData;
	// 生成位置を基準に、左右反転が必要な値へ掛ける向き。右向きが +1、左向きが -1。
	float facingSign = 1.0f;
	// 生成リクエスト時点のワールド座標。生成直後の Transform キャッシュ更新前でも粒子発生位置を安定させる。
	DirectX::SimpleMath::Vector3 spawnWorldPosition = DirectX::SimpleMath::Vector3::Zero;
	// 追従エフェクト用の予約。現段階ではワールド固定エフェクトを使う。
	GameObjectId followTargetId = INVALID_GAME_OBJECT_ID;
	bool followTarget = false;
	// 何フレーム目のエフェクト処理を行っているか。
	int currentFrame = 0;
	bool playing = true;
	std::vector<bool> emittedEmitters;
	std::vector<EffectParticleRuntime> particles;
};
