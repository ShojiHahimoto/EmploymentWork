#pragma once

#include <SimpleMath.h>

namespace GameUI
{
	/// <summary>
	/// 画面間で共有する色と余白の基準値。
	/// </summary>
	struct Theme
	{
		static constexpr float BaseSpacing = 8.0f;
		static constexpr float PanelPadding = 16.0f;
		static constexpr float BorderThickness = 1.0f;

		static const DirectX::SimpleMath::Color Background; // 画面全体の基調色。
		static const DirectX::SimpleMath::Color Surface; // パネルと通常ボタンの面。
		static const DirectX::SimpleMath::Color Border; // 通常状態の境界線。
		static const DirectX::SimpleMath::Color Accent; // 選択や操作位置を示す強調色。
		static const DirectX::SimpleMath::Color Hover; // ポインターが重なった状態。
		static const DirectX::SimpleMath::Color Pressed; // 押下中の状態。
		static const DirectX::SimpleMath::Color Disabled; // 操作できない状態。
		static const DirectX::SimpleMath::Color Focus; // キーボードフォーカスの境界線。
	};
}
