#pragma once

#include <d3d11.h>

#include <cstdint>
#include <vector>

namespace TexturePreparation
{
	struct Image
	{
		UINT width = 0;
		UINT height = 0;
		DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
		std::vector<uint8_t> pixels;
	};

	HRESULT DecodeFile(const wchar_t *path, Image &image);
	HRESULT DecodeMemory(const uint8_t *bytes, UINT size, Image &image);
	HRESULT Upload(ID3D11Device *device, const Image &image, ID3D11ShaderResourceView **view);
}
