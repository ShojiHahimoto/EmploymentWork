#include "System/BattleResultSystem.h"

#include "Component/BattleTimerComponent.h"
#include "Component/HealthComponent.h"
#include "Core/GameObject.h"
#include "Core/World.h"
#include "Data/JsonValue.h"

#include <algorithm>
#include <fstream>
#include <iterator>

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

/// <summary>演出時間をJSONから読み、未指定や不正な値には既定時間を使う。</summary>
/// <returns>固定フレーム単位の演出時間。</returns>
BattleRoundTiming BattleResultSystem::LoadTiming()
{
	BattleRoundTiming timing;
	std::ifstream file("assets/BattleConfig/RoundTiming.json", std::ios::binary);
	std::string content{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
	if (content.starts_with("\xEF\xBB\xBF"))
	{
		content.erase(0, 3);
	}

	JsonValue root;
	std::string error;
	if (!JsonParser::Parse(content, root, error))
	{
		return timing;
	}

	const auto read = [&root](const char* key, int fallback)
	{
		const auto* value = root.Find(key);
		const double frames = value ? value->AsNumber(fallback) : fallback;
		return frames >= 0 && frames <= 36000 ? static_cast<int>(frames) : fallback;
	};

	timing.roundIntroDelayFrames = read("roundIntroDelayFrames", timing.roundIntroDelayFrames);
	timing.roundDisplayFrames = read("roundDisplayFrames", timing.roundDisplayFrames);
	timing.roundToFightFrames = read("roundToFightFrames", timing.roundToFightFrames);
	timing.fightDisplayFrames = read("fightDisplayFrames", timing.fightDisplayFrames);
	timing.fightToControlFrames = read("fightToControlFrames", timing.fightToControlFrames);
	timing.roundEndDisplayDelayFrames = read("roundEndDisplayDelayFrames", timing.roundEndDisplayDelayFrames);
	timing.roundEndDisplayFrames = read("roundEndDisplayFrames", timing.roundEndDisplayFrames);
	timing.roundEndAfterDisplayFrames = read("roundEndAfterDisplayFrames", timing.roundEndAfterDisplayFrames);
	timing.releaseToVictoryFrames = read("releaseToVictoryFrames", timing.releaseToVictoryFrames);
	timing.victoryMotionFrames = read("victoryMotionFrames", timing.victoryMotionFrames);
	timing.matchResultDisplayFrames = read("matchResultDisplayFrames", timing.matchResultDisplayFrames);
	timing.doubleKnockoutDownFrames = read("doubleKnockoutDownFrames", timing.doubleKnockoutDownFrames);
	timing.fadeOutFrames = read("fadeOutFrames", timing.fadeOutFrames);
	timing.fadeInFrames = read("fadeInFrames", timing.fadeInFrames);
	return timing;
}

/// <summary>前ラウンドの決着理由を消し、タイマーを停止した状態で開始待ちに入る。</summary>
/// <param name="world">ラウンドタイマーを保持する World。</param>
/// <param name="round">得点を保持したまま開始待ちに切り替えるラウンド状態。</param>
/// <param name="introFrames">操作解禁までのフレーム数。</param>
void BattleResultSystem::BeginRound(World& world, BattleRoundState& round, int introFrames)
{
	round.endReason = RoundEndReason::None;
	round.waitFrames = introFrames;
	FindBattleTimer(world)->isRunning = introFrames == 0;
}

/// <summary>ラウンドの待ち時間を1フレーム進め、開始待ちが終わったときだけタイマーを動かす。</summary>
/// <param name="world">ラウンドタイマーを保持する World。</param>
/// <param name="round">残り待ち時間と決着理由を保持するラウンド状態。</param>
void BattleResultSystem::AdvanceWait(World& world, BattleRoundState& round)
{
	if (round.waitFrames > 0)
	{
		--round.waitFrames;
	}

	if (round.waitFrames == 0 && round.endReason == RoundEndReason::None)
	{
		FindBattleTimer(world)->isRunning = true;
	}
}

/// <summary>両者の取得ポイントから、2ポイント先取制の試合結果を判定する。</summary>
/// <param name="round">両者の取得ポイントを保持するラウンド状態。</param>
/// <returns>試合結果。両者とも2ポイント未満なら None。</returns>
BattleResult BattleResultSystem::GetMatchResult(const BattleRoundState& round)
{
	if (round.points[0] >= 2 && round.points[1] >= 2)
	{
		return BattleResult::Draw;
	}
	if (round.points[0] >= 2)
	{
		return BattleResult::Player1Win;
	}
	if (round.points[1] >= 2)
	{
		return BattleResult::Player2Win;
	}

	return BattleResult::None;
}

/// <summary>ラウンドを判定し、決着フレームで一度だけ得点を反映する。</summary>
/// <param name="world">BattlePlayerId、HealthComponent、BattleTimerComponent を保持する World。</param>
/// <param name="round">取得ポイントと現在のラウンド状態。</param>
void BattleResultSystem::Update(World& world, BattleRoundState& round)
{
	if (world.HasBattleResult())
	{
		return;
	}

	// 同じフレームで KO とタイムアップが重なった場合は、先にダメージ結果を優先する。
	if (ResolveKnockOut(world))
	{
		round.endReason = world.GetBattleResult() == BattleResult::Draw
			? RoundEndReason::DoubleKnockOut : RoundEndReason::KnockOut;
	}
	else if (UpdateTimerAndResolveTimeUp(world))
	{
		round.endReason = RoundEndReason::TimeOver;
	}
	else
	{
		return;
	}

	const auto result = world.GetBattleResult();
	if (result == BattleResult::Draw || result == BattleResult::Player1Win)
	{
		++round.points[0];
	}
	if (result == BattleResult::Draw || result == BattleResult::Player2Win)
	{
		++round.points[1];
	}

	FindBattleTimer(world)->isRunning = false;
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
