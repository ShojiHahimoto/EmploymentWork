#include "System/EffectSystem.h"

#include "Component/EffectComponent.h"
#include "Component/TransformComponent.h"
#include "Core/GameObject.h"
#include "System/TransformSystem.h"
#include "World/World.h"

#include <algorithm>
#include <cstdint>

using namespace DirectX::SimpleMath;

namespace
{
	/// <summary>
	/// 乱数エンジンを持たず、同じ入力から 0.0f - 1.0f の擬似乱数を作る。
	/// </summary>
	/// <param name="seed">粒子ごとに変えるシード値。</param>
	/// <returns>0.0f 以上 1.0f 以下の値。</returns>
	float Hash01(uint32_t seed)
	{
		seed ^= seed << 13;
		seed ^= seed >> 17;
		seed ^= seed << 5;
		return static_cast<float>(seed & 0x00ffffff) / static_cast<float>(0x00ffffff);
	}

	/// <summary>
	/// min と max の間を補間する。
	/// </summary>
	/// <param name="minValue">最小値。</param>
	/// <param name="maxValue">最大値。</param>
	/// <param name="rate">0.0f - 1.0f の補間率。</param>
	/// <returns>補間後の値。</returns>
	float LerpFloat(float minValue, float maxValue, float rate)
	{
		return minValue + (maxValue - minValue) * rate;
	}

	/// <summary>
	/// エミッター設定から粒子を生成し、EffectComponent に追加する。
	/// </summary>
	/// <param name="objectId">エフェクト GameObject ID。乱数シードに使う。</param>
	/// <param name="basePosition">エフェクト発生元のワールド座標。</param>
	/// <param name="effect">粒子を追加する EffectComponent。</param>
	/// <param name="emitter">粒子生成設定。</param>
	/// <param name="emitterIndex">エミッター番号。乱数シードに使う。</param>
	void EmitParticles(
		GameObjectId objectId,
		const Vector3& basePosition,
		EffectComponent& effect,
		const EffectEmitterData& emitter,
		size_t emitterIndex)
	{
		for (int particleIndex = 0; particleIndex < emitter.emitCount; ++particleIndex)
		{
			const uint32_t seedBase =
				static_cast<uint32_t>(objectId * 73856093u)
				^ static_cast<uint32_t>((emitterIndex + 1) * 19349663u)
				^ static_cast<uint32_t>((particleIndex + 1) * 83492791u);

			const float spawnRateX = Hash01(seedBase + 1);
			const float spawnRateY = Hash01(seedBase + 2);
			const float velocityRateX = Hash01(seedBase + 3);
			const float velocityRateY = Hash01(seedBase + 4);
			const float rotationRate = Hash01(seedBase + 5);
			const float angularRate = Hash01(seedBase + 6);

			const Vector2 randomSpawn(
				LerpFloat(-emitter.spawnRange.x, emitter.spawnRange.x, spawnRateX),
				LerpFloat(-emitter.spawnRange.y, emitter.spawnRange.y, spawnRateY));
			const Vector2 velocity(
				LerpFloat(emitter.velocityMin.x, emitter.velocityMax.x, velocityRateX) * effect.facingSign,
				LerpFloat(emitter.velocityMin.y, emitter.velocityMax.y, velocityRateY));

			EffectParticleRuntime particle;
			particle.position = basePosition + Vector3(
				(emitter.baseOffset.x + randomSpawn.x) * effect.facingSign,
				emitter.baseOffset.y + randomSpawn.y,
				emitter.baseOffset.z);
			particle.velocity = velocity;
			particle.acceleration = emitter.acceleration;
			particle.startScale = emitter.startScale;
			particle.endScale = emitter.endScale;
			particle.rotationDegrees = LerpFloat(0.0f, 360.0f, rotationRate);
			particle.angularVelocityDegrees = LerpFloat(
				emitter.angularVelocityMin,
				emitter.angularVelocityMax,
				angularRate);
			particle.lifeFrames = std::max(1, emitter.particleLifeFrames);
			particle.startColor = emitter.startColor;
			particle.endColor = emitter.endColor;
			particle.texturePath = emitter.texturePath;
			particle.blendMode = emitter.blendMode;
			particle.depthEnabled = emitter.depthEnabled;
			effect.particles.push_back(particle);
		}
	}

	/// <summary>
	/// 粒子の位置、速度、寿命を 1 フレーム進める。
	/// </summary>
	/// <param name="particle">更新する粒子。</param>
	void UpdateParticle(EffectParticleRuntime& particle)
	{
		if (!particle.alive)
		{
			return;
		}

		particle.position.x += particle.velocity.x;
		particle.position.y += particle.velocity.y;
		particle.velocity += particle.acceleration;
		particle.rotationDegrees += particle.angularVelocityDegrees;
		++particle.ageFrames;
		if (particle.ageFrames >= particle.lifeFrames)
		{
			particle.alive = false;
		}
	}

	/// <summary>
	/// 再生終了済みで、生存粒子も残っていないか確認する。
	/// </summary>
	/// <param name="effect">確認する EffectComponent。</param>
	/// <returns>削除してよい場合は true。</returns>
	bool ShouldDestroyEffect(const EffectComponent& effect)
	{
		if (effect.effectData.loop || effect.currentFrame < effect.effectData.totalFrames)
		{
			return false;
		}

		for (const EffectParticleRuntime& particle : effect.particles)
		{
			if (particle.alive)
			{
				return false;
			}
		}

		return true;
	}
}

void EffectSystem::Update(World& world)
{
	for (GameObject& object : world.GetGameObjects())
	{
		if (object.tag != GameObjectTag::Effect)
		{
			continue;
		}

		EffectComponent* effect = world.GetComponent<EffectComponent>(object.id);
		const TransformComponent* transform = world.GetTransform(object.id);
		if (!effect || !transform || !effect->playing)
		{
			continue;
		}

		// ワールド固定エフェクトは生成リクエスト時点の座標を使う。
		// 生成直後は Transform の world cache がまだ更新前の可能性があるため、GetWorldPosition へ依存しない。
		const Vector3 basePosition = effect->followTarget
			? TransformSystem::GetWorldPosition(*transform)
			: effect->spawnWorldPosition;
		for (size_t emitterIndex = 0; emitterIndex < effect->effectData.emitters.size(); ++emitterIndex)
		{
			if (emitterIndex >= effect->emittedEmitters.size())
			{
				effect->emittedEmitters.resize(effect->effectData.emitters.size(), false);
			}

			const EffectEmitterData& emitter = effect->effectData.emitters[emitterIndex];
			if (!effect->emittedEmitters[emitterIndex] && effect->currentFrame >= emitter.startFrame)
			{
				EmitParticles(object.id, basePosition, *effect, emitter, emitterIndex);
				effect->emittedEmitters[emitterIndex] = true;
			}
		}

		for (EffectParticleRuntime& particle : effect->particles)
		{
			UpdateParticle(particle);
		}

		++effect->currentFrame;
		if (effect->effectData.loop && effect->currentFrame >= effect->effectData.totalFrames)
		{
			effect->currentFrame = 0;
			std::fill(effect->emittedEmitters.begin(), effect->emittedEmitters.end(), false);
		}

		if (ShouldDestroyEffect(*effect))
		{
			world.RequestDestroy(object.id);
		}
	}
}
