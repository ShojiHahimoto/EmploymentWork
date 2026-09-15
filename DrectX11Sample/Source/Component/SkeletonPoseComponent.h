#pragma once

#include "Component/Component.h"
#include "Data/SkeletonPose.h"

/// <summary>
/// 対戦オブジェクトの姿勢とデバッグ再生情報を保持する。
/// </summary>
struct SkeletonPoseComponent : public Component, public SkeletonPose
{
	// スキニング確認用の仮ポーズを有効にする。将来の MotionData 再生が入ったら削除または Debug 専用に畳む。
	bool enableDebugPose = false;

	// Debug ポーズをフレームベースで動かすためのカウンタ。
	int debugPoseFrame = 0;
};
