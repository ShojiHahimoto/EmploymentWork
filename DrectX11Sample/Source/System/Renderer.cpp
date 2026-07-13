#include "System/Renderer.h"
#include "System/Application.h"
#include "Resource/ModelResource.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

using namespace DirectX;
using namespace DirectX::SimpleMath;

namespace
{
	const char* DebugCubeShaderSource = R"(
cbuffer TransformBuffer : register(b0)
{
	matrix worldViewProjection;
};

struct VS_INPUT
{
	float3 position : POSITION;
	float4 color : COLOR;
};

struct PS_INPUT
{
	float4 position : SV_POSITION;
	float4 color : COLOR;
};

PS_INPUT VSMain(VS_INPUT input)
{
	PS_INPUT output;
	output.position = mul(float4(input.position, 1.0f), worldViewProjection);
	output.color = input.color;
	return output;
}

float4 PSMain(PS_INPUT input) : SV_TARGET
{
	return input.color;
}
)";

	const char* ModelShaderSource = R"(
cbuffer TransformBuffer : register(b0)
{
	matrix worldViewProjection;
};

struct VS_INPUT
{
	float3 position : POSITION;
	float3 normal : NORMAL;
	float2 uv : TEXCOORD;
	uint4 boneIndices : BLENDINDICES;
	float4 boneWeights : BLENDWEIGHT;
};

struct PS_INPUT
{
	float4 position : SV_POSITION;
	float3 normal : NORMAL;
};

PS_INPUT VSMain(VS_INPUT input)
{
	PS_INPUT output;
	output.position = mul(float4(input.position, 1.0f), worldViewProjection);
	output.normal = normalize(input.normal);
	return output;
}

float4 PSMain(PS_INPUT input) : SV_TARGET
{
	float light = saturate(dot(normalize(input.normal), normalize(float3(0.3f, 0.8f, -0.5f))));
	return float4(0.35f + light * 0.55f, 0.38f + light * 0.45f, 0.42f + light * 0.35f, 1.0f);
}
)";
}

D3D_FEATURE_LEVEL Renderer::m_FeatureLevel = D3D_FEATURE_LEVEL_11_0;

ID3D11Device* Renderer::m_pDevice = nullptr;
ID3D11DeviceContext* Renderer::m_pDeviceContext = nullptr;
IDXGISwapChain* Renderer::m_pSwapChain = nullptr;
ID3D11RenderTargetView* Renderer::m_pRenderTargetView = nullptr;
ID3D11DepthStencilView* Renderer::m_pDepthStencilView = nullptr;

ID3D11Buffer* Renderer::m_pWorldBuffer = nullptr;
ID3D11Buffer* Renderer::m_pViewBuffer = nullptr;
ID3D11Buffer* Renderer::m_pProjectionBuffer = nullptr;
ID3D11Buffer* Renderer::m_pTextureBuffer = nullptr;

ID3D11DepthStencilState* Renderer::m_pDepthStateEnable = nullptr;
ID3D11DepthStencilState* Renderer::m_pDepthStateDisable = nullptr;

ID3D11BlendState* Renderer::m_BlendState[MAX_BLENDSTATE] = {};
ID3D11BlendState* Renderer::m_pBlendStateATC = nullptr;

ID3D11RasterizerState* Renderer::m_pRasterizerSolid = nullptr;
ID3D11RasterizerState* Renderer::m_pRasterizerWireframe = nullptr;

Matrix Renderer::m_ViewMatrix = Matrix::Identity;
Matrix Renderer::m_ProjectionMatrix = Matrix::Identity;

ID3D11VertexShader* Renderer::m_pDebugCubeVertexShader = nullptr;
ID3D11PixelShader* Renderer::m_pDebugCubePixelShader = nullptr;
ID3D11InputLayout* Renderer::m_pDebugCubeInputLayout = nullptr;
ID3D11Buffer* Renderer::m_pDebugCubeVertexBuffer = nullptr;
ID3D11Buffer* Renderer::m_pDebugCubeIndexBuffer = nullptr;
ID3D11Buffer* Renderer::m_pDebugCubeConstantBuffer = nullptr;

ID3D11VertexShader* Renderer::m_pModelVertexShader = nullptr;
ID3D11PixelShader* Renderer::m_pModelPixelShader = nullptr;
ID3D11InputLayout* Renderer::m_pModelInputLayout = nullptr;
ID3D11Buffer* Renderer::m_pModelConstantBuffer = nullptr;

HRESULT Renderer::Init()
{
	DXGI_SWAP_CHAIN_DESC sd = {};

	sd.BufferCount = 1;
	sd.BufferDesc.Width = Application::GetWidth();
	sd.BufferDesc.Height = Application::GetHeight();
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = Application::GetWindow();
	sd.SampleDesc.Count = 1;
	sd.Windowed = TRUE;

	HRESULT hr = D3D11CreateDeviceAndSwapChain(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,
		0,
		nullptr,
		0,
		D3D11_SDK_VERSION,
		&sd,
		&m_pSwapChain,
		&m_pDevice,
		&m_FeatureLevel,
		&m_pDeviceContext
	);

	if (FAILED(hr)) return hr;

	D3D11_DEPTH_STENCIL_DESC depthDesc = {};
	depthDesc.DepthEnable = TRUE;
	depthDesc.DepthFunc = D3D11_COMPARISON_LESS;
	depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	hr = m_pDevice->CreateDepthStencilState(&depthDesc, &m_pDepthStateEnable);
	if (FAILED(hr)) return hr;

	depthDesc.DepthEnable = FALSE;
	hr = m_pDevice->CreateDepthStencilState(&depthDesc, &m_pDepthStateDisable);
	if (FAILED(hr)) return hr;

	D3D11_BLEND_DESC blendDesc = {};
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	hr = m_pDevice->CreateBlendState(&blendDesc, &m_BlendState[BS_ALPHABLEND]);
	if (FAILED(hr)) return hr;

	D3D11_RASTERIZER_DESC rsDesc = {};
	rsDesc.FillMode = D3D11_FILL_SOLID;
	rsDesc.CullMode = D3D11_CULL_NONE;
	hr = m_pDevice->CreateRasterizerState(&rsDesc, &m_pRasterizerSolid);
	if (FAILED(hr)) return hr;

	rsDesc.FillMode = D3D11_FILL_WIREFRAME;
	hr = m_pDevice->CreateRasterizerState(&rsDesc, &m_pRasterizerWireframe);
	if (FAILED(hr)) return hr;

	hr = CreateRenderAndDepthResources(Application::GetWidth(), Application::GetHeight());
	if (FAILED(hr)) return hr;

	hr = CreateDebugCubeResources();
	if (FAILED(hr)) return hr;

	return CreateModelRenderResources();
}

void Renderer::DrawStart()
{
	float clearColor[4] = { 0.0f, 0.0f, 0.35f, 1.0f };

	m_pDeviceContext->ClearRenderTargetView(m_pRenderTargetView, clearColor);
	m_pDeviceContext->ClearDepthStencilView(m_pDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

	m_pDeviceContext->OMSetRenderTargets(1, &m_pRenderTargetView, m_pDepthStencilView);
	m_pDeviceContext->OMSetDepthStencilState(m_pDepthStateEnable, 0);
	m_pDeviceContext->RSSetState(m_pRasterizerSolid);
}

void Renderer::DrawEnd()
{
	m_pSwapChain->Present(1, 0);
}

ID3D11Device* Renderer::GetDevice()
{
	return m_pDevice;
}

ID3D11DeviceContext* Renderer::GetDeviceContext()
{
	return m_pDeviceContext;
}

HRESULT Renderer::ResizeWindow(int width, int height)
{
	if (!m_pSwapChain || width <= 0 || height <= 0)
	{
		return E_INVALIDARG;
	}

	m_pDeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
	SAFE_RELEASE(m_pRenderTargetView);
	SAFE_RELEASE(m_pDepthStencilView);

	HRESULT hr = m_pSwapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
	if (FAILED(hr)) return hr;

	return CreateRenderAndDepthResources(width, height);
}

HRESULT Renderer::CreateRenderAndDepthResources(int width, int height)
{
	ID3D11Texture2D* backBuffer = nullptr;

	HRESULT hr = m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
	if (FAILED(hr)) return hr;

	hr = m_pDevice->CreateRenderTargetView(backBuffer, nullptr, &m_pRenderTargetView);
	backBuffer->Release();
	if (FAILED(hr)) return hr;

	D3D11_TEXTURE2D_DESC depthDesc = {};
	depthDesc.Width = width;
	depthDesc.Height = height;
	depthDesc.MipLevels = 1;
	depthDesc.ArraySize = 1;
	depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthDesc.SampleDesc.Count = 1;
	depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

	ID3D11Texture2D* depthBuffer = nullptr;

	hr = m_pDevice->CreateTexture2D(&depthDesc, nullptr, &depthBuffer);
	if (FAILED(hr)) return hr;

	hr = m_pDevice->CreateDepthStencilView(depthBuffer, nullptr, &m_pDepthStencilView);
	depthBuffer->Release();
	if (FAILED(hr)) return hr;

	SetViewport(width, height);

	return S_OK;
}

void Renderer::SetViewport(int width, int height)
{
	D3D11_VIEWPORT vp = {};

	vp.Width = static_cast<float>(width);
	vp.Height = static_cast<float>(height);
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;

	m_pDeviceContext->RSSetViewports(1, &vp);
}

HRESULT Renderer::CreateDebugCubeResources()
{
	ID3DBlob* vertexShaderBlob = nullptr;
	ID3DBlob* pixelShaderBlob = nullptr;

	HRESULT hr = CompileShader(DebugCubeShaderSource, "VSMain", "vs_5_0", &vertexShaderBlob);
	if (FAILED(hr)) return hr;

	hr = CompileShader(DebugCubeShaderSource, "PSMain", "ps_5_0", &pixelShaderBlob);
	if (FAILED(hr))
	{
		SAFE_RELEASE(vertexShaderBlob);
		return hr;
	}

	hr = m_pDevice->CreateVertexShader(
		vertexShaderBlob->GetBufferPointer(),
		vertexShaderBlob->GetBufferSize(),
		nullptr,
		&m_pDebugCubeVertexShader);
	if (FAILED(hr))
	{
		SAFE_RELEASE(vertexShaderBlob);
		SAFE_RELEASE(pixelShaderBlob);
		return hr;
	}

	hr = m_pDevice->CreatePixelShader(
		pixelShaderBlob->GetBufferPointer(),
		pixelShaderBlob->GetBufferSize(),
		nullptr,
		&m_pDebugCubePixelShader);
	if (FAILED(hr))
	{
		SAFE_RELEASE(vertexShaderBlob);
		SAFE_RELEASE(pixelShaderBlob);
		return hr;
	}

	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	hr = m_pDevice->CreateInputLayout(
		layout,
		_countof(layout),
		vertexShaderBlob->GetBufferPointer(),
		vertexShaderBlob->GetBufferSize(),
		&m_pDebugCubeInputLayout);

	SAFE_RELEASE(vertexShaderBlob);
	SAFE_RELEASE(pixelShaderBlob);

	if (FAILED(hr)) return hr;

	const DebugCubeVertex vertices[] =
	{
		{ Vector3(-1.0f, -1.0f, -1.0f), Color(1.0f, 0.2f, 0.2f, 1.0f) },
		{ Vector3(-1.0f,  1.0f, -1.0f), Color(0.2f, 1.0f, 0.2f, 1.0f) },
		{ Vector3( 1.0f,  1.0f, -1.0f), Color(0.2f, 0.4f, 1.0f, 1.0f) },
		{ Vector3( 1.0f, -1.0f, -1.0f), Color(1.0f, 1.0f, 0.2f, 1.0f) },
		{ Vector3(-1.0f, -1.0f,  1.0f), Color(1.0f, 0.2f, 1.0f, 1.0f) },
		{ Vector3(-1.0f,  1.0f,  1.0f), Color(0.2f, 1.0f, 1.0f, 1.0f) },
		{ Vector3( 1.0f,  1.0f,  1.0f), Color(1.0f, 0.6f, 0.2f, 1.0f) },
		{ Vector3( 1.0f, -1.0f,  1.0f), Color(0.9f, 0.9f, 0.9f, 1.0f) },
	};

	const uint16_t indices[] =
	{
		0, 1, 2, 0, 2, 3,
		4, 6, 5, 4, 7, 6,
		4, 5, 1, 4, 1, 0,
		3, 2, 6, 3, 6, 7,
		1, 5, 6, 1, 6, 2,
		4, 0, 3, 4, 3, 7,
	};

	D3D11_BUFFER_DESC vertexBufferDesc = {};
	vertexBufferDesc.ByteWidth = sizeof(vertices);
	vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA vertexData = {};
	vertexData.pSysMem = vertices;

	hr = m_pDevice->CreateBuffer(&vertexBufferDesc, &vertexData, &m_pDebugCubeVertexBuffer);
	if (FAILED(hr)) return hr;

	D3D11_BUFFER_DESC indexBufferDesc = {};
	indexBufferDesc.ByteWidth = sizeof(indices);
	indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA indexData = {};
	indexData.pSysMem = indices;

	hr = m_pDevice->CreateBuffer(&indexBufferDesc, &indexData, &m_pDebugCubeIndexBuffer);
	if (FAILED(hr)) return hr;

	D3D11_BUFFER_DESC constantBufferDesc = {};
	constantBufferDesc.ByteWidth = sizeof(DebugCubeConstantBuffer);
	constantBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	return m_pDevice->CreateBuffer(&constantBufferDesc, nullptr, &m_pDebugCubeConstantBuffer);
}

HRESULT Renderer::CreateModelRenderResources()
{
	ID3DBlob* vertexShaderBlob = nullptr;
	ID3DBlob* pixelShaderBlob = nullptr;

	HRESULT hr = CompileShader(ModelShaderSource, "VSMain", "vs_5_0", &vertexShaderBlob);
	if (FAILED(hr)) return hr;

	hr = CompileShader(ModelShaderSource, "PSMain", "ps_5_0", &pixelShaderBlob);
	if (FAILED(hr))
	{
		SAFE_RELEASE(vertexShaderBlob);
		return hr;
	}

	hr = m_pDevice->CreateVertexShader(
		vertexShaderBlob->GetBufferPointer(),
		vertexShaderBlob->GetBufferSize(),
		nullptr,
		&m_pModelVertexShader);
	if (FAILED(hr))
	{
		SAFE_RELEASE(vertexShaderBlob);
		SAFE_RELEASE(pixelShaderBlob);
		return hr;
	}

	hr = m_pDevice->CreatePixelShader(
		pixelShaderBlob->GetBufferPointer(),
		pixelShaderBlob->GetBufferSize(),
		nullptr,
		&m_pModelPixelShader);
	if (FAILED(hr))
	{
		SAFE_RELEASE(vertexShaderBlob);
		SAFE_RELEASE(pixelShaderBlob);
		return hr;
	}

	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, static_cast<UINT>(offsetof(ModelVertex, position)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, static_cast<UINT>(offsetof(ModelVertex, normal)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, static_cast<UINT>(offsetof(ModelVertex, uv)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT, 0, static_cast<UINT>(offsetof(ModelVertex, boneIndices)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "BLENDWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, static_cast<UINT>(offsetof(ModelVertex, boneWeights)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	hr = m_pDevice->CreateInputLayout(
		layout,
		_countof(layout),
		vertexShaderBlob->GetBufferPointer(),
		vertexShaderBlob->GetBufferSize(),
		&m_pModelInputLayout);

	SAFE_RELEASE(vertexShaderBlob);
	SAFE_RELEASE(pixelShaderBlob);

	if (FAILED(hr)) return hr;

	D3D11_BUFFER_DESC constantBufferDesc = {};
	constantBufferDesc.ByteWidth = sizeof(DebugCubeConstantBuffer);
	constantBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	return m_pDevice->CreateBuffer(&constantBufferDesc, nullptr, &m_pModelConstantBuffer);
}

HRESULT Renderer::CompileShader(const char* source, const char* entryPoint, const char* target, ID3DBlob** blob)
{
	UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
	flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	ID3DBlob* errorBlob = nullptr;
	HRESULT hr = D3DCompile(
		source,
		strlen(source),
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

void Renderer::SetViewProjection(const Matrix& view, const Matrix& projection)
{
	m_ViewMatrix = view;
	m_ProjectionMatrix = projection;
}

void Renderer::DrawDebugCube(const Matrix& world)
{
	if (!m_pDebugCubeVertexBuffer || !m_pDebugCubeIndexBuffer || !m_pDebugCubeConstantBuffer)
	{
		return;
	}

	DebugCubeConstantBuffer constantBuffer = {};
	constantBuffer.worldViewProjection = (world * m_ViewMatrix * m_ProjectionMatrix).Transpose();
	m_pDeviceContext->UpdateSubresource(m_pDebugCubeConstantBuffer, 0, nullptr, &constantBuffer, 0, 0);

	const UINT stride = sizeof(DebugCubeVertex);
	const UINT offset = 0;

	m_pDeviceContext->IASetInputLayout(m_pDebugCubeInputLayout);
	m_pDeviceContext->IASetVertexBuffers(0, 1, &m_pDebugCubeVertexBuffer, &stride, &offset);
	m_pDeviceContext->IASetIndexBuffer(m_pDebugCubeIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
	m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	m_pDeviceContext->VSSetShader(m_pDebugCubeVertexShader, nullptr, 0);
	m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pDebugCubeConstantBuffer);
	m_pDeviceContext->PSSetShader(m_pDebugCubePixelShader, nullptr, 0);

	m_pDeviceContext->DrawIndexed(36, 0, 0);
}

void Renderer::DrawModel(const ModelResource& model, const Matrix& world)
{
	if (!m_pModelInputLayout || !m_pModelVertexShader || !m_pModelPixelShader || !m_pModelConstantBuffer)
	{
		return;
	}

	DebugCubeConstantBuffer constantBuffer = {};
	constantBuffer.worldViewProjection = (world * m_ViewMatrix * m_ProjectionMatrix).Transpose();
	m_pDeviceContext->UpdateSubresource(m_pModelConstantBuffer, 0, nullptr, &constantBuffer, 0, 0);

	m_pDeviceContext->IASetInputLayout(m_pModelInputLayout);
	m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	m_pDeviceContext->VSSetShader(m_pModelVertexShader, nullptr, 0);
	m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pModelConstantBuffer);
	m_pDeviceContext->PSSetShader(m_pModelPixelShader, nullptr, 0);

	for (const ModelMesh& mesh : model.GetMeshes())
	{
		if (!mesh.vertexBuffer || !mesh.indexBuffer || mesh.indices.empty())
		{
			continue;
		}

		const UINT stride = sizeof(ModelVertex);
		const UINT offset = 0;
		m_pDeviceContext->IASetVertexBuffers(0, 1, &mesh.vertexBuffer, &stride, &offset);
		m_pDeviceContext->IASetIndexBuffer(mesh.indexBuffer, DXGI_FORMAT_R32_UINT, 0);
		m_pDeviceContext->DrawIndexed(static_cast<UINT>(mesh.indices.size()), 0, 0);
	}
}

void Renderer::SetDepthEnable(bool Enable)
{
	if (Enable)
	{
		m_pDeviceContext->OMSetDepthStencilState(m_pDepthStateEnable, 0);
	}
	else
	{
		m_pDeviceContext->OMSetDepthStencilState(m_pDepthStateDisable, 0);
	}
}

void Renderer::ReleaseDebugCubeResources()
{
	SAFE_RELEASE(m_pDebugCubeConstantBuffer);
	SAFE_RELEASE(m_pDebugCubeIndexBuffer);
	SAFE_RELEASE(m_pDebugCubeVertexBuffer);
	SAFE_RELEASE(m_pDebugCubeInputLayout);
	SAFE_RELEASE(m_pDebugCubePixelShader);
	SAFE_RELEASE(m_pDebugCubeVertexShader);
}

void Renderer::ReleaseModelRenderResources()
{
	SAFE_RELEASE(m_pModelConstantBuffer);
	SAFE_RELEASE(m_pModelInputLayout);
	SAFE_RELEASE(m_pModelPixelShader);
	SAFE_RELEASE(m_pModelVertexShader);
}

void Renderer::Uninit()
{
	ModelResourceManager::UnloadAll();
	ReleaseModelRenderResources();
	ReleaseDebugCubeResources();

	SAFE_RELEASE(m_pDepthStencilView);
	SAFE_RELEASE(m_pRenderTargetView);

	SAFE_RELEASE(m_pDepthStateEnable);
	SAFE_RELEASE(m_pDepthStateDisable);

	for (int i = 0; i < MAX_BLENDSTATE; i++)
	{
		SAFE_RELEASE(m_BlendState[i]);
	}

	SAFE_RELEASE(m_pRasterizerSolid);
	SAFE_RELEASE(m_pRasterizerWireframe);

	SAFE_RELEASE(m_pSwapChain);
	SAFE_RELEASE(m_pDeviceContext);
	SAFE_RELEASE(m_pDevice);
}
