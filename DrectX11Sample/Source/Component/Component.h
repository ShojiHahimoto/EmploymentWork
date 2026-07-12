#pragma once

struct Component
{
	// 異なる Component 型を GameObject の同一配列で保持するための共通基底。
	// Component 自体にゲームロジックは持たせない。
	virtual ~Component() = default;
};
