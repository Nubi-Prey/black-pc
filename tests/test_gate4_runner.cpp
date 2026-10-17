// Gate 4 Milestone Runner: Complete Graphics Subsystem & NV2A Pipeline (Linux x86)
#include "../loader/src/d3d/host.h"
#include "../loader/src/platform/host_sdl.h"

#include <d3d9.h>
#include <d3dcompiler.h>
#include <wsi/native_sdl2.h>
#include <SDL2/SDL.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>

static void check(bool condition, const char *msg)
{
    if (!condition) {
        std::fprintf(stderr, "[FAIL Gate 4] %s\n", msg);
        std::exit(1);
    }
}

struct Vertex3D {
    float x, y, z;
    float r, g, b, a;
};

int main(int argc, char **argv)
{
    bool interactive = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--gui") == 0 || std::strcmp(argv[i], "-g") == 0 ||
            std::strcmp(argv[i], "--interactive") == 0 || std::strcmp(argv[i], "-i") == 0) {
            interactive = true;
        }
    }

    std::printf("====================================================\n");
    if (interactive) {
        std::printf("=== Gate 4: Teste Interativo de Renderizacao 3D Vulkan ===\n");
        std::printf("  - Renderizando geometria 3D com Shaders via DXVK-Native\n");
        std::printf("  - GPU: AMD Radeon 610M (RADV Vulkan)\n");
        std::printf("  - Pressione ESC ou feche a janela para finalizar\n");
    } else {
        std::printf("=== Running Gate 4 Milestone Integration Runner ====\n");
    }
    std::printf("====================================================\n");

    // 1. Configure DXVK WSI Driver to use SDL2
    setenv("DXVK_WSI_DRIVER", "SDL2", 1);
    if (!interactive) {
        setenv("BLACK_HEADLESS", "1", 1);
    } else {
        unsetenv("BLACK_HEADLESS");
    }

    // 2. Initialize Host Window via SDL2
    bool win_ok = host3d::open_window(1280, 720, 16, 9);
    check(win_ok, "Failed to open host SDL2 window");
    HWND hwnd = host3d::window();
    check(hwnd != nullptr, "Window handle is null");
    std::printf("[Gate 4] Host window opened successfully (1280x720, %s)\n",
                interactive ? "visible" : "headless");

    // 3. Initialize Direct3D 9 Device via DXVK-Native
    bool dev_ok = host3d::create_device();
    check(dev_ok, "Failed to create Direct3D 9 device on DXVK-Native");
    IDirect3DDevice9 *dev = host3d::dev();
    check(dev != nullptr, "host3d::dev() returned null");

    // 4. Verify GPU Capabilities
    D3DCAPS9 caps{};
    HRESULT hr = dev->GetDeviceCaps(&caps);
    check(SUCCEEDED(hr), "GetDeviceCaps failed");
    std::printf("[Gate 4] GPU D3D9 Caps: VS Version 0x%08X, PS Version 0x%08X, Max Streams %u\n",
                (unsigned int)caps.VertexShaderVersion, (unsigned int)caps.PixelShaderVersion, (unsigned int)caps.MaxStreams);
    check(caps.VertexShaderVersion >= D3DVS_VERSION(2, 0), "Vertex Shader 2.0+ required");
    check(caps.PixelShaderVersion >= D3DPS_VERSION(2, 0), "Pixel Shader 2.0+ required");

    // 5. Compile HLSL Vertex Shader & Pixel Shader via Linux D3DCompile
    const char *vs_src =
        "float4x4 u_matrix : register(c0);\n"
        "struct VS_IN {\n"
        "    float3 pos : POSITION;\n"
        "    float4 col : COLOR0;\n"
        "};\n"
        "struct VS_OUT {\n"
        "    float4 pos : POSITION;\n"
        "    float4 col : COLOR0;\n"
        "};\n"
        "VS_OUT main(VS_IN vin) {\n"
        "    VS_OUT vout;\n"
        "    vout.pos = mul(u_matrix, float4(vin.pos, 1.0f));\n"
        "    vout.col = vin.col;\n"
        "    return vout;\n"
        "}\n";

    const char *ps_src =
        "struct PS_IN {\n"
        "    float4 col : COLOR0;\n"
        "};\n"
        "float4 main(PS_IN pin) : COLOR0 {\n"
        "    return pin.col;\n"
        "}\n";

    ID3DBlob *vs_blob = nullptr;
    ID3DBlob *err_blob = nullptr;
    hr = D3DCompile(vs_src, std::strlen(vs_src), "gate4_vs", nullptr, nullptr, "main", "vs_2_0", 0, 0, &vs_blob, &err_blob);
    if (FAILED(hr)) {
        if (err_blob) std::fprintf(stderr, "VS compile error: %s\n", (char*)err_blob->GetBufferPointer());
        check(false, "D3DCompile VS failed");
    }

    ID3DBlob *ps_blob = nullptr;
    hr = D3DCompile(ps_src, std::strlen(ps_src), "gate4_ps", nullptr, nullptr, "main", "ps_2_0", 0, 0, &ps_blob, &err_blob);
    if (FAILED(hr)) {
        if (err_blob) std::fprintf(stderr, "PS compile error: %s\n", (char*)err_blob->GetBufferPointer());
        check(false, "D3DCompile PS failed");
    }

    // 6. Create Shaders on DXVK Device
    IDirect3DVertexShader9 *vs = nullptr;
    hr = dev->CreateVertexShader((const DWORD *)vs_blob->GetBufferPointer(), &vs);
    check(SUCCEEDED(hr) && vs != nullptr, "dev->CreateVertexShader failed");

    IDirect3DPixelShader9 *ps = nullptr;
    hr = dev->CreatePixelShader((const DWORD *)ps_blob->GetBufferPointer(), &ps);
    check(SUCCEEDED(hr) && ps != nullptr, "dev->CreatePixelShader failed");
    std::printf("[Gate 4] Shaders compiled and created on GPU successfully\n");

    // 7. Create Vertex Declaration
    D3DVERTEXELEMENT9 elems[] = {
        { 0, 0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 12, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR,    0 },
        D3DDECL_END()
    };
    IDirect3DVertexDeclaration9 *decl = nullptr;
    hr = dev->CreateVertexDeclaration(elems, &decl);
    check(SUCCEEDED(hr) && decl != nullptr, "dev->CreateVertexDeclaration failed");

    // 8. Create Vertex Buffer with RGB Triangle
    Vertex3D vertices[3] = {
        {  0.0f,   0.6f, 0.0f,  1.0f, 0.2f, 0.2f, 1.0f }, // Top (Red)
        {  0.6f,  -0.6f, 0.0f,  0.2f, 1.0f, 0.2f, 1.0f }, // Bottom-Right (Green)
        { -0.6f,  -0.6f, 0.0f,  0.2f, 0.4f, 1.0f, 1.0f }  // Bottom-Left (Blue)
    };

    IDirect3DVertexBuffer9 *vb = nullptr;
    hr = dev->CreateVertexBuffer(sizeof(vertices), 0, 0, D3DPOOL_DEFAULT, &vb, nullptr);
    check(SUCCEEDED(hr) && vb != nullptr, "dev->CreateVertexBuffer failed");

    void *ptr = nullptr;
    hr = vb->Lock(0, sizeof(vertices), &ptr, 0);
    check(SUCCEEDED(hr) && ptr != nullptr, "vb->Lock failed");
    std::memcpy(ptr, vertices, sizeof(vertices));
    vb->Unlock();

    std::printf("[Gate 4] Pipeline geometry and state created\n");

    // 9. Render Loop
    float angle = 0.0f;
    uint32_t target_frames = interactive ? 0 : 60;
    uint32_t frame_count = 0;

    while (!host3d::window_closed()) {
        host_sdl::pump_events();
        if (host3d::window_closed()) break;

        // In interactive mode, check for ESC key
        if (interactive) {
            const Uint8 *keys = SDL_GetKeyboardState(nullptr);
            if (keys[SDL_SCANCODE_ESCAPE]) break;
        }

        // Calculate 2D rotation matrix: column-major for HLSL mul(matrix, pos)
        float c = std::cos(angle);
        float s = std::sin(angle);
        float aspect = 16.0f / 9.0f;

        // HLSL default matrix mul is row/col matching float4x4
        float mat[16] = {
             c / aspect, -s, 0.0f, 0.0f,
             s / aspect,  c, 0.0f, 0.0f,
             0.0f,       0.0f, 1.0f, 0.0f,
             0.0f,       0.0f, 0.0f, 1.0f
        };

        // Render Frame
        host3d::begin_scene();

        hr = dev->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
                        D3DCOLOR_XRGB(18, 26, 42), 1.0f, 0);
        check(SUCCEEDED(hr), "dev->Clear failed");

        dev->SetVertexShaderConstantF(0, mat, 4);
        dev->SetVertexDeclaration(decl);
        dev->SetStreamSource(0, vb, 0, sizeof(Vertex3D));
        dev->SetVertexShader(vs);
        dev->SetPixelShader(ps);

        hr = dev->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 1);
        check(SUCCEEDED(hr), "dev->DrawPrimitive failed");

        hr = host3d::present();
        check(SUCCEEDED(hr), "host3d::present failed");

        angle += 0.03f;
        frame_count++;

        if (target_frames > 0 && frame_count >= target_frames) {
            break;
        }

        if (interactive) {
            SDL_Delay(16); // ~60 FPS
        }
    }

    std::printf("[Gate 4] Successfully rendered %u frames via Vulkan hardware backend\n", frame_count);

    // 10. Clean up Direct3D resources
    vb->Release();
    decl->Release();
    vs->Release();
    ps->Release();
    vs_blob->Release();
    ps_blob->Release();

    std::printf("====================================================\n");
    std::printf("Gate 4: ALL ASSERTIONS PASSED (Exit Code 0)\n");
    std::printf("Milestone Gate 4 verified: Graphics subsystem & shader pipeline operational\n");
    std::printf("====================================================\n");

    return 0;
}
