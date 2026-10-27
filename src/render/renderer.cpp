#include "renderer.h"
#include <algorithm>
#include <wrl/client.h>
ID3D11Device *g_pd3dDevice = nullptr;
ID3D11DeviceContext *g_pd3dDeviceContext = nullptr;
IDXGISwapChain1 *g_pSwapChain = nullptr;
IDCompositionDevice *g_compositionDevice = nullptr;
IDCompositionTarget *g_compositionTarget = nullptr;
IDCompositionVisual *g_compositionVisual = nullptr;
ID3D11Texture2D *g_backBuffer = nullptr;
ID3D11Texture2D *g_multisampleTexture = nullptr;
UINT g_samples = 1;
bool g_menuOpen = true;
bool g_SwapChainOccluded = false;
UINT g_ResizeWidth = 0, g_ResizeHeight = 0;
ID3D11RenderTargetView *g_mainRenderTargetView = nullptr;

bool CreateDeviceD3D(HWND hWnd) {
    using Microsoft::WRL::ComPtr;
    D3D_FEATURE_LEVEL level{};
    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    HRESULT hr =
        D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                          levels, 2, D3D11_SDK_VERSION, &g_pd3dDevice, &level, &g_pd3dDeviceContext);
    if (FAILED(hr))
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                               levels, 2, D3D11_SDK_VERSION, &g_pd3dDevice, &level, &g_pd3dDeviceContext);
    if (FAILED(hr))
        return false;
    ComPtr<IDXGIDevice> dxgiDevice;
    ComPtr<IDXGIAdapter> adapter;
    ComPtr<IDXGIFactory2> factory;
    if (FAILED(g_pd3dDevice->QueryInterface(IID_PPV_ARGS(dxgiDevice.GetAddressOf()))) ||
        FAILED(dxgiDevice->GetAdapter(adapter.GetAddressOf())) ||
        FAILED(adapter->GetParent(IID_PPV_ARGS(factory.GetAddressOf()))))
        return false;
    ComPtr<IDXGIDevice1> latencyDevice;
    if (SUCCEEDED(dxgiDevice.As(&latencyDevice)))
        latencyDevice->SetMaximumFrameLatency(1);
    RECT client{};
    GetClientRect(hWnd, &client);
    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width = static_cast<UINT>((std::max)(1L, client.right));
    desc.Height = static_cast<UINT>((std::max)(1L, client.bottom));
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.Scaling = DXGI_SCALING_STRETCH;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
    if (FAILED(factory->CreateSwapChainForComposition(g_pd3dDevice, &desc, nullptr, &g_pSwapChain)))
        return false;
    if (FAILED(DCompositionCreateDevice(dxgiDevice.Get(), __uuidof(IDCompositionDevice),
                                        reinterpret_cast<void **>(&g_compositionDevice))) ||
        FAILED(g_compositionDevice->CreateTargetForHwnd(hWnd, TRUE, &g_compositionTarget)) ||
        FAILED(g_compositionDevice->CreateVisual(&g_compositionVisual)) ||
        FAILED(g_compositionVisual->SetContent(g_pSwapChain)) ||
        FAILED(
            g_compositionVisual->SetBitmapInterpolationMode(DCOMPOSITION_BITMAP_INTERPOLATION_MODE_LINEAR)) ||
        FAILED(g_compositionTarget->SetRoot(g_compositionVisual)) || FAILED(g_compositionDevice->Commit()))
        return false;
    return CreateRenderTarget();
}

void CleanupDeviceD3D() {
    if (g_compositionVisual)
        g_compositionVisual->SetContent(nullptr);
    if (g_compositionTarget)
        g_compositionTarget->SetRoot(nullptr);
    if (g_compositionDevice)
        g_compositionDevice->Commit();
    if (g_compositionVisual) {
        g_compositionVisual->Release();
        g_compositionVisual = nullptr;
    }
    if (g_compositionTarget) {
        g_compositionTarget->Release();
        g_compositionTarget = nullptr;
    }
    if (g_compositionDevice) {
        g_compositionDevice->Release();
        g_compositionDevice = nullptr;
    }
    CleanupRenderTarget();
    if (g_pSwapChain) {
        g_pSwapChain->Release();
        g_pSwapChain = nullptr;
    }
    if (g_pd3dDeviceContext) {
        g_pd3dDeviceContext->Release();
        g_pd3dDeviceContext = nullptr;
    }
    if (g_pd3dDevice) {
        g_pd3dDevice->Release();
        g_pd3dDevice = nullptr;
    }
}

bool CreateRenderTarget() {
    if (FAILED(g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&g_backBuffer))))
        return false;
    D3D11_TEXTURE2D_DESC desc{};
    g_backBuffer->GetDesc(&desc);
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;
    g_samples = 1;
    for (UINT count : {4u, 2u}) {
        UINT quality = 0;
        if (FAILED(g_pd3dDevice->CheckMultisampleQualityLevels(desc.Format, count, &quality)) || !quality)
            continue;
        desc.SampleDesc.Count = count;
        desc.SampleDesc.Quality = 0;
        if (SUCCEEDED(g_pd3dDevice->CreateTexture2D(&desc, nullptr, &g_multisampleTexture))) {
            if (SUCCEEDED(g_pd3dDevice->CreateRenderTargetView(g_multisampleTexture, nullptr,
                                                               &g_mainRenderTargetView))) {
                g_samples = count;
                return true;
            }
            g_multisampleTexture->Release();
            g_multisampleTexture = nullptr;
        }
    }
    return SUCCEEDED(g_pd3dDevice->CreateRenderTargetView(g_backBuffer, nullptr, &g_mainRenderTargetView));
}

void ResolveRenderTarget() {
    g_pd3dDeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
    if (g_multisampleTexture)
        g_pd3dDeviceContext->ResolveSubresource(g_backBuffer, 0, g_multisampleTexture, 0,
                                                DXGI_FORMAT_B8G8R8A8_UNORM);
}

void CleanupRenderTarget() {
    if (g_pd3dDeviceContext)
        g_pd3dDeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
    if (g_mainRenderTargetView) {
        g_mainRenderTargetView->Release();
        g_mainRenderTargetView = nullptr;
    }
    if (g_multisampleTexture) {
        g_multisampleTexture->Release();
        g_multisampleTexture = nullptr;
    }
    if (g_backBuffer) {
        g_backBuffer->Release();
        g_backBuffer = nullptr;
    }
}
