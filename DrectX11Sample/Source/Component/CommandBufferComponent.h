#pragma once

#include "Component/Component.h"

#include <array>
#include <string>

/// <summary>
/// 入力履歴から成立したコマンドを、行動可能になるまで一時的に保持する。
/// </summary>
struct BufferedCommandInput
{
	// 実行する CharacterAttackDataComponent の slotId。空文字なら未使用。
	std::string attackSlotId;

	// コマンド成立フレーム。新しい入力を優先するための比較に使う。
	int commandAcceptedFrame = 0;

	// このフレーム番号を過ぎると先行入力として無効になる。
	int bufferExpireFrame = 0;

	// 同一フレーム内で複数候補がある場合の優先度。必殺技などを通常技より高くする。
	int priority = 0;

	bool valid = false;
};

struct CommandBufferComponent : public Component
{
	// 同一フレームに通常技とコマンド技が複数成立しても、優先度で選べるよう少し余裕を持たせる。
	static constexpr int MaxBufferedCommands = 8;

	std::array<BufferedCommandInput, MaxBufferedCommands> commands = {};
};
