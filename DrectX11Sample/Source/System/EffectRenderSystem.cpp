#include "System/EffectRenderSystem.h"

#include "Component/CameraComponent.h"
#include "Component/EffectComponent.h"
#include "Core/GameObject.h"
#include "System/Renderer.h"
#include "World/World.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <unordered_map>

using namespace DirectX;
using namespace DirectX::SimpleMath;

namespace
{
	const char* EffectShaderSource = R"(
cbuffer EffectTransformBuffer : register(b0)
{
	matrix worldViewProjection;
	float4 effectColor;
};

Texture2D effectTexture : register(t0);
SamplerState effectSampler : register(s0);

struct VS_INPUT
{
	float3 position : POSITION;
	float2 uv : TEXCOORD;
};

struct PS_INPUT
{
	float4 position : SV_POSITION;
	float2 uv : TEXCOORD;
};

PS_INPUT VSMain(VS_INPUT input)
{
	PS_INPUT output;
	output.position = mul(float4(input.position, 1.0f), worldViewProjection);
	output.uv = input.uv;
	return output;
}

float4 PSMain(PS_INPUT input) : SV_TARGET
{
	return effectTexture.Sample(effectSampler, input.uv) * effectColor;
}
)";

	struct EffectVertex
	{
		Vector3 position;
		Vector2 uv;
	};

	struct EffectConstantBuffer
	{
		Matrix worldViewProjection;
		Color effectColor;
	};

	struct EffectRenderResources
	{
		ID3D11VertexShader* vertexShader = nullptr;
		ID3D11PixelShader* pixelShader = nullptr;
		ID3D11InputLayout* inputLayout = nullptr;
		ID3D11Buffer* vertexBuffer = nullptr;
		ID3D11Buffer* indexBuffer = nullptr;
		ID3D11Buffer* constantBuffer = nullptr;
		ID3D11SamplerState* samplerState = nullptr;
		ID3D11ShaderResourceView* whiteTextureView = nullptr;
		ID3D11BlendState* alphaBlendState = nullptr;
		ID3D11BlendState* additiveBlendState = nullptr;
		ID3D11DepthStencilState* depthEnableState = nullptr;
		ID3D11DepthStencilState* depthDisableState = nullptr;
		std::unordered_map<std::string, ID3D11ShaderResourceView*> textureCache;
	};

	EffectRenderResources g_resources;

	/// <summary>
	/// Shader を文字列からコンパイルする。
	/// </summary>
	/// <param name="source">HLSL ソース。</param>
	/// <param name="entryPoint">エントリーポイント名。</param>
	/// <param name="target">Shader Model。</param>
	/// <param name="blob">コンパイル結果の受け取り先。</param>
	/// <returns>コンパイル結果の HRESULT。</returns>
	HRESULT CompileShader(const char* source, const char* entryPoint, const char* target, ID3DBlob** blob)
	{
		UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
		flags |= D3DCOMPILE_DEBUG;
#endif

		ID3DBlob* errorBlob = nullptr;
		const HRESULT hr = D3DCompile(
			source,
			std::strlen(source),
			nullptr,
			nullptr,
			nullptr,
			entryPoint,
			target,
			flags,
			0,
			blob,
			&errorBlob);

		SAFE_RELEASE(errorBlob);
		return hr;
	}

	/// <summary>
	/// 1x1 の白テクスチャを作成する。
	/// </summary>
	/// <returns>作成結果の HRESULT。</returns>
	HRESULT CreateWhiteTexture()
	{
		ID3D11Device* device = Renderer::GetDevice();
		if (!device)
		{
			return E_FAIL;
		}

		const uint32_t whitePixel = 0xffffffff;
		D3D11_TEXTURE2D_DESC textureDesc = {};
		textureDesc.Width = 1;
		textureDesc.Height = 1;
		textureDesc.MipLevels = 1;
		textureDesc.ArraySize = 1;
		textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.Usage = D3D11_USAGE_DEFAULT;
		textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

		D3D11_SUBRESOURCE_DATA initialData = {};
		initialData.pSysMem = &whitePixel;
		initialData.SysMemPitch = sizeof(whitePixel);

		ID3D11Texture2D* texture = nullptr;
		HRESULT hr = device->CreateTexture2D(&textureDesc, &initialData, &texture);
		if (FAILED(hr))
		{
			return hr;
		}

		hr = device->CreateShaderResourceView(texture, nullptr, &g_resources.whiteTextureView);
		SAFE_RELEASE(texture);
		return hr;
	}

	/// <summary>
	/// EffectRenderSystem の DirectX11 リソースを遅延作成する。
	/// </summary>
	/// <returns>作成に成功した場合は true。</returns>
	bool EnsureResources()
	{
		if (g_resources.vertexShader && g_resources.pixelShader && g_resources.inputLayout)
		{
			return true;
		}

		ID3D11Device* device = Renderer::GetDevice();
		if (!device)
		{
			return false;
		}

		ID3DBlob* vertexShaderBlob = nullptr;
		HRESULT hr = CompileShader(EffectShaderSource, "VSMain", "vs_5_0", &vertexShaderBlob);
		if (FAILED(hr))
		{
			SAFE_RELEASE(vertexShaderBlob);
			return false;
		}

		hr = device->CreateVertexShader(
			vertexShaderBlob->GetBufferPointer(),
			vertexShaderBlob->GetBufferSize(),
			nullptr,
			&g_resources.vertexShader);
		if (FAILED(hr))
		{
			SAFE_RELEASE(vertexShaderBlob);
			return false;
		}

		const D3D11_INPUT_ELEMENT_DESC layout[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		hr = device->CreateInputLayout(
			layout,
			static_cast<UINT>(std::size(layout)),
			vertexShaderBlob->GetBufferPointer(),
			vertexShaderBlob->GetBufferSize(),
			&g_resources.inputLayout);
		SAFE_RELEASE(vertexShaderBlob);
		if (FAILED(hr))
		{
			return false;
		}

		ID3DBlob* pixelShaderBlob = nullptr;
		hr = CompileShader(EffectShaderSource, "PSMain", "ps_5_0", &pixelShaderBlob);
		if (FAILED(hr))
		{
			SAFE_RELEASE(pixelShaderBlob);
			return false;
		}

		hr = device->CreatePixelShader(
			pixelShaderBlob->GetBufferPointer(),
			pixelShaderBlob->GetBufferSize(),
			nullptr,
			&g_resources.pixelShader);
		SAFE_RELEASE(pixelShaderBlob);
		if (FAILED(hr))
		{
			return false;
		}

		const EffectVertex vertices[] =
		{
			{ Vector3(-0.5f, 0.5f, 0.0f), Vector2(0.0f, 0.0f) },
			{ Vector3(0.5f, 0.5f, 0.0f), Vector2(1.0f, 0.0f) },
			{ Vector3(0.5f, -0.5f, 0.0f), Vector2(1.0f, 1.0f) },
			{ Vector3(-0.5f, -0.5f, 0.0f), Vector2(0.0f, 1.0f) },
		};
		const uint16_t indices[] = { 0, 1, 2, 0, 2, 3 };

		D3D11_BUFFER_DESC vertexBufferDesc = {};
		vertexBufferDesc.ByteWidth = sizeof(vertices);
		vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
		vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		D3D11_SUBRESOURCE_DATA vertexInitialData = {};
		vertexInitialData.pSysMem = vertices;
		hr = device->CreateBuffer(&vertexBufferDesc, &vertexInitialData, &g_resources.vertexBuffer);
		if (FAILED(hr))
		{
			return false;
		}

		D3D11_BUFFER_DESC indexBufferDesc = {};
		indexBufferDesc.ByteWidth = sizeof(indices);
		indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
		indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		D3D11_SUBRESOURCE_DATA indexInitialData = {};
		indexInitialData.pSysMem = indices;
		hr = device->CreateBuffer(&indexBufferDesc, &indexInitialData, &g_resources.indexBuffer);
		if (FAILED(hr))
		{
			return false;
		}

		D3D11_BUFFER_DESC constantBufferDesc = {};
		constantBufferDesc.ByteWidth = sizeof(EffectConstantBuffer);
		constantBufferDesc.Usage = D3D11_USAGE_DEFAULT;
		constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		hr = device->CreateBuffer(&constantBufferDesc, nullptr, &g_resources.constantBuffer);
		if (FAILED(hr))
		{
			return false;
		}

		D3D11_SAMPLER_DESC samplerDesc = {};
		samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
		hr = device->CreateSamplerState(&samplerDesc, &g_resources.samplerState);
		if (FAILED(hr))
		{
			return false;
		}

		D3D11_BLEND_DESC alphaBlendDesc = {};
		alphaBlendDesc.RenderTarget[0].BlendEnable = TRUE;
		alphaBlendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		alphaBlendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		alphaBlendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		alphaBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		alphaBlendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
		alphaBlendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		alphaBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		hr = device->CreateBlendState(&alphaBlendDesc, &g_resources.alphaBlendState);
		if (FAILED(hr))
		{
			return false;
		}

		D3D11_BLEND_DESC additiveBlendDesc = alphaBlendDesc;
		additiveBlendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
		hr = device->CreateBlendState(&additiveBlendDesc, &g_resources.additiveBlendState);
		if (FAILED(hr))
		{
			return false;
		}

		D3D11_DEPTH_STENCIL_DESC depthDesc = {};
		depthDesc.DepthEnable = TRUE;
		depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		depthDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		hr = device->CreateDepthStencilState(&depthDesc, &g_resources.depthEnableState);
		if (FAILED(hr))
		{
			return false;
		}

		depthDesc.DepthEnable = FALSE;
		hr = device->CreateDepthStencilState(&depthDesc, &g_resources.depthDisableState);
		if (FAILED(hr))
		{
			return false;
		}

		return SUCCEEDED(CreateWhiteTexture());
	}

	/// <summary>
	/// texturePath に対応する ShaderResourceView を取得する。
	/// </summary>
	/// <param name="texturePath">読み込むテクスチャパス。空文字なら白テクスチャ。</param>
	/// <returns>描画に使う ShaderResourceView。</returns>
	ID3D11ShaderResourceView* GetTextureView(const std::string& texturePath)
	{
		if (texturePath.empty())
		{
			return g_resources.whiteTextureView;
		}

		const auto found = g_resources.textureCache.find(texturePath);
		if (found != g_resources.textureCache.end())
		{
			return found->second ? found->second : g_resources.whiteTextureView;
		}

		ID3D11ShaderResourceView* textureView = nullptr;
		const HRESULT hr = Renderer::LoadTextureFromFile(texturePath, &textureView);
		if (FAILED(hr))
		{
			g_resources.textureCache[texturePath] = nullptr;
			return g_resources.whiteTextureView;
		}

		g_resources.textureCache[texturePath] = textureView;
		return textureView;
	}

	/// <summary>
	/// 粒子の現在フレームに対応する色を計算する。
	/// </summary>
	/// <param name="particle">計算対象の粒子。</param>
	/// <returns>現在色。</returns>
	Color CalculateParticleColor(const EffectParticleRuntime& particle)
	{
		const float rate = particle.lifeFrames <= 1
			? 1.0f
			: std::clamp(static_cast<float>(particle.ageFrames) / static_cast<float>(particle.lifeFrames - 1), 0.0f, 1.0f);

		return Color(
			particle.startColor.x + (particle.endColor.x - particle.startColor.x) * rate,
			particle.startColor.y + (particle.endColor.y - particle.startColor.y) * rate,
			particle.startColor.z + (particle.endColor.z - particle.startColor.z) * rate,
			particle.startColor.w + (particle.endColor.w - particle.startColor.w) * rate);
	}

	/// <summary>
	/// 粒子の現在フレームに対応する大きさを計算する。
	/// </summary>
	/// <param name="particle">計算対象の粒子。</param>
	/// <returns>現在スケール。</returns>
	float CalculateParticleScale(const EffectParticleRuntime& particle)
	{
		const float rate = particle.lifeFrames <= 1
			? 1.0f
			: std::clamp(static_cast<float>(particle.ageFrames) / static_cast<float>(particle.lifeFrames - 1), 0.0f, 1.0f);
		return particle.startScale + (particle.endScale - particle.startScale) * rate;
	}

	/// <summary>
	/// BlendMode から使用する BlendState を取得する。
	/// </summary>
	/// <param name="blendMode">粒子に設定された合成方式。</param>
	/// <returns>描画に使う BlendState。</returns>
	ID3D11BlendState* GetBlendState(EffectBlendMode blendMode)
	{
		return blendMode == EffectBlendMode::Alpha
			? g_resources.alphaBlendState
			: g_resources.additiveBlendState;
	}
}

void EffectRenderSystem::Draw(World& world, const CameraComponent& camera)
{
	if (!EnsureResources())
	{
		return;
	}

	ID3D11DeviceContext* context = Renderer::GetDeviceContext();
	if (!context)
	{
		return;
	}

	const UINT stride = sizeof(EffectVertex);
	const UINT offset = 0;
	const float blendFactor[4] = {};

	context->IASetInputLayout(g_resources.inputLayout);
	context->IASetVertexBuffers(0, 1, &g_resources.vertexBuffer, &stride, &offset);
	context->IASetIndexBuffer(g_resources.indexBuffer, DXGI_FORMAT_R16_UINT, 0);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->VSSetShader(g_resources.vertexShader, nullptr, 0);
	context->VSSetConstantBuffers(0, 1, &g_resources.constantBuffer);
	context->PSSetShader(g_resources.pixelShader, nullptr, 0);
	context->PSSetConstantBuffers(0, 1, &g_resources.constantBuffer);
	context->PSSetSamplers(0, 1, &g_resources.samplerState);

	for (GameObject& object : world.GetGameObjects())
	{
		if (object.tag != GameObjectTag::Effect)
		{
			continue;
		}

		EffectComponent* effect = world.GetComponent<EffectComponent>(object.id);
		if (!effect)
		{
			continue;
		}

		for (const EffectParticleRuntime& particle : effect->particles)
		{
			if (!particle.alive)
			{
				continue;
			}

			const float scale = CalculateParticleScale(particle);
			if (scale <= 0.0f)
			{
				continue;
			}

			const Matrix worldMatrix =
				Matrix::CreateScale(scale, scale, 1.0f)
				* Matrix::CreateRotationZ(XMConvertToRadians(particle.rotationDegrees))
				* Matrix::CreateTranslation(particle.position);

			EffectConstantBuffer constantBuffer = {};
			constantBuffer.worldViewProjection = (worldMatrix * camera.viewMatrix * camera.projectionMatrix).Transpose();
			constantBuffer.effectColor = CalculateParticleColor(particle);
			context->UpdateSubresource(g_resources.constantBuffer, 0, nullptr, &constantBuffer, 0, 0);

			ID3D11ShaderResourceView* textureView = GetTextureView(particle.texturePath);
			context->PSSetShaderResources(0, 1, &textureView);
			context->OMSetDepthStencilState(particle.depthEnabled ? g_resources.depthEnableState : g_resources.depthDisableState, 0);
			context->OMSetBlendState(GetBlendState(particle.blendMode), blendFactor, 0xffffffff);
			context->DrawIndexed(6, 0, 0);
		}
	}

	ID3D11ShaderResourceView* nullTexture = nullptr;
	context->PSSetShaderResources(0, 1, &nullTexture);
	context->OMSetBlendState(nullptr, blendFactor, 0xffffffff);
	Renderer::SetDepthEnable(true);
}

void EffectRenderSystem::ReleaseResources()
{
	for (auto& texture : g_resources.textureCache)
	{
		SAFE_RELEASE(texture.second);
	}
	g_resources.textureCache.clear();

	SAFE_RELEASE(g_resources.depthDisableState);
	SAFE_RELEASE(g_resources.depthEnableState);
	SAFE_RELEASE(g_resources.additiveBlendState);
	SAFE_RELEASE(g_resources.alphaBlendState);
	SAFE_RELEASE(g_resources.whiteTextureView);
	SAFE_RELEASE(g_resources.samplerState);
	SAFE_RELEASE(g_resources.constantBuffer);
	SAFE_RELEASE(g_resources.indexBuffer);
	SAFE_RELEASE(g_resources.vertexBuffer);
	SAFE_RELEASE(g_resources.inputLayout);
	SAFE_RELEASE(g_resources.pixelShader);
	SAFE_RELEASE(g_resources.vertexShader);
}
