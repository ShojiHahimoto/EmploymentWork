#include "UI/UIRoot.h"

#include "Core/Renderer.h"
#include "UI/UITheme.h"

#include "UI/UIRenderer.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <utility>

using DirectX::SimpleMath::Color;

namespace GameUI
{
const Color Theme::Background(0.94f, 0.96f, 0.97f, 1.0f);
const Color Theme::Surface(1.0f, 1.0f, 1.0f, 1.0f);
const Color Theme::Border(0.72f, 0.78f, 0.81f, 1.0f);
const Color Theme::Accent(0.08f, 0.48f, 0.50f, 1.0f);
const Color Theme::Hover(0.86f, 0.94f, 0.94f, 1.0f);
const Color Theme::Pressed(0.73f, 0.88f, 0.88f, 1.0f);
const Color Theme::Disabled(0.87f, 0.89f, 0.90f, 1.0f);
const Color Theme::Focus(0.05f, 0.42f, 0.44f, 1.0f);
const Color Theme::HoverOverlay(0.03f, 0.52f, 0.50f, 0.13f);
const Color Theme::PressedOverlay(0.02f, 0.38f, 0.39f, 0.22f);
const Color Theme::Text(0.12f, 0.16f, 0.18f, 1.0f);
const Color Theme::PrimaryText(1.0f, 1.0f, 1.0f, 1.0f);
const Color Theme::DisabledText(0.35f, 0.39f, 0.40f, 0.58f);

/// <summary>幅または高さが0以下の空矩形かを返す。</summary>
/// <returns>描画可能な面積がなければtrue。</returns>
bool Rect::IsEmpty() const
{
	return right <= left || bottom <= top;
}

/// <summary>点が左上を含み右下を含まない矩形内にあるかを返す。</summary>
/// <param name="point">判定するクライアント座標。</param>
/// <returns>点が矩形内にある場合は true。</returns>
bool Rect::Contains(POINT point) const
{
	return point.x >= left && point.x < right && point.y >= top && point.y < bottom;
}

/// <summary>他の矩形と重なる領域を返す。重なりがなければ空矩形を返す。</summary>
/// <param name="other">交差判定する矩形。</param>
/// <returns>交差部分または空矩形。</returns>
Rect Rect::Intersect(const Rect &other) const
{
	Rect result;
	result.left = std::max(left, other.left);
	result.top = std::max(top, other.top);
	result.right = std::min(right, other.right);
	result.bottom = std::min(bottom, other.bottom);
	if (result.IsEmpty())
	{
		return {};
	}
	return result;
}

/// <summary>指定量だけ内側へ縮めた矩形を返す。</summary>
/// <param name="horizontal">左右それぞれから引く量。</param>
/// <param name="vertical">上下それぞれから引く量。</param>
/// <returns>内側へ縮めた矩形。</returns>
Rect Rect::Inset(int horizontal, int vertical) const
{
	return Rect{left + horizontal, top + vertical, right - horizontal, bottom - vertical};
}

/// <summary>指定矩形を除いた領域を、互いに重ならない最大4矩形として返す。</summary>
/// <param name="other">除外する範囲。</param>
/// <returns>描画・入力に利用できる残りの範囲。</returns>
std::vector<Rect> Rect::Subtract(const Rect& other) const
{
	if (IsEmpty()) return {};
	const Rect overlap = Intersect(other);
	if (overlap.IsEmpty()) return {*this};
	std::vector<Rect> result;
	for (const Rect part : {Rect{left, top, right, overlap.top}, Rect{left, overlap.bottom, right, bottom},
		Rect{left, overlap.top, overlap.left, overlap.bottom}, Rect{overlap.right, overlap.top, right, overlap.bottom}})
		if (!part.IsEmpty()) result.push_back(part);
	return result;
}

/// <summary>Win32描画APIへ渡す矩形形式に変換する。</summary>
/// <returns>Win32 RECT 形式の座標。</returns>
RECT Rect::ToNativeRect() const
{
	return RECT{left, top, right, bottom};
}

/// <summary>指定数の部品を、余白・高さ・間隔を保った縦スタックとして配置する。</summary>
/// <param name="parent">配置を収める親矩形。</param>
/// <param name="itemCount">作成する矩形の数。</param>
/// <param name="itemHeight">各矩形の高さ。</param>
/// <param name="gap">隣接矩形の間隔。</param>
/// <param name="horizontalPadding">左右の内側余白。</param>
/// <param name="verticalPadding">先頭側の内側余白。</param>
/// <returns>親矩形でクリップした各部品の配置。</returns>
std::vector<Rect> Layout::VerticalStack(const Rect &parent, int itemCount, int itemHeight, int gap,
										int horizontalPadding, int verticalPadding)
{
	std::vector<Rect> result;
	if (parent.IsEmpty() || itemCount <= 0 || itemHeight <= 0)
	{
		return result;
	}

	result.reserve(static_cast<size_t>(itemCount));
	const int usableWidth = std::max(0, parent.right - parent.left - horizontalPadding * 2);
	const int startY = parent.top + verticalPadding;
	for (int index = 0; index < itemCount; ++index)
	{
		const int top = startY + index * (itemHeight + std::max(0, gap));
		Rect item{parent.left + horizontalPadding, top, parent.left + horizontalPadding + usableWidth,
				  top + itemHeight};
		item = item.Intersect(parent);
		result.push_back(item);
	}
	return result;
}

namespace
{
std::wstring Wide(std::string_view s)
{
	if (s.empty())
		return {};
	int n =
		MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), nullptr, 0);
	std::wstring w(n, 0);
	if (n)
		MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), w.data(), n);
	return w;
}
std::string Utf8(std::wstring_view w)
{
	if (w.empty())
		return {};
	int n = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, w.data(), static_cast<int>(w.size()), nullptr,
								0, nullptr, nullptr);
	std::string s(n, 0);
	if (n)
		WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, w.data(), static_cast<int>(w.size()), s.data(), n,
							nullptr, nullptr);
	return s;
}
size_t Prev(const std::wstring &s, size_t p)
{
	if (!p)
		return 0;
	--p;
	if (p && s[p] >= 0xdc00 && s[p] <= 0xdfff && s[p - 1] >= 0xd800 && s[p - 1] <= 0xdbff)
		--p;
	return p;
}
size_t Next(const std::wstring &s, size_t p)
{
	if (p >= s.size())
		return s.size();
	if (s[p] >= 0xd800 && s[p] <= 0xdbff && p + 1 < s.size())
		++p;
	return p + 1;
}
bool Pressed(const InputFrame &f, Key k)
{
	return f.keysPressed[static_cast<size_t>(k)];
}
} // namespace
/// <summary>描画領域と入力を設定し、当フレームの部品登録を開始する。</summary>
/// <param name="v">このルートが入力と描画を受け持つ画面領域。</param>
/// <param name="f">このフレームの入力状態。</param>
void Root::BeginFrame(const Rect &v, const InputFrame &f)
{
	if (f.pointerPressed || f.focusMove || !f.textInput.empty() || f.focusLost
		|| std::any_of(f.keysPressed.begin(), f.keysPressed.end(), [](bool key) { return key; })) wheelEditId.clear();
	viewport = v;
	input = f;
	elements.clear();
	scrollStack.clear();
	wheelConsumed = false;
	endedFrame = false;
}
void Root::RegisterElement(Element e, bool transform)
{
	// 部品の値参照は当フレームのみ有効。入力解決後に新たな部品を追加しない。
	if (endedFrame || (!e.id.empty() && FindElement(e.id)))
		return;
	e.clipBounds = viewport;
	if (!scrollStack.empty())
	{
		const auto &s = scrollStack.back();
		e.clipBounds = s.bounds.Intersect(s.parentClip);
		if (transform)
		{
			e.bounds.left += s.bounds.left;
			e.bounds.right += s.bounds.left;
			e.bounds.top += s.bounds.top - s.offset;
			e.bounds.bottom += s.bounds.top - s.offset;
		}
	}
	elements.push_back(std::move(e));
}
/// <summary>指定素材を使う背景パネルを登録する。素材がなければ単色面にフォールバックする。</summary>
/// <param name="b">パネルの画面矩形。</param>
/// <param name="imageId">9スライスで使う背景素材。</param>
void Root::AddPanel(const Rect &b, std::string_view imageId)
{
	Element e;
	e.type = ElementType::Panel;
	e.bounds = b;
	e.imageId = imageId;
	RegisterElement(e);
}
/// <summary>入力を受け取らない区切り線または目盛りを登録する。</summary>
/// <param name="bounds">細い矩形で指定する描画範囲。</param>
/// <param name="color">指定がなければテーマの境界色。</param>
void Root::AddRule(const Rect& bounds, std::optional<Color> color)
{
	Element e;
	e.type = ElementType::Rule;
	e.bounds = bounds;
	e.color = color;
	RegisterElement(e);
}
/// <summary>淡色背景と縦アクセントを持つセクション見出しを登録する。</summary>
/// <param name="id">見出しの安定ID。</param>
/// <param name="bounds">見出しの矩形。</param>
/// <param name="text">UTF-8の見出し文字列。</param>
/// <param name="fontSize">96 DPI基準の文字サイズ。</param>
void Root::AddSectionHeader(std::string_view id, const Rect& bounds, std::string_view text, float fontSize)
{
	AddPanel(bounds, Theme::LabelBackgroundImage);
	const int accentWidth = std::min(6, std::max(1, bounds.right - bounds.left));
	const std::string accentId = std::string(id) + "/accent";
	AddImage(accentId, {bounds.left, bounds.top, bounds.left + accentWidth, bounds.bottom},
		Theme::SectionAccentImage);
	AddLabel(id, {bounds.left + 14, bounds.top, bounds.right - 6, bounds.bottom}, text, true, fontSize, false, true);
}
/// <summary>透過画像を指定矩形へ表示する部品を登録する。</summary>
/// <param name="id">描画部品の安定ID。</param>
/// <param name="bounds">画像の表示矩形。</param>
/// <param name="imageId">画像一覧JSONに登録された画像名。</param>
/// <param name="opacity">表示不透明度。</param>
void Root::AddImage(std::string_view id, const Rect& bounds, std::string_view imageId, float opacity)
{
	Element e;
	e.type = ElementType::Image;
	e.id = id;
	e.imageId = imageId;
	e.bounds = bounds;
	e.opacity = std::isfinite(opacity) ? std::clamp(opacity, 0.0f, 1.0f) : 1.0f;
	RegisterElement(e);
}
/// <summary>9スライス設定付きの画像部品を登録する。</summary>
/// <param name="id">描画部品の安定ID。</param>
/// <param name="bounds">伸縮後の表示矩形。</param>
/// <param name="imageId">画像一覧JSONに登録された画像名。</param>
/// <param name="opacity">表示不透明度。</param>
void Root::AddNineSliceImage(std::string_view id, const Rect& bounds, std::string_view imageId, float opacity)
{
	Element e;
	e.type = ElementType::NineSliceImage;
	e.id = id;
	e.imageId = imageId;
	e.bounds = bounds;
	e.opacity = std::isfinite(opacity) ? std::clamp(opacity, 0.0f, 1.0f) : 1.0f;
	RegisterElement(e);
}
/// <summary>操作ボタンを登録する。ID は画面内で一意にする。</summary>
/// <param name="id">フレーム間で安定して維持する一意な識別子。</param>
/// <param name="b">ボタン矩形。</param>
/// <param name="enabled">操作可能なら true。</param>
/// <param name="imageId">ボタンの背景画像。空なら通常のボタン素材。</param>
void Root::AddButton(std::string_view id, const Rect &b, bool enabled, std::string_view imageId)
{
	AddButton(id, b, std::string_view{}, enabled);
	if (!imageId.empty())
	{
		if (Element *element = FindElement(std::string(id)))
		{
			element->imageId = imageId;
		}
	}
}
/// <summary>文字列リテラルを文字付きボタンの登録へ転送する。</summary>
/// <param name="id">部品ID。
/// 一意な部品ID。</param>
/// <param name="bounds">ボタン矩形。</param>
/// <param name="text">UTF-8の表示文字列。
/// UTF-8表示文字列。</param>
/// <param name="enabled">操作可否。</param>
/// <param name="fontSize">96 DPI基準の文字サイズ。</param>
/// <remarks>文字列リテラルがbool引数へ誤変換されないよう文字付きボタンへ転送する。</remarks>
void Root::AddButton(std::string_view id, const Rect& bounds, const char* text, bool enabled, float fontSize)
{
	AddButton(id, bounds, std::string_view(text ? text : ""), enabled, fontSize);
}

/// <summary>文字付きの標準ボタンを登録する。</summary>
/// <param name="id">部品ID。
/// フレーム間で安定して維持する一意な識別子。</param>
/// <param name="b">ボタン矩形。</param>
/// <param name="text">UTF-8の表示文字列。
/// UTF-8のボタン表示文字列。</param>
/// <param name="enabled">操作可否。
/// 操作可能ならtrue。</param>
/// <param name="fontSize">96 DPI基準の文字サイズ。</param>
/// <param name="bold">trueならボタン名を太字にする。</param>
/// <remarks>文字付きボタンを登録する。押下結果は WasActivated で照会できる。</remarks>
void Root::AddButton(std::string_view id, const Rect &b, std::string_view text, bool enabled, float fontSize, bool bold)
{
	Element e;
	e.type = ElementType::Button;
	e.id = id;
	e.bounds = b;
	e.label = text;
	e.bold = bold;
	e.imageId = Theme::ButtonImage;
	e.enabled = enabled;
	e.fontSize = fontSize;

	RegisterElement(e);
}
/// <summary>強調用の色付きボタンを登録する。</summary>
/// <param name="id">フレーム間で安定して維持する一意な識別子。</param>
/// <param name="bounds">ボタン矩形。</param>
/// <param name="text">UTF-8のボタン表示文字列。</param>
/// <param name="enabled">操作可能ならtrue。</param>
void Root::AddPrimaryButton(std::string_view id, const Rect& bounds, std::string_view text, bool enabled)
{
	AddButton(id, bounds, text, enabled);
	if (Element* element = FindElement(std::string(id)))
	{
		element->imageId = Theme::PrimaryButtonImage;
		element->bold = true;
	}
}
/// <summary>素材自体に背景と図柄が含まれる画像ボタンを登録する。</summary>
/// <param name="id">フレーム間で安定して維持する一意な識別子。</param>
/// <param name="bounds">ボタン矩形。</param>
/// <param name="imageId">ボタン全体を描く画像素材。</param>
/// <param name="fallbackText">画像がない場合に表示する文字列。</param>
/// <param name="enabled">操作可能ならtrue。</param>
/// <param name="showFocusOutline">キーボードフォーカス枠を描画するか。</param>
void Root::AddImageButton(std::string_view id, const Rect& bounds, std::string_view imageId,
	std::string_view fallbackText, bool enabled, bool showFocusOutline)
{
	AddButton(id, bounds, fallbackText, enabled);
	if (Element* element = FindElement(std::string(id)))
	{
		element->imageId = imageId;
		element->stretchBackground = false;
		element->showFocusOutline = showFocusOutline;
	}
}
/// <summary>標準ボタン背景に透過アイコンと文字を組み合わせたボタンを登録する。</summary>
/// <param name="id">フレーム間で安定して維持する一意な識別子。</param>
/// <param name="bounds">ボタン矩形。</param>
/// <param name="text">UTF-8の表示文字列。</param>
/// <param name="iconImageId">透過アイコン素材。</param>
/// <param name="enabled">操作可能ならtrue。</param>
/// <param name="showFocusOutline">フォーカス枠を表示する場合true。</param>
/// <param name="primary">選択中のモードなどを強調色で表示する場合true。</param>
/// <param name="fontSize">表示文字サイズ。省略時は他の標準ボタンと同じ。</param>
void Root::AddIconButton(std::string_view id, const Rect& bounds, std::string_view text,
	std::string_view iconImageId, bool enabled, bool showFocusOutline, bool primary, float fontSize)
{
	AddButton(id, bounds, text, enabled, fontSize);

	if (Element* element = FindElement(std::string(id)))
	{
		element->iconImageId = iconImageId;
		element->showFocusOutline = showFocusOutline;

		if (primary)
		{
			element->imageId = Theme::PrimaryButtonImage;
			element->bold = true;
		}
	}
}
/// <summary>日本語を含むラベルを表示する。</summary>
/// <param name="id">描画順と状態確認に使う任意の識別子。</param>
/// <param name="b">文字を描画する矩形。</param>
/// <param name="text">UTF-8の表示文字列。</param>
/// <param name="enabled">無効表示にする場合はfalse。</param>
/// <param name="size">96 DPI基準の文字サイズ。</param>
/// <param name="centered">trueなら文字を矩形の中央へ揃える。</param>
/// <param name="bold">trueなら太字で表示する。</param>
/// <param name="color">指定がなければテーマの文字色。</param>
void Root::AddLabel(std::string_view id, const Rect &b, std::string_view text, bool enabled,
	float size, bool centered, bool bold, std::optional<Color> color)
{
	Element e;
	e.type = ElementType::Label;
	e.id = id;
	e.bounds = b;
	e.label = text;
	e.enabled = enabled;
	e.fontSize = size;
	e.centered = centered;
	e.bold = bold;
	e.color = color;
	RegisterElement(e);
}
/// <summary>IME文字入力、カーソル移動、削除に対応するテキスト欄を登録する。</summary>
/// <param name="id">編集状態を保持する一意な識別子。</param>
/// <param name="b">入力欄の画面矩形。</param>
/// <param name="value">UTF-8文字列。変更はこの参照へ即時反映する。</param>
/// <param name="limit">保存先へ反映する最大UTF-8バイト数。</param>
/// <param name="enabled">操作可能ならtrue。</param>
/// <param name="bold">入力文字を太字で表示する場合はtrue。</param>
void Root::AddTextInput(std::string_view id, const Rect &b, std::string &value, size_t limit, bool enabled, bool bold)
{
	Element e;
	e.type = ElementType::TextInput;
	e.id = id;
	e.bounds = b;
	e.textValue = &value;
	e.maxUtf8Bytes = limit;
	e.bold = bold;
	e.enabled = enabled;
	RegisterElement(e);
}
/// <summary>整数を直接入力し、上下キーやホイールでも調整できる欄を登録する。</summary>
/// <param name="id">編集状態を保持する一意な識別子。</param>
/// <param name="b">入力欄の画面矩形。</param>
/// <param name="value">変更対象の整数。</param>
/// <param name="lo">許可する最小値。</param>
/// <param name="hi">許可する最大値。</param>
/// <param name="step">上下キーとホイールの変化量。</param>
/// <param name="enabled">操作可能ならtrue。</param>
void Root::AddIntegerInput(std::string_view id, const Rect &b, int &value, int lo, int hi, int step,
						   bool enabled)
{
	Element e;
	e.type = ElementType::IntegerInput;
	e.id = id;
	e.bounds = b;
	e.integerValue = &value;
	e.integerMinimum = std::min(lo, hi);
	e.integerMaximum = std::max(lo, hi);
	e.numericStep = static_cast<float>(std::max(1, step));
	e.enabled = enabled;
	RegisterElement(e);
}
/// <summary>小数を直接入力し、上下キーやホイールでも調整できる欄を登録する。</summary>
/// <param name="id">編集状態を保持する一意な識別子。</param>
/// <param name="b">入力欄の画面矩形。</param>
/// <param name="value">変更対象の小数。</param>
/// <param name="lo">許可する最小値。</param>
/// <param name="hi">許可する最大値。</param>
/// <param name="step">上下キーとホイールの変化量。</param>
/// <param name="precision">表示する小数桁数。</param>
/// <param name="enabled">操作可能ならtrue。</param>
void Root::AddFloatInput(std::string_view id, const Rect &b, float &value, float lo, float hi, float step,
						 int precision, bool enabled)
{
	Element e;
	e.type = ElementType::FloatInput;
	e.id = id;
	e.bounds = b;
	e.floatValue = &value;
	e.floatMinimum = std::min(lo, hi);
	e.floatMaximum = std::max(lo, hi);
	e.numericStep = std::isfinite(step) ? std::abs(step) : 0.1f;
	e.precision = std::clamp(precision, 0, 8);
	e.enabled = enabled && std::isfinite(lo) && std::isfinite(hi);
	RegisterElement(e);
}
/// <summary>チェック状態をクリックまたは決定キーで切り替える。</summary>
/// <param name="id">状態を識別する一意なID。</param>
/// <param name="b">チェック欄全体の矩形。</param>
/// <param name="text">UTF-8の説明文字列。</param>
/// <param name="value">変更対象のチェック状態。</param>
/// <param name="enabled">操作可能ならtrue。</param>
/// <param name="fontSize">表示文字サイズ。</param>
void Root::AddCheckbox(std::string_view id, const Rect &b, std::string_view text, bool &value, bool enabled, float fontSize)
{
	Element e;
	e.type = ElementType::Checkbox;
	e.fontSize = fontSize;
	e.id = id;
	e.bounds = b;
	e.label = text;
	e.booleanValue = &value;
	e.enabled = enabled;
	RegisterElement(e);
}
/// <summary>候補一覧から一つを選ぶドロップダウンを登録する。</summary>
/// <param name="id">開閉状態を保持する一意なID。</param>
/// <param name="b">選択欄の矩形。</param>
/// <param name="options">UTF-8の選択候補。</param>
/// <param name="index">現在の候補番号。空欄・範囲外なら先頭候補に補正する。</param>
/// <param name="enabled">操作可能ならtrue。</param>
void Root::AddDropdown(std::string_view id, const Rect &b, const std::vector<std::string> &options,
					   int &index, bool enabled)
{
	Element e;
	e.type = ElementType::Dropdown;
	e.id = id;
	e.bounds = b;
	e.options = options;
	e.selectedIndex = &index;
	e.enabled = enabled && !options.empty();
	if (!options.empty())
		index = std::clamp(index, 0, static_cast<int>(options.size()) - 1);
	RegisterElement(e);
}
/// <summary>縦スクロール領域を開始し、以後の部品を内容座標で登録する。</summary>
/// <param name="id">スクロール領域の安定ID。</param>
/// <param name="b">画面上の表示領域。</param>
/// <param name="height">内容全体の高さ。</param>
/// <param name="offset">現在位置。0が先頭、最大値が末尾。</param>
void Root::BeginScrollArea(std::string_view id, const Rect &b, int height, int &offset)
{
	Rect bounds = b;
	Rect clip = viewport;
	if (!scrollStack.empty())
	{
		auto &p = scrollStack.back();
		bounds.left += p.bounds.left;
		bounds.right += p.bounds.left;
		bounds.top += p.bounds.top - p.offset;
		bounds.bottom += p.bounds.top - p.offset;
		clip = p.bounds.Intersect(p.parentClip);
	}
	int maximum = std::max(0, height - std::max(0, b.bottom - b.top));
	if (!wheelConsumed && bounds.Intersect(clip).Contains(input.pointerPosition) && input.wheelDelta)
	{
		offset = static_cast<int>(std::clamp(static_cast<long long>(offset) - input.wheelDelta / 3, 0LL,
											 static_cast<long long>(maximum)));
		wheelConsumed = true;
	}
	offset = std::clamp(offset, 0, maximum);
	scrollStack.push_back({std::string(id), bounds, clip, height, &offset, offset});
}
/// <summary>現在のスクロール領域を終了し、親領域へ戻る。</summary>
void Root::EndScrollArea()
{
	if (scrollStack.empty()) return;
	const ScrollContext context = scrollStack.back();
	scrollStack.pop_back();
	const int height = context.bounds.bottom - context.bounds.top;
	if (height <= 0 || context.contentHeight <= height) return;
	// 子部品より後に描き、スクロール位置を視覚的に示す。操作はホイールで行う。
	Element track;
	track.type = ElementType::ScrollTrack;
	track.bounds = {context.bounds.right - 6, context.bounds.top, context.bounds.right, context.bounds.bottom};
	track.clipBounds = context.bounds.Intersect(context.parentClip);
	elements.push_back(track);
	Element thumb = track;
	thumb.type = ElementType::ScrollThumb;
	const int size = std::min(height, std::max(12, static_cast<int>(static_cast<double>(height) * height / context.contentHeight)));
	const int top = context.bounds.top + static_cast<int>(static_cast<double>(context.offset) * (height - size) / (context.contentHeight - height));
	thumb.bounds.top = top;
	thumb.bounds.bottom = top + size;
	elements.push_back(thumb);
}
Root::Element *Root::FindElement(const std::string &id)
{
	return const_cast<Element *>(std::as_const(*this).FindElement(id));
}
const Root::Element *Root::FindElement(const std::string &id) const
{
	if (id.empty())
		return nullptr;
	for (auto &e : elements)
		if (e.id == id)
			return &e;
	return nullptr;
}
/// <summary>ID に一致する登録ボタンを検索する。</summary>
/// <param name="id">検索するボタンID。</param>
/// <returns>一致したボタン。見つからない場合は nullptr。</returns>
Root::Element *Root::FindButton(const std::string &id)
{
	return FindElement(id);
}
/// <summary>ID に一致する登録ボタンを読み取り専用で検索する。</summary>
/// <param name="id">検索するボタンID。</param>
/// <returns>一致したボタン。見つからない場合は nullptr。</returns>
const Root::Element *Root::FindButton(const std::string &id) const
{
	return FindElement(id);
}
bool Root::IsFocusable(const Element &e) const
{
	return e.enabled && !e.id.empty() && e.type != ElementType::Panel && e.type != ElementType::Label &&
		   e.type != ElementType::ScrollTrack && e.type != ElementType::ScrollThumb &&
		   e.type != ElementType::Image && e.type != ElementType::NineSliceImage &&
		   !e.bounds.Intersect(e.clipBounds).IsEmpty();
}
/// <summary>画面上で最前面にある有効ボタンを読み取り専用で検索する。</summary>
/// <param name="p">ヒットテストするクライアント座標。</param>
/// <returns>最前面のボタン。該当がなければ nullptr。</returns>
const Root::Element *Root::FindTopButtonAt(POINT p) const
{
	for (auto i = elements.rbegin(); i != elements.rend(); ++i)
	{
		if (!i->bounds.Intersect(i->clipBounds).Contains(p)) continue;
		if (i->type == ElementType::Rule || i->type == ElementType::Panel || i->type == ElementType::Label ||
			i->type == ElementType::ScrollTrack || i->type == ElementType::ScrollThumb ||
			i->type == ElementType::Image || i->type == ElementType::NineSliceImage)
			continue;
		return IsFocusable(*i) ? &*i : nullptr;
	}
	return nullptr;
}
/// <summary>画面上で最前面にある、指定点の有効ボタンを検索する。</summary>
/// <param name="p">ヒットテストするクライアント座標。</param>
/// <returns>最前面のボタン。該当がなければ nullptr。</returns>
Root::Element *Root::FindTopButtonAt(POINT p)
{
	return const_cast<Element *>(std::as_const(*this).FindTopButtonAt(p));
}
Root::Element *Root::FindTopInteractiveAt(POINT p)
{
	return FindTopButtonAt(p);
}
/// <summary>削除・無効化されたフォーカスや押下捕捉を次の有効要素へ整える。</summary>
void Root::EnsureValidFocus()
{
	auto e = FindElement(focusedId);
	if (!e || !IsFocusable(*e))
	{
		focusedId.clear();
		keyboardFocusActive = false;
	}
	e = FindElement(capturedId);
	if (!e || !IsFocusable(*e))
		capturedId.clear();
	e = FindElement(expandedDropdownId);
	if (!e || !IsFocusable(*e))
		expandedDropdownId.clear();
}
void Root::ActivateElement(Element &e)
{
	if (e.activated)
		return;
	e.activated = true;
	if (e.booleanValue)
	{
		*e.booleanValue = !*e.booleanValue;
		e.changed = true;
	}
	if ((e.integerValue || e.floatValue) && input.pointerReleased && e.bounds.Contains(input.pointerPosition))
	{
		const int spinnerWidth = std::min(28, std::max(20, (e.bounds.right - e.bounds.left) / 5));
		if (input.pointerPosition.x >= e.bounds.right - spinnerWidth)
		{
			UpdateNumericElement(e, true);
			const int direction = input.pointerPosition.y < e.bounds.top + (e.bounds.bottom - e.bounds.top) / 2 ? 1 : -1;
			if (e.integerValue)
			{
				const int step = std::max(1, static_cast<int>(e.numericStep + 0.5f));
				const int updated = static_cast<int>(std::clamp(
					static_cast<long long>(*e.integerValue) + static_cast<long long>(direction * step),
					static_cast<long long>(e.integerMinimum), static_cast<long long>(e.integerMaximum)));
				e.changed |= updated != *e.integerValue;
				*e.integerValue = updated;
			}
			if (e.floatValue)
			{
				const float updated = std::clamp(*e.floatValue + direction * e.numericStep,
					e.floatMinimum, e.floatMaximum);
				e.changed |= updated != *e.floatValue;
				*e.floatValue = updated;
			}
			return;
		}
	}
	if (e.type == ElementType::Dropdown)
		expandedDropdownId = expandedDropdownId == e.id ? "" : e.id;
	if (e.type == ElementType::DropdownOption)
	{
		e.changed = *e.selectedIndex != e.optionIndex;
		*e.selectedIndex = e.optionIndex;
		auto parent = FindElement(expandedDropdownId);
		if (parent)
			parent->changed = e.changed;
		expandedDropdownId.clear();
	}
}
/// <summary>Stage edits separately from external numeric values; invalid partial numbers never
/// escape.</summary>
void Root::UpdateNumericElement(Element &e, bool commit)
{
	auto &s = textEditStates[e.id];
	if (!commit)
		return;
	std::string value = Utf8(s.text);
	if (e.integerValue)
	{
		int n = 0;
		auto r = std::from_chars(value.data(), value.data() + value.size(), n);
		if (r.ec == std::errc{} && r.ptr == value.data() + value.size())
		{
			n = std::clamp(n, e.integerMinimum, e.integerMaximum);
			e.changed |= n != *e.integerValue;
			*e.integerValue = n;
		}
	}
	if (e.floatValue)
	{
		float n = 0;
		auto r = std::from_chars(value.data(), value.data() + value.size(), n);
		if (r.ec == std::errc{} && r.ptr == value.data() + value.size() && std::isfinite(n))
		{
			n = std::clamp(n, e.floatMinimum, e.floatMaximum);
			e.changed |= n != *e.floatValue;
			*e.floatValue = n;
		}
	}
	s.initialized = false;
}
void Root::UpdateTextElement(Element &e)
{
	// UTF-16で編集し、外部のUTF-8容量に収まる場合だけ反映する。
	// IME未確定文字はここへ挿入せず、Drawでのみ重ねて表示する。
	auto &s = textEditStates[e.id];
	auto old = s.text;
	if (Pressed(input, Key::Left))
		s.cursor = Prev(s.text, s.cursor);
	if (Pressed(input, Key::Right))
		s.cursor = Next(s.text, s.cursor);
	if (Pressed(input, Key::Home))
		s.cursor = 0;
	if (Pressed(input, Key::End))
		s.cursor = s.text.size();
	if (Pressed(input, Key::Backspace) && s.cursor)
	{
		auto p = Prev(s.text, s.cursor);
		s.text.erase(p, s.cursor - p);
		s.cursor = p;
	}
	if (Pressed(input, Key::Delete))
		s.text.erase(s.cursor, Next(s.text, s.cursor) - s.cursor);
	s.text.insert(s.cursor, input.textInput);
	s.cursor += input.textInput.size();
	std::string utf = Utf8(s.text);
	if ((!s.text.empty() && utf.empty()) || (e.textValue && utf.size() > e.maxUtf8Bytes))
	{
		s.text = old;
		s.cursor = std::min(s.cursor, s.text.size());
		return;
	}
	if (e.textValue)
	{
		e.changed |= *e.textValue != utf;
		*e.textValue = utf;
		s.lastWrittenUtf8 = utf;
	}
}
void Root::ApplyFocusedInput()
{
	auto e = FindElement(focusedId);
	if (!e || !keyboardFocusActive || input.composing)
		return;
	bool edit = e->textValue || e->integerValue || e->floatValue;
	if (edit)
	{
		UpdateTextElement(*e);
		if (Pressed(input, Key::Enter) || input.activatePressed)
		{
			UpdateNumericElement(*e, true);
			textEditStates[e->id].original = e->textValue ? *e->textValue : "";
			// 値の確定後は編集を終了する。IME変換中のEnterは上のcomposing判定で除外する。
			focusedId.clear();
			capturedId.clear();
			keyboardFocusActive = false;
			return;
		}
		int direction = Pressed(input, Key::Up) ? 1 : Pressed(input, Key::Down) ? -1 : 0;
		if (!wheelConsumed && e->bounds.Intersect(e->clipBounds).Contains(input.pointerPosition) &&
			input.wheelDelta)
		{
			direction = input.wheelDelta / 120;
			wheelConsumed = true;
			if (e->integerValue || e->floatValue) wheelEditId = e->id;
		}
		if (direction && (e->integerValue || e->floatValue))
		{
			UpdateNumericElement(*e, true);
			if (e->integerValue)
			{
				auto n = std::clamp(
					static_cast<double>(*e->integerValue) + direction * static_cast<double>(e->numericStep),
					static_cast<double>(e->integerMinimum), static_cast<double>(e->integerMaximum));
				e->changed |= *e->integerValue != static_cast<int>(n);
				*e->integerValue = static_cast<int>(n);
			}
			if (e->floatValue)
			{
				float n =
					std::clamp(*e->floatValue + direction * e->numericStep, e->floatMinimum, e->floatMaximum);
				e->changed |= n != *e->floatValue;
				*e->floatValue = n;
			}
		}
	}
	else if (e->type == ElementType::Dropdown && (Pressed(input, Key::Up) || Pressed(input, Key::Down)))
	{
		int n = std::clamp(*e->selectedIndex + (Pressed(input, Key::Down) ? 1 : -1), 0,
						   static_cast<int>(e->options.size()) - 1);
		e->changed = n != *e->selectedIndex;
		*e->selectedIndex = n;
	}
	else if (input.activatePressed || Pressed(input, Key::Enter))
		ActivateElement(*e);
}
/// <summary>画面固有のナビゲーションで選んだボタンへフォーカスを移す。</summary>
/// <param name="id">このフレームで登録した有効なボタンのID。</param>
void Root::FocusButton(std::string_view id)
{
	const auto* element = FindElement(std::string(id));
	if (element && element->type == ElementType::Button && IsFocusable(*element))
	{
		focusedId = element->id;
		keyboardFocusActive = true;
	}
}

/// <summary>登録済み部品からフォーカス移動と押下捕捉を解決する。</summary>
void Root::EndFrame()
{
	// 全部品の登録後に入力を一度だけ解決する。描画から更新処理を再実行しない。
	if (endedFrame)
		return;
	// Popups are appended after normal content to establish both draw and hit-test priority.
	if (auto p = FindElement(expandedDropdownId))
	{
		Element parent = *p;
		int row = std::max(24, parent.bounds.bottom - parent.bounds.top);
		int count = std::min(static_cast<int>(parent.options.size()),
							 std::max(1, (viewport.bottom - viewport.top) / row - 1));
		int &start = dropdownScrollOffsets[parent.id];
		start = std::clamp(start, 0, std::max(0, static_cast<int>(parent.options.size()) - count));
		int top = std::min(parent.bounds.bottom, viewport.bottom - count * row);
		top = std::max(viewport.top, top);
		Rect popup{parent.bounds.left, top, parent.bounds.right, top + count * row};
		if (popup.Contains(input.pointerPosition) && input.wheelDelta)
		{
			start = std::clamp(start - input.wheelDelta / 120, 0,
							   std::max(0, static_cast<int>(parent.options.size()) - count));
			wheelConsumed = true;
		}
		for (int i = 0; i < count; ++i)
		{
			Element e;
			e.type = ElementType::DropdownOption;
			e.id = parent.id + "##option" + std::to_string(i);
			e.bounds = {popup.left, top + i * row, popup.right, top + (i + 1) * row};
			e.label = parent.options[start + i];
			e.optionIndex = start + i;
			e.selectedIndex = parent.selectedIndex;
			RegisterElement(e, false);
		}
	}
	// Popup options must exist before validating the previous frame's pointer capture.
	EnsureValidFocus();
	std::string previous = focusedId;
	if (input.focusLost)
	{
		if (auto old = FindElement(focusedId)) UpdateNumericElement(*old, true);
		capturedId.clear();
		focusedId.clear();
		expandedDropdownId.clear();
		keyboardFocusActive = false;
	}
	else if (!input.composing && (input.cancelPressed || Pressed(input, Key::Escape)))
	{
		if (auto e = FindElement(focusedId))
		{
			auto &s = textEditStates[e->id];
			if (e->textValue && s.initialized)
			{
				e->changed = *e->textValue != s.original;
				*e->textValue = s.original;
			}
			s.initialized = false;
		}
		capturedId.clear();
		expandedDropdownId.clear();
		keyboardFocusActive = false;
	}
	else
	{
		if (input.focusMove && !input.composing)
		{
			std::vector<std::string> ids;
			for (auto &e : elements)
				if (IsFocusable(e) && e.type != ElementType::DropdownOption)
					ids.push_back(e.id);
			if (!ids.empty())
			{
				auto it = std::find(ids.begin(), ids.end(), focusedId);
				int n = static_cast<int>(ids.size());
				int i = it == ids.end() ? (input.focusMove > 0 ? -1 : 0) : static_cast<int>(it - ids.begin());
				focusedId = ids[(i + input.focusMove % n + n) % n];
				keyboardFocusActive = true;
			}
		}
		auto hit = FindTopInteractiveAt(input.pointerPosition);
		if (input.pointerPressed)
		{
			capturedId = hit ? hit->id : "";
			focusedId = capturedId;
			keyboardFocusActive = hit != nullptr;
			if (hit && (hit->integerValue || hit->floatValue))
			{
				const int spinnerWidth = std::min(28, std::max(20, (hit->bounds.right - hit->bounds.left) / 5));
				// 直接入力の確定と、その後の矢印クリックを別操作にする。増減自体は従来どおり解放時。
				if (input.pointerPosition.x >= hit->bounds.right - spinnerWidth) UpdateNumericElement(*hit, true);
			}
			if (!hit || (hit->type != ElementType::DropdownOption && hit->id != expandedDropdownId))
				expandedDropdownId.clear();
		}
		if (previous != focusedId)
		{
			if (auto old = FindElement(previous))
				UpdateNumericElement(*old, true);
		}
		for (auto &e : elements)
		{
			if (!(e.textValue || e.integerValue || e.floatValue))
				continue;
			auto &s = textEditStates[e.id];
			std::string external;
			if (e.textValue)
				external = *e.textValue;
			else if (e.integerValue)
				external = std::to_string(*e.integerValue);
			else
			{
				std::ostringstream out;
				out.imbue(std::locale::classic());
				out << std::fixed << std::setprecision(e.precision) << *e.floatValue;
				external = out.str();
			}
			if (!s.initialized || e.id != focusedId || external != s.lastWrittenUtf8)
			{
				s.text = Wide(external);
				s.cursor = s.text.size();
				s.lastWrittenUtf8 = external;
				s.initialized = true;
				s.original = external;
			}
			if (previous != focusedId && e.id == focusedId)
				s.original = external;
		}
		if (input.pointerReleased)
		{
			auto e = FindElement(capturedId);
			if (e && e == hit)
				ActivateElement(*e);
			capturedId.clear();
		}
		ApplyFocusedInput();
	}
	const auto hoveredElement = FindTopInteractiveAt(input.pointerPosition);
	for (auto &e : elements)
		e.hovered = (&e == hoveredElement);
	for (auto it = textEditStates.begin(); it != textEditStates.end();)
		if (!FindElement(it->first))
			it = textEditStates.erase(it);
		else
			++it;
	endedFrame = true;
}

/// <summary>文字入力・数値ステップ・ホイールの操作単位を取得する。</summary>
/// <returns>編集欄がなければ空ID。単発ボタン操作は呼び出し側が扱う。</returns>
Root::EditInteraction Root::GetEditInteraction() const
{
	for (const auto& e : elements)
	{
		if (!e.integerValue && !e.floatValue && !e.textValue) continue;
		const int spinnerWidth = std::min(28, std::max(20, (e.bounds.right - e.bounds.left) / 5));
		if (e.activated && (e.integerValue || e.floatValue) && input.pointerPosition.x >= e.bounds.right - spinnerWidth)
			return {"step/" + e.id, false, false};
		if (e.changed && (!keyboardFocusActive || e.id != focusedId)) return {"text/" + e.id, false, false};
	}
	if (WantsTextInput())
	{
		const auto* e = FindElement(focusedId);
		if (wheelEditId == focusedId) return {"wheel/" + focusedId, false, true};
		if ((e->integerValue || e->floatValue) && (Pressed(input, Key::Up) || Pressed(input, Key::Down)))
			return {"step/" + focusedId, false, false};
		return {"text/" + focusedId, true, false};
	}
	return {};
}

bool Root::ShouldCollapseInlineSection(std::string_view prefix, const Rect& section, const Rect& auxiliary) const
{
	if (input.focusLost) return true;
	if (focusedId.starts_with(prefix)) return false; // プルダウン候補も同じ項目に含める。
	const auto* focused = FindElement(focusedId);
	if (focused && (focused->type == ElementType::ScrollTrack || focused->type == ElementType::ScrollThumb)) return false;
	if (focused) return true;
	return input.pointerPressed && !section.Contains(input.pointerPosition) && !auxiliary.Contains(input.pointerPosition);
}
/// <summary>素材を先に描き、部品の状態表現と動的な文字をその上へ重ねる。</summary>
/// <remarks>登録部品をテーマ色で描画する。</remarks>
/// <param name="renderer">矩形を画面へ描くレンダラー。</param>
/// <param name="background">UIと同じ描画で合成する任意の背景。</param>
/// <param name="strongButtonFocus">暗い画面上でボタンのフォーカスを高コントラストにする。</param>
void Root::Draw(Renderer &renderer, const DrawCommand* background, bool strongButtonFocus) const
{
	if (!endedFrame)
	{
		return;
	}

	std::vector<DrawCommand> commands;
	commands.reserve(elements.size() * 4 + (background ? 1 : 0));
	if (background)
	{
		commands.push_back(*background);
	}

	auto pushImage = [&commands](const Rect& bounds, const Rect& clip, std::string_view imageId,
		bool nineSlice, float opacity = 1.0f)
	{
		if (bounds.IsEmpty() || !UIRenderer::HasImage(imageId)) return false;
		DrawCommand command;
		command.type = nineSlice ? DrawCommandType::NineSliceImage : DrawCommandType::Image;
		command.bounds = bounds.ToNativeRect();
		command.clip = clip.ToNativeRect();
		command.imageId = imageId;
		command.color.w = opacity;
		commands.push_back(std::move(command));
		return true;
	};
	auto pushFill = [&commands](const Rect& bounds, const Rect& clip, const Color& color)
	{
		if (bounds.IsEmpty()) return;
		DrawCommand command;
		command.type = DrawCommandType::FillRectangle;
		command.bounds = bounds.ToNativeRect();
		command.clip = clip.ToNativeRect();
		command.color = color;
		commands.push_back(std::move(command));
	};
	auto pushStroke = [&commands](const Rect& bounds, const Rect& clip, const Color& color, float width)
	{
		if (bounds.IsEmpty()) return;
		DrawCommand command;
		command.type = DrawCommandType::StrokeRectangle;
		command.bounds = bounds.ToNativeRect();
		command.clip = clip.ToNativeRect();
		command.color = color;
		command.strokeWidth = width;
		commands.push_back(std::move(command));
	};
	auto pushText = [&commands](const Rect& bounds, const Rect& clip, const std::wstring& text,
		float fontSize, const Color& color, TextHorizontalAlignment alignment = TextHorizontalAlignment::Near,
		bool bold = false)
	{
		if (bounds.IsEmpty() || text.empty()) return;
		DrawCommand command;
		command.type = DrawCommandType::Text;
		command.bounds = bounds.ToNativeRect();
		command.clip = clip.ToNativeRect();
		command.text = text;
		command.fontSize = fontSize;
		command.color = color;
		command.horizontalAlignment = alignment;
		command.bold = bold;
		commands.push_back(std::move(command));
	};
	auto focused = [this](const Element& element)
	{
		return keyboardFocusActive && focusedId == element.id;
	};
	auto interactionOverlay = [&](const Element& element)
	{
		if (!element.enabled) return;
		if (capturedId == element.id) pushFill(element.bounds, element.clipBounds, Theme::PressedOverlay);
		else if (element.hovered) pushFill(element.bounds, element.clipBounds, Theme::HoverOverlay);
	};
	auto getElementText = [this](const Element& element)
	{
		std::wstring text = Wide(element.label);
		if (element.textValue || element.integerValue || element.floatValue)
		{
			const auto state = textEditStates.find(element.id);
			if (state != textEditStates.end())
			{
				text = state->second.text;
				if (element.id == focusedId && keyboardFocusActive)
					text.insert(state->second.cursor, input.compositionText + L"|");
			}
		}
		if (element.type == ElementType::Dropdown && !element.options.empty() && element.selectedIndex)
			text = Wide(element.options[*element.selectedIndex]);
		return text;
	};

	for (auto &e : elements)
	{
		if (e.bounds.Intersect(e.clipBounds).IsEmpty()) continue;
		if (e.type == ElementType::Rule)
		{
			pushFill(e.bounds, e.clipBounds, e.color.value_or(Theme::Border));
			continue;
		}
		if (e.type == ElementType::Image || e.type == ElementType::NineSliceImage)
		{
			pushImage(e.bounds, e.clipBounds, e.imageId, e.type == ElementType::NineSliceImage, e.opacity);
			continue;
		}
		if (e.type == ElementType::Panel)
		{
			if (!pushImage(e.bounds, e.clipBounds, e.imageId, true))
			{
				pushFill(e.bounds, e.clipBounds, Theme::Surface);
				pushStroke(e.bounds, e.clipBounds, Theme::Border, Theme::BorderThickness);
			}
			continue;
		}
		if (e.type == ElementType::ScrollTrack || e.type == ElementType::ScrollThumb)
		{
			pushFill(e.bounds, e.clipBounds, e.type == ElementType::ScrollThumb ? Theme::Accent : Theme::Disabled);
			continue;
		}
		if (e.type == ElementType::Button)
		{
			const bool primary = e.imageId == Theme::PrimaryButtonImage;
			Rect imageBounds = e.bounds;
			if (!e.stretchBackground)
			{
				const float imageAspect = std::max(0.01f, UIRenderer::GetImageAspectRatio(e.imageId));
				const float buttonWidth = static_cast<float>(e.bounds.right - e.bounds.left);
				const float buttonHeight = static_cast<float>(e.bounds.bottom - e.bounds.top);
				float imageWidth = buttonWidth;
				float imageHeight = buttonHeight;
				if (buttonWidth / std::max(1.0f, buttonHeight) > imageAspect)
					imageWidth = buttonHeight * imageAspect;
				else
					imageHeight = buttonWidth / imageAspect;
				const int fittedWidth = std::max(1, static_cast<int>(imageWidth + 0.5f));
				const int fittedHeight = std::max(1, static_cast<int>(imageHeight + 0.5f));
				const int centerX = (e.bounds.left + e.bounds.right) / 2;
				const int centerY = (e.bounds.top + e.bounds.bottom) / 2;
				imageBounds = {centerX - fittedWidth / 2, centerY - fittedHeight / 2,
					centerX + (fittedWidth + 1) / 2, centerY + (fittedHeight + 1) / 2};
			}
			const bool imageLoaded = pushImage(imageBounds, e.clipBounds, e.imageId,
				e.stretchBackground, e.enabled ? 1.0f : 0.48f);
			if (!imageLoaded)
			{
				pushFill(e.bounds, e.clipBounds, primary ? Theme::Accent : Theme::Surface);
				pushStroke(e.bounds, e.clipBounds, Theme::Border, Theme::BorderThickness);
			}
			interactionOverlay(e);
			if (focused(e) && e.showFocusOutline)
			{
				if (strongButtonFocus)
				{
					pushStroke(e.bounds, e.clipBounds, Color(0.06f, 0.15f, 0.17f, 1.0f), 6.0f);
					pushStroke(e.bounds, e.clipBounds, Color(0.39f, 1.0f, 0.87f, 1.0f), 3.0f);
				}
				else
				{
					pushStroke(e.bounds, e.clipBounds, Theme::Focus, 2.0f);
				}
			}

			const bool fullImageButton = !e.stretchBackground && imageLoaded;
			if (!e.iconImageId.empty() && !fullImageButton)
			{
				const int iconSize = std::clamp(e.bounds.bottom - e.bounds.top - 10, 14, 24);
				const int iconTop = e.bounds.top + ((e.bounds.bottom - e.bounds.top) - iconSize) / 2;
				const int iconWidth = static_cast<int>(iconSize * UIRenderer::GetImageAspectRatio(e.iconImageId));
				const int iconLeft = e.label.empty() ? (e.bounds.left + e.bounds.right - iconWidth) / 2 : e.bounds.left + 8;
				const Rect iconBounds{iconLeft, iconTop, iconLeft + iconWidth, iconTop + iconSize};
				pushImage(iconBounds, e.clipBounds, e.iconImageId, false, e.enabled ? 1.0f : 0.5f);
			}
			if (!fullImageButton)
			{
				Rect textBounds = e.bounds.Inset(8, 2);
				if (!e.iconImageId.empty()) textBounds.left += static_cast<int>(
					std::clamp(e.bounds.bottom - e.bounds.top - 10, 14, 24)
					* UIRenderer::GetImageAspectRatio(e.iconImageId)) + 8;
				pushText(textBounds, e.clipBounds, getElementText(e), e.fontSize,
					!e.enabled ? Theme::DisabledText : primary ? Theme::PrimaryText : Theme::Text,
					TextHorizontalAlignment::Center, e.bold);
			}
			continue;
		}
		if (e.type == ElementType::Checkbox)
		{
			interactionOverlay(e);
			const int iconSize = std::clamp(e.bounds.bottom - e.bounds.top - 8, 16, 24);
			const int iconTop = e.bounds.top + ((e.bounds.bottom - e.bounds.top) - iconSize) / 2;
			const Rect iconBounds{e.bounds.left + 5, iconTop, e.bounds.left + 5 + iconSize, iconTop + iconSize};
			const bool checked = e.booleanValue && *e.booleanValue;
			const std::string_view checkboxImage = checked ? Theme::CheckboxOnImage : Theme::CheckboxOffImage;
			if (!pushImage(iconBounds, e.clipBounds, checkboxImage, false, e.enabled ? 1.0f : 0.48f))
			{
				pushFill(iconBounds, e.clipBounds, Theme::Surface);
				pushStroke(iconBounds, e.clipBounds, Theme::Border, 1.0f);
				if (checked) pushText(iconBounds, e.clipBounds, L"✓", 17.0f, Theme::Accent, TextHorizontalAlignment::Center);
			}
			if (focused(e)) pushStroke(e.bounds, e.clipBounds, Theme::Focus, 2.0f);
			const Rect textBounds{iconBounds.right + 8, e.bounds.top + 2, e.bounds.right - 6, e.bounds.bottom - 2};
			pushText(textBounds, e.clipBounds, getElementText(e), e.fontSize,
				e.enabled ? Theme::Text : Theme::DisabledText);
			continue;
		}
		if (e.type == ElementType::Label)
		{
			pushText(e.bounds.Inset(4, 1), e.clipBounds, getElementText(e), e.fontSize,
				e.enabled ? e.color.value_or(Theme::Text) : Theme::DisabledText,
				e.centered ? TextHorizontalAlignment::Center : TextHorizontalAlignment::Near, e.bold);
			continue;
		}

		std::string_view fieldImage = Theme::TextFieldImage;
		if (e.type == ElementType::Dropdown) fieldImage = Theme::ButtonImage;
		const bool selectedOption = e.type == ElementType::DropdownOption && e.selectedIndex
			&& *e.selectedIndex == e.optionIndex;
		// 候補行は隙間のない不透明な面で重ね、背後の項目を完全に隠す。
		if (e.type == ElementType::DropdownOption)
		{
			pushFill(e.bounds, e.clipBounds, Theme::Surface);
			if (selectedOption) pushFill(e.bounds, e.clipBounds, Theme::HoverOverlay);
		}
		else if (!pushImage(e.bounds, e.clipBounds, fieldImage, true, e.enabled ? 1.0f : 0.5f))
		{
			pushFill(e.bounds, e.clipBounds, Theme::Surface);
			pushStroke(e.bounds, e.clipBounds, Theme::Border, Theme::BorderThickness);
		}
		interactionOverlay(e);
		if (focused(e)) pushStroke(e.bounds, e.clipBounds, Theme::Focus, 2.0f);

		Rect textBounds = e.bounds.Inset(8, 2);
		if (e.type == ElementType::IntegerInput || e.type == ElementType::FloatInput)
		{
			// 背景だけを伸縮し、増減アイコンはクリック領域内で縦横比を維持する。
			const int width = std::min(28, std::max(20, (e.bounds.right - e.bounds.left) / 5));
			const int middle = (e.bounds.top + e.bounds.bottom) / 2;
			textBounds.right -= width;
			for (int index = 0; index < 2; ++index)
			{
				const char* icon = index == 0 ? "up" : "down";
				const float aspect = UIRenderer::GetImageAspectRatio(icon);
				const int availableHeight = std::max(1, std::min(7, (e.bounds.bottom - e.bounds.top) / 2 - 4));
				const int iconWidth = std::max(1, std::min(width - 8, static_cast<int>(availableHeight * aspect)));
				const int iconHeight = std::max(1, static_cast<int>(iconWidth / aspect));
				const int left = e.bounds.right - width + (width - iconWidth) / 2;
				const int top = (index == 0 ? e.bounds.top : middle) + (middle - e.bounds.top - iconHeight) / 2;
				pushImage({left, top, left + iconWidth, top + iconHeight}, e.clipBounds,
					icon, false, e.enabled ? 1.0f : 0.45f);
			}
		}
		if (e.type == ElementType::Dropdown)
		{
			textBounds.right -= 24;
			const int arrowWidth = std::min(14, std::max(8, e.bounds.right - e.bounds.left - 8));
			const int arrowHeight = std::min(10, std::max(6, e.bounds.bottom - e.bounds.top - 6));
			const Rect arrowBounds{e.bounds.right - arrowWidth - 8,
				e.bounds.top + ((e.bounds.bottom - e.bounds.top) - arrowHeight) / 2,
				e.bounds.right - 8,
				e.bounds.top + ((e.bounds.bottom - e.bounds.top) + arrowHeight) / 2};
			pushImage(arrowBounds, e.clipBounds, Theme::DropdownArrowImage, false, e.enabled ? 1.0f : 0.45f);
		}
		const bool numericInput = e.type == ElementType::IntegerInput || e.type == ElementType::FloatInput;
		pushText(textBounds, e.clipBounds, getElementText(e), e.fontSize,
			e.enabled ? Theme::Text : Theme::DisabledText,
			numericInput ? TextHorizontalAlignment::Center : TextHorizontalAlignment::Near, e.bold);
	}
	UIRenderer::Draw(renderer, commands);
}
/// <summary>指定ボタンがこのフレームに確定されたかを返す。</summary>
/// <param name="id">判定するボタンID。</param>
/// <returns>このフレームに押下確定した場合は true。</returns>
bool Root::WasActivated(std::string_view id) const
{
	auto e = FindElement(std::string(id));
	return endedFrame && e && e->activated;
}
/// <summary>指定部品の値がこのフレームに変更されたかを返す。</summary>
/// <param name="id">確認する部品ID。</param>
/// <returns>値が変更されていればtrue。</returns>
bool Root::WasChanged(std::string_view id) const
{
	auto e = FindElement(std::string(id));
	return endedFrame && e && e->changed;
}
/// <summary>ポインター入力を UI が捕捉中、または UI 上にあるかを返す。</summary>
/// <returns>UI側でポインターを処理すべき場合は true。</returns>
bool Root::WantsPointerCapture() const
{
	if (!capturedId.empty() || !expandedDropdownId.empty())
		return true;
	for (auto &e : elements)
		if (e.type != ElementType::Rule && e.type != ElementType::Image && e.type != ElementType::NineSliceImage &&
			e.bounds.Intersect(e.clipBounds).Contains(input.pointerPosition))
			return true;
	return wheelConsumed;
}
/// <summary>キーボード移動先があり、UI 操作中かを返す。</summary>
/// <returns>UIがフォーカス入力を受けている場合は true。</returns>
bool Root::WantsKeyboardCapture() const
{
	return keyboardFocusActive && FindElement(focusedId);
}
/// <summary>IME文字入力を受け取るテキスト欄がフォーカス中か返す。</summary>
/// <returns>テキスト入力が必要ならtrue。</returns>
bool Root::WantsTextInput() const
{
	auto e = FindElement(focusedId);
	return keyboardFocusActive && e && (e->textValue || e->integerValue || e->floatValue);
}
/// <summary>現在フォーカス中の部品 ID を返す。</summary>
/// <returns>フォーカス中の安定ID。登録部品がなければ空文字。</returns>
const std::string &Root::GetFocusedId() const
{
	return focusedId;
}
} // namespace GameUI
