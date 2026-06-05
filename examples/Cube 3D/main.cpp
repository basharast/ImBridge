
#pragma comment(lib, "user32")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")
#pragma comment(lib, "windowscodecs")
#pragma comment(lib, "ole32")

///////////////////////////////////////////////////////////////////////////////////////////////////

#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wincodec.h>

#include <math.h> // sin, cos
#include <vector>
#include <string>
#include "xube.h" // 3d model
#include "resource.h"

///////////////////////////////////////////////////////////////////////////////////////////////////

#define TITLE "Minimal D3D11 by d7samurai"

///////////////////////////////////////////////////////////////////////////////////////////////////

struct float2 { float x, y; };
struct float3 { float x, y, z; };
struct float4 { float x, y, z, w; };
struct matrix { float m[4][4]; };

matrix operator*(const matrix& m1, const matrix& m2);

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

// Helper to print HRESULT-based debug messages
static void DebugOutputHRESULT(const char* prefix, HRESULT hr)
{
    char debugMsg[256];
    sprintf_s(debugMsg, "%s hr=0x%08X\n", prefix, static_cast<unsigned>(hr));
    OutputDebugStringA(debugMsg);
}

// Load texture from embedded PNG resource
static bool LoadTextureFromResource(ID3D11Device* device, int resourceId, ID3D11ShaderResourceView** outSRV)
{
    if (!device || !outSRV) return false;
    *outSRV = nullptr;
    
    std::vector<BYTE> pngData;
    if (!LoadResourceData(resourceId, RT_RCDATA, pngData)) {
        OutputDebugStringA("[Cube3D] Failed to load texture resource\n");
        return false;
    }
    
    // Create a memory stream from the PNG data
    IWICImagingFactory* wicFactory = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wicFactory));
    if (FAILED(hr)) {
        DebugOutputHRESULT("[Cube3D] CoCreateInstance(CLSID_WICImagingFactory) failed", hr);
        return false;
    }
    
    IWICStream* stream = nullptr;
    hr = wicFactory->CreateStream(&stream);
    if (FAILED(hr)) {
        DebugOutputHRESULT("[Cube3D] CreateStream failed", hr);
        wicFactory->Release();
        return false;
    }
    
    hr = stream->InitializeFromMemory(pngData.data(), static_cast<DWORD>(pngData.size()));
    if (FAILED(hr)) {
        DebugOutputHRESULT("[Cube3D] InitializeFromMemory failed", hr);
        stream->Release();
        wicFactory->Release();
        return false;
    }
    
    IWICBitmapDecoder* decoder = nullptr;
    hr = wicFactory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr)) {
        DebugOutputHRESULT("[Cube3D] CreateDecoderFromStream failed", hr);
        stream->Release();
        wicFactory->Release();
        return false;
    }
    
    IWICBitmapFrameDecode* frame = nullptr;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) {
        DebugOutputHRESULT("[Cube3D] GetFrame failed", hr);
        decoder->Release();
        stream->Release();
        wicFactory->Release();
        return false;
    }
    
    IWICFormatConverter* converter = nullptr;
    hr = wicFactory->CreateFormatConverter(&converter);
    if (FAILED(hr)) {
        DebugOutputHRESULT("[Cube3D] CreateFormatConverter failed", hr);
        frame->Release();
        decoder->Release();
        stream->Release();
        wicFactory->Release();
        return false;
    }
    
    hr = converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) {
        DebugOutputHRESULT("[Cube3D] Initialize format converter failed", hr);
        converter->Release();
        frame->Release();
        decoder->Release();
        stream->Release();
        wicFactory->Release();
        return false;
    }
    
    UINT width = 0, height = 0;
    hr = converter->GetSize(&width, &height);
    if (FAILED(hr) || width == 0 || height == 0) {
        if (FAILED(hr)) DebugOutputHRESULT("[Cube3D] GetSize failed", hr);
        else OutputDebugStringA("[Cube3D] Converter returned invalid image dimensions\n");
        converter->Release();
        frame->Release();
        decoder->Release();
        stream->Release();
        wicFactory->Release();
        return false;
    }
    
    std::vector<BYTE> pixels(width * height * 4);
    hr = converter->CopyPixels(nullptr, width * 4, static_cast<UINT>(pixels.size()), pixels.data());
    if (FAILED(hr)) {
        DebugOutputHRESULT("[Cube3D] CopyPixels failed", hr);
        converter->Release();
        frame->Release();
        decoder->Release();
        stream->Release();
        wicFactory->Release();
        return false;
    }
    
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    
    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = pixels.data();
    initData.SysMemPitch = width * 4;
    
    ID3D11Texture2D* texture = nullptr;
    hr = device->CreateTexture2D(&desc, &initData, &texture);
    if (FAILED(hr)) {
        DebugOutputHRESULT("[Cube3D] CreateTexture2D failed", hr);
        converter->Release();
        frame->Release();
        decoder->Release();
        stream->Release();
        wicFactory->Release();
        return false;
    }
    
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;
    
    hr = device->CreateShaderResourceView(texture, &srvDesc, outSRV);
    texture->Release();
    converter->Release();
    frame->Release();
    decoder->Release();
    stream->Release();
    wicFactory->Release();
    if (FAILED(hr)) DebugOutputHRESULT("[Cube3D] CreateShaderResourceView failed", hr);
    return SUCCEEDED(hr);
}

static bool LoadTextureFromFile(ID3D11Device* device, const wchar_t* filename, ID3D11ShaderResourceView** outSRV)
{
    if (!device || !filename || !outSRV) return false;
    *outSRV = nullptr;

    IWICImagingFactory* wicFactory = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wicFactory));
    if (FAILED(hr)) return false;

    IWICBitmapDecoder* decoder = nullptr;
    hr = wicFactory->CreateDecoderFromFilename(filename, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr)) { wicFactory->Release(); return false; }

    IWICBitmapFrameDecode* frame = nullptr;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) { decoder->Release(); wicFactory->Release(); return false; }

    IWICFormatConverter* converter = nullptr;
    hr = wicFactory->CreateFormatConverter(&converter);
    if (FAILED(hr)) { frame->Release(); decoder->Release(); wicFactory->Release(); return false; }

    hr = converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) { converter->Release(); frame->Release(); decoder->Release(); wicFactory->Release(); return false; }

    UINT width = 0, height = 0;
    hr = converter->GetSize(&width, &height);
    if (FAILED(hr) || width == 0 || height == 0) { converter->Release(); frame->Release(); decoder->Release(); wicFactory->Release(); return false; }

    std::vector<BYTE> pixels(width * height * 4);
    hr = converter->CopyPixels(nullptr, width * 4, static_cast<UINT>(pixels.size()), pixels.data());
    if (FAILED(hr)) { converter->Release(); frame->Release(); decoder->Release(); wicFactory->Release(); return false; }

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = pixels.data();
    initData.SysMemPitch = width * 4;

    ID3D11Texture2D* texture = nullptr;
    hr = device->CreateTexture2D(&desc, &initData, &texture);
    if (FAILED(hr)) { converter->Release(); frame->Release(); decoder->Release(); wicFactory->Release(); return false; }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;

    hr = device->CreateShaderResourceView(texture, &srvDesc, outSRV);
    texture->Release();
    converter->Release();
    frame->Release();
    decoder->Release();
    wicFactory->Release();
    return SUCCEEDED(hr);
}

// Load shader source from embedded resource
static bool LoadShaderSource(int resourceId, std::vector<char>& outSource)
{
    std::vector<BYTE> data;
    if (!LoadResourceData(resourceId, RT_RCDATA, data)) {
        OutputDebugStringA("[Cube3D] Failed to load shader resource\n");
        return false;
    }
    // Ensure null-terminated string
    outSource.assign(data.begin(), data.end());
    outSource.push_back('\0');
    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    // Initialize COM early - required for WIC texture loading and D3D COM interop
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    LARGE_INTEGER perfCounterFreq, perfCounterLast, perfCounterNow;
    QueryPerformanceFrequency(&perfCounterFreq);
    QueryPerformanceCounter(&perfCounterLast);

    WNDCLASSA wndclass = { 0, DefWindowProcA, 0, 0, 0, 0, 0, 0, 0, TITLE };

    RegisterClassA(&wndclass);

    HWND window = CreateWindowExA(0, TITLE, TITLE, WS_POPUP | WS_MAXIMIZE | WS_VISIBLE, 0, 0, 0, 0, nullptr, nullptr, nullptr, nullptr);

    ///////////////////////////////////////////////////////////////////////////////////////////////

    D3D_FEATURE_LEVEL featurelevels[] = {
        D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3,  D3D_FEATURE_LEVEL_9_1
    };

    DXGI_SWAP_CHAIN_DESC swapchaindesc = {};
    swapchaindesc.BufferDesc.Width  = 0; // use window width
    swapchaindesc.BufferDesc.Height = 0; // use window height
    swapchaindesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swapchaindesc.SampleDesc.Count  = 1;
    swapchaindesc.BufferUsage       = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapchaindesc.BufferCount       = 2;
    swapchaindesc.OutputWindow      = window;
    swapchaindesc.Windowed          = TRUE;
    swapchaindesc.SwapEffect        = DXGI_SWAP_EFFECT_DISCARD;

    IDXGISwapChain* swapchain = nullptr;
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* devicecontext = nullptr;
    D3D_FEATURE_LEVEL actualFeatureLevel = D3D_FEATURE_LEVEL_9_1;

    // Only enable debug layer if explicitly requested - it requires the D3D11 SDK debug layer
    // DLL (d3d11sdklayers.dll) which is NOT present on most Windows 8.1 devices and causes
    // D3D11CreateDeviceAndSwapChain to fail entirely, leading to access violations.
    UINT createFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
    createFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        createFlags, featurelevels, ARRAYSIZE(featurelevels), D3D11_SDK_VERSION,
        &swapchaindesc, &swapchain, &device, &actualFeatureLevel, &devicecontext);

    // Fallback: if hardware driver fails, try WARP software renderer
    if (FAILED(hr)) {
        OutputDebugStringA("[Cube3D] Hardware device failed, trying WARP...\n");
        hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
            createFlags, featurelevels, ARRAYSIZE(featurelevels), D3D11_SDK_VERSION,
            &swapchaindesc, &swapchain, &device, &actualFeatureLevel, &devicecontext);
    }

    if (FAILED(hr) || !device || !devicecontext || !swapchain) {
        OutputDebugStringA("[Cube3D] FATAL: D3D11 device creation failed!\n");
        return 1;
    }

    swapchain->GetDesc(&swapchaindesc); // update swapchaindesc with actual window size

    ///////////////////////////////////////////////////////////////////////////////////////////////

    ID3D11Texture2D* cameracolorbuffer = nullptr;

    hr = swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&cameracolorbuffer);
    if (FAILED(hr) || !cameracolorbuffer) {
        OutputDebugStringA("[Cube3D] Failed to get swap chain buffer\n");
        return 1;
    }

    // Try SRGB format first for correct gamma, fall back to UNORM for 8.1 compatibility
    D3D11_RENDER_TARGET_VIEW_DESC cameraRTVdesc = {};
    cameraRTVdesc.Format        = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    cameraRTVdesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

    ID3D11RenderTargetView* cameraRTV = nullptr;

    hr = device->CreateRenderTargetView(cameracolorbuffer, &cameraRTVdesc, &cameraRTV);
    if (FAILED(hr)) {
        // Fallback: use non-SRGB format (works on all feature levels including 8.1)
        OutputDebugStringA("[Cube3D] SRGB RTV failed, falling back to UNORM\n");
        cameraRTVdesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        hr = device->CreateRenderTargetView(cameracolorbuffer, &cameraRTVdesc, &cameraRTV);
        if (FAILED(hr)) {
            // Last resort: let D3D pick the format
            hr = device->CreateRenderTargetView(cameracolorbuffer, nullptr, &cameraRTV);
        }
    }
    if (FAILED(hr) || !cameraRTV) {
        OutputDebugStringA("[Cube3D] Failed to create render target view\n");
        return 1;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////

    D3D11_TEXTURE2D_DESC cameradepthbufferdesc;

    cameracolorbuffer->GetDesc(&cameradepthbufferdesc); // copy framebuffer properties; they're mostly the same

    cameradepthbufferdesc.Format    = DXGI_FORMAT_D24_UNORM_S8_UINT;
    cameradepthbufferdesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    ID3D11Texture2D* cameradepthbuffer = nullptr;

    hr = device->CreateTexture2D(&cameradepthbufferdesc, nullptr, &cameradepthbuffer);
    if (FAILED(hr) || !cameradepthbuffer) {
        // Fallback: try D16 format which is more widely supported on 8.1/9.x feature levels
        OutputDebugStringA("[Cube3D] D24S8 depth failed, trying D16\n");
        cameradepthbufferdesc.Format = DXGI_FORMAT_D16_UNORM;
        hr = device->CreateTexture2D(&cameradepthbufferdesc, nullptr, &cameradepthbuffer);
        if (FAILED(hr) || !cameradepthbuffer) {
            OutputDebugStringA("[Cube3D] Failed to create depth buffer\n");
            return 1;
        }
    }

    ID3D11DepthStencilView* cameraDSV = nullptr;

    hr = device->CreateDepthStencilView(cameradepthbuffer, nullptr, &cameraDSV);
    if (FAILED(hr) || !cameraDSV) {
        OutputDebugStringA("[Cube3D] Failed to create depth stencil view\n");
        return 1;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////

    ID3DBlob* cameraVSblob = nullptr;
    ID3D11VertexShader* cameraVS = nullptr;

    std::vector<char> shaderSource;
    if (!LoadShaderSource(IDR_SHADER_GPU, shaderSource)) {
        OutputDebugStringA("[Cube3D] Failed to load shader from resource\n");
        return 1;
    }

    // Select shader profile based on actual device feature level for 8.1 compatibility
    // Feature level 9.x only supports vs_4_0_level_9_x profiles
    const char* vsProfile = "vs_4_0_level_9_3";
    const char* psProfile = "ps_4_0_level_9_3";
    if (actualFeatureLevel >= D3D_FEATURE_LEVEL_11_0) {
        vsProfile = "vs_5_0";
        psProfile = "ps_5_0";
    } else if (actualFeatureLevel >= D3D_FEATURE_LEVEL_10_0) {
        vsProfile = "vs_4_0";
        psProfile = "ps_4_0";
    }

    ID3DBlob* errorBlob = nullptr;
    hr = D3DCompile(shaderSource.data(), shaderSource.size() - 1, nullptr, nullptr, nullptr, "CameraVS", vsProfile, 0, 0, &cameraVSblob, &errorBlob);
    if (FAILED(hr) || !cameraVSblob) {
        if (errorBlob) {
            OutputDebugStringA("[Cube3D] VS compile error: ");
            OutputDebugStringA((const char*)errorBlob->GetBufferPointer());
            errorBlob->Release();
        }
        OutputDebugStringA("[Cube3D] Failed to compile vertex shader\n");
        return 1;
    }
    if (errorBlob) errorBlob->Release();
    errorBlob = nullptr;

    hr = device->CreateVertexShader(cameraVSblob->GetBufferPointer(), cameraVSblob->GetBufferSize(), nullptr, &cameraVS);
    if (FAILED(hr) || !cameraVS) {
        OutputDebugStringA("[Cube3D] Failed to create vertex shader\n");
        return 1;
    }

    D3D11_INPUT_ELEMENT_DESC inputelementdesc[] = // maps to vertexdesc struct in gpu.hlsl via semantic names ("POS", "NOR", "TEX", "COL")
    {
        { "POS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,                            0, D3D11_INPUT_PER_VERTEX_DATA, 0 }, // float3 position
        { "NOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }, // float3 normal
        { "TEX", 0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }, // float2 texcoord
        { "COL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }, // float3 color
    };

    ID3D11InputLayout* inputlayout = nullptr;

    hr = device->CreateInputLayout(inputelementdesc, ARRAYSIZE(inputelementdesc), cameraVSblob->GetBufferPointer(), cameraVSblob->GetBufferSize(), &inputlayout);
    if (FAILED(hr) || !inputlayout) {
        OutputDebugStringA("[Cube3D] Failed to create input layout\n");
        return 1;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////

    ID3DBlob* cameraPSblob = nullptr;

    hr = D3DCompile(shaderSource.data(), shaderSource.size() - 1, nullptr, nullptr, nullptr, "CameraPS", psProfile, 0, 0, &cameraPSblob, &errorBlob);
    if (FAILED(hr) || !cameraPSblob) {
        if (errorBlob) {
            OutputDebugStringA("[Cube3D] PS compile error: ");
            OutputDebugStringA((const char*)errorBlob->GetBufferPointer());
            errorBlob->Release();
        }
        OutputDebugStringA("[Cube3D] Failed to compile pixel shader\n");
        return 1;
    }
    if (errorBlob) errorBlob->Release();

    ID3D11PixelShader* cameraPS = nullptr;

    hr = device->CreatePixelShader(cameraPSblob->GetBufferPointer(), cameraPSblob->GetBufferSize(), nullptr, &cameraPS);
    if (FAILED(hr) || !cameraPS) {
        OutputDebugStringA("[Cube3D] Failed to create pixel shader\n");
        return 1;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////

    D3D11_RASTERIZER_DESC rasterizerdesc = {};
    rasterizerdesc.FillMode = D3D11_FILL_SOLID;
    rasterizerdesc.CullMode = D3D11_CULL_BACK;

    ID3D11RasterizerState* rasterizerstate = nullptr;

    device->CreateRasterizerState(&rasterizerdesc, &rasterizerstate);

    ///////////////////////////////////////////////////////////////////////////////////////////////

    D3D11_SAMPLER_DESC samplerdesc = {};
    samplerdesc.Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerdesc.AddressU       = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerdesc.AddressV       = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerdesc.AddressW       = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerdesc.ComparisonFunc = D3D11_COMPARISON_NEVER;

    ID3D11SamplerState* samplerstate = nullptr;

    device->CreateSamplerState(&samplerdesc, &samplerstate);

    ///////////////////////////////////////////////////////////////////////////////////////////////

    D3D11_DEPTH_STENCIL_DESC depthstencildesc = {};
    depthstencildesc.DepthEnable    = TRUE;
    depthstencildesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    depthstencildesc.DepthFunc      = D3D11_COMPARISON_LESS;

    ID3D11DepthStencilState* depthstencilstate = nullptr;

    device->CreateDepthStencilState(&depthstencildesc, &depthstencilstate);

    ///////////////////////////////////////////////////////////////////////////////////////////////

    struct Constants { matrix transform, projection; float4 lightvector; float2 uvOffset; float2 padding; };

    D3D11_BUFFER_DESC constantbufferdesc = {};
    constantbufferdesc.ByteWidth      = sizeof(Constants) + 0xf & 0xfffffff0; // ensure constant buffer size is multiple of 16 bytes
    constantbufferdesc.Usage          = D3D11_USAGE_DYNAMIC; // because updated from CPU every frame
    constantbufferdesc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    constantbufferdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    ID3D11Buffer* constantbuffer = nullptr;

    hr = device->CreateBuffer(&constantbufferdesc, nullptr, &constantbuffer);
    if (FAILED(hr) || !constantbuffer) {
        OutputDebugStringA("[Cube3D] Failed to create constant buffer\n");
        return 1;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////

    ID3D11ShaderResourceView* textureSRV = nullptr;
    if (!LoadTextureFromResource(device, IDR_TEXTURE_IMM, &textureSRV)) {
        OutputDebugStringA("[Cube3D] Failed to load texture from resource, falling back to embedded texture.\n");

        D3D11_TEXTURE2D_DESC texturedesc = {};
        texturedesc.Width            = TEXTURE_WIDTH;  // in xube.h
        texturedesc.Height           = TEXTURE_HEIGHT; // in xube.h
        texturedesc.MipLevels        = 1;
        texturedesc.ArraySize        = 1;
        texturedesc.Format           = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB; // same as framebuffer(view)
        texturedesc.SampleDesc.Count = 1;
        texturedesc.Usage            = D3D11_USAGE_IMMUTABLE; // because will never be updated
        texturedesc.BindFlags        = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA textureSRD = {};
        textureSRD.pSysMem     = texturedata; // in xube.h
        textureSRD.SysMemPitch = TEXTURE_WIDTH * sizeof(UINT); // 1 UINT = 4 bytes per pixel, 0xAARRGGBB

        ID3D11Texture2D* texture = nullptr;
        hr = device->CreateTexture2D(&texturedesc, &textureSRD, &texture);
        if (FAILED(hr)) {
            // Fallback: try non-SRGB format for 8.1 compatibility
            OutputDebugStringA("[Cube3D] SRGB texture failed, trying UNORM\n");
            texturedesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            hr = device->CreateTexture2D(&texturedesc, &textureSRD, &texture);
        }
        if (SUCCEEDED(hr) && texture) {
            device->CreateShaderResourceView(texture, nullptr, &textureSRV);
            texture->Release();
        }
    } else {
        OutputDebugStringA("[Cube3D] Loaded texture from resource successfully.\n");
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////

    D3D11_BUFFER_DESC vertexbufferdesc = {};
    vertexbufferdesc.ByteWidth = sizeof(vertexdata);
    vertexbufferdesc.Usage     = D3D11_USAGE_IMMUTABLE; // because will never be updated 
    vertexbufferdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vertexbufferSRD = { vertexdata }; // in xube.h

    ID3D11Buffer* vertexbuffer = nullptr;

    hr = device->CreateBuffer(&vertexbufferdesc, &vertexbufferSRD, &vertexbuffer);
    if (FAILED(hr) || !vertexbuffer) {
        OutputDebugStringA("[Cube3D] Failed to create vertex buffer\n");
        return 1;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////

    D3D11_BUFFER_DESC indexbufferdesc = {};
    indexbufferdesc.ByteWidth = sizeof(indexdata);
    indexbufferdesc.Usage     = D3D11_USAGE_IMMUTABLE; // because will never be updated
    indexbufferdesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA indexbufferSRD = { indexdata }; // in xube.h

    ID3D11Buffer* indexbuffer = nullptr;

    hr = device->CreateBuffer(&indexbufferdesc, &indexbufferSRD, &indexbuffer);
    if (FAILED(hr) || !indexbuffer) {
        OutputDebugStringA("[Cube3D] Failed to create index buffer\n");
        return 1;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////

    FLOAT clearcolor[4] = { 0.025f, 0.025f, 0.025f, 1.0f };

    UINT stride = 11 * sizeof(float); // vertex size (11 floats: float3 position, float3 normal, float2 texcoord, float3 color)
    UINT offset = 0;

    D3D11_VIEWPORT viewport = { 0.0f, 0.0f, (float)swapchaindesc.BufferDesc.Width, (float)swapchaindesc.BufferDesc.Height, 0.0f, 1.0f };
    
    ///////////////////////////////////////////////////////////////////////////////////////////////

    float w = viewport.Width / viewport.Height; // width (aspect ratio, since height is 1.0)
    float h = 1.0f;                             // height
    float n = 1.0f;                             // near
    float f = 9.0f;                             // far

    float3 modelrotation    = { 0.0f, 0.0f, 0.0f };
    float3 modelscale       = { 1.0f, 1.0f, 1.0f };
    float3 modeltranslation = { 0.0f, 0.0f, 4.0f };

    ///////////////////////////////////////////////////////////////////////////////////////////////

    while (true)
    {
        MSG msg;

        while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_KEYDOWN) return 0; // PRESS ANY KEY TO EXIT
            DispatchMessageA(&msg);
        }

        ///////////////////////////////////////////////////////////////////////////////////////////

        matrix rotatex   = { 1, 0, 0, 0, 0, (float)cos(modelrotation.x), -(float)sin(modelrotation.x), 0, 0, (float)sin(modelrotation.x), (float)cos(modelrotation.x), 0, 0, 0, 0, 1 };
        matrix rotatey   = { (float)cos(modelrotation.y), 0, (float)sin(modelrotation.y), 0, 0, 1, 0, 0, -(float)sin(modelrotation.y), 0, (float)cos(modelrotation.y), 0, 0, 0, 0, 1 };
        matrix rotatez   = { (float)cos(modelrotation.z), -(float)sin(modelrotation.z), 0, 0, (float)sin(modelrotation.z), (float)cos(modelrotation.z), 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
        matrix scale     = { modelscale.x, 0, 0, 0, 0, modelscale.y, 0, 0, 0, 0, modelscale.z, 0, 0, 0, 0, 1 };
        matrix translate = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, modeltranslation.x, modeltranslation.y, modeltranslation.z, 1 };

        QueryPerformanceCounter(&perfCounterNow);
        float deltaSeconds = (float)(perfCounterNow.QuadPart - perfCounterLast.QuadPart) / (float)perfCounterFreq.QuadPart;
        perfCounterLast = perfCounterNow;

        // Clamp delta to avoid large jumps (e.g., after a breakpoint)
        if (deltaSeconds > 0.1f) deltaSeconds = 0.1f;

        const float rotSpeedX = 0.5f;   // example: 0.5 rad/sec
        const float rotSpeedY = 0.9f;
        const float rotSpeedZ = 0.1f;

        modelrotation.x += rotSpeedX * deltaSeconds;
        modelrotation.y += rotSpeedY * deltaSeconds;
        modelrotation.z += rotSpeedZ * deltaSeconds;

        ///////////////////////////////////////////////////////////////////////////////////////////

        D3D11_MAPPED_SUBRESOURCE constantbufferMSR;

        devicecontext->Map(constantbuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &constantbufferMSR); // update constant buffer every frame
        {
            Constants* constants = (Constants*)constantbufferMSR.pData;

            constants->transform   = rotatex * rotatey * rotatez * scale * translate;
            constants->projection  = { 2 * n / w, 0, 0, 0, 0, 2 * n / h, 0, 0, 0, 0, f / (f - n), 1, 0, 0, n * f / (n - f), 0 };
            constants->lightvector = { 1.0f, -1.0f, 1.0f, 0.0f };
            constants->uvOffset = { sinf(modelrotation.x * 0.4f) * 0.2f, cosf(modelrotation.y * 0.4f) * 0.2f };
            constants->padding = { 0.0f, 0.0f };
        }
        devicecontext->Unmap(constantbuffer, 0);

        ///////////////////////////////////////////////////////////////////////////////////////////

        devicecontext->ClearRenderTargetView(cameraRTV, clearcolor);
        devicecontext->ClearDepthStencilView(cameraDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);

        devicecontext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        devicecontext->IASetInputLayout(inputlayout);
        devicecontext->IASetVertexBuffers(0, 1, &vertexbuffer, &stride, &offset);
        devicecontext->IASetIndexBuffer(indexbuffer, DXGI_FORMAT_R32_UINT, 0);

        devicecontext->VSSetShader(cameraVS, nullptr, 0);
        devicecontext->VSSetConstantBuffers(0, 1, &constantbuffer);

        devicecontext->RSSetViewports(1, &viewport);
        devicecontext->RSSetState(rasterizerstate);

        devicecontext->PSSetShader(cameraPS, nullptr, 0);
        devicecontext->PSSetShaderResources(0, 1, &textureSRV);
        devicecontext->PSSetSamplers(0, 1, &samplerstate);

        devicecontext->OMSetRenderTargets(1, &cameraRTV, cameraDSV);
        devicecontext->OMSetDepthStencilState(depthstencilstate, 0);
        devicecontext->OMSetBlendState(nullptr, nullptr, 0xffffffff); // use default blend mode (i.e. no blending)

        ///////////////////////////////////////////////////////////////////////////////////////////

        devicecontext->DrawIndexed(ARRAYSIZE(indexdata), 0, 0);

        ///////////////////////////////////////////////////////////////////////////////////////////

        swapchain->Present(1, 0);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

matrix operator*(const matrix& m1, const matrix& m2)
{
    return
    {
        m1.m[0][0] * m2.m[0][0] + m1.m[0][1] * m2.m[1][0] + m1.m[0][2] * m2.m[2][0] + m1.m[0][3] * m2.m[3][0],
        m1.m[0][0] * m2.m[0][1] + m1.m[0][1] * m2.m[1][1] + m1.m[0][2] * m2.m[2][1] + m1.m[0][3] * m2.m[3][1],
        m1.m[0][0] * m2.m[0][2] + m1.m[0][1] * m2.m[1][2] + m1.m[0][2] * m2.m[2][2] + m1.m[0][3] * m2.m[3][2],
        m1.m[0][0] * m2.m[0][3] + m1.m[0][1] * m2.m[1][3] + m1.m[0][2] * m2.m[2][3] + m1.m[0][3] * m2.m[3][3],
        m1.m[1][0] * m2.m[0][0] + m1.m[1][1] * m2.m[1][0] + m1.m[1][2] * m2.m[2][0] + m1.m[1][3] * m2.m[3][0],
        m1.m[1][0] * m2.m[0][1] + m1.m[1][1] * m2.m[1][1] + m1.m[1][2] * m2.m[2][1] + m1.m[1][3] * m2.m[3][1],
        m1.m[1][0] * m2.m[0][2] + m1.m[1][1] * m2.m[1][2] + m1.m[1][2] * m2.m[2][2] + m1.m[1][3] * m2.m[3][2],
        m1.m[1][0] * m2.m[0][3] + m1.m[1][1] * m2.m[1][3] + m1.m[1][2] * m2.m[2][3] + m1.m[1][3] * m2.m[3][3],
        m1.m[2][0] * m2.m[0][0] + m1.m[2][1] * m2.m[1][0] + m1.m[2][2] * m2.m[2][0] + m1.m[2][3] * m2.m[3][0],
        m1.m[2][0] * m2.m[0][1] + m1.m[2][1] * m2.m[1][1] + m1.m[2][2] * m2.m[2][1] + m1.m[2][3] * m2.m[3][1],
        m1.m[2][0] * m2.m[0][2] + m1.m[2][1] * m2.m[1][2] + m1.m[2][2] * m2.m[2][2] + m1.m[2][3] * m2.m[3][2],
        m1.m[2][0] * m2.m[0][3] + m1.m[2][1] * m2.m[1][3] + m1.m[2][2] * m2.m[2][3] + m1.m[2][3] * m2.m[3][3],
        m1.m[3][0] * m2.m[0][0] + m1.m[3][1] * m2.m[1][0] + m1.m[3][2] * m2.m[2][0] + m1.m[3][3] * m2.m[3][0],
        m1.m[3][0] * m2.m[0][1] + m1.m[3][1] * m2.m[1][1] + m1.m[3][2] * m2.m[2][1] + m1.m[3][3] * m2.m[3][1],
        m1.m[3][0] * m2.m[0][2] + m1.m[3][1] * m2.m[1][2] + m1.m[3][2] * m2.m[2][2] + m1.m[3][3] * m2.m[3][2],
        m1.m[3][0] * m2.m[0][3] + m1.m[3][1] * m2.m[1][3] + m1.m[3][2] * m2.m[2][3] + m1.m[3][3] * m2.m[3][3],
    };
}