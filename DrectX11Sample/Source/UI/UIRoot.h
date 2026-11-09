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

		/// <summary>幅または高さが0以下の空矩形かを返す。</summary>
		/// <returns>描画可能な面積がなければtrue。</returns>
		bool IsEmpty() const;
		/// <summary>点が左上を含み右下を含まない矩形内にあるかを返す。</summary>
		/// <param name="point">判定するクライアント座標。</param>
		/// <returns>点が矩形内にある場合は true。</returns>
		bool Contains(POINT point) const;
		/// <summary>他の矩形と重なる領域を返す。重なりがなければ空矩形を返す。</summary>
		/// <param name="other">交差判定する矩形。</param>
		/// <returns>交差部分または空矩形。</returns>
		Rect Intersect(const Rect& other) const;
		/// <summary>指定量だけ内側へ縮めた矩形を返す。</summary>
		/// <param name="horizontal">左右それぞれから引く量。</param>
		/// <param name="vertical">上下それぞれから引く量。</param>
		/// <returns>内側へ縮めた矩形。</returns>
		Rect Inset(int horizontal, int vertical) const;
		/// <summary>Win32描画APIへ渡す矩形形式に変換する。</summary>
		/// <returns>Win32 RECT 形式の座標。</returns>
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
		/// <summary>指定数の部品を、余白・高さ・間隔を保った縦スタックとして配置する。</summary>
		/// <param name="parent">配置を収める親矩形。</param>
		/// <param name="itemCount">作成する矩形の数。</param>
		/// <param name="itemHeight">各矩形の高さ。</param>
		/// <param name="gap">隣接矩形の間隔。</param>
		/// <param name="horizontalPadding">左右の内側余白。</param>
		/// <param name="verticalPadding">先頭側の内側余白。</param>
		/// <returns>親矩形でクリップした各部品の配置。</returns>
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
		/// <summary>描画領域と入力を設定し、当フレームの部品登録を開始する。</summary>
		/// <param name="viewport">このルートが入力と描画を受け持つ画面領域。</param>
		/// <param name="input">このフレームの入力状態。</param>
		void BeginFrame(const Rect& viewport, const InputFrame& input);
		/// <summary>指定素材を使う背景パネルを登録する。素材がなければ単色面にフォールバックする。</summary>
		/// <param name="bounds">パネルの画面矩形。</param>
		/// <param name="imageId">9スライスで使う背景素材。</param>
		void AddPanel(const Rect& bounds, std::string_view imageId = "panel");
		/// <summary>入力を受け取らない区切り線または目盛りを登録する。</summary>
		/// <param name="bounds">細い矩形で指定する描画範囲。</param>
		void AddRule(const Rect& bounds);
		/// <summary>淡色背景と縦アクセントを持つセクション見出しを登録する。</summary>
		/// <param name="id">見出しの安定ID。</param>
		/// <param name="bounds">見出しの矩形。</param>
		/// <param name="text">UTF-8の見出し文字列。</param>
		/// <param name="fontSize">96 DPI基準の文字サイズ。</param>
		void AddSectionHeader(std::string_view id, const Rect& bounds, std::string_view text, float fontSize = 18.0f);
		/// <summary>透過画像を指定矩形へ表示する部品を登録する。</summary>
		/// <param name="id">描画部品の安定ID。</param>
		/// <param name="bounds">画像の表示矩形。</param>
		/// <param name="imageId">画像一覧JSONに登録された画像名。</param>
		/// <param name="opacity">表示不透明度。</param>
		void AddImage(std::string_view id, const Rect& bounds, std::string_view imageId, float opacity = 1.0f);
		/// <summary>9スライス設定付きの画像部品を登録する。</summary>
		/// <param name="id">描画部品の安定ID。</param>
		/// <param name="bounds">伸縮後の表示矩形。</param>
		/// <param name="imageId">画像一覧JSONに登録された画像名。</param>
		/// <param name="opacity">表示不透明度。</param>
		void AddNineSliceImage(std::string_view id, const Rect& bounds, std::string_view imageId, float opacity = 1.0f);
		/// <summary>操作ボタンを登録する。ID は画面内で一意にする。</summary>
		/// <param name="id">フレーム間で安定して維持する一意な識別子。</param>
		/// <param name="bounds">ボタン矩形。</param>
		/// <param name="enabled">操作可能なら true。</param>
		void AddButton(std::string_view id, const Rect& bounds, bool enabled = true);
		/// <summary>日本語を含むラベルを表示する。</summary>
		/// <param name="id">描画順と状態確認に使う任意の識別子。</param>
		/// <param name="bounds">文字を描画する矩形。</param>
		/// <param name="text">UTF-8の表示文字列。</param>
		/// <param name="enabled">無効表示にする場合はfalse。</param>
		/// <param name="fontSize">96 DPI基準の文字サイズ。</param>
		/// <param name="centered">trueなら文字を矩形の中央へ揃える。</param>
		/// <param name="bold">trueなら太字で表示する。</param>
		void AddLabel(std::string_view id, const Rect& bounds, std::string_view text, bool enabled = true,
			float fontSize = 18.0f, bool centered = false, bool bold = false);
		/// <summary>文字付きボタンを登録する。押下結果は WasActivated で照会できる。</summary>
		/// <param name="id">フレーム間で安定して維持する一意な識別子。</param>
		/// <param name="bounds">ボタン矩形。</param>
		/// <param name="text">UTF-8のボタン表示文字列。</param>
		/// <param name="enabled">操作可能ならtrue。</param>
		void AddButton(std::string_view id, const Rect& bounds, std::string_view text, bool enabled = true);
		/// <summary>強調用の色付きボタンを登録する。</summary>
		/// <param name="id">フレーム間で安定して維持する一意な識別子。</param>
		/// <param name="bounds">ボタン矩形。</param>
		/// <param name="text">UTF-8のボタン表示文字列。</param>
		/// <param name="enabled">操作可能ならtrue。</param>
		void AddPrimaryButton(std::string_view id, const Rect& bounds, std::string_view text, bool enabled = true);
		/// <summary>素材自体に背景と図柄が含まれる画像ボタンを登録する。</summary>
		/// <param name="id">フレーム間で安定して維持する一意な識別子。</param>
		/// <param name="bounds">ボタン矩形。</param>
		/// <param name="imageId">ボタン全体を描く画像素材。</param>
		/// <param name="fallbackText">画像がない場合に表示する文字列。</param>
		/// <param name="enabled">操作可能ならtrue。</param>
		/// <param name="showFocusOutline">キーボードフォーカス枠を描画するか。</param>
		void AddImageButton(std::string_view id, const Rect& bounds, std::string_view imageId,
			std::string_view fallbackText, bool enabled = true, bool showFocusOutline = true);
		/// <summary>標準ボタン背景に透過アイコンと文字を組み合わせたボタンを登録する。</summary>
		/// <param name="id">フレーム間で安定して維持する一意な識別子。</param>
		/// <param name="bounds">ボタン矩形。</param>
		/// <param name="text">UTF-8の表示文字列。</param>
		/// <param name="iconImageId">透過アイコン素材。</param>
		/// <param name="enabled">操作可能ならtrue。</param>
		void AddIconButton(std::string_view id, const Rect& bounds, std::string_view text,
			std::string_view iconImageId, bool enabled = true, bool showFocusOutline = true);
		/// <summary>文字列リテラルがbool引数へ誤変換されないよう文字付きボタンへ転送する。</summary>
		/// <param name="id">一意な部品ID。</param>
		/// <param name="bounds">ボタン矩形。</param>
		/// <param name="text">UTF-8表示文字列。</param>
		/// <param name="enabled">操作可否。</param>
		void AddButton(std::string_view id, const Rect& bounds, const char* text, bool enabled = true)
		{
			AddButton(id, bounds, std::string_view(text ? text : ""), enabled);
		}
		/// <summary>IME文字入力、カーソル移動、削除に対応するテキスト欄を登録する。</summary>
		/// <param name="id">編集状態を保持する一意な識別子。</param>
		/// <param name="bounds">入力欄の画面矩形。</param>
		/// <param name="value">UTF-8文字列。変更はこの参照へ即時反映する。</param>
		/// <param name="maxUtf8Bytes">保存先へ反映する最大UTF-8バイト数。</param>
		/// <param name="enabled">操作可能ならtrue。</param>
		/// <param name="bold">入力文字を太字で表示する場合はtrue。</param>
		void AddTextInput(std::string_view id, const Rect& bounds, std::string& value, size_t maxUtf8Bytes, bool enabled = true, bool bold = false);
		/// <summary>整数を直接入力し、上下キーやホイールでも調整できる欄を登録する。</summary>
		/// <param name="id">編集状態を保持する一意な識別子。</param>
		/// <param name="bounds">入力欄の画面矩形。</param>
		/// <param name="value">変更対象の整数。</param>
		/// <param name="minimum">許可する最小値。</param>
		/// <param name="maximum">許可する最大値。</param>
		/// <param name="step">上下キーとホイールの変化量。</param>
		/// <param name="enabled">操作可能ならtrue。</param>
		void AddIntegerInput(std::string_view id, const Rect& bounds, int& value, int minimum, int maximum, int step = 1, bool enabled = true);
		/// <summary>小数を直接入力し、上下キーやホイールでも調整できる欄を登録する。</summary>
		/// <param name="id">編集状態を保持する一意な識別子。</param>
		/// <param name="bounds">入力欄の画面矩形。</param>
		/// <param name="value">変更対象の小数。</param>
		/// <param name="minimum">許可する最小値。</param>
		/// <param name="maximum">許可する最大値。</param>
		/// <param name="step">上下キーとホイールの変化量。</param>
		/// <param name="precision">表示する小数桁数。</param>
		/// <param name="enabled">操作可能ならtrue。</param>
		void AddFloatInput(std::string_view id, const Rect& bounds, float& value, float minimum, float maximum, float step = 0.1f, int precision = 2, bool enabled = true);
		/// <summary>チェック状態をクリックまたは決定キーで切り替える。</summary>
		/// <param name="id">状態を識別する一意なID。</param>
		/// <param name="bounds">チェック欄全体の矩形。</param>
		/// <param name="text">UTF-8の説明文字列。</param>
		/// <param name="checked">変更対象のチェック状態。</param>
		/// <param name="enabled">操作可能ならtrue。</param>
		void AddCheckbox(std::string_view id, const Rect& bounds, std::string_view text, bool& checked, bool enabled = true);
		/// <summary>候補一覧から一つを選ぶドロップダウンを登録する。</summary>
		/// <param name="id">開閉状態を保持する一意なID。</param>
		/// <param name="bounds">選択欄の矩形。</param>
		/// <param name="options">UTF-8の選択候補。</param>
		/// <param name="selectedIndex">現在の候補番号。空欄・範囲外なら先頭候補に補正する。</param>
		/// <param name="enabled">操作可能ならtrue。</param>
		void AddDropdown(std::string_view id, const Rect& bounds, const std::vector<std::string>& options, int& selectedIndex, bool enabled = true);
		/// <summary>縦スクロール領域を開始し、以後の部品を内容座標で登録する。</summary>
		/// <param name="id">スクロール領域の安定ID。</param>
		/// <param name="bounds">画面上の表示領域。</param>
		/// <param name="contentHeight">内容全体の高さ。</param>
		/// <param name="scrollOffset">現在位置。0が先頭、最大値が末尾。</param>
		void BeginScrollArea(std::string_view id, const Rect& bounds, int contentHeight, int& scrollOffset);
		/// <summary>現在のスクロール領域を終了し、親領域へ戻る。</summary>
		void EndScrollArea();
		/// <summary>登録済み部品からフォーカス移動と押下捕捉を解決する。</summary>
		void EndFrame();
		/// <summary>登録部品をテーマ色で描画する。</summary>
		/// <param name="renderer">矩形を画面へ描くレンダラー。</param>
		void Draw(Renderer& renderer) const;

		/// <summary>指定ボタンがこのフレームに確定されたかを返す。</summary>
		/// <param name="id">判定するボタンID。</param>
		/// <returns>このフレームに押下確定した場合は true。</returns>
		bool WasActivated(std::string_view id) const;
		/// <summary>指定部品の値がこのフレームに変更されたかを返す。</summary>
		/// <param name="id">確認する部品ID。</param>
		/// <returns>値が変更されていればtrue。</returns>
		bool WasChanged(std::string_view id) const;
		/// <summary>ポインター入力を UI が捕捉中、または UI 上にあるかを返す。</summary>
		/// <returns>UI側でポインターを処理すべき場合は true。</returns>
		bool WantsPointerCapture() const;
		/// <summary>キーボード移動先があり、UI 操作中かを返す。</summary>
		/// <returns>UIがフォーカス入力を受けている場合は true。</returns>
		bool WantsKeyboardCapture() const;
		/// <summary>IME文字入力を受け取るテキスト欄がフォーカス中か返す。</summary>
		/// <returns>テキスト入力が必要ならtrue。</returns>
		bool WantsTextInput() const;
		/// <summary>現在フォーカス中の部品 ID を返す。</summary>
		/// <returns>フォーカス中の安定ID。登録部品がなければ空文字。</returns>
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

		/// <summary>ID に一致する登録ボタンを検索する。</summary>
		/// <param name="id">検索するボタンID。</param>
		/// <returns>一致したボタン。見つからない場合は nullptr。</returns>
		Element* FindButton(const std::string& id);
		/// <summary>ID に一致する登録ボタンを読み取り専用で検索する。</summary>
		/// <param name="id">検索するボタンID。</param>
		/// <returns>一致したボタン。見つからない場合は nullptr。</returns>
		const Element* FindButton(const std::string& id) const;
		Element* FindElement(const std::string& id);
		const Element* FindElement(const std::string& id) const;
		/// <summary>画面上で最前面にある、指定点の有効ボタンを検索する。</summary>
		/// <param name="point">ヒットテストするクライアント座標。</param>
		/// <returns>最前面のボタン。該当がなければ nullptr。</returns>
		Element* FindTopButtonAt(POINT point);
		/// <summary>画面上で最前面にある有効ボタンを読み取り専用で検索する。</summary>
		/// <param name="point">ヒットテストするクライアント座標。</param>
		/// <returns>最前面のボタン。該当がなければ nullptr。</returns>
		const Element* FindTopButtonAt(POINT point) const;
		Element* FindTopInteractiveAt(POINT point);
		void RegisterElement(Element element, bool applyScrollTransform = true);
		void ActivateElement(Element& element);
		void ApplyFocusedInput();
		void UpdateNumericElement(Element& element, bool commit);
		void UpdateTextElement(Element& element);
		bool IsFocusable(const Element& element) const;
		/// <summary>削除・無効化されたフォーカスや押下捕捉を次の有効要素へ整える。</summary>
		void EnsureValidFocus();

		Rect viewport; // hit test と描画を制限する範囲。
		InputFrame input; // BeginFrame で受け取った入力状態。
		std::vector<Element> elements; // 登録順が描画順とフォーカス順になる。
		std::vector<ScrollContext> scrollStack; // 入れ子スクロール領域の座標変換。
		std::unordered_map<std::string, TextEditState> textEditStates; // フレームをまたぐ文字入力の編集位置。
		std::unordered_map<std::string, int> dropdownScrollOffsets; // 長い候補一覧の表示位置。
		std::string focusedId; // キーボード操作対象の安定ID。
		std::string capturedId; // 押し始めたボタン。外へドラッグしても所有を維持する。
		std::string expandedDropdownId; // 現在開いている選択リスト。
		bool keyboardFocusActive = false; // UIが明示的にキーボード操作を受けたか。
		bool wheelConsumed = false; // 同じホイール差分を複数のスクロール領域へ重複適用しない。
		bool endedFrame = false; // 入力解決後のみ描画・結果照会を許可する。
	};
}
