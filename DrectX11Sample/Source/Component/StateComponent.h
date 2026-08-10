#pragma once

#include "Component/Component.h"

enum class PlayerActionState
{
	Idle,
	FrontWalk,
	BackWalk,
	VerticalJumpStartup,
	FrontJumpStartup,
	BackJumpStartup,
	VerticalJump,
	FrontJump,
	BackJump,
	Fall,
	GroundAttack,
	AirAttack,
	LandingRecovery,
	Hitstun,
	Guardstun,
	AirHitstun,
	Down,
	WakeUp,
};

// 対面方向
enum class FacingDirection
{
	Right,
	Left
};

struct StateComponent : public Component
{
	// StateUpdateSystem が確定した、今フレームの最終行動。
	PlayerActionState currentActionState = PlayerActionState::Idle;

	// プレイヤーの対面方向。
	FacingDirection facingDirection = FacingDirection::Right;

	// 現在の ActionState に入った瞬間の対面方向。
	// ジャンプなど、途中で向きが変わると困る行動の基準方向として使う。
	FacingDirection actionStartFacingDirection = FacingDirection::Right;

	// currentActionState に入ってからの経過フレーム。
	// StateUpdateSystem が State 遷移と合わせて更新する。
	int actionFrame = 0;

	// 現在の ActionState を何フレーム維持するか。
	// 攻撃開始時は StateUpdateSystem が AttackData から計算して保存する。
	int actionDurationFrames = 0;

	// 接地、被弾、キャンセルなどの判定材料。
	// 今後 GroundSystem / HitResolveSystem / AttackSystem が更新する想定。
	bool isGrounded = true;
	bool hitstunRequested = false;
	// 現在の Hitstun を何フレーム維持するか。HitResolveSystem が AttackData から設定する。
	int hitstunDurationFrames = 30;
	// 現在の Guardstun を何フレーム維持するか。HitResolveSystem が AttackData から設定する。
	int guardstunDurationFrames = 30;
	bool cancelEnabled = false;
};
