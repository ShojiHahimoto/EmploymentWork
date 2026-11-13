#pragma once

#include <SimpleMath.h>

#include <string_view>

namespace GameUI
{
	/// <summary>
	/// 画面間で共有する色と余白の基準値。
	/// </summary>
	struct Theme
	{
		static constexpr std::string_view PanelImage = "panel";
		static constexpr std::string_view HeaderImage = "header";
		static constexpr std::string_view PlaybackPanelImage = "playback_panel";
		static constexpr std::string_view LabelBackgroundImage = "label_background";
		static constexpr std::string_view SectionAccentImage = "section_accent";
		static constexpr std::string_view HeaderAccentImage = "header_accent";
		static constexpr std::string_view ButtonImage = "button";
		static constexpr std::string_view PrimaryButtonImage = "button_primary";
		static constexpr std::string_view TextFieldImage = "panel";
		static constexpr std::string_view NumericFieldImage = "number_field";
		static constexpr std::string_view DropdownArrowImage = "down";
		static constexpr std::string_view CheckboxOffImage = "checkbox_off";
		static constexpr std::string_view CheckboxOnImage = "checkbox_on";

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
		static const DirectX::SimpleMath::Color HoverOverlay; // 素材の上へ重ねるホバー色。
		static const DirectX::SimpleMath::Color PressedOverlay; // 素材の上へ重ねる押下色。
		static const DirectX::SimpleMath::Color Text; // 通常状態の文字色。
		static const DirectX::SimpleMath::Color PrimaryText; // 強調ボタン上の文字色。
		static const DirectX::SimpleMath::Color DisabledText; // 無効状態の文字色。
	};
}
