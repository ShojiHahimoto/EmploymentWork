#pragma once

#include "Component/Component.h"

#include <SimpleMath.h>

#include <string>

/// <summary>
/// 2D AABB 判定に使う矩形情報。
/// </summary>
struct HitBoxRect2D
{
	// Transform の位置を基準にした中心オフセット。FacingDirection に応じて X だけ反転して使う。
	DirectX::SimpleMath::Vector2 offset = DirectX::SimpleMath::Vector2::Zero;
	// AABB の横幅と縦幅。0 以下の場合は判定やデバッグ描画の対象外にする。
	DirectX::SimpleMath::Vector2 size = DirectX::SimpleMath::Vector2::One;
	bool enabled = true;
};

/// <summary>
/// 現在実行中の攻撃スロットと、この攻撃中に相手へヒット済みかを保持する。
/// </summary>
struct CurrentAttackData
{
	// CharacterAttackDataComponent の slotId と対応する。空文字なら現在攻撃スロットなし。
	std::string slotId;
	// 1vs1 前提なので、同じ攻撃中の多段ヒット防止は相手 ID ではなく bool で管理する。
	bool hasHit = false;
};

/// <summary>
/// プレイヤーの押し合い、被弾、現在攻撃情報をまとめて保持する Component。
/// </summary>
struct HitBoxComponent : public Component
{
	// プレイヤー同士や壁との押し合い、めり込み解消に使う矩形。
	HitBoxRect2D pushBox;
	// 攻撃を受ける側の当たり判定。しゃがみや姿勢変更時はここを書き換える。
	HitBoxRect2D hurtBox;
	// 攻撃中に参照するスロット情報と、この攻撃が既に当たったかどうか。
	CurrentAttackData currentAttack;
};
