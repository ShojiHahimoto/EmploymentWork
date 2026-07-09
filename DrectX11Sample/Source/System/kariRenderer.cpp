#include "System/Renderer.h"
#include "System/Application.h"

//=====================================================
// static変数定義（★全部必須）
//=====================================================

D3D_FEATURE_LEVEL Renderer::m_FeatureLevel = D3D_FEATURE_LEVEL_11_0;

ID3D11Device* Renderer::m_pDevice = nullptr;
ID3D11DeviceContext* Renderer::m_pDeviceContext = nullptr;
IDXGISwapChain* Renderer::m_pSwapChain = nullptr;
ID3D11RenderTargetView* Renderer::m_pRenderTargetView = nullptr;
ID3D11DepthStencilView* Renderer::m_pDepthStencilView = nullptr;

// 定数バッファ
ID3D11Buffer* Renderer::m_pWorldBuffer = nullptr;
ID3D11Buffer* Renderer::m_pViewBuffer = nullptr;
ID3D11Buffer* Renderer::m_pProjectionBuffer = nullptr;
ID3D11Buffer* Renderer::m_pTextureBuffer = nullptr;

// Depth
ID3D11DepthStencilState* Renderer::m_pDepthStateEnable = nullptr;
ID3D11DepthStencilState* Renderer::m_pDepthStateDisable = nullptr;

// Blend
ID3D11BlendState* Renderer::m_BlendState[MAX_BLENDSTATE] = {};
ID3D11BlendState* Renderer::m_pBlendStateATC = nullptr;

// Rasterizer
ID3D11RasterizerState* Renderer::m_pRasterizerSolid = nullptr;
ID3D11RasterizerState* Renderer::m_pRasterizerWireframe = nullptr;


//------------------------------------------
// 初期化
//------------------------------------------
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

    //------------------------------------------
    // デバイス生成
    //------------------------------------------
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

    //------------------------------------------
    // 深度ステート作成（重要）
    //------------------------------------------
    D3D11_DEPTH_STENCIL_DESC depthDesc = {};

    depthDesc.DepthEnable = TRUE;
    depthDesc.DepthFunc = D3D11_COMPARISON_LESS;
    depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;

    m_pDevice->CreateDepthStencilState(&depthDesc, &m_pDepthStateEnable);

    depthDesc.DepthEnable = FALSE;
    m_pDevice->CreateDepthStencilState(&depthDesc, &m_pDepthStateDisable);

    //------------------------------------------
    // ブレンドステート作成
    //------------------------------------------
    D3D11_BLEND_DESC blendDesc = {};

    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    m_pDevice->CreateBlendState(&blendDesc, &m_BlendState[BS_ALPHABLEND]);

    //------------------------------------------
    // ラスタライザステート
    //------------------------------------------
    D3D11_RASTERIZER_DESC rsDesc = {};

    rsDesc.FillMode = D3D11_FILL_SOLID;
    rsDesc.CullMode = D3D11_CULL_BACK;
    m_pDevice->CreateRasterizerState(&rsDesc, &m_pRasterizerSolid);

    rsDesc.FillMode = D3D11_FILL_WIREFRAME;
    m_pDevice->CreateRasterizerState(&rsDesc, &m_pRasterizerWireframe);

    //------------------------------------------
    // RenderTarget作成
    //------------------------------------------
    return CreateRenderAndDepthResources(
        Application::GetWidth(),
        Application::GetHeight()
    );
}


//------------------------------------------
// 描画開始
//------------------------------------------
void Renderer::DrawStart()
{
    float clearColor[4] = { 0.2f, 0.3f, 0.4f, 1.0f };

    //------------------------------------------
    // 背景クリア
    //------------------------------------------
    m_pDeviceContext->ClearRenderTargetView(m_pRenderTargetView, clearColor);

    //------------------------------------------
    // 深度クリア
    //------------------------------------------
    m_pDeviceContext->ClearDepthStencilView(
        m_pDepthStencilView,
        D3D11_CLEAR_DEPTH,
        1.0f,
        0
    );

    //------------------------------------------
    // ステート設定（毎フレーム戻すのが基本）
    //------------------------------------------
    m_pDeviceContext->OMSetRenderTargets(1, &m_pRenderTargetView, m_pDepthStencilView);

    m_pDeviceContext->OMSetDepthStencilState(m_pDepthStateEnable, 0);
    m_pDeviceContext->RSSetState(m_pRasterizerSolid);
}

//------------------------------------------
// 描画終了
//------------------------------------------
void Renderer::DrawEnd()
{
    m_pSwapChain->Present(1, 0);
}


//------------------------------------------
// リサイズ
//------------------------------------------
HRESULT Renderer::ResizeWindow(int width, int height)
{
    SAFE_RELEASE(m_pRenderTargetView);
    SAFE_RELEASE(m_pDepthStencilView);

    HRESULT hr = m_pSwapChain->ResizeBuffers(
        0,
        width,
        height,
        DXGI_FORMAT_UNKNOWN,
        0
    );

    if (FAILED(hr)) return hr;

    return CreateRenderAndDepthResources(width, height);
}


//------------------------------------------
// RenderTarget / Depth生成
//------------------------------------------
HRESULT Renderer::CreateRenderAndDepthResources(int width, int height)
{
    ID3D11Texture2D* backBuffer = nullptr;

    HRESULT hr = m_pSwapChain->GetBuffer(
        0,
        __uuidof(ID3D11Texture2D),
        (void**)&backBuffer
    );

    if (FAILED(hr)) return hr;

    //------------------------------------------
    // RenderTarget作成
    //------------------------------------------
    hr = m_pDevice->CreateRenderTargetView(
        backBuffer,
        nullptr,
        &m_pRenderTargetView
    );

    backBuffer->Release();

    if (FAILED(hr)) return hr;

    //------------------------------------------
    // 深度バッファ
    //------------------------------------------
    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width = width;
    depthDesc.Height = height;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    ID3D11Texture2D* depthBuffer = nullptr;

    hr = m_pDevice->CreateTexture2D(
        &depthDesc,
        nullptr,
        &depthBuffer
    );
    if (FAILED(hr)) return hr;

    hr = m_pDevice->CreateDepthStencilView(
        depthBuffer,
        nullptr,
        &m_pDepthStencilView
    );
    depthBuffer->Release();

    if (FAILED(hr)) return hr;

    //------------------------------------------
    // Viewport
    //------------------------------------------
    SetViewport(width, height);

    return S_OK;
}


//------------------------------------------
// Viewport
//------------------------------------------
void Renderer::SetViewport(int width, int height)
{
    D3D11_VIEWPORT vp = {};

    vp.Width = (FLOAT)width;
    vp.Height = (FLOAT)height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;

    m_pDeviceContext->RSSetViewports(1, &vp);
}


//------------------------------------------
// Depth有効/無効
//------------------------------------------
void Renderer::SetDepthEnable(bool Enable)
{
    if (Enable)
        m_pDeviceContext->OMSetDepthStencilState(m_pDepthStateEnable, 0);
    else
        m_pDeviceContext->OMSetDepthStencilState(m_pDepthStateDisable, 0);
}


//------------------------------------------
// 解放
//------------------------------------------
void Renderer::Uninit()
{
    SAFE_RELEASE(m_pDepthStencilView);
    SAFE_RELEASE(m_pRenderTargetView);

    // State
    SAFE_RELEASE(m_pDepthStateEnable);
    SAFE_RELEASE(m_pDepthStateDisable);

    for (int i = 0; i < MAX_BLENDSTATE; i++)
        SAFE_RELEASE(m_BlendState[i]);

    SAFE_RELEASE(m_pRasterizerSolid);
    SAFE_RELEASE(m_pRasterizerWireframe);

    SAFE_RELEASE(m_pSwapChain);
    SAFE_RELEASE(m_pDeviceContext);
    SAFE_RELEASE(m_pDevice);
}
