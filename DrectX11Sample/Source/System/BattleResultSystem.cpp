#include "System/BattleResultSystem.h"

#include "Component/BattleTimerComponent.h"
#include "Component/HealthComponent.h"
#include "Core/GameObject.h"
#include "World/World.h"

#include <algorithm>

namespace
{
	/// <summary>
	/// World 内のラウンドタイマー Component を 1 つ取得する。
	/// </summary>
	/// <param name="world">検索対象の World。</param>
	/// <returns>見つかった BattleTimerComponent。存在しない場合は nullptr。</returns>
	BattleTimerComponent* FindBattleTimer(World& world)
	{
		for (GameObject& object : world.GetGameObjects())
		{
			if (BattleTimerComponent* timer = world.GetComponent<BattleTimerComponent>(object.id))
			{
				return timer;
			}
		}

		return nullptr;
	}

	/// <summary>
	/// 両プレイヤーの現在 HP を比較して勝敗を決める。
	/// </summary>
	/// <param name="world">BattlePlayerId と HealthComponent を保持する World。</param>
	/// <returns>HP から決まる勝敗。Health が不足している場合は None。</returns>
	BattleResult DecideResultByHealth(const World& world)
	{
		const GameObjectId player1Id = world.GetBattlePlayerId(0);
		const GameObjectId player2Id = world.GetBattlePlayerId(1);
		const HealthComponent* player1Health = world.GetComponent<HealthComponent>(player1Id);
		const HealthComponent* player2Health = world.GetComponent<HealthComponent>(player2Id);
		if (!player1Health || !player2Health)
		{
			return BattleResult::None;
		}

		if (player1Health->currentHp == player2Health->currentHp)
		{
			return BattleResult::Draw;
		}

		return player1Health->currentHp > player2Health->currentHp
			? BattleResult::Player1Win
			: BattleResult::Player2Win;
	}
}

void BattleResultSystem::Update(World& world)
{
	if (world.HasBattleResult())
	{
		return;
	}

	// 同じフレームで KO とタイムアップが重なった場合は、先にダメージ結果を優先する。
	if (ResolveKnockOut(world))
	{
		return;
	}

	UpdateTimerAndResolveTimeUp(world);
}

/// <summary>
/// どちらかの HP が 0 になっていれば KO として勝敗を確定する。
/// </summary>
/// <param name="world">BattlePlayerId と HealthComponent を保持する World。</param>
/// <returns>勝敗を確定した場合は true。</returns>
bool BattleResultSystem::ResolveKnockOut(World& world)
{
	const GameObjectId player1Id = world.GetBattlePlayerId(0);
	const GameObjectId player2Id = world.GetBattlePlayerId(1);
	const HealthComponent* player1Health = world.GetComponent<HealthComponent>(player1Id);
	const HealthComponent* player2Health = world.GetComponent<HealthComponent>(player2Id);
	if (!player1Health || !player2Health)
	{
		return false;
	}

	const bool player1Defeated = player1Health->currentHp <= 0;
	const bool player2Defeated = player2Health->currentHp <= 0;
	if (!player1Defeated && !player2Defeated)
	{
		return false;
	}

	if (player1Defeated && player2Defeated)
	{
		world.SetBattleResult(BattleResult::Draw);
	}
	else if (player1Defeated)
	{
		world.SetBattleResult(BattleResult::Player2Win);
	}
	else
	{
		world.SetBattleResult(BattleResult::Player1Win);
	}

	return true;
}

/// <summary>
/// ラウンドタイマーを 1 フレーム進め、0 になったら残り HP で勝敗を確定する。
/// </summary>
/// <param name="world">BattleTimerComponent と HealthComponent を保持する World。</param>
/// <returns>タイムアップで勝敗を確定した場合は true。</returns>
bool BattleResultSystem::UpdateTimerAndResolveTimeUp(World& world)
{
	BattleTimerComponent* timer = FindBattleTimer(world);
	if (!timer || !timer->isRunning)
	{
		return false;
	}

	timer->remainingFrames = std::max(0, timer->remainingFrames - 1);
	if (timer->remainingFrames > 0)
	{
		return false;
	}

	const BattleResult result = DecideResultByHealth(world);
	if (result == BattleResult::None)
	{
		return false;
	}

	world.SetBattleResult(result);
	return true;
}
