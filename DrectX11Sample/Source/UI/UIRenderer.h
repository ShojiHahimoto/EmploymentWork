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
		bool bold = false; // 文字を太字で描画する。
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
		static bool Initialize(ID3D11Device* device, int width, int height);
		static void Shutdown();
		static bool Resize(int width, int height);
		struct PreparedImage
		{
			int width = 0;
			int height = 0;
			int sliceLeft = 0;
			int sliceTop = 0;
			int sliceRight = 0;
			int sliceBottom = 0;
			std::vector<unsigned char> pixels;
		};

		using PreparedImages = std::unordered_map<std::string, PreparedImage>;
		static PreparedImages PrepareAssets(const std::string &manifestPath, const std::vector<std::string> &imageIds);
		static bool UploadAssets(const PreparedImages &images);
		static void UnloadAssets();
		static void Draw(Renderer& renderer, const std::vector<DrawCommand>& commands);
		static bool IsInitialized();
		static bool HasImage(std::string_view imageId);
		static float GetImageAspectRatio(std::string_view imageId);
		static float MeasureText(std::wstring_view text, float fontSize, bool bold = false);

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
