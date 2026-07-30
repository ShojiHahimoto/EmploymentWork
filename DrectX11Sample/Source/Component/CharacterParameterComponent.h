#pragma once

#include "Component/Component.h"
#include "Data/CharacterData.h"

/// <summary>
/// 対戦中の System が参照するキャラクター基本パラメータ。
/// </summary>
struct CharacterParameterComponent : public Component
{
	// JSON から Spawn 時にコピーされた、この GameObject 専用のパラメータ。
	CharacterParameterData parameter;
};
