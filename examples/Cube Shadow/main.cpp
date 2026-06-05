
#pragma comment(lib, "user32")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")
#pragma comment(lib, "windowscodecs")
#pragma comment(lib, "ole32")

///////////////////////////////////////////////////////////////////////////////////////////////////

#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <vector>
#include <stdio.h>
#include "resource.h"

// Load raw data from embedded resource using Windows resource APIs
static bool LoadResourceData(int resourceId, LPCSTR resourceType, std::vector<BYTE>& outData)
{
    HRSRC hRes = FindResourceA(nullptr, MAKEINTRESOURCEA(resourceId), resourceType);
    if (!hRes) return false;
    
    HGLOBAL hData = LoadResource(nullptr, hRes);
    if (!hData) return false;
    
    DWORD size = SizeofResource(nullptr, hRes);
    void* ptr = LockResource(hData);
    if (!ptr || size == 0) return false;
    
    outData.resize(size);
    memcpy(outData.data(), ptr, size);
    return true;
}

// Load shader source from embedded resource
static bool LoadShaderSource(int resourceId, std::vector<char>& outSource)
{
    std::vector<BYTE> data;
    if (!LoadResourceData(resourceId, RT_RCDATA, data)) {
        OutputDebugStringA("[CubeShadow] Failed to load shader resource\n");
        return false;
    }
    // Ensure null-terminated string
    outSource.assign(data.begin(), data.end());
    outSource.push_back('\0');
    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

#define TITLE "Minimal D3D11 elaborations II by d7samurai"

///////////////////////////////////////////////////////////////////////////////////////////////////

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    LARGE_INTEGER perfCounterFreq, perfCounterLast, perfCounterNow;
    QueryPerformanceFrequency(&perfCounterFreq);
    QueryPerformanceCounter(&perfCounterLast);

    WNDCLASSA wndClass = { 0, DefWindowProcA, 0, 0, 0, 0, 0, 0, 0, TITLE };

    RegisterClassA(&wndClass);

    HWND window = CreateWindowExA(0, TITLE, TITLE, WS_POPUP | WS_MAXIMIZE | WS_VISIBLE, 0, 0, 0, 0, nullptr, nullptr, nullptr, nullptr);

    ///////////////////////////////////////////////////////////////////////////////////////////////

    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3,  D3D_FEATURE_LEVEL_9_1
    };

    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    swapChainDesc.BufferDesc.Width  = 0; // use window width
    swapChainDesc.BufferDesc.Height = 0; // use window height
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; // can't specify SRGB framebuffer directly when using FLIP model swap effect. see lines 49, 66
    swapChainDesc.SampleDesc.Count  = 1;
    swapChainDesc.BufferUsage       = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount       = 2;
    swapChainDesc.OutputWindow      = window;
    swapChainDesc.Windowed          = TRUE;
    swapChainDesc.SwapEffect        = DXGI_SWAP_EFFECT_DISCARD;

    IDXGISwapChain* swapChain = nullptr;
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* deviceContext = nullptr;

    UINT createFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
    createFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        createFlags, featureLevels, ARRAYSIZE(featureLevels), D3D11_SDK_VERSION,
        &swapChainDesc, &swapChain, &device, nullptr, &deviceContext);

    // Fallback: if hardware driver fails, try WARP software renderer
    if (FAILED(hr)) {
        OutputDebugStringA("[CubeShadow] Hardware device failed, trying WARP...\n");
        hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
            createFlags, featureLevels, ARRAYSIZE(featureLevels), D3D11_SDK_VERSION,
            &swapChainDesc, &swapChain, &device, nullptr, &deviceContext);
    }

    if (FAILED(hr) || !device || !deviceContext || !swapChain) {
        OutputDebugStringA("[CubeShadow] FATAL: D3D11 device creation failed!\n");
        return 1;
    }

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    swapChain->GetDesc(&swapChainDesc); // update swapChainDesc with actual window size

    ///////////////////////////////////////////////////////////////////////////////////////////////

    ID3D11Texture2D* framebufferTexture;

    swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&framebufferTexture);

    D3D11_RENDER_TARGET_VIEW_DESC framebufferDesc = {};
    framebufferDesc.Format        = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB; // ... so do this to get _SRGB swapchain (rendertarget view)
    framebufferDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

    ID3D11RenderTargetView* framebufferRTV;

    device->CreateRenderTargetView(framebufferTexture, &framebufferDesc, &framebufferRTV);

    ///////////////////////////////////////////////////////////////////////////////////////////////

    D3D11_TEXTURE2D_DESC framebufferDepthDesc;

    framebufferTexture->GetDesc(&framebufferDepthDesc); // copy from framebuffer properties

    framebufferDepthDesc.Format    = DXGI_FORMAT_D24_UNORM_S8_UINT;
    framebufferDepthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    ID3D11Texture2D* framebufferDepthTexture;

    device->CreateTexture2D(&framebufferDepthDesc, nullptr, &framebufferDepthTexture);

    ID3D11DepthStencilView* framebufferDSV;

    device->CreateDepthStencilView(framebufferDepthTexture, nullptr, &framebufferDSV);

    ///////////////////////////////////////////////////////////////////////////////////////////////

    D3D11_TEXTURE2D_DESC shadowmapDepthDesc = {};
    shadowmapDepthDesc.Width            = 2048;
    shadowmapDepthDesc.Height           = 2048;
    shadowmapDepthDesc.MipLevels        = 1;
    shadowmapDepthDesc.ArraySize        = 1;
    shadowmapDepthDesc.Format           = DXGI_FORMAT_R32_TYPELESS;
    shadowmapDepthDesc.SampleDesc.Count = 1;
    shadowmapDepthDesc.Usage            = D3D11_USAGE_DEFAULT;
    shadowmapDepthDesc.BindFlags        = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

    ID3D11Texture2D* shadowmapDepthTexture;

    device->CreateTexture2D(&shadowmapDepthDesc, nullptr, &shadowmapDepthTexture);

    D3D11_DEPTH_STENCIL_VIEW_DESC shadowmapDSVdesc = {};
    shadowmapDSVdesc.Format        = DXGI_FORMAT_D32_FLOAT;
    shadowmapDSVdesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;

    ID3D11DepthStencilView* shadowmapDSV;

    device->CreateDepthStencilView(shadowmapDepthTexture, &shadowmapDSVdesc, &shadowmapDSV);

    D3D11_SHADER_RESOURCE_VIEW_DESC shadowmapSRVdesc = {};
    shadowmapSRVdesc.Format              = DXGI_FORMAT_R32_FLOAT;
    shadowmapSRVdesc.ViewDimension       = D3D11_SRV_DIMENSION_TEXTURE2D;
    shadowmapSRVdesc.Texture2D.MipLevels = 1;

    ID3D11ShaderResourceView* shadowmapSRV;

    device->CreateShaderResourceView(shadowmapDepthTexture, &shadowmapSRVdesc, &shadowmapSRV);

    ///////////////////////////////////////////////////////////////////////////////////////////////

    struct float4 { float x, y, z, w; };

    struct Constants
    {
        float4 CameraProjection[4];
        float4 LightProjection[4];
        float4 LightRotation;
        float4 ModelRotation;
        float4 ModelTranslation;
        float4 ShadowmapSize;
    };

    D3D11_BUFFER_DESC constantBufferDesc = {};
    constantBufferDesc.ByteWidth      = sizeof(Constants) + 0xf & 0xfffffff0; // ensure constant buffer size is multiple of 16 bytes
    constantBufferDesc.Usage          = D3D11_USAGE_DYNAMIC;
    constantBufferDesc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    constantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    ID3D11Buffer* constantBuffer;

    device->CreateBuffer(&constantBufferDesc, nullptr, &constantBuffer);

    ///////////////////////////////////////////////////////////////////////////////////////////////

    float vertexData[] = { -1, 1, -1, 0, 0, 1, 1, -1, 9.5f, 0, -0.58f, 0.58f, -1, 2, 2, 0.58f, 0.58f, -1, 7.5f, 2, -0.58f, 0.58f, -1, 0, 0, 0.58f, 0.58f, -1, 0, 0, -0.58f, 0.58f, -0.58f, 0, 0, 0.58f, 0.58f, -0.58f, 0, 0 }; // pos.x, pos.y, pos.z, tex.u, tex.v, ...

    D3D11_BUFFER_DESC vertexBufferDesc = {};
    vertexBufferDesc.ByteWidth           = sizeof(vertexData);
    vertexBufferDesc.Usage               = D3D11_USAGE_IMMUTABLE;
    vertexBufferDesc.BindFlags           = D3D11_BIND_SHADER_RESOURCE; // using regular shader resource as vertex buffer for manual vertex fetch
    vertexBufferDesc.MiscFlags           = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    vertexBufferDesc.StructureByteStride = 5 * sizeof(float); // 5 floats per vertex (float3 position, float2 texcoord)

    D3D11_SUBRESOURCE_DATA vertexBufferData = { vertexData };

    ID3D11Buffer* vertexBuffer;

    device->CreateBuffer(&vertexBufferDesc, &vertexBufferData, &vertexBuffer);

    D3D11_SHADER_RESOURCE_VIEW_DESC vertexBufferSRVdesc = {};
    vertexBufferSRVdesc.Format             = DXGI_FORMAT_UNKNOWN;
    vertexBufferSRVdesc.ViewDimension      = D3D11_SRV_DIMENSION_BUFFER;
    vertexBufferSRVdesc.Buffer.NumElements = vertexBufferDesc.ByteWidth / vertexBufferDesc.StructureByteStride;

    ID3D11ShaderResourceView* vertexBufferSRV;

    device->CreateShaderResourceView(vertexBuffer, &vertexBufferSRVdesc, &vertexBufferSRV);

    ///////////////////////////////////////////////////////////////////////////////////////////////

    D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
    depthStencilDesc.DepthEnable    = TRUE;
    depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    depthStencilDesc.DepthFunc      = D3D11_COMPARISON_LESS;

    ID3D11DepthStencilState* depthStencilState;

    device->CreateDepthStencilState(&depthStencilDesc, &depthStencilState);

    ///////////////////////////////////////////////////////////////////////////////////////////////

    D3D11_RASTERIZER_DESC rasterizerDesc = {};
    rasterizerDesc.FillMode = D3D11_FILL_SOLID;
    rasterizerDesc.CullMode = D3D11_CULL_BACK;

    ID3D11RasterizerState* cullBackRS;

    device->CreateRasterizerState(&rasterizerDesc, &cullBackRS);

    rasterizerDesc.CullMode = D3D11_CULL_FRONT;

    ID3D11RasterizerState* cullFrontRS;

    device->CreateRasterizerState(&rasterizerDesc, &cullFrontRS);

    ///////////////////////////////////////////////////////////////////////////////////////////////

    std::vector<char> shaderSource;
    if (!LoadShaderSource(IDR_SHADER_SHADERS, shaderSource)) {
        OutputDebugStringA("[CubeShadow] Failed to load shader source from resource\n");
        return 1;
    }

    ID3DBlob* framebufferVSBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;
    hr = D3DCompile(shaderSource.data(), shaderSource.size() - 1, nullptr, nullptr, nullptr, "framebuffer_vs", "vs_5_0", 0, 0, &framebufferVSBlob, &errorBlob);
    if (FAILED(hr) || !framebufferVSBlob) {
        if (errorBlob) {
            OutputDebugStringA("[CubeShadow] VS compile error: ");
            OutputDebugStringA((const char*)errorBlob->GetBufferPointer());
            errorBlob->Release();
        }
        OutputDebugStringA("[CubeShadow] Failed to compile framebuffer vertex shader\n");
        return 1;
    }
    if (errorBlob) errorBlob->Release();
    errorBlob = nullptr;

    ID3D11VertexShader* framebufferVS;
    device->CreateVertexShader(framebufferVSBlob->GetBufferPointer(), framebufferVSBlob->GetBufferSize(), nullptr, &framebufferVS);

    ///////////////////////////////////////////////////////////////////////////////////////////////

    ID3DBlob* framebufferPSBlob = nullptr;
    hr = D3DCompile(shaderSource.data(), shaderSource.size() - 1, nullptr, nullptr, nullptr, "framebuffer_ps", "ps_5_0", 0, 0, &framebufferPSBlob, &errorBlob);
    if (FAILED(hr) || !framebufferPSBlob) {
        if (errorBlob) {
            OutputDebugStringA("[CubeShadow] PS compile error: ");
            OutputDebugStringA((const char*)errorBlob->GetBufferPointer());
            errorBlob->Release();
        }
        OutputDebugStringA("[CubeShadow] Failed to compile framebuffer pixel shader\n");
        return 1;
    }
    if (errorBlob) errorBlob->Release();
    errorBlob = nullptr;

    ID3D11PixelShader* framebufferPS;
    device->CreatePixelShader(framebufferPSBlob->GetBufferPointer(), framebufferPSBlob->GetBufferSize(), nullptr, &framebufferPS);

    ///////////////////////////////////////////////////////////////////////////////////////////////

    ID3DBlob* shadowmapVSBlob = nullptr;
    hr = D3DCompile(shaderSource.data(), shaderSource.size() - 1, nullptr, nullptr, nullptr, "shadowmap_vs", "vs_5_0", 0, 0, &shadowmapVSBlob, &errorBlob);
    if (FAILED(hr) || !shadowmapVSBlob) {
        if (errorBlob) {
            OutputDebugStringA("[CubeShadow] Shadow VS compile error: ");
            OutputDebugStringA((const char*)errorBlob->GetBufferPointer());
            errorBlob->Release();
        }
        OutputDebugStringA("[CubeShadow] Failed to compile shadowmap vertex shader\n");
        return 1;
    }
    if (errorBlob) errorBlob->Release();
    errorBlob = nullptr;

    ID3D11VertexShader* shadowmapVS;
    device->CreateVertexShader(shadowmapVSBlob->GetBufferPointer(), shadowmapVSBlob->GetBufferSize(), nullptr, &shadowmapVS);

    ///////////////////////////////////////////////////////////////////////////////////////////////

    FLOAT framebufferClear[4]    = { 0.025f, 0.025f, 0.025f, 1 };

    D3D11_VIEWPORT framebufferVP = { 0, 0, static_cast<float>(framebufferDepthDesc.Width), static_cast<float>(framebufferDepthDesc.Height), 0, 1 };
    D3D11_VIEWPORT shadowmapVP   = { 0, 0, static_cast<float>(shadowmapDepthDesc.Width), static_cast<float>(shadowmapDepthDesc.Height), 0, 1 };

    ID3D11ShaderResourceView* nullSRV = nullptr; // null srv used for unbinding resources

    ///////////////////////////////////////////////////////////////////////////////////////////////

    Constants constants        = { 2.0f / (framebufferVP.Width / framebufferVP.Height), 0, 0, 0, 0, 2, 0, 0, 0, 0, 1.125f, 1, 0, 0, -1.125f, 0, // camera projection matrix (perspective)
                                   0.5f, 0, 0, 0, 0, 0.5f, 0, 0, 0, 0, 0.125f, 0, 0, 0, -0.125f, 1 };                                           // light projection matrix (orthographic)

    constants.LightRotation    = { 0.8f, 0.6f, 0.0f };
    constants.ModelRotation    = { 0.0f, 0.0f, 0.0f };
    constants.ModelTranslation = { 0.0f, 0.0f, 4.0f };

    constants.ShadowmapSize    = { shadowmapVP.Width, shadowmapVP.Height };

    ///////////////////////////////////////////////////////////////////////////////////////////////

    while (true)
    {
        MSG msg;

        while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_KEYDOWN) return 0;
            DispatchMessageA(&msg);
        }

        ///////////////////////////////////////////////////////////////////////////////////////////

        QueryPerformanceCounter(&perfCounterNow);
        float deltaSeconds = (float)(perfCounterNow.QuadPart - perfCounterLast.QuadPart) / (float)perfCounterFreq.QuadPart;
        perfCounterLast = perfCounterNow;

        // Clamp delta to avoid large jumps (e.g., after a breakpoint)
        if (deltaSeconds > 0.1f) deltaSeconds = 0.1f;

        const float rotSpeedX = 0.5f;   // example: 0.5 rad/sec
        const float rotSpeedY = 0.9f;
        const float rotSpeedZ = 0.1f;

        constants.ModelRotation.x += rotSpeedX * deltaSeconds;
        constants.ModelRotation.y += rotSpeedY * deltaSeconds;
        constants.ModelRotation.z += rotSpeedZ * deltaSeconds;

        ///////////////////////////////////////////////////////////////////////////////////////////

        D3D11_MAPPED_SUBRESOURCE mappedSubresource;

        deviceContext->Map(constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubresource);

        *reinterpret_cast<Constants*>(mappedSubresource.pData) = constants;

        deviceContext->Unmap(constantBuffer, 0);

        ///////////////////////////////////////////////////////////////////////////////////////////

        deviceContext->ClearDepthStencilView(shadowmapDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);

        deviceContext->OMSetRenderTargets(0, nullptr, shadowmapDSV); // null rendertarget for depth only
        deviceContext->OMSetDepthStencilState(depthStencilState, 0);

        deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP); // using triangle strip this time

        deviceContext->VSSetConstantBuffers(0, 1, &constantBuffer);
        deviceContext->VSSetShaderResources(0, 1, &vertexBufferSRV);
        deviceContext->VSSetShader(shadowmapVS, nullptr, 0);

        deviceContext->RSSetViewports(1, &shadowmapVP);
        deviceContext->RSSetState(cullFrontRS);

        deviceContext->PSSetShader(nullptr, nullptr, 0); // null pixelshader for depth only

        ///////////////////////////////////////////////////////////////////////////////////////////

        deviceContext->DrawInstanced(8, 24, 0, 0); // render shadowmap (light pov)

        ///////////////////////////////////////////////////////////////////////////////////////////

        deviceContext->ClearRenderTargetView(framebufferRTV, framebufferClear);
        deviceContext->ClearDepthStencilView(framebufferDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);

        deviceContext->OMSetRenderTargets(1, &framebufferRTV, framebufferDSV);

        deviceContext->VSSetShader(framebufferVS, nullptr, 0);

        deviceContext->RSSetViewports(1, &framebufferVP);
        deviceContext->RSSetState(cullBackRS);

        deviceContext->PSSetShaderResources(1, 1, &shadowmapSRV);
        deviceContext->PSSetShader(framebufferPS, nullptr, 0);

        ///////////////////////////////////////////////////////////////////////////////////////////

        deviceContext->DrawInstanced(8, 24, 0, 0); // render framebuffer (camera pov)

        ///////////////////////////////////////////////////////////////////////////////////////////

        deviceContext->PSSetShaderResources(1, 1, &nullSRV); // release shadowmap as srv to avoid srv/dsv conflict

        ///////////////////////////////////////////////////////////////////////////////////////////

        swapChain->Present(1, 0);
    }
}