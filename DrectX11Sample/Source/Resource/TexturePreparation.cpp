#include "Resource/TexturePreparation.h"

#include <wincodec.h>
#include <wrl/client.h>
#include <utility>

using Microsoft::WRL::ComPtr;

namespace
{
	/// <summary>画像の先頭フレームをRGBAへ変換してCPUメモリに展開する。</summary>
	/// <param name="factory">色形式の変換に使うWICファクトリー。</param>
	/// <param name="decoder">ファイルまたはメモリを開いたWICデコーダー。</param>
	/// <param name="image">成功時に更新する画像データ。</param>
	/// <returns>WIC処理の結果。</returns>
	HRESULT Decode(IWICImagingFactory *factory, IWICBitmapDecoder *decoder, TexturePreparation::Image &image)
	{
		ComPtr<IWICBitmapFrameDecode> frame;
		HRESULT hr = decoder->GetFrame(0, &frame);
		if (FAILED(hr))
		{
			return hr;
		}

		TexturePreparation::Image decoded;
		hr = frame->GetSize(&decoded.width, &decoded.height);
		if (FAILED(hr))
		{
			return hr;
		}

		if (!decoded.width || !decoded.height || decoded.width > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION ||
			decoded.height > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION)
		{
			return E_INVALIDARG;
		}

		ComPtr<IWICMetadataQueryReader> metadata;
		if (SUCCEEDED(frame->GetMetadataQueryReader(&metadata)))
		{
			PROPVARIANT value{};
			GUID container{};
			if (SUCCEEDED(decoder->GetContainerFormat(&container)) && container == GUID_ContainerFormatPng)
			{
				if (SUCCEEDED(metadata->GetMetadataByName(L"/sRGB/RenderingIntent", &value)) &&
					value.vt == VT_UI1)
				{
					decoded.format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
				}
			}
			else if (SUCCEEDED(metadata->GetMetadataByName(L"System.Image.ColorSpace", &value)) &&
					 value.vt == VT_UI2 && value.uiVal == 1)
			{
				decoded.format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
			}
			PropVariantClear(&value);
		}

		ComPtr<IWICFormatConverter> converter;
		hr = factory->CreateFormatConverter(&converter);
		if (FAILED(hr))
		{
			return hr;
		}

		hr = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone,
								   nullptr, 0, WICBitmapPaletteTypeCustom);
		if (FAILED(hr))
		{
			return hr;
		}

		const UINT pitch = decoded.width * 4;
		const UINT size = pitch * decoded.height;
		decoded.pixels.resize(size);
		hr = converter->CopyPixels(nullptr, pitch, size, decoded.pixels.data());
		if (SUCCEEDED(hr))
		{
			image = std::move(decoded);
		}

		return hr;
	}
}

/// <summary>COM初期化済みのスレッドで画像をCPUメモリへデコードする。</summary>
/// <param name="path">画像ファイルのパス。</param>
/// <param name="image">成功時に更新する画像データ。</param>
/// <returns>WIC処理の結果。</returns>
HRESULT TexturePreparation::DecodeFile(const wchar_t *path, Image &image)
{
	ComPtr<IWICImagingFactory> factory;
	HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
	if (FAILED(hr))
	{
		return hr;
	}

	ComPtr<IWICBitmapDecoder> decoder;
	hr = factory->CreateDecoderFromFilename(path, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
	return FAILED(hr) ? hr : Decode(factory.Get(), decoder.Get(), image);
}

/// <summary>COM初期化済みのスレッドでメモリ上の圧縮画像をデコードする。</summary>
/// <param name="bytes">デコード完了まで有効な入力バイト列。</param>
/// <param name="size">入力のバイト数。</param>
/// <param name="image">成功時に更新する画像データ。</param>
/// <returns>WIC処理の結果。</returns>
HRESULT TexturePreparation::DecodeMemory(const uint8_t *bytes, UINT size, Image &image)
{
	if (!bytes || !size)
	{
		return E_INVALIDARG;
	}

	ComPtr<IWICImagingFactory> factory;
	HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
	if (FAILED(hr))
	{
		return hr;
	}

	ComPtr<IWICStream> stream;
	hr = factory->CreateStream(&stream);
	if (FAILED(hr))
	{
		return hr;
	}

	hr = stream->InitializeFromMemory(const_cast<BYTE *>(bytes), size);
	if (FAILED(hr))
	{
		return hr;
	}

	ComPtr<IWICBitmapDecoder> decoder;
	hr = factory->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad, &decoder);
	return FAILED(hr) ? hr : Decode(factory.Get(), decoder.Get(), image);
}

/// <summary>デコード済み画像から描画用テクスチャを作成する。</summary>
/// <param name="device">登録先のDirect3Dデバイス。</param>
/// <param name="image">CPU側のRGBA画像。</param>
/// <param name="view">作成したビューの受け取り先。</param>
/// <returns>Direct3D処理の結果。</returns>
HRESULT TexturePreparation::Upload(ID3D11Device *device, const Image &image, ID3D11ShaderResourceView **view)
{
	if (!device || !view || image.pixels.empty())
	{
		return E_INVALIDARG;
	}

	D3D11_TEXTURE2D_DESC desc{};
	desc.Width = image.width;
	desc.Height = image.height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = image.format;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA data{};
	data.pSysMem = image.pixels.data();
	data.SysMemPitch = image.width * 4;

	ComPtr<ID3D11Texture2D> texture;
	const HRESULT hr = device->CreateTexture2D(&desc, &data, &texture);
	return FAILED(hr) ? hr : device->CreateShaderResourceView(texture.Get(), nullptr, view);
}
