#pragma once

#include <SimpleMath.h>

#include <string>
#include <vector>

enum class EffectBlendMode
{
	Alpha,
	Additive,
	Unknown
};

/// <summary>
/// エフェクト内の 1 種類の粒子発生設定を保持する。
/// </summary>
struct EffectEmitterData
{
	// このエミッターが粒子を発生させる EffectComponent::currentFrame。
	int startFrame = 0;
	// startFrame に同時生成する粒子数。
	int emitCount = 1;
	// 生成した粒子が生きるフレーム数。
	int particleLifeFrames = 12;
	// 空文字なら EffectRenderSystem の白テクスチャを使う。
	std::string texturePath;
	// Alpha は通常合成、Additive は火花や光用の加算合成。
	EffectBlendMode blendMode = EffectBlendMode::Additive;
	// false の場合はモデルより手前に重ねる演出として描画する。
	bool depthEnabled = false;
	// エフェクト発生位置からの基準オフセット。
	DirectX::SimpleMath::Vector3 baseOffset = DirectX::SimpleMath::Vector3::Zero;
	// 粒子ごとのランダムな発生範囲。
	DirectX::SimpleMath::Vector2 spawnRange = DirectX::SimpleMath::Vector2::Zero;
	// 粒子ごとの初速範囲。
	DirectX::SimpleMath::Vector2 velocityMin = DirectX::SimpleMath::Vector2::Zero;
	DirectX::SimpleMath::Vector2 velocityMax = DirectX::SimpleMath::Vector2::Zero;
	// 毎フレーム速度へ加算する値。
	DirectX::SimpleMath::Vector2 acceleration = DirectX::SimpleMath::Vector2::Zero;
	// 生存中に線形補間する大きさ。
	float startScale = 0.4f;
	float endScale = 0.0f;
	// 生存中に線形補間する色。
	DirectX::SimpleMath::Color startColor = DirectX::SimpleMath::Color(1.0f, 0.8f, 0.2f, 0.9f);
	DirectX::SimpleMath::Color endColor = DirectX::SimpleMath::Color(1.0f, 0.2f, 0.05f, 0.0f);
	// 粒子ごとの回転速度範囲。単位は degree / frame。
	float angularVelocityMin = -10.0f;
	float angularVelocityMax = 10.0f;
};

/// <summary>
/// assets/EffectData 配下の JSON 1 つに対応するエフェクト定義。
/// </summary>
struct EffectData
{
	std::string effectDataId;
	std::string displayName;
	int totalFrames = 20;
	bool loop = false;
	std::vector<EffectEmitterData> emitters;
};
