#pragma once

#include <SimpleMath.h>
#include <Windows.h>
#include <d3d11.h>
#include <d2d1.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

class Renderer;

namespace GameUI
{
	enum class DrawCommandType
	{
		FillRectangle,
		StrokeRectangle,
		Line,
		Text,
		Image,
		NineSliceImage
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
		std::string imageId; // assets.jsonで登録した画像ID。
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
		/// <summary>画像一覧JSONを読み、PNGのWICソースとDirect2D描画用ビットマップをキャッシュする。</summary>
		/// <param name="manifestPath">UI画像メタデータJSONへのパス。</param>
		/// <returns>一覧を読み込み、少なくとも1枚の画像を利用可能にできた場合はtrue。</returns>
		static bool LoadAssets(const std::string& manifestPath);
		/// <summary>画像キャッシュだけを解放する。</summary>
		static void UnloadAssets();
		/// <summary>矩形・文字・画像の描画要求を登録順に合成し、透明なUIレイヤーとして画面へ描く。</summary>
		/// <param name="renderer">最終合成に使うDirect3D11レンダラー。</param>
		/// <param name="commands">矩形・文字・画像の描画要求。</param>
		static void Draw(Renderer& renderer, const std::vector<DrawCommand>& commands);
		/// <summary>描画リソースが利用可能か返す。</summary>
		/// <returns>初期化済みならtrue。</returns>
		static bool IsInitialized();
		/// <summary>名前付きUI画像がロード済みか確認する。</summary>
		/// <param name="imageId">画像一覧JSONに登録された画像名。</param>
		/// <returns>描画可能なDirect2Dビットマップがあればtrue。</returns>
		static bool HasImage(std::string_view imageId);
		/// <summary>画像の横幅と高さの比率を返す。</summary>
		/// <param name="imageId">登録された画像名。</param>
		/// <returns>横幅/高さ。未登録の場合は1。</returns>
		static float GetImageAspectRatio(std::string_view imageId);
		/// <summary>描画に使うフォントサイズで文字列の横幅を測る。</summary>
		/// <param name="text">測定するUTF-16文字列。</param>
		/// <param name="fontSize">96 DPI基準の文字サイズ。</param>
		/// <returns>文字列の横幅。描画前または失敗時は0。</returns>
		static float MeasureText(std::wstring_view text, float fontSize);

	private:
		struct ImageAsset
		{
			Microsoft::WRL::ComPtr<IWICBitmap> source;
			Microsoft::WRL::ComPtr<ID2D1Bitmap> targetBitmap;
			int width = 0;
			int height = 0;
			int sliceLeft = 0;
			int sliceTop = 0;
			int sliceRight = 0;
			int sliceBottom = 0;
		};
		static std::unordered_map<std::string, ImageAsset> imageAssets;
		static bool CreateTargetBitmaps();
		static bool CreateSurfaceResources();
		static void ReleaseSurfaceResources();
	};
}
