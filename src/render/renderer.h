#pragma once
#include <Windows.h>
#include <d3d11.h>
#include <dcomp.h>
#include <dxgi1_2.h>
extern ID3D11Device *g_pd3dDevice;
extern ID3D11DeviceContext *g_pd3dDeviceContext;
extern IDXGISwapChain1 *g_pSwapChain;
extern IDCompositionDevice *g_compositionDevice;
extern IDCompositionTarget *g_compositionTarget;
extern IDCompositionVisual *g_compositionVisual;
extern ID3D11Texture2D *g_backBuffer;
extern ID3D11Texture2D *g_multisampleTexture;
extern UINT g_samples;
extern bool g_menuOpen;
extern bool g_SwapChainOccluded;
extern UINT g_ResizeWidth;
extern UINT g_ResizeHeight;
extern ID3D11RenderTargetView *g_mainRenderTargetView;
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
bool CreateRenderTarget();
void CleanupRenderTarget();
void ResolveRenderTarget();
int ValidateAlphaRender();
