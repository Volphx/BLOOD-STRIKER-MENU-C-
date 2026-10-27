#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include "renderer.h"
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <wrl/client.h>
int ValidateAlphaRender() {
    using Microsoft::WRL::ComPtr;
    wchar_t exe[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    const auto folder = std::filesystem::path(exe).parent_path();
    std::ofstream report(folder / L"alpha-render-validation.json");
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    auto *draw = ImGui::GetForegroundDrawList();
    draw->AddLine({35.25f, 35.5f}, {400.75f, 180.25f}, IM_COL32_WHITE, 1.25f);
    draw->AddLine({35.25f, 90.5f}, {400.75f, 230.25f}, IM_COL32(51, 230, 191, 180), 0.8f);
    draw->AddCircle({130, 125}, 50, IM_COL32(235, 245, 255, 255), 96, 1.1f);
    draw->AddText({35, 250}, IM_COL32_WHITE, "Alpha / MSAA - no black matte");
    draw->AddText({35, 277}, IM_COL32_WHITE, "Nome: A\xc3\xa7\xc3\xa3o \xe5\xbd\xa1\xe4\xb9\x88\xe5\xbd\xa1");
    ImGui::Render();
    const float clear[] = {0, 0, 0, 0};
    g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
    g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    ResolveRenderTarget();
    D3D11_TEXTURE2D_DESC desc{};
    g_backBuffer->GetDesc(&desc);
    const UINT width = desc.Width, height = desc.Height;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.MiscFlags = 0;
    ComPtr<ID3D11Texture2D> staging;
    if (FAILED(g_pd3dDevice->CreateTexture2D(&desc, nullptr, &staging))) {
        report << "{\"status\":\"CREATE_STAGING_FAILED\"}";
        return 1;
    }
    g_pd3dDeviceContext->CopyResource(staging.Get(), g_backBuffer);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(g_pd3dDeviceContext->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped))) {
        report << "{\"status\":\"MAP_FAILED\"}";
        return 1;
    }
    uint64_t fractional = 0, transparent = 0, badPremultiply = 0, blackPixels = 0;
    std::ofstream raw(folder / L"alpha-render-test.bgra", std::ios::binary);
    for (UINT y = 0; y < height; ++y) {
        const auto *row = static_cast<const unsigned char *>(mapped.pData) + y * mapped.RowPitch;
        raw.write(reinterpret_cast<const char *>(row), static_cast<std::streamsize>(width) * 4);
        for (UINT x = 0; x < width; ++x) {
            const auto *pixel = row + x * 4;
            const int a = pixel[3];
            const int peak = (std::max)({int(pixel[0]), int(pixel[1]), int(pixel[2])});
            if (a > 0 && a < 255)
                ++fractional;
            if (a == 0)
                ++transparent;
            if (peak > a + 2)
                ++badPremultiply;
            if (a > 2 && peak < a / 2)
                ++blackPixels;
        }
    }
    g_pd3dDeviceContext->Unmap(staging.Get(), 0);
    DXGI_SWAP_CHAIN_DESC1 swapDesc{};
    g_pSwapChain->GetDesc1(&swapDesc);
    const auto font = ImGui::GetFont();
    const bool unicodeNames = font->FindGlyphNoFallback(0x00e7) && font->FindGlyphNoFallback(0x5f61) &&
                              font->FindGlyphNoFallback(0x4e48);
    const bool ok = fractional > 0 && transparent > 0 && !badPremultiply && !blackPixels &&
                    swapDesc.AlphaMode == DXGI_ALPHA_MODE_PREMULTIPLIED;
    report << "{\n\"status\":\"" << (ok ? "PASS" : "FAIL") << "\",\n\"width\":" << width
           << ",\n\"height\":" << height << ",\n\"msaaSamples\":" << g_samples
           << ",\n\"premultipliedAlpha\":true,\n\"fractionalAlphaPixels\":" << fractional
           << ",\n\"transparentPixels\":" << transparent
           << ",\n\"invalidPremultipliedPixels\":" << badPremultiply
           << ",\n\"unexpectedBlackPixels\":" << blackPixels
           << ",\n\"unicodeNameGlyphs\":" << (unicodeNames ? "true" : "false") << "\n}\n";
    return ok ? 0 : 1;
}
