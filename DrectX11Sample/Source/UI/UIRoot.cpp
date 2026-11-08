#include "UI/UIRoot.h"

#include "System/Renderer.h"
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
void DrawBorder(Renderer &renderer, const Rect &bounds, const Color &color)
{
	const Rect top{bounds.left, bounds.top, bounds.right, bounds.top + 1};
	const Rect bottom{bounds.left, bounds.bottom - 1, bounds.right, bounds.bottom};
	const Rect left{bounds.left, bounds.top, bounds.left + 1, bounds.bottom};
	const Rect right{bounds.right - 1, bounds.top, bounds.right, bounds.bottom};
	for (const Rect &edge : {top, bottom, left, right})
	{
		const Rect clipped = edge.Intersect(bounds);
		if (!clipped.IsEmpty())
		{
			renderer.DrawScreenRect(clipped.ToNativeRect(), color);
		}
	}
}

const Color Theme::Background(0.94f, 0.96f, 0.97f, 1.0f);
const Color Theme::Surface(1.0f, 1.0f, 1.0f, 1.0f);
const Color Theme::Border(0.72f, 0.78f, 0.81f, 1.0f);
const Color Theme::Accent(0.08f, 0.48f, 0.50f, 1.0f);
const Color Theme::Hover(0.86f, 0.94f, 0.94f, 1.0f);
const Color Theme::Pressed(0.73f, 0.88f, 0.88f, 1.0f);
const Color Theme::Disabled(0.87f, 0.89f, 0.90f, 1.0f);
const Color Theme::Focus(0.05f, 0.42f, 0.44f, 1.0f);

bool Rect::IsEmpty() const
{
	return right <= left || bottom <= top;
}

bool Rect::Contains(POINT point) const
{
	return point.x >= left && point.x < right && point.y >= top && point.y < bottom;
}

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

Rect Rect::Inset(int horizontal, int vertical) const
{
	return Rect{left + horizontal, top + vertical, right - horizontal, bottom - vertical};
}

RECT Rect::ToNativeRect() const
{
	return RECT{left, top, right, bottom};
}

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
void Root::BeginFrame(const Rect &v, const InputFrame &f)
{
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
void Root::AddPanel(const Rect &b)
{
	Element e;
	e.bounds = b;
	RegisterElement(e);
}
void Root::AddButton(std::string_view id, const Rect &b, bool enabled)
{
	AddButton(id, b, std::string_view{}, enabled);
}
void Root::AddButton(std::string_view id, const Rect &b, std::string_view text, bool enabled)
{
	Element e;
	e.type = ElementType::Button;
	e.id = id;
	e.bounds = b;
	e.label = text;
	e.enabled = enabled;
	RegisterElement(e);
}
void Root::AddLabel(std::string_view id, const Rect &b, std::string_view text, bool enabled, float size)
{
	Element e;
	e.type = ElementType::Label;
	e.id = id;
	e.bounds = b;
	e.label = text;
	e.enabled = enabled;
	e.fontSize = size;
	RegisterElement(e);
}
void Root::AddTextInput(std::string_view id, const Rect &b, std::string &value, size_t limit, bool enabled)
{
	Element e;
	e.type = ElementType::TextInput;
	e.id = id;
	e.bounds = b;
	e.textValue = &value;
	e.maxUtf8Bytes = limit;
	e.enabled = enabled;
	RegisterElement(e);
}
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
void Root::AddCheckbox(std::string_view id, const Rect &b, std::string_view text, bool &value, bool enabled)
{
	Element e;
	e.type = ElementType::Checkbox;
	e.id = id;
	e.bounds = b;
	e.label = text;
	e.booleanValue = &value;
	e.enabled = enabled;
	RegisterElement(e);
}
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
Root::Element *Root::FindButton(const std::string &id)
{
	return FindElement(id);
}
const Root::Element *Root::FindButton(const std::string &id) const
{
	return FindElement(id);
}
bool Root::IsFocusable(const Element &e) const
{
	return e.enabled && !e.id.empty() && e.type != ElementType::Panel && e.type != ElementType::Label && e.type != ElementType::ScrollTrack && e.type != ElementType::ScrollThumb &&
		   !e.bounds.Intersect(e.clipBounds).IsEmpty();
}
const Root::Element *Root::FindTopButtonAt(POINT p) const
{
	for (auto i = elements.rbegin(); i != elements.rend(); ++i)
		if (i->bounds.Intersect(i->clipBounds).Contains(p))
			return IsFocusable(*i) ? &*i : nullptr;
	return nullptr;
}
Root::Element *Root::FindTopButtonAt(POINT p)
{
	return const_cast<Element *>(std::as_const(*this).FindTopButtonAt(p));
}
Root::Element *Root::FindTopInteractiveAt(POINT p)
{
	return FindTopButtonAt(p);
}
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
		}
		int direction = Pressed(input, Key::Up) ? 1 : Pressed(input, Key::Down) ? -1 : 0;
		if (!wheelConsumed && e->bounds.Intersect(e->clipBounds).Contains(input.pointerPosition) &&
			input.wheelDelta)
		{
			direction = input.wheelDelta / 120;
			wheelConsumed = true;
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
void Root::EndFrame()
{
	// 全部品の登録後に入力を一度だけ解決する。描画から更新処理を再実行しない。
	if (endedFrame)
		return;
	EnsureValidFocus();
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
	std::string previous = focusedId;
	if (input.focusLost)
	{
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
	for (auto &e : elements)
		e.hovered = (&e == FindTopInteractiveAt(input.pointerPosition));
	for (auto it = textEditStates.begin(); it != textEditStates.end();)
		if (!FindElement(it->first))
			it = textEditStates.erase(it);
		else
			++it;
	endedFrame = true;
}
void Root::Draw(Renderer &renderer) const
{
	if (!endedFrame)
		return;
	std::vector<DrawCommand> commands;
	for (auto &e : elements)
	{
		Rect clip = e.bounds.Intersect(e.clipBounds);
		if (clip.IsEmpty())
			continue;
		DrawCommand c;
		c.bounds = e.bounds.ToNativeRect();
		c.clip = e.clipBounds.ToNativeRect();
		if (e.type == ElementType::ScrollTrack || e.type == ElementType::ScrollThumb)
		{
			c.color = e.type == ElementType::ScrollThumb ? Theme::Accent : Theme::Disabled;
			commands.push_back(c);
			continue;
		}
		if (e.type != ElementType::Label)
		{
			c.color = !e.enabled		   ? Theme::Disabled
					  : !capturedId.empty() && e.id == capturedId ? Theme::Pressed
					  : e.hovered		   ? Theme::Hover
										   : Theme::Surface;
			commands.push_back(c);
			c.type = DrawCommandType::StrokeRectangle;
			c.color = keyboardFocusActive && focusedId == e.id ? Theme::Focus : Theme::Border;
			commands.push_back(c);
		}
		std::wstring label = Wide(e.label);
		if (e.textValue || e.integerValue || e.floatValue)
		{
			auto s = textEditStates.find(e.id);
			if (s != textEditStates.end())
			{
				label = s->second.text;
				if (e.id == focusedId && keyboardFocusActive)
					label.insert(s->second.cursor, input.compositionText + L"|");
			}
		}
		if (e.booleanValue)
			label = (*e.booleanValue ? L"[x] " : L"[ ] ") + label;
		if (e.type == ElementType::Dropdown && !e.options.empty())
			label = Wide(e.options[*e.selectedIndex]) + L" v";
		c.type = DrawCommandType::Text;
		c.bounds = e.bounds.Inset(6, 2).ToNativeRect();
		c.text = label;
		c.fontSize = e.fontSize;
		if (e.type == ElementType::Button)
			c.horizontalAlignment = TextHorizontalAlignment::Center;
		c.color = Color(0.12f, 0.16f, 0.18f, e.enabled ? 1.0f : 0.5f);
		commands.push_back(c);
	}
	UIRenderer::Draw(renderer, commands);
}
bool Root::WasActivated(std::string_view id) const
{
	auto e = FindElement(std::string(id));
	return endedFrame && e && e->activated;
}
bool Root::WasChanged(std::string_view id) const
{
	auto e = FindElement(std::string(id));
	return endedFrame && e && e->changed;
}
bool Root::WantsPointerCapture() const
{
	if (!capturedId.empty() || !expandedDropdownId.empty())
		return true;
	for (auto &e : elements)
		if (e.bounds.Intersect(e.clipBounds).Contains(input.pointerPosition))
			return true;
	return wheelConsumed;
}
bool Root::WantsKeyboardCapture() const
{
	return keyboardFocusActive && FindElement(focusedId);
}
bool Root::WantsTextInput() const
{
	auto e = FindElement(focusedId);
	return keyboardFocusActive && e && (e->textValue || e->integerValue || e->floatValue);
}
const std::string &Root::GetFocusedId() const
{
	return focusedId;
}
} // namespace GameUI
