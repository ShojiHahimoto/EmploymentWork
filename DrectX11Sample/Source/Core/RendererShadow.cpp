#include "Core/Renderer.h"

#include "Resource/ModelResource.h"

#include <wrl/client.h>

using namespace DirectX::SimpleMath;
using Microsoft::WRL::ComPtr;

namespace
{
	constexpr UINT ShadowMapSize = 2048;
	constexpr float LightDistance = 120.0f;
	constexpr float ShadowDepthBias = 0.00035f;
	constexpr float ShadowFloorBrightness = 0.53f;

	struct FloorConstants
	{
		Matrix cameraViewProjection;
		Matrix lightViewProjection;
		Vector4 bounds;
		Vector4 settings;
	};

	ComPtr<ID3D11DepthStencilView> shadowDepth;
	ComPtr<ID3D11ShaderResourceView> shadowView;
	ComPtr<ID3D11RasterizerState> shadowRasterizer;
	ComPtr<ID3D11SamplerState> shadowSampler;
	ComPtr<ID3D11VertexShader> depthShader;
	ComPtr<ID3D11VertexShader> floorVertexShader;
	ComPtr<ID3D11PixelShader> floorPixelShader;
	ComPtr<ID3D11Buffer> floorBuffer;
	ComPtr<ID3D11DepthStencilState> floorDepthState;
	ComPtr<ID3D11BlendState> floorBlendState;
	Matrix lightViewProjection;
	bool shadowReady = false;
}

/// <summary>シャドウマップと床描画に必要なDirect3Dリソースを作る。</summary>
/// <returns>生成またはシェーダーコンパイルの結果。</returns>
HRESULT Renderer::CreateShadowResources()
{
	const auto compile = [](const wchar_t *name, const char *entry, const char *target, ID3DBlob **blob)
	{
		HRESULT hr = E_FAIL;
		for (const wchar_t *root : {L"Source/Shader/", L"DrectX11Sample/Source/Shader/",
									L"../../DrectX11Sample/Source/Shader/"})
		{
			hr = CompileShaderFromFile(std::wstring(root) + name, entry, target, blob);
			if (SUCCEEDED(hr))
			{
				break;
			}
		}

		return hr;
	};

	ComPtr<ID3DBlob> depthCode;
	ComPtr<ID3DBlob> floorVS;
	ComPtr<ID3DBlob> floorPS;
	HRESULT hr = compile(L"VS_ShadowDepth.hlsl", "VSMain", "vs_5_0", depthCode.GetAddressOf());
	if (SUCCEEDED(hr))
	{
		hr = compile(L"VS_ShadowReceiver.hlsl", "VSMain", "vs_5_0", floorVS.GetAddressOf());
	}
	if (SUCCEEDED(hr))
	{
		hr = compile(L"PS_ShadowReceiver.hlsl", "PSMain", "ps_5_0", floorPS.GetAddressOf());
	}
	if (FAILED(hr))
	{
		return hr;
	}

	hr = m_pDevice->CreateVertexShader(depthCode->GetBufferPointer(), depthCode->GetBufferSize(), nullptr, &depthShader);
	if (SUCCEEDED(hr))
	{
		hr = m_pDevice->CreateVertexShader(floorVS->GetBufferPointer(), floorVS->GetBufferSize(), nullptr,
										 &floorVertexShader);
	}
	if (SUCCEEDED(hr))
	{
		hr = m_pDevice->CreatePixelShader(floorPS->GetBufferPointer(), floorPS->GetBufferSize(), nullptr,
										&floorPixelShader);
	}
	if (FAILED(hr))
	{
		return hr;
	}

	D3D11_TEXTURE2D_DESC texture{};
	texture.Width = ShadowMapSize;
	texture.Height = ShadowMapSize;
	texture.MipLevels = 1;
	texture.ArraySize = 1;
	texture.Format = DXGI_FORMAT_R32_TYPELESS;
	texture.SampleDesc.Count = 1;
	texture.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

	ComPtr<ID3D11Texture2D> map;
	hr = m_pDevice->CreateTexture2D(&texture, nullptr, &map);
	if (FAILED(hr))
	{
		return hr;
	}

	D3D11_DEPTH_STENCIL_VIEW_DESC depth{};
	depth.Format = DXGI_FORMAT_D32_FLOAT;
	depth.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	hr = m_pDevice->CreateDepthStencilView(map.Get(), &depth, &shadowDepth);
	if (FAILED(hr))
	{
		return hr;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC view{};
	view.Format = DXGI_FORMAT_R32_FLOAT;
	view.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	view.Texture2D.MipLevels = 1;
	hr = m_pDevice->CreateShaderResourceView(map.Get(), &view, &shadowView);
	if (FAILED(hr))
	{
		return hr;
	}

	D3D11_RASTERIZER_DESC rasterizer{};
	rasterizer.FillMode = D3D11_FILL_SOLID;
	rasterizer.CullMode = D3D11_CULL_NONE;
	rasterizer.DepthClipEnable = TRUE;
	hr = m_pDevice->CreateRasterizerState(&rasterizer, &shadowRasterizer);
	if (FAILED(hr))
	{
		return hr;
	}

	D3D11_SAMPLER_DESC sampler{};
	sampler.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
	sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
	sampler.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
	sampler.BorderColor[0] = 1.0f;
	sampler.MaxAnisotropy = 1;
	hr = m_pDevice->CreateSamplerState(&sampler, &shadowSampler);
	if (FAILED(hr))
	{
		return hr;
	}

	D3D11_BUFFER_DESC buffer{};
	D3D11_DEPTH_STENCIL_DESC floorDepth{};
	floorDepth.DepthEnable = TRUE;
	floorDepth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	floorDepth.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	hr = m_pDevice->CreateDepthStencilState(&floorDepth, &floorDepthState);
	if (FAILED(hr))
	{
		return hr;
	}

	D3D11_BLEND_DESC floorBlend{};
	auto& target = floorBlend.RenderTarget[0];
	target.BlendEnable = TRUE;
	target.SrcBlend = D3D11_BLEND_SRC_ALPHA;
	target.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	target.BlendOp = D3D11_BLEND_OP_ADD;
	target.SrcBlendAlpha = D3D11_BLEND_ZERO;
	target.DestBlendAlpha = D3D11_BLEND_ONE;
	target.BlendOpAlpha = D3D11_BLEND_OP_ADD;
	target.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	hr = m_pDevice->CreateBlendState(&floorBlend, &floorBlendState);
	if (FAILED(hr))
	{
		return hr;
	}

	buffer.ByteWidth = sizeof(FloorConstants);
	buffer.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	return m_pDevice->CreateBuffer(&buffer, nullptr, &floorBuffer);
}

/// <summary>シャドウマップと床描画用のGPUリソースを解放する。</summary>
void Renderer::ReleaseShadowResources()
{
	shadowReady = false;
	floorDepthState.Reset();
	floorBlendState.Reset();
	floorBuffer.Reset();
	floorPixelShader.Reset();
	floorVertexShader.Reset();
	depthShader.Reset();
	shadowSampler.Reset();
	shadowRasterizer.Reset();
	shadowView.Reset();
	shadowDepth.Reset();
}

/// <summary>光源視点からモデルの深度を記録し、元の描画先と出力状態を復元する。</summary>
/// <param name="casters">通常描画と同じワールド・ボーン行列を持つモデル一覧。</param>
/// <param name="center">光源カメラの注視点。</param>
/// <param name="coverage">正射影範囲の一辺の長さ。</param>
void Renderer::DrawShadowMap(std::span<const ShadowCaster> casters, const Vector3 &center, float coverage)
{
	shadowReady = false;
	if (!shadowDepth || !depthShader || casters.empty() || coverage <= 0.0f)
	{
		return;
	}

	Vector3 towardLight(0.0f, 0.8f, -0.5f);
	towardLight.Normalize();
	const auto lightView = Matrix::CreateLookAt(center + towardLight * LightDistance, center, Vector3::Up);
	const auto lightProjection = Matrix::CreateOrthographic(coverage, coverage, 1.0f, LightDistance * 2.0f);
	lightViewProjection = lightView * lightProjection;

	auto *context = m_pDeviceContext;
	ComPtr<ID3D11RenderTargetView> oldColor;
	ComPtr<ID3D11DepthStencilView> oldDepth;
	ComPtr<ID3D11DepthStencilState> oldDepthState;
	ComPtr<ID3D11RasterizerState> oldRasterizer;
	UINT stencil = 0;
	UINT viewportCount = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
	D3D11_VIEWPORT oldViewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE]{};
	context->OMGetRenderTargets(1, oldColor.GetAddressOf(), oldDepth.GetAddressOf());
	context->OMGetDepthStencilState(oldDepthState.GetAddressOf(), &stencil);
	context->RSGetState(oldRasterizer.GetAddressOf());
	context->RSGetViewports(&viewportCount, oldViewports);

	ID3D11ShaderResourceView *emptyView = nullptr;
	context->PSSetShaderResources(0, 1, &emptyView);
	context->OMSetRenderTargets(0, nullptr, shadowDepth.Get());
	context->OMSetDepthStencilState(m_pDepthStateEnable, 0);
	context->ClearDepthStencilView(shadowDepth.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
	const D3D11_VIEWPORT viewport{0, 0, ShadowMapSize, ShadowMapSize, 0, 1};
	context->RSSetViewports(1, &viewport);
	context->RSSetState(shadowRasterizer.Get());

	context->IASetInputLayout(m_pModelInputLayout);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->VSSetShader(depthShader.Get(), nullptr, 0);
	context->VSSetConstantBuffers(0, 1, &m_pModelConstantBuffer);
	context->PSSetShader(nullptr, nullptr, 0);

	const UINT stride = sizeof(ModelVertex);
	const UINT offset = 0;
	for (const auto &caster : casters)
	{
		if (!caster.model)
		{
			continue;
		}

		const auto constants = BuildModelConstants(caster.world * lightViewProjection,
												   Color(1, 1, 1, 1), caster.skinningMatrices);
		context->UpdateSubresource(m_pModelConstantBuffer, 0, nullptr, &constants, 0, 0);
		for (const auto &mesh : caster.model->GetMeshes())
		{
			if (!mesh.vertexBuffer || !mesh.indexBuffer || mesh.indices.empty())
			{
				continue;
			}

			context->IASetVertexBuffers(0, 1, &mesh.vertexBuffer, &stride, &offset);
			context->IASetIndexBuffer(mesh.indexBuffer, DXGI_FORMAT_R32_UINT, 0);
			context->DrawIndexed(static_cast<UINT>(mesh.indices.size()), 0, 0);
			shadowReady = true;
		}
	}

	context->OMSetRenderTargets(1, oldColor.GetAddressOf(), oldDepth.Get());
	context->OMSetDepthStencilState(oldDepthState.Get(), stencil);
	context->RSSetState(oldRasterizer.Get());
	context->RSSetViewports(viewportCount, oldViewports);
}

/// <summary>描画済みの床にシャドウマップの影だけを合成する。</summary>
/// <param name="minX">床の左端。</param>
/// <param name="maxX">床の右端。</param>
/// <param name="nearZ">床の手前端。</param>
/// <param name="farZ">床の奥端。</param>
void Renderer::DrawShadowFloor(float minX, float maxX, float nearZ, float farZ)
{
	if (!shadowReady || !floorBuffer)
	{
		return;
	}

	const FloorConstants constants{
		(m_ViewMatrix * m_ProjectionMatrix).Transpose(), lightViewProjection.Transpose(),
		Vector4(minX, nearZ, maxX, farZ), Vector4(ShadowDepthBias, ShadowFloorBrightness, 0, 0)
	};

	auto *context = m_pDeviceContext;
	context->UpdateSubresource(floorBuffer.Get(), 0, nullptr, &constants, 0, 0);
	context->IASetInputLayout(nullptr);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->VSSetShader(floorVertexShader.Get(), nullptr, 0);
	context->VSSetConstantBuffers(0, 1, floorBuffer.GetAddressOf());
	context->PSSetShader(floorPixelShader.Get(), nullptr, 0);
	context->PSSetConstantBuffers(0, 1, floorBuffer.GetAddressOf());
	context->PSSetShaderResources(0, 1, shadowView.GetAddressOf());
	context->PSSetSamplers(0, 1, shadowSampler.GetAddressOf());
	ComPtr<ID3D11BlendState> previousBlend;
	ComPtr<ID3D11DepthStencilState> previousDepth;
	FLOAT blendFactor[4]{};
	UINT sampleMask = 0;
	UINT stencil = 0;
	context->OMGetBlendState(previousBlend.GetAddressOf(), blendFactor, &sampleMask);
	context->OMGetDepthStencilState(previousDepth.GetAddressOf(), &stencil);
	context->OMSetBlendState(floorBlendState.Get(), nullptr, 0xffffffff);
	context->OMSetDepthStencilState(floorDepthState.Get(), 0);
	context->Draw(6, 0);

	ID3D11ShaderResourceView *emptyView = nullptr;
	context->PSSetShaderResources(0, 1, &emptyView);
	context->OMSetDepthStencilState(previousDepth.Get(), stencil);
	context->OMSetBlendState(previousBlend.Get(), blendFactor, sampleMask);
}
