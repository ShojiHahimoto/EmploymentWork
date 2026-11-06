#pragma once

#include <SimpleMath.h>
#include <Windows.h>
#include <d3d11.h>

#include <string>
#include <string_view>
#include <vector>

class Renderer;

namespace GameUI
{
	enum class DrawCommandType
	{
		FillRectangle,
		StrokeRectangle,
		Line,
		Text
	};

	enum class TextHorizontalAlignment
	{
		Near,
		Center,
		Far
	};

	enum class TextVerticalAlignment
	{
		Near,
		Center,
		Far
	};

	/// <summary>画面上へ順番に重ねて描くUI要素。</summary>
	struct DrawCommand
	{
		DrawCommandType type = DrawCommandType::FillRectangle; // 描画内容。
		RECT bounds = {}; // クライアント座標上の描画範囲。
		RECT clip = {}; // Empty means the complete surface.
		POINT lineStart = {}; // Line用の始点。
		POINT lineEnd = {}; // Line用の終点。
		DirectX::SimpleMath::Color color = DirectX::SimpleMath::Color(1.0f, 1.0f, 1.0f, 1.0f); // 塗り・文字色。
		std::wstring text; // UTF-16文字列。日本語とIME入力を直接描画できる。
		float fontSize = 18.0f; // 96 DPI基準の文字サイズ。
		float strokeWidth = 1.0f; // 枠線幅。
		TextHorizontalAlignment horizontalAlignment = TextHorizontalAlignment::Near; // 横方向の文字揃え。
		TextVerticalAlignment verticalAlignment = TextVerticalAlignment::Center; // 縦方向の文字揃え。
		bool wrapText = false; // trueなら矩形内で折り返す。
	};

	/// <summary>
	/// DirectWrite/Direct2Dで日本語を含むUIを描画し、Direct3D11の画面へ合成する。
	/// ImGuiとは独立した、リリースビルドでも利用できる描画経路。
	/// </summary>
	class UIRenderer
	{
	public:
		/// <summary>Direct2D、DirectWrite、WICと画面合成用テクスチャを初期化する。</summary>
		/// <param name="device">描画テクスチャを作るDirect3D11デバイス。</param>
		/// <param name="width">クライアント領域の幅。</param>
		/// <param name="height">クライアント領域の高さ。</param>
		/// <returns>必要な描画リソースをすべて作成できた場合はtrue。</returns>
		static bool Initialize(ID3D11Device* device, int width, int height);
		/// <summary>ウィンドウ終了時にUI描画リソースを解放する。</summary>
		static void Shutdown();
		/// <summary>ウィンドウサイズに合わせて合成先を作り直す。</summary>
		/// <param name="width">新しいクライアント領域の幅。</param>
		/// <param name="height">新しいクライアント領域の高さ。</param>
		/// <returns>新しいサイズのリソースを作成できた場合はtrue。</returns>
		static bool Resize(int width, int height);
		/// <summary>描画要求を登録順に合成し、透明なUIレイヤーとして画面へ描く。</summary>
		/// <param name="renderer">最終合成に使うDirect3D11レンダラー。</param>
		/// <param name="commands">矩形・文字の描画要求。</param>
		static void Draw(Renderer& renderer, const std::vector<DrawCommand>& commands);
		/// <summary>描画リソースが利用可能か返す。</summary>
		/// <returns>初期化済みならtrue。</returns>
		static bool IsInitialized();
		/// <summary>描画に使うフォントサイズで文字列の横幅を測る。</summary>
		/// <param name="text">測定するUTF-16文字列。</param>
		/// <param name="fontSize">96 DPI基準の文字サイズ。</param>
		/// <returns>文字列の横幅。描画前または失敗時は0。</returns>
		static float MeasureText(std::wstring_view text, float fontSize);

	private:
		static bool CreateSurfaceResources();
		static void ReleaseSurfaceResources();
	};
}
