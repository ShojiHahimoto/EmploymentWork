#include "UI/UIRenderer.h"

#include "System/Renderer.h"

#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <algorithm>
#include <unordered_map>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "windowscodecs.lib")

namespace GameUI
{
using Microsoft::WRL::ComPtr;

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
std::unordered_map<int, ComPtr<IDWriteTextFormat>> textFormats;
ID3D11Device *device = nullptr;
int surfaceWidth = 0;
int surfaceHeight = 0;
bool initialized = false;
std::vector<DrawCommand> cachedCommands;

bool Same(const DrawCommand &a, const DrawCommand &b)
{
	return EqualRect(&a.bounds, &b.bounds) && EqualRect(&a.clip, &b.clip) && a.type == b.type &&
		   a.text == b.text && a.color == b.color && a.fontSize == b.fontSize &&
		   a.strokeWidth == b.strokeWidth && a.horizontalAlignment == b.horizontalAlignment &&
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

IDWriteTextFormat *GetTextFormat(float requestedSize)
{
	const int sizeKey = std::clamp(static_cast<int>(requestedSize + 0.5f), 8, 96);
	auto found = textFormats.find(sizeKey);
	if (found != textFormats.end())
	{
		return found->second.Get();
	}

	ComPtr<IDWriteTextFormat> format;
	HRESULT hr = textFactory->CreateTextFormat(L"Yu Gothic UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
											   DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
											   static_cast<float>(sizeKey), L"ja-JP", format.GetAddressOf());
	if (FAILED(hr))
	{
		hr = textFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
										   DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
										   static_cast<float>(sizeKey), L"ja-JP", format.GetAddressOf());
	}
	if (FAILED(hr))
	{
		return nullptr;
	}

	format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
	textFormats.emplace(sizeKey, format);
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
	textFormats.clear();
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
			IDWriteTextFormat *format = GetTextFormat(command.fontSize);
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

float UIRenderer::MeasureText(std::wstring_view text, float fontSize)
{
	if (!initialized || text.empty())
	{
		return 0.0f;
	}

	IDWriteTextFormat *format = GetTextFormat(fontSize);
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
	compositeTextureView.Reset();
	compositeTexture.Reset();
	brush.Reset();
	surfaceTarget.Reset();
	surfaceBitmap.Reset();
	surfaceWidth = 0;
	surfaceHeight = 0;
}
} // namespace GameUI
