#include "UI/UIRenderer.h"

#include "System/Renderer.h"
#include "Data/JsonValue.h"
#include "System/Debugger.h"

#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <new>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "windowscodecs.lib")

namespace GameUI
{
float UIRenderer::GetImageAspectRatio(std::string_view imageId)
{
	const auto found = imageAssets.find(std::string(imageId));
	return found != imageAssets.end() && found->second.height > 0
		? static_cast<float>(found->second.width) / found->second.height : 1.0f;
}
using Microsoft::WRL::ComPtr;
std::unordered_map<std::string, UIRenderer::ImageAsset> UIRenderer::imageAssets;

namespace
{
ComPtr<ID2D1Factory> d2dFactory;
ComPtr<IDWriteFactory> textFactory;
ComPtr<IWICImagingFactory> imagingFactory;
ComPtr<IWICBitmap> surfaceBitmap;
ComPtr<ID2D1RenderTarget> surfaceTarget;
ComPtr<ID2D1SolidColorBrush> brush;
ComPtr<ID3D11Texture2D> compositeTexture;
ComPtr<ID3D11ShaderResourceView> compositeTextureView;
ComPtr<IDWriteFontCollection> privateFontCollection;
ComPtr<IDWriteFontCollectionLoader> privateFontCollectionLoader;
std::unordered_map<int, ComPtr<IDWriteTextFormat>> textFormats;
std::vector<std::wstring> privateFontFiles;
ID3D11Device *device = nullptr;
int surfaceWidth = 0;
int surfaceHeight = 0;
bool initialized = false;
std::vector<DrawCommand> cachedCommands;

std::filesystem::path ResolveManifestPath(const std::string& manifestPath);

/// <summary>ファイルパス一覧をDirectWriteへ渡すカスタムフォント列挙子。</summary>
class PrivateFontFileEnumerator final : public IDWriteFontFileEnumerator
{
public:
	PrivateFontFileEnumerator(IDWriteFactory* factory, const std::vector<std::wstring>& files)
		: factory(factory), files(files)
	{
	}

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override
	{
		if (!object) return E_POINTER;
		*object = nullptr;
		if (iid == __uuidof(IUnknown) || iid == __uuidof(IDWriteFontFileEnumerator))
		{
			*object = static_cast<IDWriteFontFileEnumerator*>(this);
			AddRef();
			return S_OK;
		}
		return E_NOINTERFACE;
	}
	ULONG STDMETHODCALLTYPE AddRef() override { return ++references; }
	ULONG STDMETHODCALLTYPE Release() override
	{
		const ULONG remaining = --references;
		if (remaining == 0) delete this;
		return remaining;
	}
	HRESULT STDMETHODCALLTYPE MoveNext(BOOL* hasCurrentFile) override
	{
		if (!hasCurrentFile) return E_POINTER;
		if (beforeFirst)
		{
			beforeFirst = false;
			currentIndex = 0;
		}
		else if (currentIndex < files.size())
		{
			++currentIndex;
		}
		currentFile.Reset();
		*hasCurrentFile = currentIndex < files.size() ? TRUE : FALSE;
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE GetCurrentFontFile(IDWriteFontFile** fontFile) override
	{
		if (!fontFile) return E_POINTER;
		*fontFile = nullptr;
		if (beforeFirst || currentIndex >= files.size()) return E_FAIL;
		if (!currentFile)
		{
			const HRESULT hr = factory->CreateFontFileReference(files[currentIndex].c_str(), nullptr,
				currentFile.GetAddressOf());
			if (FAILED(hr)) return hr;
		}
		*fontFile = currentFile.Get();
		(*fontFile)->AddRef();
		return S_OK;
	}

private:
	std::atomic<ULONG> references{1};
	ComPtr<IDWriteFactory> factory;
	std::vector<std::wstring> files;
	ComPtr<IDWriteFontFile> currentFile;
	size_t currentIndex = 0;
	bool beforeFirst = true;
};

/// <summary>登録した同梱フォント一覧からDirectWriteの列挙子を生成する。</summary>
class PrivateFontCollectionLoader final : public IDWriteFontCollectionLoader
{
public:
	explicit PrivateFontCollectionLoader(const std::vector<std::wstring>& files) : files(files) {}

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override
	{
		if (!object) return E_POINTER;
		*object = nullptr;
		if (iid == __uuidof(IUnknown) || iid == __uuidof(IDWriteFontCollectionLoader))
		{
			*object = static_cast<IDWriteFontCollectionLoader*>(this);
			AddRef();
			return S_OK;
		}
		return E_NOINTERFACE;
	}
	ULONG STDMETHODCALLTYPE AddRef() override { return ++references; }
	ULONG STDMETHODCALLTYPE Release() override
	{
		const ULONG remaining = --references;
		if (remaining == 0) delete this;
		return remaining;
	}
	HRESULT STDMETHODCALLTYPE CreateEnumeratorFromKey(IDWriteFactory* factory, const void* collectionKey,
		UINT32 collectionKeySize, IDWriteFontFileEnumerator** enumerator) override
	{
		if (!factory || !collectionKey || collectionKeySize != sizeof(uint32_t) || !enumerator)
			return E_INVALIDARG;
		*enumerator = new (std::nothrow) PrivateFontFileEnumerator(factory, files);
		return *enumerator ? S_OK : E_OUTOFMEMORY;
	}

private:
	std::atomic<ULONG> references{1};
	std::vector<std::wstring> files;
};

void UnregisterPrivateFonts()
{
	privateFontCollection.Reset();
	if (privateFontCollectionLoader && textFactory)
		textFactory->UnregisterFontCollectionLoader(privateFontCollectionLoader.Get());
	privateFontCollectionLoader.Reset();
	privateFontFiles.clear();
}

bool RegisterPrivateFonts()
{
	const std::filesystem::path manifest = ResolveManifestPath("assets/texture/UI/Parts/assets.json");
	const std::filesystem::path fontDirectory = manifest.parent_path().parent_path().parent_path()
		.parent_path() / "font" / "static";
	const std::filesystem::path files[] = {
		fontDirectory / "NotoSansJP-Regular.ttf",
		fontDirectory / "NotoSansJP-Bold.ttf"};
	for (const auto& path : files)
	{
		std::error_code error;
		if (!std::filesystem::is_regular_file(path, error)) continue;
		privateFontFiles.push_back(std::filesystem::absolute(path, error).wstring());
	}
	if (privateFontFiles.empty())
	{
		DebugLog("[UI] Noto Sans JP font files were not registered; using system fallback.");
		return false;
	}

	ComPtr<PrivateFontCollectionLoader> loader;
	loader.Attach(new (std::nothrow) PrivateFontCollectionLoader(privateFontFiles));
	if (!loader)
	{
		UnregisterPrivateFonts();
		return false;
	}
	HRESULT hr = textFactory->RegisterFontCollectionLoader(loader.Get());
	if (SUCCEEDED(hr))
	{
		privateFontCollectionLoader = loader;
		const uint32_t collectionKey = 1;
		hr = textFactory->CreateCustomFontCollection(loader.Get(), &collectionKey, sizeof(collectionKey),
			privateFontCollection.GetAddressOf());
	}
	BOOL hasExpectedFamily = FALSE;
	UINT32 familyIndex = 0;
	if (SUCCEEDED(hr))
		hr = privateFontCollection->FindFamilyName(L"Noto Sans JP", &familyIndex, &hasExpectedFamily);
	if (SUCCEEDED(hr) && !hasExpectedFamily) hr = DWRITE_E_NOFONT;
	if (FAILED(hr))
	{
		DebugLog("[UI] Could not create a private Noto Sans JP font collection. hr=", hr);
		UnregisterPrivateFonts();
		return false;
	}
	DebugLog("[UI] Registered bundled Noto Sans JP regular and bold fonts.");
	return true;
}

bool Same(const DrawCommand &a, const DrawCommand &b)
{
	return EqualRect(&a.bounds, &b.bounds) && EqualRect(&a.clip, &b.clip) && a.type == b.type &&
		   a.text == b.text && a.imageId == b.imageId && a.color == b.color && a.fontSize == b.fontSize &&
		   a.strokeWidth == b.strokeWidth && a.bold == b.bold && a.horizontalAlignment == b.horizontalAlignment &&
		   a.verticalAlignment == b.verticalAlignment && a.wrapText == b.wrapText &&
		   a.lineStart.x == b.lineStart.x && a.lineStart.y == b.lineStart.y && a.lineEnd.x == b.lineEnd.x &&
		   a.lineEnd.y == b.lineEnd.y;
}

D2D1_COLOR_F ToD2DColor(const DirectX::SimpleMath::Color &color)
{
	return D2D1::ColorF(color.x, color.y, color.z, color.w);
}

D2D1_RECT_F ToD2DRect(const RECT &rect)
{
	return D2D1::RectF(static_cast<float>(rect.left), static_cast<float>(rect.top),
					   static_cast<float>(rect.right), static_cast<float>(rect.bottom));
}

std::filesystem::path ResolveManifestPath(const std::string& manifestPath)
{
	const std::filesystem::path requested(manifestPath);
	std::error_code error;
	if (requested.is_absolute() && std::filesystem::is_regular_file(requested, error)) return requested;
	if (std::filesystem::is_regular_file(requested, error)) return std::filesystem::absolute(requested, error);

	auto searchAncestors = [&](std::filesystem::path current) -> std::filesystem::path
	{
		for (int depth = 0; depth < 10 && !current.empty(); ++depth)
		{
			const std::filesystem::path candidates[] = {
				current / requested,
				current / "DrectX11Sample" / requested,
				current / "assets/texture/UI/Parts/assets.json"};
			for (const auto& candidate : candidates)
				if (std::filesystem::is_regular_file(candidate, error)) return candidate;
			const auto parent = current.parent_path();
			if (parent == current) break;
			current = parent;
		}
		return {};
	};

	std::filesystem::path current = std::filesystem::current_path(error);
	if (auto candidate = searchAncestors(current); !candidate.empty()) return candidate;

	std::wstring executablePath(32768, L'\0');
	const DWORD length = GetModuleFileNameW(nullptr, executablePath.data(), static_cast<DWORD>(executablePath.size()));
	if (length > 0 && length < executablePath.size())
	{
		executablePath.resize(length);
		current = std::filesystem::path(executablePath).parent_path();
		if (auto candidate = searchAncestors(current); !candidate.empty()) return candidate;
	}
	return requested;
}

void DrawBitmapNineSlice(ID2D1RenderTarget* target, ID2D1Bitmap* bitmap,
	const D2D1_RECT_F& destination, int assetWidth, int assetHeight,
	int sliceLeft, int sliceTop, int sliceRight, int sliceBottom, float opacity)
{
	const float sourceWidth = static_cast<float>(assetWidth);
	const float sourceHeight = static_cast<float>(assetHeight);
	const float destinationWidth = destination.right - destination.left;
	const float destinationHeight = destination.bottom - destination.top;
	if (destinationWidth <= 0 || destinationHeight <= 0 || sourceWidth <= 0 || sourceHeight <= 0) return;

	const float left = static_cast<float>(std::clamp(sliceLeft, 0, assetWidth));
	const float right = static_cast<float>(std::clamp(sliceRight, 0, assetWidth));
	const float top = static_cast<float>(std::clamp(sliceTop, 0, assetHeight));
	const float bottom = static_cast<float>(std::clamp(sliceBottom, 0, assetHeight));
	if (left + right >= sourceWidth || top + bottom >= sourceHeight) return;
	// 小さい表示先では角だけを縦横別に潰さず、素材全体の縮小率で角を揃えて縮める。
	const float cornerScale = std::min({1.0f, destinationWidth / sourceWidth, destinationHeight / sourceHeight});
	const float destinationLeft = left * cornerScale;
	const float destinationRight = right * cornerScale;
	const float destinationTop = top * cornerScale;
	const float destinationBottom = bottom * cornerScale;
	const float sourceX[] = {0.0f, left, sourceWidth - right, sourceWidth};
	const float sourceY[] = {0.0f, top, sourceHeight - bottom, sourceHeight};
	const float destinationX[] = {destination.left, destination.left + destinationLeft,
		destination.right - destinationRight, destination.right};
	const float destinationY[] = {destination.top, destination.top + destinationTop,
		destination.bottom - destinationBottom, destination.bottom};
	for (int row = 0; row < 3; ++row)
	{
		for (int column = 0; column < 3; ++column)
		{
			if (sourceX[column + 1] <= sourceX[column] || sourceY[row + 1] <= sourceY[row]
				|| destinationX[column + 1] <= destinationX[column]
				|| destinationY[row + 1] <= destinationY[row]) continue;
			const D2D1_RECT_F source = D2D1::RectF(sourceX[column], sourceY[row],
				sourceX[column + 1], sourceY[row + 1]);
			const D2D1_RECT_F dest = D2D1::RectF(destinationX[column], destinationY[row],
				destinationX[column + 1], destinationY[row + 1]);
			target->DrawBitmap(bitmap, dest, opacity, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, source);
		}
	}
}

IDWriteTextFormat *GetTextFormat(float requestedSize, bool bold = false)
{
	const int sizeKey = std::clamp(static_cast<int>(requestedSize + 0.5f), 8, 96);
	const int cacheKey = sizeKey * 2 + (bold ? 1 : 0);
	auto found = textFormats.find(cacheKey);
	if (found != textFormats.end())
	{
		return found->second.Get();
	}

	ComPtr<IDWriteTextFormat> format;
	const DWRITE_FONT_WEIGHT weight = bold ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL;
	HRESULT hr = E_FAIL;
	if (privateFontCollection)
		hr = textFactory->CreateTextFormat(L"Noto Sans JP", privateFontCollection.Get(), weight,
			DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, static_cast<float>(sizeKey),
			L"ja-JP", format.GetAddressOf());
	if (FAILED(hr))
		hr = textFactory->CreateTextFormat(L"Noto Sans JP", nullptr, weight,
			DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, static_cast<float>(sizeKey),
			L"ja-JP", format.GetAddressOf());
	if (FAILED(hr))
	{
		hr = textFactory->CreateTextFormat(L"Yu Gothic UI", nullptr, weight,
			DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, static_cast<float>(sizeKey),
			L"ja-JP", format.GetAddressOf());
	}
	if (FAILED(hr))
	{
		hr = textFactory->CreateTextFormat(L"Segoe UI", nullptr, weight,
			DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, static_cast<float>(sizeKey),
			L"ja-JP", format.GetAddressOf());
	}
	if (FAILED(hr))
	{
		return nullptr;
	}

	format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
	textFormats.emplace(cacheKey, format);
	return format.Get();
}
} // namespace

bool UIRenderer::Initialize(ID3D11Device *d3dDevice, int width, int height)
{
	Shutdown();
	if (!d3dDevice || width <= 0 || height <= 0)
	{
		return false;
	}

	device = d3dDevice;
	device->AddRef();

	HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory), nullptr,
								   reinterpret_cast<void **>(d2dFactory.GetAddressOf()));
	if (FAILED(hr))
	{
		Shutdown();
		return false;
	}

	hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
							 reinterpret_cast<IUnknown **>(textFactory.GetAddressOf()));
	if (FAILED(hr))
	{
		Shutdown();
		return false;
	}
	RegisterPrivateFonts();

	hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
						  IID_PPV_ARGS(imagingFactory.GetAddressOf()));
	if (FAILED(hr))
	{
		Shutdown();
		return false;
	}

	surfaceWidth = width;
	surfaceHeight = height;
	if (!CreateSurfaceResources())
	{
		Shutdown();
		return false;
	}
	initialized = true;
	return true;
}

void UIRenderer::Shutdown()
{
	initialized = false;
	ReleaseSurfaceResources();
	imageAssets.clear();
	textFormats.clear();
	UnregisterPrivateFonts();
	imagingFactory.Reset();
	textFactory.Reset();
	d2dFactory.Reset();
	if (device)
	{
		device->Release();
		device = nullptr;
	}
}

bool UIRenderer::Resize(int width, int height)
{
	if (!device || width <= 0 || height <= 0)
	{
		return false;
	}
	if (surfaceWidth == width && surfaceHeight == height && surfaceTarget && compositeTextureView)
	{
		return true;
	}

	ReleaseSurfaceResources();
	surfaceWidth = width;
	surfaceHeight = height;
	initialized = CreateSurfaceResources();
	return initialized;
}

bool UIRenderer::LoadAssets(const std::string& manifestPath)
{
	if (!imagingFactory || !surfaceTarget) return false;
	const std::filesystem::path resolvedManifest = ResolveManifestPath(manifestPath);
	std::ifstream stream(resolvedManifest, std::ios::binary);
	if (!stream)
	{
		DebugLog("[UI] Could not open UI asset manifest: " + manifestPath);
		return false;
	}
	std::string jsonText((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
	if (jsonText.size() >= 3 && static_cast<unsigned char>(jsonText[0]) == 0xEF
		&& static_cast<unsigned char>(jsonText[1]) == 0xBB && static_cast<unsigned char>(jsonText[2]) == 0xBF)
		jsonText.erase(0, 3);
	JsonValue root;
	std::string parseError;
	if (!JsonParser::Parse(jsonText, root, parseError) || !root.Find("assets") || !root.Find("assets")->IsArray())
	{
		DebugLog("[UI] Invalid UI asset manifest: " + parseError);
		return false;
	}
	const std::filesystem::path basePath = resolvedManifest.parent_path();
	std::unordered_map<std::string, ImageAsset> loadedAssets;
	for (const JsonValue& value : root.Find("assets")->AsArray())
	{
		const JsonValue* name = value.Find("name");
		const JsonValue* file = value.Find("file");
		const JsonValue* slice = value.Find("slice");
		if (!name || !name->IsString() || !file || !file->IsString() || !slice || !slice->IsArray()
			|| slice->AsArray().size() != 4)
		{
			DebugLog("[UI] Skipping malformed UI asset entry.");
			continue;
		}
		const std::string id = name->AsString();
		if (id.empty() || loadedAssets.contains(id))
		{
			DebugLog("[UI] Skipping empty or duplicate UI asset ID: " + id);
			continue;
		}
		const std::filesystem::path imagePath = basePath / file->AsString();
		ComPtr<IWICBitmapDecoder> decoder;
		HRESULT hr = imagingFactory->CreateDecoderFromFilename(imagePath.c_str(), nullptr, GENERIC_READ,
			WICDecodeMetadataCacheOnLoad, decoder.GetAddressOf());
		ComPtr<IWICBitmapFrameDecode> frame;
		if (SUCCEEDED(hr)) hr = decoder->GetFrame(0, frame.GetAddressOf());
		ComPtr<IWICFormatConverter> converter;
		if (SUCCEEDED(hr)) hr = imagingFactory->CreateFormatConverter(converter.GetAddressOf());
		if (SUCCEEDED(hr)) hr = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppPBGRA,
			WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
		ImageAsset asset;
		if (SUCCEEDED(hr)) hr = imagingFactory->CreateBitmapFromSource(converter.Get(), WICBitmapCacheOnLoad,
			asset.source.GetAddressOf());
		UINT imageWidth = 0, imageHeight = 0;
		if (SUCCEEDED(hr)) hr = asset.source->GetSize(&imageWidth, &imageHeight);
		if (FAILED(hr) || imageWidth == 0 || imageHeight == 0)
		{
			DebugLog("[UI] Could not load UI image; skipping ID: ", id, " hr=", hr);
			continue;
		}
		asset.width = static_cast<int>(imageWidth);
		asset.height = static_cast<int>(imageHeight);
		const auto& slices = slice->AsArray();
		if (!slices[0].IsNumber() || !slices[1].IsNumber() || !slices[2].IsNumber() || !slices[3].IsNumber())
		{
			DebugLog("[UI] Skipping UI image with non-numeric slice: " + id);
			continue;
		}
		asset.sliceLeft = static_cast<int>(slices[0].AsNumber());
		asset.sliceTop = static_cast<int>(slices[1].AsNumber());
		asset.sliceRight = static_cast<int>(slices[2].AsNumber());
		asset.sliceBottom = static_cast<int>(slices[3].AsNumber());
		if (asset.sliceLeft < 0 || asset.sliceTop < 0 || asset.sliceRight < 0 || asset.sliceBottom < 0
			|| asset.sliceLeft + asset.sliceRight >= asset.width
			|| asset.sliceTop + asset.sliceBottom >= asset.height)
		{
			DebugLog("[UI] Skipping UI image with invalid slice bounds: " + id);
			continue;
		}
		loadedAssets.emplace(id, std::move(asset));
	}
	if (loadedAssets.empty())
	{
		DebugLog("[UI] No usable UI images were found in the manifest.");
		return false;
	}
	imageAssets = std::move(loadedAssets);
	cachedCommands.clear();
	if (!CreateTargetBitmaps())
	{
		imageAssets.clear();
		DebugLog("[UI] Could not create Direct2D UI image bitmaps.");
		return false;
	}
	DebugLog("[UI] Loaded UI image assets: ", imageAssets.size());
	return true;
}

void UIRenderer::UnloadAssets()
{
	imageAssets.clear();
	cachedCommands.clear();
}

bool UIRenderer::CreateTargetBitmaps()
{
	if (!surfaceTarget) return false;
	for (auto& [id, asset] : imageAssets)
	{
		asset.targetBitmap.Reset();
		if (!asset.source || FAILED(surfaceTarget->CreateBitmapFromWicBitmap(asset.source.Get(), nullptr,
			asset.targetBitmap.GetAddressOf()))) return false;
	}
	return true;
}

void UIRenderer::Draw(Renderer &renderer, const std::vector<DrawCommand> &commands)
{
	if (!initialized || commands.empty())
	{
		return;
	}

	if (commands.size() == cachedCommands.size() &&
		std::equal(commands.begin(), commands.end(), cachedCommands.begin(), Same))
	{
		renderer.DrawFullscreenTexture(compositeTextureView.Get(), surfaceWidth, surfaceHeight);
		return;
	}
	surfaceTarget->BeginDraw();
	surfaceTarget->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));
	for (const DrawCommand &command : commands)
	{
		if (command.bounds.right <= command.bounds.left || command.bounds.bottom <= command.bounds.top)
		{
			continue;
		}

		brush->SetColor(ToD2DColor(command.color));
		const D2D1_RECT_F bounds = ToD2DRect(command.bounds);
		const RECT clip = command.clip.right > command.clip.left && command.clip.bottom > command.clip.top
							  ? command.clip
							  : RECT{0, 0, surfaceWidth, surfaceHeight};
		surfaceTarget->PushAxisAlignedClip(ToD2DRect(clip), D2D1_ANTIALIAS_MODE_ALIASED);
		if (command.type == DrawCommandType::Image || command.type == DrawCommandType::NineSliceImage)
		{
			const auto image = imageAssets.find(command.imageId);
			if (image != imageAssets.end() && image->second.targetBitmap)
			{
				if (command.type == DrawCommandType::Image)
					surfaceTarget->DrawBitmap(image->second.targetBitmap.Get(), bounds, command.color.w,
						D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
				else
					DrawBitmapNineSlice(surfaceTarget.Get(), image->second.targetBitmap.Get(), bounds,
						image->second.width, image->second.height, image->second.sliceLeft,
						image->second.sliceTop, image->second.sliceRight, image->second.sliceBottom,
						command.color.w);
			}
			surfaceTarget->PopAxisAlignedClip();
			continue;
		}
		switch (command.type)
		{
		case DrawCommandType::FillRectangle:
			surfaceTarget->FillRectangle(bounds, brush.Get());
			break;
		case DrawCommandType::StrokeRectangle:
			surfaceTarget->DrawRectangle(bounds, brush.Get(), std::max(1.0f, command.strokeWidth));
			break;
		case DrawCommandType::Line:
			surfaceTarget->DrawLine(
				D2D1::Point2F(static_cast<float>(command.lineStart.x),
							  static_cast<float>(command.lineStart.y)),
				D2D1::Point2F(static_cast<float>(command.lineEnd.x), static_cast<float>(command.lineEnd.y)),
				brush.Get(), std::max(1.0f, command.strokeWidth));
			break;
		case DrawCommandType::Text: {
			if (command.text.empty())
			{
				break;
			}
			IDWriteTextFormat *format = GetTextFormat(command.fontSize, command.bold);
			if (!format)
			{
				break;
			}
			format->SetTextAlignment(command.horizontalAlignment == TextHorizontalAlignment::Center
										 ? DWRITE_TEXT_ALIGNMENT_CENTER
									 : command.horizontalAlignment == TextHorizontalAlignment::Far
										 ? DWRITE_TEXT_ALIGNMENT_TRAILING
										 : DWRITE_TEXT_ALIGNMENT_LEADING);
			format->SetParagraphAlignment(
				command.verticalAlignment == TextVerticalAlignment::Center ? DWRITE_PARAGRAPH_ALIGNMENT_CENTER
				: command.verticalAlignment == TextVerticalAlignment::Far  ? DWRITE_PARAGRAPH_ALIGNMENT_FAR
																		   : DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
			format->SetWordWrapping(command.wrapText ? DWRITE_WORD_WRAPPING_WRAP
													 : DWRITE_WORD_WRAPPING_NO_WRAP);
			surfaceTarget->DrawText(command.text.c_str(), static_cast<UINT32>(command.text.size()), format,
									bounds, brush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP,
									DWRITE_MEASURING_MODE_NATURAL);
			break;
		}
		}
		surfaceTarget->PopAxisAlignedClip();
	}

	if (FAILED(surfaceTarget->EndDraw()))
	{
		int width = surfaceWidth, height = surfaceHeight;
		ReleaseSurfaceResources();
		surfaceWidth = width;
		surfaceHeight = height;
		initialized = CreateSurfaceResources();
		return;
	}

	WICRect lockRect{0, 0, surfaceWidth, surfaceHeight};
	ComPtr<IWICBitmapLock> bitmapLock;
	if (FAILED(surfaceBitmap->Lock(&lockRect, WICBitmapLockRead, bitmapLock.GetAddressOf())))
	{
		return;
	}

	UINT stride = 0;
	UINT bufferSize = 0;
	BYTE *pixels = nullptr;
	if (FAILED(bitmapLock->GetStride(&stride)) || FAILED(bitmapLock->GetDataPointer(&bufferSize, &pixels)) ||
		!pixels)
	{
		return;
	}

	ID3D11DeviceContext *context = Renderer::GetDeviceContext();
	if (!context)
	{
		return;
	}
	context->UpdateSubresource(compositeTexture.Get(), 0, nullptr, pixels, stride, 0);
	cachedCommands = commands;
	renderer.DrawFullscreenTexture(compositeTextureView.Get(), surfaceWidth, surfaceHeight);
}

bool UIRenderer::IsInitialized()
{
	return initialized;
}

bool UIRenderer::HasImage(std::string_view imageId)
{
	const auto image = imageAssets.find(std::string(imageId));
	return image != imageAssets.end() && image->second.targetBitmap != nullptr;
}

float UIRenderer::MeasureText(std::wstring_view text, float fontSize, bool bold)
{
	if (!initialized || text.empty())
	{
		return 0.0f;
	}

	IDWriteTextFormat *format = GetTextFormat(fontSize, bold);
	if (!format)
	{
		return 0.0f;
	}

	ComPtr<IDWriteTextLayout> layout;
	const HRESULT hr = textFactory->CreateTextLayout(text.data(), static_cast<UINT32>(text.size()), format,
													 8192.0f, 128.0f, layout.GetAddressOf());
	if (FAILED(hr))
	{
		return 0.0f;
	}

	DWRITE_TEXT_METRICS metrics = {};
	if (FAILED(layout->GetMetrics(&metrics)))
	{
		return 0.0f;
	}
	return metrics.widthIncludingTrailingWhitespace;
}

bool UIRenderer::CreateSurfaceResources()
{
	if (!device || !imagingFactory || surfaceWidth <= 0 || surfaceHeight <= 0)
	{
		return false;
	}

	HRESULT hr = imagingFactory->CreateBitmap(static_cast<UINT>(surfaceWidth),
											  static_cast<UINT>(surfaceHeight), GUID_WICPixelFormat32bppPBGRA,
											  WICBitmapCacheOnLoad, surfaceBitmap.GetAddressOf());
	if (FAILED(hr))
	{
		return false;
	}

	const D2D1_RENDER_TARGET_PROPERTIES targetProperties = D2D1::RenderTargetProperties(
		D2D1_RENDER_TARGET_TYPE_DEFAULT,
		D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED), 96.0f, 96.0f);
	hr = d2dFactory->CreateWicBitmapRenderTarget(surfaceBitmap.Get(), targetProperties,
												 surfaceTarget.GetAddressOf());
	if (FAILED(hr))
	{
		return false;
	}
	if (!imageAssets.empty() && !CreateTargetBitmaps()) return false;

	hr = surfaceTarget->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f), brush.GetAddressOf());
	if (FAILED(hr))
	{
		return false;
	}

	D3D11_TEXTURE2D_DESC textureDescription = {};
	textureDescription.Width = static_cast<UINT>(surfaceWidth);
	textureDescription.Height = static_cast<UINT>(surfaceHeight);
	textureDescription.MipLevels = 1;
	textureDescription.ArraySize = 1;
	textureDescription.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	textureDescription.SampleDesc.Count = 1;
	textureDescription.Usage = D3D11_USAGE_DEFAULT;
	textureDescription.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	hr = device->CreateTexture2D(&textureDescription, nullptr, compositeTexture.GetAddressOf());
	if (FAILED(hr))
	{
		return false;
	}
	hr = device->CreateShaderResourceView(compositeTexture.Get(), nullptr,
										  compositeTextureView.GetAddressOf());
	return SUCCEEDED(hr);
}

void UIRenderer::ReleaseSurfaceResources()
{
	cachedCommands.clear();
	for (auto& [id, asset] : imageAssets) asset.targetBitmap.Reset();
	compositeTextureView.Reset();
	compositeTexture.Reset();
	brush.Reset();
	surfaceTarget.Reset();
	surfaceBitmap.Reset();
	surfaceWidth = 0;
	surfaceHeight = 0;
}
} // namespace GameUI
