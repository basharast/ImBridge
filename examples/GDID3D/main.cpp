/*
 * ImWindows D3D11 — 3D Spinning Cube Demo
 * Features: perspective cube, per-face colors, MVP constant buffer,
 *           animated background, GDI text overlay composited by host
 * Target: Windows 8.1+ (x86/ARM)
 */

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

// Target Windows 8.1 (0x0603) if not already defined
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0603
#endif
#ifndef WINVER
#define WINVER 0x0603
#endif
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <stdio.h>
#include <math.h>
#include <stdint.h>

// ---- D3D objects ----
IDXGISwapChain*          swapchain  = nullptr;
ID3D11Device*            dev        = nullptr;
ID3D11DeviceContext*     devcon     = nullptr;
ID3D11RenderTargetView*  backbuffer = nullptr;
ID3D11Buffer*            pVB        = nullptr;
ID3D11Buffer*            pIB        = nullptr;
ID3D11Buffer*            pCB        = nullptr;  // MVP constant buffer
ID3D11VertexShader*      pVS        = nullptr;
ID3D11PixelShader*       pPS        = nullptr;
ID3D11InputLayout*       pLayout    = nullptr;
ID3D11Texture2D*         pDSTex     = nullptr;  // Depth buffer texture
ID3D11DepthStencilView*  pDSV       = nullptr;
ID3D11DepthStencilState* pDSState   = nullptr;
ID3D11RasterizerState* pRasterState = nullptr;

static D3D11_VIEWPORT g_VP;
static float  g_Angle  = 0.0f;
static int    g_Frame  = 0;
static HWND   g_hWnd   = nullptr;

// ---- Minimal row-major 4x4 matrix math ----
struct Mat4 { float m[4][4]; };

static Mat4 MatId() {
    Mat4 r = {};
    r.m[0][0] = r.m[1][1] = r.m[2][2] = r.m[3][3] = 1.f;
    return r;
}
static Mat4 MatMul(const Mat4& a, const Mat4& b) {
    Mat4 r = {};
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            for (int k = 0; k < 4; k++)
                r.m[i][j] += a.m[i][k] * b.m[k][j];
    return r;
}
static Mat4 RotY(float a) {
    Mat4 r = MatId();
    r.m[0][0] =  cosf(a); r.m[0][2] = sinf(a);
    r.m[2][0] = -sinf(a); r.m[2][2] = cosf(a);
    return r;
}
static Mat4 RotX(float a) {
    Mat4 r = MatId();
    r.m[1][1] =  cosf(a); r.m[1][2] = -sinf(a);
    r.m[2][1] =  sinf(a); r.m[2][2] =  cosf(a);
    return r;
}
static Mat4 Trans(float x, float y, float z) {
    Mat4 r = MatId();
    r.m[3][0] = x; r.m[3][1] = y; r.m[3][2] = z;
    return r;
}
static Mat4 Persp(float fov, float asp, float n, float f) {
    Mat4 r = {};
    float ys = 1.f / tanf(fov * 0.5f);
    r.m[0][0] = ys / asp;
    r.m[1][1] = ys;
    r.m[2][2] = f / (f - n);
    r.m[2][3] = 1.f;
    r.m[3][2] = -n * f / (f - n);
    return r;
}

// ---- Vertex: position + RGBA ----
struct Vertex { float x, y, z, r, g, b, a; };

// ---- Cube: 24 verts (4 per face × 6 faces), 36 indices ----
static Vertex   g_Verts[24];
static uint16_t g_Idx[36];

static void BuildCube() {
    const float fc[6][4] = {
        { 1.0f, 0.3f, 0.3f, 1.0f }, // front  — red
        { 0.3f, 0.9f, 0.9f, 1.0f }, // back   — cyan
        { 0.3f, 1.0f, 0.3f, 1.0f }, // left   — green
        { 0.9f, 0.3f, 0.9f, 1.0f }, // right  — magenta
        { 0.3f, 0.5f, 1.0f, 1.0f }, // top    — blue
        { 1.0f, 0.9f, 0.2f, 1.0f }, // bottom — yellow
    };
    const float fv[6][4][3] = {
        {{ -.5f,-.5f,-.5f },{ .5f,-.5f,-.5f },{ .5f,.5f,-.5f },{ -.5f,.5f,-.5f }},  // front
        {{  .5f,-.5f, .5f },{-.5f,-.5f, .5f },{-.5f,.5f, .5f },{  .5f,.5f, .5f }},  // back
        {{ -.5f,-.5f, .5f },{-.5f,-.5f,-.5f },{-.5f,.5f,-.5f },{ -.5f,.5f, .5f }},  // left
        {{  .5f,-.5f,-.5f },{ .5f,-.5f, .5f },{ .5f,.5f, .5f },{  .5f,.5f,-.5f }},  // right
        {{ -.5f,.5f,-.5f },{ .5f, .5f,-.5f },{ .5f,.5f, .5f },{ -.5f,.5f, .5f }},   // top
        {{ -.5f,-.5f, .5f },{ .5f,-.5f, .5f },{ .5f,-.5f,-.5f },{ -.5f,-.5f,-.5f }},// bottom
    };
    for (int f = 0; f < 6; f++) {
        for (int v = 0; v < 4; v++) {
            int i = f * 4 + v;
            g_Verts[i] = { fv[f][v][0], fv[f][v][1], fv[f][v][2],
                           fc[f][0], fc[f][1], fc[f][2], fc[f][3] };
        }
        int b = f * 4;
        g_Idx[f*6+0]=b+0; g_Idx[f*6+1]=b+1; g_Idx[f*6+2]=b+2;
        g_Idx[f*6+3]=b+0; g_Idx[f*6+4]=b+2; g_Idx[f*6+5]=b+3;
    }
}

// ---- HLSL: cube vertex + pixel shader ----
static const char* g_VS = R"(
cbuffer CB : register(b0) { row_major matrix mvp; };
struct VI { float3 p : POSITION; float4 c : COLOR; };
struct VO { float4 p : SV_POSITION; float4 c : COLOR; };
VO main(VI i) { VO o; o.p = mul(float4(i.p, 1.0f), mvp); o.c = i.c; return o; }
)";
static const char* g_PS = R"(
struct PI { float4 p : SV_POSITION; float4 c : COLOR; };
float4 main(PI i) : SV_TARGET { return i.c; }
)";

typedef HRESULT(WINAPI* D3DCompile_t)(LPCVOID,SIZE_T,LPCSTR,const void*,const void*,LPCSTR,LPCSTR,UINT,UINT,ID3DBlob**,ID3DBlob**);
static D3DCompile_t pfnCompile = nullptr;

static ID3DBlob* Compile(const char* src, const char* entry, const char* model) {
    ID3DBlob *b = nullptr, *e = nullptr;
    if (pfnCompile) pfnCompile(src, strlen(src), "s", nullptr, nullptr, entry, model, 0, 0, &b, &e);
    if (e) e->Release();
    return b;
}

static bool InitD3D() {
    HMODULE hC = LoadLibraryA("d3dcompiler_47.dll");
    if (!hC) hC = LoadLibraryA("d3dcompiler.dll");
    if (hC) pfnCompile = (D3DCompile_t)GetProcAddress(hC, "D3DCompile");
    if (!pfnCompile) return false;

    ID3DBlob* vsB = Compile(g_VS, "main", "vs_4_0_level_9_1");
    ID3DBlob* psB = Compile(g_PS, "main", "ps_4_0_level_9_1");
    if (!vsB || !psB) { if(vsB) vsB->Release(); if(psB) psB->Release(); return false; }

    dev->CreateVertexShader(vsB->GetBufferPointer(), vsB->GetBufferSize(), nullptr, &pVS);
    dev->CreatePixelShader(psB->GetBufferPointer(),  psB->GetBufferSize(), nullptr, &pPS);

    D3D11_INPUT_ELEMENT_DESC lay[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    dev->CreateInputLayout(lay, 2, vsB->GetBufferPointer(), vsB->GetBufferSize(), &pLayout);
    vsB->Release(); psB->Release();

    BuildCube();

    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT; bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.ByteWidth = sizeof(g_Verts);
    D3D11_SUBRESOURCE_DATA vd = { g_Verts };
    dev->CreateBuffer(&bd, &vd, &pVB);

    bd.BindFlags = D3D11_BIND_INDEX_BUFFER; bd.ByteWidth = sizeof(g_Idx);
    D3D11_SUBRESOURCE_DATA id = { g_Idx };
    dev->CreateBuffer(&bd, &id, &pIB);

    // Constant buffer: 64 bytes (one Mat4), CPU writable each frame
    bd.Usage = D3D11_USAGE_DYNAMIC; bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE; bd.ByteWidth = 64;
    dev->CreateBuffer(&bd, nullptr, &pCB);

    // Depth stencil: D16_UNORM is well-supported on all W10M feature levels
    D3D11_TEXTURE2D_DESC dd = {};
    dd.Width=640; dd.Height=480; dd.MipLevels=1; dd.ArraySize=1;
    dd.Format=DXGI_FORMAT_D16_UNORM; dd.SampleDesc.Count=1; dd.BindFlags=D3D11_BIND_DEPTH_STENCIL;
    dev->CreateTexture2D(&dd, nullptr, &pDSTex);
    if (pDSTex) dev->CreateDepthStencilView(pDSTex, nullptr, &pDSV);

    D3D11_DEPTH_STENCIL_DESC dsd = {};
    dsd.DepthEnable=TRUE; dsd.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ALL;
    dsd.DepthFunc=D3D11_COMPARISON_LESS;
    dev->CreateDepthStencilState(&dsd, &pDSState);

    D3D11_RASTERIZER_DESC rd = {};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_BACK;
    rd.FrontCounterClockwise = TRUE;   // <-- critical fix
    dev->CreateRasterizerState(&rd, &pRasterState);

    return pVB && pIB && pCB && pVS && pPS && pLayout;
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT msg, WPARAM wP, LPARAM lP) {
    if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProc(hWnd, msg, wP, lP);
}

static void RenderFrame(HDC hdc) {
    devcon->OMSetRenderTargets(1, &backbuffer, pDSV);
    devcon->RSSetViewports(1, &g_VP);
    if (pDSState) devcon->OMSetDepthStencilState(pDSState, 0);
    if (pRasterState) devcon->RSSetState(pRasterState);
    
    // Animated deep-space background
    float bg[4] = {
        0.02f + 0.02f * sinf(g_Angle),
        0.02f + 0.02f * sinf(g_Angle + 2.094f),
        0.06f + 0.06f * sinf(g_Angle + 4.188f),
        1.f
    };
    devcon->ClearRenderTargetView(backbuffer, bg);
    if (pDSV) devcon->ClearDepthStencilView(pDSV, D3D11_CLEAR_DEPTH, 1.f, 0);

    if (pVB && pIB && pVS && pPS && pCB && pLayout) {
        // Fixed tilt + continuous Y rotation
        Mat4 model = MatMul(RotX(0.65f), RotY(g_Angle));
        Mat4 view = Trans(0.f, 0.f, 2.8f);  // Move camera back (view matrix)
        Mat4 proj = Persp(0.7854f, 640.f / 480.f, 0.1f, 100.f);
        
        // CORRECT ORDER: proj × view × model
        Mat4 mvp = MatMul(MatMul(model, view), proj);

        D3D11_MAPPED_SUBRESOURCE ms;
        if (SUCCEEDED(devcon->Map(pCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) {
            memcpy(ms.pData, &mvp, sizeof(mvp));
            devcon->Unmap(pCB, 0);
        }

        UINT stride = sizeof(Vertex), offset = 0;
        devcon->IASetVertexBuffers(0, 1, &pVB, &stride, &offset);
        devcon->IASetIndexBuffer(pIB, DXGI_FORMAT_R16_UINT, 0);
        devcon->IASetInputLayout(pLayout);
        devcon->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        devcon->VSSetShader(pVS, nullptr, 0);
        devcon->PSSetShader(pPS, nullptr, 0);
        devcon->VSSetConstantBuffers(0, 1, &pCB);
        devcon->DrawIndexed(36, 0, 0);
    }

    swapchain->Present(0, 0);

    // GDI text overlay
    if (hdc) {
        RECT rc = { 0, 0, 640, 480 };
        SetBkMode(hdc, TRANSPARENT);

        RECT tr = { 12, 6, 620, 30 };
        SetTextColor(hdc, RGB(220, 220, 255));
        DrawTextW(hdc, L"ImWindows - ARM32 D3D11 Sandbox", -1, &tr,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        wchar_t buf[16];
        swprintf(buf, 16, L"Fr %d", g_Frame);
        RECT fr = { 12, 32, 628, 54 };
        SetTextColor(hdc, RGB(80, 200, 120));
        DrawTextW(hdc, buf, -1, &fr, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

        RECT br = { 12, 452, 620, 476 };
        SetTextColor(hdc, RGB(120, 120, 170));
#if _WIN32_WINNT >= 0x0A00
        DrawTextW(hdc, L"HW Accelerated - W10M ARM32", -1, &br,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE);
#else
        DrawTextW(hdc, L"HW Accelerated - Win8.1 x86", -1, &br,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE);
#endif
    }

    g_Angle += 0.022f;
    if (g_Angle > 6.283185f) g_Angle -= 6.283185f;
    g_Frame++;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc); wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc; wc.hInstance = hInstance;
    wc.lpszClassName = L"CubeWnd";
    RegisterClassExW(&wc);

    g_hWnd = CreateWindowExW(0, L"CubeWnd", L"ImWindows 3D Cube",
        WS_OVERLAPPEDWINDOW, 0, 0, 640, 480, nullptr, nullptr, hInstance, nullptr);
    ShowWindow(g_hWnd, nCmdShow);

    // Swap chain
    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount = 1;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferDesc.Width  = 640; scd.BufferDesc.Height = 480;
    scd.BufferUsage       = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow      = g_hWnd;
    scd.SampleDesc.Count  = 1; scd.Windowed = TRUE;

    D3D_FEATURE_LEVEL levels[] = {
        D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3,  D3D_FEATURE_LEVEL_9_1
    };
    D3D_FEATURE_LEVEL fl;
    if (FAILED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE,
        nullptr, 0, levels, 4, D3D11_SDK_VERSION, &scd, &swapchain, &dev, &fl, &devcon)))
        return -1;

    // Render target view
    ID3D11Texture2D* pBB = nullptr;
    swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBB);
    dev->CreateRenderTargetView(pBB, nullptr, &backbuffer);
    if (pBB) pBB->Release();

    // Viewport (stored globally — re-bound in RenderFrame each frame)
    ZeroMemory(&g_VP, sizeof(g_VP));
    g_VP.Width = 640.f; g_VP.Height = 480.f; g_VP.MaxDepth = 1.f;

    bool ok = InitD3D();

    // Get overlay DC (in D3D mode the host routes it through the software pixel overlay path)
    HDC hdc = GetDC(g_hWnd);

    MSG msg = {};
    while (TRUE) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg); DispatchMessage(&msg);
            if (msg.message == WM_QUIT) break;
        } else {
            if (ok) RenderFrame(hdc);
            Sleep(16); // ~60 fps
        }
    }

    ReleaseDC(g_hWnd, hdc);

    if (pDSState)  pDSState->Release();
    if (pDSV)      pDSV->Release();
    if (pDSTex)    pDSTex->Release();
    if (pCB)       pCB->Release();
    if (pIB)       pIB->Release();
    if (pVB)       pVB->Release();
    if (pLayout)   pLayout->Release();
    if (pPS)       pPS->Release();
    if (pVS)       pVS->Release();
    if (backbuffer) backbuffer->Release();
    if (swapchain)  swapchain->Release();
    if (devcon)     devcon->Release();
    if (dev)        dev->Release();

    return (int)msg.wParam;
}
