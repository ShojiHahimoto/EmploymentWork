#pragma once

#include <Windows.h>

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

class Renderer;

namespace GameUI
{
	/// <summary>
	/// UI の画面矩形。座標はクライアント領域の左上を原点とする。
	/// </summary>
	struct Rect
	{
		int left = 0; // 左端を含む。
		int top = 0; // 上端を含む。
		int right = 0; // 右端を含まない。
		int bottom = 0; // 下端を含まない。

		bool IsEmpty() const;
		bool Contains(POINT point) const;
		Rect Intersect(const Rect& other) const;
		std::vector<Rect> Subtract(const Rect& other) const;
		Rect Inset(int horizontal, int vertical) const;
		RECT ToNativeRect() const;
	};

	/// <summary>
	/// 1 フレーム分の UI 入力。ゲーム入力との競合を避けるため、入力元は UI の外で選別する。
	/// </summary>
	enum class Key : size_t { Backspace, Delete, Left, Right, Home, End, Up, Down, Enter, Escape, Count };
	struct InputFrame
	{
		POINT pointerPosition = {}; // クライアント領域でのポインター位置。
		POINT pointerDelta = {}; // 前回更新からのポインター移動量。ビューポート操作でも共有する。
		bool pointerPressed = false; // 押下されたフレームだけ true。
		bool pointerDown = false; // 押下状態の間 true。
		bool pointerReleased = false; // 離されたフレームだけ true。
		bool middlePointerPressed = false; // 中ボタン押下エッジ。
		bool middlePointerDown = false; // 中ボタン押下状態。
		bool middlePointerReleased = false; // 中ボタン解放エッジ。
		bool rightPointerPressed = false; // 右ボタン押下エッジ。
		bool rightPointerDown = false; // 右ボタン押下状態。
		bool rightPointerReleased = false; // 右ボタン解放エッジ。
		int wheelDelta = 0; // Windows標準の120単位を基準とするホイール差分。
		int focusMove = 0; // 次へは正、前へは負のフォーカス移動数。
		bool activatePressed = false; // 決定入力の押下エッジ。
		bool cancelPressed = false; // 取消入力の押下エッジ。
		std::wstring textInput; // WM_CHARで確定した文字。UTF-16なのでIME確定文字を保持できる。
		std::wstring compositionText; // IME変換中の未確定文字。
		std::array<bool, static_cast<size_t>(Key::Count)> keysPressed = {};
		bool focusLost = false;
		bool composing = false;
	};


	/// <summary>
	/// 親矩形から余白と間隔を引いた、均等な縦並びの配置を作る。
	/// </summary>
	class Layout
	{
	public:
		static std::vector<Rect> VerticalStack(
			const Rect& parent,
			int itemCount,
			int itemHeight,
			int gap,
			int horizontalPadding,
			int verticalPadding);
	};

	/// <summary>
	/// UI 部品の配置、入力フォーカス、ポインター捕捉、基本描画を一括管理する。
	/// </summary>
	class Root
	{
	public:
		/// <summary>編集履歴へ渡す入力の区切り。</summary>
		struct EditInteraction { std::string id; bool ongoing = false; bool wheel = false; };
		EditInteraction GetEditInteraction() const;
		/// <summary>展開した項目を、別項目へのフォーカス移動で閉じるか判定する。</summary>
		/// <param name="prefix">展開項目とプルダウン候補に共通のID接頭辞。</param>
		/// <param name="section">展開項目の画面上の範囲。</param>
		/// <param name="auxiliary">適用部位の選択など、開いたまま操作できる補助領域。</param>
		/// <returns>別項目への移動やフォーカス喪失でtrue。</returns>
		bool ShouldCollapseInlineSection(std::string_view prefix, const Rect& section, const Rect& auxiliary = {}) const;
		void BeginFrame(const Rect& viewport, const InputFrame& input);
		void AddPanel(const Rect& bounds, std::string_view imageId = "panel");
		void AddRule(const Rect& bounds);
		void AddSectionHeader(std::string_view id, const Rect& bounds, std::string_view text, float fontSize = 18.0f);
		void AddImage(std::string_view id, const Rect& bounds, std::string_view imageId, float opacity = 1.0f);
		void AddNineSliceImage(std::string_view id, const Rect& bounds, std::string_view imageId, float opacity = 1.0f);
		void AddButton(std::string_view id, const Rect& bounds, bool enabled = true);
		void AddLabel(std::string_view id, const Rect& bounds, std::string_view text, bool enabled = true,
			float fontSize = 18.0f, bool centered = false, bool bold = false);
		void AddButton(std::string_view id, const Rect& bounds, std::string_view text,
			bool enabled = true, float fontSize = 18.0f);
		void AddPrimaryButton(std::string_view id, const Rect& bounds, std::string_view text, bool enabled = true);
		void AddImageButton(std::string_view id, const Rect& bounds, std::string_view imageId,
			std::string_view fallbackText, bool enabled = true, bool showFocusOutline = true);
		void AddIconButton(std::string_view id, const Rect& bounds, std::string_view text,
			std::string_view iconImageId, bool enabled = true, bool showFocusOutline = true, bool primary = false, float fontSize = 18.0f);
		void AddButton(std::string_view id, const Rect& bounds, const char* text,
			bool enabled = true, float fontSize = 18.0f);
		void AddTextInput(std::string_view id, const Rect& bounds, std::string& value, size_t maxUtf8Bytes, bool enabled = true, bool bold = false);
		void AddIntegerInput(std::string_view id, const Rect& bounds, int& value, int minimum, int maximum, int step = 1, bool enabled = true);
		void AddFloatInput(std::string_view id, const Rect& bounds, float& value, float minimum, float maximum, float step = 0.1f, int precision = 2, bool enabled = true);
		void AddCheckbox(std::string_view id, const Rect& bounds, std::string_view text, bool& checked, bool enabled = true, float fontSize = 18.0f);
		void AddDropdown(std::string_view id, const Rect& bounds, const std::vector<std::string>& options, int& selectedIndex, bool enabled = true);
		void BeginScrollArea(std::string_view id, const Rect& bounds, int contentHeight, int& scrollOffset);
		void EndScrollArea();
		void EndFrame();
		void Draw(Renderer& renderer) const;

		bool WasActivated(std::string_view id) const;
		bool WasChanged(std::string_view id) const;
		bool WantsPointerCapture() const;
		bool WantsKeyboardCapture() const;
		bool WantsTextInput() const;
		const std::string& GetFocusedId() const;

	private:
		enum class ElementType
		{
			Rule,
			Panel,
			Label,
			Button,
			TextInput,
			IntegerInput,
			FloatInput,
			Checkbox,
			Dropdown,
			DropdownOption,
			ScrollTrack,
			ScrollThumb,
			Image,
			NineSliceImage
		};

		struct Element
		{
			bool centered = false; // ラベルを矩形の中央へ揃える。
			bool bold = false; // 強調表示用の太字。
			bool showFocusOutline = true; // falseならフォーカス中の枠線だけを隠す。
			ElementType type = ElementType::Panel; // 描画・入力の種別。
			Rect bounds; // クライアント座標上の配置。
			std::string id; // フレームをまたいで状態を維持する識別子。
			std::string label; // UTF-8表示文字列。
			std::string imageId; // 画像描画時に参照する画像アセット名。
			std::string iconImageId; // ボタン内に重ねる透過アイコン。
			float opacity = 1.0f; // 画像部品の全体不透明度。
			bool stretchBackground = true; // falseなら背景素材全体を画像として描く。
			std::wstring displayText; // 描画時に使うUTF-16表示文字列。
			float fontSize = 18.0f; // 描画時の文字サイズ。
			bool enabled = true; // false は hit test とフォーカス対象から除外。
			bool hovered = false; // 現フレームでポインターが重なっている状態。
			bool activated = false; // 現フレームの押下確定結果。
			bool changed = false; // 現フレームに値が変更された状態。
			bool* booleanValue = nullptr; // Checkboxの外部データ。フレーム内だけ参照する。
			std::string* textValue = nullptr; // TextInputの外部データ。フレーム内だけ参照する。
			int* integerValue = nullptr; // 整数欄の外部データ。
			float* floatValue = nullptr; // 小数欄の外部データ。
			int* selectedIndex = nullptr; // Dropdownの選択番号。
			std::vector<std::string> options; // 登録時点の候補をフレーム内に保持する。
			int optionIndex = -1; // DropdownOptionが選択する候補番号。
			size_t maxUtf8Bytes = 0; // 文字入力の保存先容量。
			int integerMinimum = 0; // 数値欄の下限。
			int integerMaximum = 0; // 数値欄の上限。
			float floatMinimum = 0.0f; // 小数欄の下限。
			float floatMaximum = 0.0f; // 小数欄の上限。
			float numericStep = 1.0f; // 数値欄の補助操作幅。
			int precision = 2; // 小数欄の表示桁数。
			Rect clipBounds; // 親パネルまたはスクロール領域によるクリップ。
		};

		struct TextEditState
		{
			std::wstring text; // 編集中のUTF-16文字列。
			std::string lastWrittenUtf8; // 外部値の再読込で編集中の文字を消さないための比較値。
			size_t cursor = 0; // UTF-16上のカーソル位置。
			bool initialized = false; // 初回の外部値読み込み済みか。
			std::string original;
		};

		struct ScrollContext
		{
			std::string id; // スクロールバー表示と領域特定に使うID。
			Rect bounds; // 内容を表示する画面領域。
			Rect parentClip; // 外側のクリップ領域。
			int contentHeight = 0; // スクロール対象全体の高さ。
			int* scrollOffset = nullptr; // 現フレームのスクロール位置。
			int offset = 0; // 子要素の座標変換に使う確定位置。
		};

		Element* FindButton(const std::string& id);
		const Element* FindButton(const std::string& id) const;
		Element* FindElement(const std::string& id);
		const Element* FindElement(const std::string& id) const;
		Element* FindTopButtonAt(POINT point);
		const Element* FindTopButtonAt(POINT point) const;
		Element* FindTopInteractiveAt(POINT point);
		void RegisterElement(Element element, bool applyScrollTransform = true);
		void ActivateElement(Element& element);
		void ApplyFocusedInput();
		void UpdateNumericElement(Element& element, bool commit);
		void UpdateTextElement(Element& element);
		bool IsFocusable(const Element& element) const;
		void EnsureValidFocus();

		Rect viewport; // hit test と描画を制限する範囲。
		InputFrame input; // BeginFrame で受け取った入力状態。
		std::vector<Element> elements; // 登録順が描画順とフォーカス順になる。
		std::vector<ScrollContext> scrollStack; // 入れ子スクロール領域の座標変換。
		std::unordered_map<std::string, TextEditState> textEditStates; // フレームをまたぐ文字入力の編集位置。
		std::unordered_map<std::string, int> dropdownScrollOffsets; // 長い候補一覧の表示位置。
		std::string wheelEditId; // 同じ数値欄への連続ホイールを文字入力と区別する。
		std::string focusedId; // キーボード操作対象の安定ID。
		std::string capturedId; // 押し始めたボタン。外へドラッグしても所有を維持する。
		std::string expandedDropdownId; // 現在開いている選択リスト。
		bool keyboardFocusActive = false; // UIが明示的にキーボード操作を受けたか。
		bool wheelConsumed = false; // 同じホイール差分を複数のスクロール領域へ重複適用しない。
		bool endedFrame = false; // 入力解決後のみ描画・結果照会を許可する。
	};
}
