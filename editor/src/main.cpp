// FH6 ChaseCam editor: Win32 + Direct3D 11 + Dear ImGui.
#ifdef FOVTEST
#include "fovtest.hpp"
using MainApp = FovTest;
static const wchar_t* APP_TITLE = L"FH6 FOV Live Test";
static const int DEF_W = 760, DEF_H = 580, MIN_W = 640, MIN_H = 560;
#else
#include "app.hpp"
using MainApp = App;
static const wchar_t* APP_TITLE = L"Forza Horizon Camera Editor";
static const int DEF_W = 1120, DEF_H = 690, MIN_W = 900, MIN_H = 580;
#endif
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include "theme.hpp"
#include <d3d11.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <windows.h>
#include <cwchar>

static ID3D11Device* gDev = nullptr;
static ID3D11DeviceContext* gCtx = nullptr;
static IDXGISwapChain* gSwap = nullptr;
static ID3D11RenderTargetView* gRtv = nullptr;
static UINT gResizeW = 0, gResizeH = 0;
static HWND gHwnd = nullptr;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

// ---------------------------------------------------------------------------
// Platform hooks for the app
fs::path exeDir() {
    wchar_t buf[MAX_PATH * 4];
    DWORD n = GetModuleFileNameW(nullptr, buf, (DWORD)(sizeof buf / sizeof buf[0]));
    return fs::path(std::wstring(buf, n)).parent_path();
}

fs::path pickFolder(const fs::path& start) {
    fs::path out;
    IFileOpenDialog* dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg)))) return out;
    DWORD opts = 0;
    dlg->GetOptions(&opts);
    dlg->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
    dlg->SetTitle(L"Choose the Forza Horizon 6 folder");
    if (!start.empty()) {
        IShellItem* si = nullptr;
        if (SUCCEEDED(SHCreateItemFromParsingName(start.wstring().c_str(), nullptr, IID_PPV_ARGS(&si)))) {
            dlg->SetFolder(si);
            si->Release();
        }
    }
    if (SUCCEEDED(dlg->Show(gHwnd))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dlg->GetResult(&item))) {
            PWSTR p = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &p))) { out = fs::path(p); CoTaskMemFree(p); }
            item->Release();
        }
    }
    dlg->Release();
    return out;
}

// Borderless full screen on the window's monitor, and back to the old placement.
static WINDOWPLACEMENT gPlacement = { sizeof(WINDOWPLACEMENT) };
void setFullscreen(bool on) {
    DWORD style = (DWORD)GetWindowLongW(gHwnd, GWL_STYLE);
    if (on && (style & WS_OVERLAPPEDWINDOW)) {
        MONITORINFO mi = { sizeof(mi) };
        if (GetWindowPlacement(gHwnd, &gPlacement) && GetMonitorInfoW(MonitorFromWindow(gHwnd, MONITOR_DEFAULTTOPRIMARY), &mi)) {
            SetWindowLongW(gHwnd, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(gHwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left,
                         mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
    } else if (!on && !(style & WS_OVERLAPPEDWINDOW)) {
        SetWindowLongW(gHwnd, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(gHwnd, &gPlacement);
        SetWindowPos(gHwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
}

void openFolder(const fs::path& p) { ShellExecuteW(nullptr, L"open", p.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL); }

// ---------------------------------------------------------------------------
static void createRtv() {
    ID3D11Texture2D* back = nullptr;
    gSwap->GetBuffer(0, IID_PPV_ARGS(&back));
    gDev->CreateRenderTargetView(back, nullptr, &gRtv);
    back->Release();
}
static void releaseRtv() { if (gRtv) { gRtv->Release(); gRtv = nullptr; } }

static bool createDevice(HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    D3D_FEATURE_LEVEL got;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 2, D3D11_SDK_VERSION,
                                               &sd, &gSwap, &gDev, &got, &gCtx);
    if (hr == DXGI_ERROR_UNSUPPORTED) // no GPU driver: software renderer
        hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, levels, 2, D3D11_SDK_VERSION,
                                           &sd, &gSwap, &gDev, &got, &gCtx);
    if (FAILED(hr)) return false;
    createRtv();
    return true;
}

static void destroyDevice() {
    releaseRtv();
    if (gSwap) gSwap->Release();
    if (gCtx) gCtx->Release();
    if (gDev) gDev->Release();
}

static LRESULT WINAPI wndProc(HWND h, UINT msg, WPARAM w, LPARAM l) {
    if (ImGui_ImplWin32_WndProcHandler(h, msg, w, l)) return true;
    switch (msg) {
    case WM_SIZE:
        if (w != SIZE_MINIMIZED) { gResizeW = LOWORD(l); gResizeH = HIWORD(l); }
        return 0;
    case WM_GETMINMAXINFO: {
        float s = ImGui_ImplWin32_GetDpiScaleForHwnd(h);
        MINMAXINFO* m = (MINMAXINFO*)l;
        m->ptMinTrackSize.x = (LONG)(MIN_W * s);
        m->ptMinTrackSize.y = (LONG)(MIN_H * s);
        return 0;
    }
    case WM_SYSCOMMAND:
        if ((w & 0xfff0) == SC_KEYMENU) return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, msg, w, l);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR cmd, int) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    ImGui_ImplWin32_EnableDpiAwareness();
    float scale = ImGui_ImplWin32_GetDpiScaleForMonitor(MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY));
    int winW = DEF_W, winH = DEF_H; // optional: --size 1400x900
    if (const wchar_t* s = cmd ? wcsstr(cmd, L"--size ") : nullptr) swscanf(s + 7, L"%dx%d", &winW, &winH);

    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, wndProc, 0, 0, inst, LoadIconW(inst, L"APPICON"), LoadCursor(nullptr, IDC_ARROW),
                       nullptr, nullptr, L"FH6ChaseCam", nullptr };
    RegisterClassExW(&wc);
    gHwnd = CreateWindowW(wc.lpszClassName, APP_TITLE, WS_OVERLAPPEDWINDOW, 100, 100, (int)(winW * scale), (int)(winH * scale),
                          nullptr, nullptr, wc.hInstance, nullptr);
#ifdef FOVTEST
    SetWindowPos(gHwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE); // stays over the game (windowed or borderless)
#endif
    BOOL dark = TRUE; // dark title bar (Windows 10 2004+ / 11)
    DwmSetWindowAttribute(gHwnd, 20, &dark, sizeof dark);
    COLORREF cap = RGB(0x16, 0x18, 0x1D);
    DwmSetWindowAttribute(gHwnd, 35, &cap, sizeof cap); // caption colour (Windows 11)

    if (!createDevice(gHwnd)) {
        destroyDevice();
        MessageBoxW(nullptr, L"Direct3D 11 isn't available on this PC.", APP_TITLE, MB_ICONERROR);
        return 1;
    }
    ShowWindow(gHwnd, SW_SHOWDEFAULT);
    UpdateWindow(gHwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ui::loadFonts();
    ui::applyTheme(scale);
    ImGui_ImplWin32_Init(gHwnd);
    ImGui_ImplDX11_Init(gDev, gCtx);

    MainApp app;
    app.init();
    const float clear[4] = { 0x16 / 255.f, 0x18 / 255.f, 0x1D / 255.f, 1 };
    bool done = false;
    while (!done) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) done = true;
        }
        if (done) break;
        if (IsIconic(gHwnd)) { Sleep(30); continue; }
        if (gResizeW && gResizeH) {
            releaseRtv();
            gSwap->ResizeBuffers(0, gResizeW, gResizeH, DXGI_FORMAT_UNKNOWN, 0);
            gResizeW = gResizeH = 0;
            createRtv();
        }
        float s = ImGui_ImplWin32_GetDpiScaleForHwnd(gHwnd);
        if (s != ui::S) ui::applyTheme(s);
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        app.frame(io.DisplaySize.x, io.DisplaySize.y);
        ImGui::Render();
        gCtx->OMSetRenderTargets(1, &gRtv, nullptr);
        gCtx->ClearRenderTargetView(gRtv, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        gSwap->Present(1, 0);
    }
    app.shutdown();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    destroyDevice();
    DestroyWindow(gHwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    CoUninitialize();
    return 0;
}
