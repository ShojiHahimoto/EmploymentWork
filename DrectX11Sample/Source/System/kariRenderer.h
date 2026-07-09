#pragma once

#define _CRT_SECURE_NO_WARNINGS

#include <d3d11.h>
#include <DirectXMath.h>
#include <SimpleMath.h>
#include <d3dcompiler.h>
#include <vector>
#include <string>

// ライブラリ
#pragma comment(lib,"directxtk.lib")
#pragma comment(lib,"d3d11.lib")
#pragma comment(lib,"d3dcompiler.lib")

//------------------------------------------
// 解放マクロ
//------------------------------------------
#define SAFE_RELEASE(p){ if(p){ p->Release(); p = nullptr; } }

//------------------------------------------
// 3D頂点
//------------------------------------------
struct VETREX_3D
{
    DirectX::SimpleMath::Vector3 position;
    DirectX::SimpleMath::Vector3 normal;
    DirectX::SimpleMath::Color color;
    DirectX::SimpleMath::Vector2 uv;
};

//------------------------------------------
// ブレンドステート
//------------------------------------------
enum EBlendState
{
    BS_NONE = 0,
    BS_ALPHABLEND,
    BS_ADDITIVE,
    BS_SUBTRACTION,
    MAX_BLENDSTATE
};

//------------------------------------------
// ライン描画
//------------------------------------------
struct LineVertex
{
    DirectX::SimpleMath::Vector3 position;
    DirectX::SimpleMath::Vector3 color;
};


//------------------------------------------
// Renderer
//------------------------------------------
class Renderer
{
private:

    static D3D_FEATURE_LEVEL m_FeatureLevel;

    static ID3D11Device* m_pDevice;
    static ID3D11DeviceContext* m_pDeviceContext;
    static IDXGISwapChain* m_pSwapChain;
    static ID3D11RenderTargetView* m_pRenderTargetView;

    // ★修正：綴り修正
    static ID3D11DepthStencilView* m_pDepthStencilView;

    //------------------------------------------
    // 定数バッファ
    //------------------------------------------
    static ID3D11Buffer* m_pWorldBuffer;
    static ID3D11Buffer* m_pViewBuffer;
    static ID3D11Buffer* m_pProjectionBuffer;
    static ID3D11Buffer* m_pTextureBuffer;

    //------------------------------------------
    // ステート
    //------------------------------------------
    static ID3D11DepthStencilState* m_pDepthStateEnable;
    static ID3D11DepthStencilState* m_pDepthStateDisable;

    static ID3D11BlendState* m_BlendState[MAX_BLENDSTATE];
    static ID3D11BlendState* m_pBlendStateATC;

    static ID3D11RasterizerState* m_pRasterizerSolid;
    static ID3D11RasterizerState* m_pRasterizerWireframe;

    //------------------------------------------
    // 内部生成処理
    //------------------------------------------
    static HRESULT CreateRenderAndDepthResources(int width, int height);
    static void SetViewport(int width, int height);

public:

    //------------------------------------------
    // 初期化
    //------------------------------------------
    static HRESULT Init();

    //------------------------------------------
    // 解放
    //------------------------------------------
    static void Uninit();

    //------------------------------------------
    // 描画開始
    //------------------------------------------
    static void DrawStart();

    //------------------------------------------
    // 描画終了
    //------------------------------------------
    static void DrawEnd();

    //------------------------------------------
    // リサイズ
    //------------------------------------------
    static HRESULT ResizeWindow(int width, int height);

    //------------------------------------------
    // 深度制御
    //------------------------------------------
    static void SetDepthEnable(bool Enable);
};
