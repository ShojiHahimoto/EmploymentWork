#pragma once

#define _CRT_SECURE_NO_WARNINGS

#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <SimpleMath.h>
#include <io.h>
#include <locale.h>
#include <string>
#include <vector>

#pragma comment(lib, "directxtk.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

#define SAFE_RELEASE(p){ if(p){ p->Release(); p = nullptr; } }

struct VETREX_3D
{
	DirectX::SimpleMath::Vector3 position;
	DirectX::SimpleMath::Vector3 normal;
	DirectX::SimpleMath::Color color;
	DirectX::SimpleMath::Vector2 uv;
};

enum EBlendState
{
	BS_NONE = 0,
	BS_ALPHABLEND,
	BS_ADDITIVE,
	BS_SUBTRACTION,
	MAX_BLENDSTATE
};

struct LineVertex
{
	DirectX::SimpleMath::Vector3 position;
	DirectX::SimpleMath::Vector3 color;
};

struct DebugCubeVertex
{
	DirectX::SimpleMath::Vector3 position;
	DirectX::SimpleMath::Color color;
};

class ModelResource;

class Renderer
{
public:
	struct RenderTexture
	{
		ID3D11Texture2D* texture = nullptr;
		ID3D11RenderTargetView* renderTargetView = nullptr;
		ID3D11DepthStencilView* depthStencilView = nullptr;
		ID3D11ShaderResourceView* shaderResourceView = nullptr;
		int width = 0;
		int height = 0;
	};

private:
	struct DebugCubeConstantBuffer
	{
		DirectX::SimpleMath::Matrix worldViewProjection;
	};

	static D3D_FEATURE_LEVEL m_FeatureLevel;

	static ID3D11Device* m_pDevice;
	static ID3D11DeviceContext* m_pDeviceContext;
	static IDXGISwapChain* m_pSwapChain;
	static ID3D11RenderTargetView* m_pRenderTargetView;
	static ID3D11DepthStencilView* m_pDepthStencilView;
	static bool m_ComInitialized;

	static ID3D11Buffer* m_pWorldBuffer;
	static ID3D11Buffer* m_pViewBuffer;
	static ID3D11Buffer* m_pProjectionBuffer;
	static ID3D11Buffer* m_pTextureBuffer;

	static ID3D11DepthStencilState* m_pDepthStateEnable;
	static ID3D11DepthStencilState* m_pDepthStateDisable;

	static ID3D11BlendState* m_BlendState[MAX_BLENDSTATE];
	static ID3D11BlendState* m_pBlendStateATC;

	static ID3D11RasterizerState* m_pRasterizerSolid;
	static ID3D11RasterizerState* m_pRasterizerWireframe;

	// カメラは Renderer が所有しない。外部から渡された行列だけを保持する。
	static DirectX::SimpleMath::Matrix m_ViewMatrix;
	static DirectX::SimpleMath::Matrix m_ProjectionMatrix;

	// カメラ確認用の最小 3D 描画リソース。
	static ID3D11VertexShader* m_pDebugCubeVertexShader;
	static ID3D11PixelShader* m_pDebugCubePixelShader;
	static ID3D11InputLayout* m_pDebugCubeInputLayout;
	static ID3D11Buffer* m_pDebugCubeVertexBuffer;
	static ID3D11Buffer* m_pDebugCubeIndexBuffer;
	static ID3D11Buffer* m_pDebugCubeConstantBuffer;

	// FBX などのモデル描画用リソース。頂点には将来のスキニング用データも含める。
	static ID3D11VertexShader* m_pModelVertexShader;
	static ID3D11PixelShader* m_pModelPixelShader;
	static ID3D11InputLayout* m_pModelInputLayout;
	static ID3D11Buffer* m_pModelConstantBuffer;
	static ID3D11SamplerState* m_pModelSamplerState;
	static ID3D11ShaderResourceView* m_pWhiteTextureView;

	static HRESULT CreateRenderAndDepthResources(int width, int height);
	static void SetViewport(int width, int height);

	static HRESULT CreateDebugCubeResources();
	static HRESULT CreateModelRenderResources();
	static HRESULT CreateWhiteTextureResource();
	static HRESULT CompileShader(const char* source, const char* entryPoint, const char* target, ID3DBlob** blob);
	static void ReleaseDebugCubeResources();
	static void ReleaseModelRenderResources();

public:
	static HRESULT Init();
	static void Uninit();
	static void DrawStart();
	static void DrawEnd();

	static ID3D11Device* GetDevice();
	static ID3D11DeviceContext* GetDeviceContext();

	static HRESULT ResizeWindow(int width, int height);
	static HRESULT CreateRenderTexture(RenderTexture& renderTexture, int width, int height);
	static void ReleaseRenderTexture(RenderTexture& renderTexture);
	static void BeginRenderTexture(RenderTexture& renderTexture, const float clearColor[4]);
	static void RestoreBackBuffer();

	static void SetViewProjection(
		const DirectX::SimpleMath::Matrix& view,
		const DirectX::SimpleMath::Matrix& projection);

	static void DrawDebugCube(const DirectX::SimpleMath::Matrix& world);
	static bool DrawModel(const ModelResource& model, const DirectX::SimpleMath::Matrix& world);
	static void SetDepthEnable(bool Enable);
};
