#pragma once

#include <SimpleMath.h>

#include <algorithm>
#include <string>
#include <vector>

enum class AttackCancelType
{
	Normal,
	Special,
	Jump,
	Unknown
};

enum class AttackKind
{
	Normal,
	Special,
	Unknown
};

enum class AttackCommandId
{
	None,
	Hadouken,
	Shoryuu,
	Yoga,
	ReverseYoga,
	FullRotate,
	Unknown
};

enum class AttackUsableState
{
	Ground,
	Air,
	Unknown
};

enum class HitReactionType
{
	Normal,
	Down,
	Burst,
	HardBurst,
	Unknown
};

enum class AttackSlotType
{
	Normal,
	Special,
	Unknown
};

enum class AttackButtonId
{
	None,
	AttackA,
	AttackB,
	AttackX,
	AttackY,
	Unknown
};

/// <summary>
/// 技の発生フレーム番号、持続フレーム数、攻撃終了後の硬直フレーム数を保持する。
/// </summary>
struct AttackFrameData
{
	// 攻撃ボタンを押したフレームを 1F とした時、何フレーム目から攻撃判定が出るか。最速は 2F。
	int startup = 2;
	// 攻撃判定が出続けるフレーム数。
	int active = 0;
	// 攻撃判定が消えた後、操作不能として残るフレーム数。
	int recovery = 0;
};

/// <summary>
/// AttackFrameData::startup を、0 始まりの actionFrame 上の攻撃判定開始フレームへ変換する。
/// </summary>
/// <param name="frame">変換する技フレーム情報。</param>
/// <returns>内部 actionFrame 上の active 開始フレーム。</returns>
inline int GetAttackActiveStartFrame(const AttackFrameData& frame)
{
	return std::max(2, frame.startup) - 1;
}

/// <summary>
/// AttackFrameData::active を負数にならない持続フレーム数として取得する。
/// </summary>
/// <param name="frame">確認する技フレーム情報。</param>
/// <returns>0 以上の active フレーム数。</returns>
inline int GetAttackActiveFrameCount(const AttackFrameData& frame)
{
	return std::max(0, frame.active);
}

/// <summary>
/// 攻撃判定が終了する内部 actionFrame を、終端を含まない形で取得する。
/// </summary>
/// <param name="frame">確認する技フレーム情報。</param>
/// <returns>active 終了直後の actionFrame。</returns>
inline int GetAttackActiveEndFrameExclusive(const AttackFrameData& frame)
{
	return GetAttackActiveStartFrame(frame) + GetAttackActiveFrameCount(frame);
}

/// <summary>
/// 技全体の操作不能フレーム数を取得する。
/// </summary>
/// <param name="frame">確認する技フレーム情報。</param>
/// <returns>active 開始前、active、recovery を合計した最低 1F の総フレーム数。</returns>
inline int GetAttackTotalFrames(const AttackFrameData& frame)
{
	return std::max(1, GetAttackActiveEndFrameExclusive(frame) + std::max(0, frame.recovery));
}

/// <summary>
/// 指定 actionFrame が攻撃判定の active 範囲内か確認する。
/// </summary>
/// <param name="frame">確認する技フレーム情報。</param>
/// <param name="actionFrame">攻撃ボタンを押したフレームを 0 とする内部経過フレーム。</param>
/// <returns>active 中なら true。</returns>
inline bool IsAttackFrameActive(const AttackFrameData& frame, int actionFrame)
{
	return GetAttackActiveFrameCount(frame) > 0
		&& actionFrame >= GetAttackActiveStartFrame(frame)
		&& actionFrame < GetAttackActiveEndFrameExclusive(frame);
}

/// <summary>
/// 特定フレーム範囲で許可されるキャンセル設定を保持する。
/// </summary>
struct AttackCancelSettingData
{
	int startFrame = 0;
	int endFrame = 0;
	std::vector<AttackCancelType> cancelTypes;
};

/// <summary>
/// 技が生成する 2D 当たり判定の形状を保持する。
/// 発生タイミングは AttackData::frame から GetAttackActiveStartFrame / IsAttackFrameActive で決定する。
/// </summary>
struct AttackHitboxData
{
	DirectX::SimpleMath::Vector2 offset = DirectX::SimpleMath::Vector2::Zero;
	DirectX::SimpleMath::Vector2 size = DirectX::SimpleMath::Vector2::Zero;
};

/// <summary>
/// assets/AttackData 配下の JSON 1 つに対応する技データ。
/// </summary>
struct AttackData
{
	// 保存ファイル名と対応する技 ID。
	std::string attackDataId;
	// 調整画面やデバッグ表示で使う表示名。
	std::string displayName;
	// ヒット時に相手 HP から減算する攻撃力。
	int damage = 10;
	// この攻撃がヒットした相手を Hitstun に固定するフレーム数。
	int hitstunFrames = 30;
	// この攻撃がガードされた相手を Guardstun に固定するフレーム数。
	int guardstunFrames = 30;
	// 通常攻撃か必殺技か。入力候補の作成と編集画面で使う。
	AttackKind attackKind = AttackKind::Normal;
	// 必殺技用のコマンド種別。通常攻撃では None を使う。
	AttackCommandId commandId = AttackCommandId::None;
	// Ground / Air のどちらで発動できるか。
	AttackUsableState usableState = AttackUsableState::Ground;
	// ヒット時にどの被弾反応を起こすか。細かい距離や速度は HitReactionSystem 側の共通設定で扱う。
	HitReactionType hitReactionType = HitReactionType::Normal;
	AttackFrameData frame;
	// false の場合、cancelSetting の中身は保持するが対戦中のキャンセル判定には使わない。
	bool canAttackCancel = false;
	AttackCancelSettingData cancelSetting;
	std::vector<AttackHitboxData> hitboxes;
};

/// <summary>
/// キャラクターの技スロットと、参照する AttackData ID の対応だけを保持する。
/// </summary>
struct CharacterAttackSlotData
{
	std::string slotId;
	std::string attackDataId;
	AttackSlotType slotType = AttackSlotType::Normal;
	AttackButtonId button = AttackButtonId::None;
	// groundAttackSlots / airAttackSlots など、スロット側が要求する発動可能状態。
	AttackUsableState slotUsableState = AttackUsableState::Ground;
};

/// <summary>
/// 対戦中に参照しやすいよう、slotId と読み込み済み AttackData をまとめたデータ。
/// </summary>
struct CharacterAssignedAttackData
{
	std::string slotId;
	AttackSlotType slotType = AttackSlotType::Normal;
	AttackButtonId button = AttackButtonId::None;
	// キャラクター側スロットが要求した地上/空中種別。読み込み検証後もデバッグ確認用に保持する。
	AttackUsableState slotUsableState = AttackUsableState::Ground;
	AttackData attack;
};
