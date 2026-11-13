#pragma once
#include "UI/UIRoot.h"
#include "UI/UIRenderer.h"
#include <algorithm>
#include <cmath>

namespace GameUI
{
/// <summary>作成種別の専用UI。遷移処理を持たず、操作結果だけをControllerへ返す。</summary>
class CustomizeMenuView
{
public:
	/// <summary>マウス操作を解決する。静止カーソルでは選択を奪わない。</summary>
	/// <param name="frame">確定済み入力。</param>
	/// <param name="w">画面幅。</param>
	/// <param name="h">画面高さ。</param>
	/// <param name="selection">共有する選択番号。</param>
	/// <returns>0:技、1:キャラクター、2:戻る、-1:操作なし。</returns>
	int Update(const InputFrame& frame, int w, int h, int& selection, bool categories = false)
	{
		width = w; height = h;
		InputFrame pointer = frame;
		pointer.focusMove = 0; pointer.activatePressed = false; pointer.keysPressed.fill(false);
		root.BeginFrame({0,0,w,h}, pointer);
		root.AddButton("back", Map({18,12,96,66}));
		const int count = categories ? 3 : 2;
		for (int i=0;i<count;++i) root.AddButton(std::to_string(i), Map(ButtonRect(i,categories)));
		root.EndFrame();
		if (frame.pointerDelta.x || frame.pointerDelta.y || frame.pointerPressed)
		{
			for (int i=0;i<count;++i)
				if (Map(ButtonRect(i,categories)).Contains(frame.pointerPosition)) selection=i;
		}
		if (root.WasActivated("back")) return count;
		for (int i=0;i<count;++i) if (root.WasActivated(std::to_string(i))) return i;
		return -1;
	}

	/// <summary>確定した選択色で描画する。右側の画像領域は意図的に空ける。</summary>
	/// <param name="renderer">共通描画先。</param>
	/// <param name="w">画面幅。</param>
	/// <param name="h">画面高さ。</param>
	/// <param name="selection">現在選択している項目。</param>
	void Draw(Renderer& renderer, int w, int h, int selection, bool categories = false)
	{
		width=w; height=h;
		std::vector<DrawCommand> commands;
		DrawCommand background; background.bounds={0,0,w,h};
		background.color=DirectX::SimpleMath::Color(0.96f,0.965f,0.98f,1);
		commands.push_back(background);
		const auto image = [&](const char* id, const Rect& bounds, bool slice=false)
		{
			DrawCommand c; c.type=slice ? DrawCommandType::NineSliceImage : DrawCommandType::Image;
			c.bounds=Map(bounds).ToNativeRect(); c.imageId=id; commands.push_back(c);
		};
		const auto text = [&](const wchar_t* value, const Rect& bounds, float size, bool selected, bool centered)
		{
			DrawCommand c; c.type=DrawCommandType::Text; c.bounds=Map(bounds).ToNativeRect();
			c.text=value; c.fontSize=size; c.bold=true;
			c.color=selected ? DirectX::SimpleMath::Color(1,1,1,1) : DirectX::SimpleMath::Color(0.08f,0.09f,0.12f,1);
			c.horizontalAlignment=centered ? TextHorizontalAlignment::Center : TextHorizontalAlignment::Near;
			const float measured=UIRenderer::MeasureText(c.text,size,true);
			const float available=static_cast<float>(c.bounds.right-c.bounds.left);
			if (measured>available && measured>0) c.fontSize*=std::max(0.0f,available)/measured;
			commands.push_back(c);
		};
		image("header",{0,0,1672,80},true);
		image("header_accent",{103,0,155,80});
		image("button",{18,12,96,66},true);
		image("back",{37,25,78,53});
		text(categories ? L"技作成" : L"カスタマイズ",{166,2,820,77},48,false,false);
		// 上下の透明余白を含めて配置し、可視輪郭を上の固定入力矩形に合わせる。
		const wchar_t* labels[] = { L"地上技", L"空中技", L"必殺技" };
		for (int i=0;i<(categories ? 3 : 2);++i)
		{
			const Rect button=ButtonRect(i,categories);
			image(selection==i ? "customize_menu_selected" : "customize_menu_normal",
				{button.left-5,button.top-54,button.right+5,button.top+206});
			text(categories ? labels[i] : i==0 ? L"技作成" : L"キャラクター作成",
				button.Inset(20,0),40,selection==i,true);
		}
		UIRenderer::Draw(renderer,commands);
	}

private:
	/// <summary>入力と描画で同じボタン配置を共有する。</summary>
	/// <param name="index">上からの項目番号。</param>
	/// <param name="categories">3カテゴリの配置か。</param>
	/// <returns>基準解像度上のボタン矩形。</returns>
	static Rect ButtonRect(int index, bool categories)
	{
		const int top=(categories ? 220 : 299)+index*(categories ? 190 : 210);
		return {91,top,617,top+136};
	}
	/// <summary>画像比率を維持して基準1672x941画面を中央配置する。</summary>
	/// <param name="r">基準矩形。</param>
	/// <returns>画面上の矩形。</returns>
	Rect Map(const Rect& r) const
	{
		const float s=std::min(width/1672.0f,height/941.0f);
		const float x=(width-1672*s)/2, y=(height-941*s)/2;
		return {static_cast<int>(std::lround(x+r.left*s)),static_cast<int>(std::lround(y+r.top*s)),
			static_cast<int>(std::lround(x+r.right*s)),static_cast<int>(std::lround(y+r.bottom*s))};
	}
	Root root; // 押し始めたボタンの捕捉と外側解放を共通基盤で処理する。
	int width=1, height=1;
};
}
