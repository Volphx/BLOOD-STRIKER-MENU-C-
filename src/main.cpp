#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include "input/recoil.h"
#include "platform/window.h"
#include "render/renderer.h"
#include "ui/menu.h"
#include "ui/theme.h"
#include "visuals/esp.h"
int main(int argc, char **argv) {
    const bool renderTest = argc > 1 && std::strcmp(argv[1], "--render-test") == 0;
    if (!renderTest)
        if (const auto console = GetConsoleWindow())
            ShowWindow(console, SW_HIDE);
    WNDCLASSEXW wc = {sizeof(wc),
                      0,
                      WndProc,
                      0L,
                      0L,
                      GetModuleHandle(nullptr),
                      nullptr,
                      nullptr,
                      nullptr,
                      nullptr,
                      L"VZXCheatBSOverlay",
                      nullptr};
    ::SetProcessDPIAware();
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_NOREDIRECTIONBITMAP | WS_EX_TOOLWINDOW, wc.lpszClassName,
        L"VZX CHEAT BS", WS_POPUP, 0, 0, renderTest ? 480 : GetSystemMetrics(SM_CXSCREEN),
        renderTest ? 320 : GetSystemMetrics(SM_CYSCREEN), nullptr, nullptr, wc.hInstance, nullptr);
    if (!hwnd)
        return 2;

    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    if (!renderTest)
        ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    configureTheme(io);
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);
    if (renderTest) {
        const int result = ValidateAlphaRender();
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        CleanupDeviceD3D();
        DestroyWindow(hwnd);
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return result;
    }

    GameReader game;
    Options options;
    bool menuOpen = true;
    bool gameValid = false;
    RecoilAccumulator recoil;
    DWORD lastAttach = 0;
    ImVec4 clear_color = ImVec4(0, 0, 0, 0);
    HANDLE frameTimer = CreateWaitableTimerExW(nullptr, nullptr, 0x2, TIMER_ALL_ACCESS);
    if (!frameTimer)
        frameTimer = CreateWaitableTimerW(nullptr, FALSE, nullptr);

    bool done = false;
    while (!done) {
        const auto frameStart = std::chrono::steady_clock::now();
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        if (g_SwapChainOccluded && g_pSwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED) {
            ::Sleep(10);
            continue;
        }
        g_SwapChainOccluded = false;

        HWND foreground = GetForegroundWindow();
        DWORD foregroundPid{};
        GetWindowThreadProcessId(foreground, &foregroundPid);
        if (game.processId() && foregroundPid == game.processId()) {
            RECT client{}, overlay{};
            POINT origin{};
            if (GetClientRect(foreground, &client) && ClientToScreen(foreground, &origin) &&
                GetWindowRect(hwnd, &overlay) && client.right > 0 && client.bottom > 0 &&
                (overlay.left != origin.x || overlay.top != origin.y ||
                 overlay.right - overlay.left != client.right ||
                 overlay.bottom - overlay.top != client.bottom))
                SetWindowPos(hwnd, HWND_TOPMOST, origin.x, origin.y, client.right, client.bottom,
                             SWP_NOACTIVATE);
        }

        if (g_ResizeWidth != 0 && g_ResizeHeight != 0) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            if (!CreateRenderTarget()) {
                done = true;
                continue;
            }
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        if (GetAsyncKeyState(VK_END) & 1)
            done = true;
        if (GetAsyncKeyState(VK_INSERT) & 1) {
            menuOpen = !menuOpen;
            g_menuOpen = menuOpen;
            SetWindowLongPtrW(hwnd, GWL_EXSTYLE,
                              WS_EX_TOPMOST | WS_EX_NOREDIRECTIONBITMAP | WS_EX_TOOLWINDOW |
                                  (menuOpen ? 0 : WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE));
            if (!menuOpen)
                SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
            SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        }
        const DWORD now = GetTickCount();
        if (game.processId() || now - lastAttach >= 1000) {
            game.readSkeletons = options.skeleton || options.aim;
            gameValid = game.update();
            lastAttach = now;
        }
        const bool gameFocused = game.processId() && foregroundPid == game.processId();
        const bool recoilActive = options.recoil && gameValid && gameFocused && !menuOpen &&
                                  (GetAsyncKeyState(VK_LBUTTON) & 0x8000) &&
                                  (GetAsyncKeyState(VK_RBUTTON) & 0x8000);
        const auto recoilMove = recoil.step(ImGui::GetIO().DeltaTime, recoilActive, options.recoilHorizontal,
                                            options.recoilVertical);
        if (recoilMove.first || recoilMove.second) {
            INPUT input{};
            input.type = INPUT_MOUSE;
            input.mi.dwFlags = MOUSEEVENTF_MOVE;
            input.mi.dx = recoilMove.first;
            input.mi.dy = recoilMove.second;
            if (SendInput(1, &input, sizeof(input)) != 1)
                recoil.x = recoil.y = 0;
        }
        if (menuOpen || gameFocused)
            drawFeatures(game, options, gameValid, menuOpen);
        if (menuOpen)
            renderCleanMenu(game, options, gameValid, done);
        ImGui::Render();
        const float clear_color_with_alpha[4] = {clear_color.x * clear_color.w, clear_color.y * clear_color.w,
                                                 clear_color.z * clear_color.w, clear_color.w};
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        ResolveRenderTarget();

        HRESULT hr = g_pSwapChain->Present(options.vsync ? 1 : 0, 0);
        g_SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
        if (!options.vsync && frameTimer) {
            const double elapsed =
                std::chrono::duration<double>(std::chrono::steady_clock::now() - frameStart).count();
            const double remaining = 1.0 / options.frameLimit - elapsed;
            if (remaining > 0) {
                LARGE_INTEGER due{};
                due.QuadPart = -static_cast<LONGLONG>(remaining * 10000000);
                if (SetWaitableTimer(frameTimer, &due, 0, nullptr, nullptr, FALSE))
                    WaitForSingleObject(frameTimer, 50);
            }
        }
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    if (frameTimer)
        CloseHandle(frameTimer);

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}
