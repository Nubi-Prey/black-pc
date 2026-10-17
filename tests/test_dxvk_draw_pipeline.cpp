// Unit Test: Full DXVK-Native Pipeline (Shaders, Geometry and Rendering) for Linux x86 (Phase 4)
#include <d3d9.h>
#include <wsi/native_sdl2.h>
#include <SDL2/SDL.h>

#include <cstdio>
#include <cstdlib>
#include <vector>

static void check(bool cond, const char *msg)
{
    if (!cond) {
        std::fprintf(stderr, "[FAIL] %s\n", msg);
        std::exit(1);
    }
}

static std::vector<uint8_t> read_file(const char *path)
{
    FILE *f = std::fopen(path, "rb");
    check(f != nullptr, "Failed to open shader file");
    std::fseek(f, 0, SEEK_END);
    long sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> data(sz);
    std::fread(data.data(), 1, sz, f);
    std::fclose(f);
    return data;
}

int main()
{
    std::printf("=== Running test_dxvk_draw_pipeline (Phase 4) ===\n");

    // 1. Initialize SDL2
    check(SDL_Init(SDL_INIT_VIDEO) == 0, "SDL_Init failed");

    // 2. Create SDL2 window with Vulkan flag
    SDL_Window *win = SDL_CreateWindow("DXVK Draw Pipeline Test",
                                       SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                       640, 480, SDL_WINDOW_HIDDEN | SDL_WINDOW_VULKAN);
    check(win != nullptr, "SDL_CreateWindow failed");

    // 3. Create Direct3D 9 device via DXVK
    IDirect3D9 *d3d = Direct3DCreate9(D3D_SDK_VERSION);
    check(d3d != nullptr, "Direct3DCreate9 failed");

    HWND hwnd = dxvk::wsi::toHwnd(win);
    D3DPRESENT_PARAMETERS pp{};
    pp.BackBufferWidth = 640;
    pp.BackBufferHeight = 480;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.BackBufferCount = 1;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow = hwnd;
    pp.Windowed = TRUE;
    pp.EnableAutoDepthStencil = TRUE;
    pp.AutoDepthStencilFormat = D3DFMT_D24S8;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

    IDirect3DDevice9 *dev = nullptr;
    HRESULT hr = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
                                  D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &dev);
    check(SUCCEEDED(hr) && dev != nullptr, "CreateDevice failed");
    std::printf("[test_dxvk_draw_pipeline] Direct3D9 device initialized successfully\n");

    // 4. Load pre-compiled shaders
    auto vs_code = read_file("/tmp/test_vs.bin");
    auto ps_code = read_file("/tmp/test_ps.bin");
    check(vs_code.size() > 0 && ps_code.size() > 0, "Shader code empty");

    IDirect3DVertexShader9 *vs = nullptr;
    hr = dev->CreateVertexShader((const DWORD *)vs_code.data(), &vs);
    check(SUCCEEDED(hr) && vs != nullptr, "CreateVertexShader failed in DXVK");
    std::printf("[test_dxvk_draw_pipeline] Vertex Shader created in Vulkan (%zu bytes)\n", vs_code.size());

    IDirect3DPixelShader9 *ps = nullptr;
    hr = dev->CreatePixelShader((const DWORD *)ps_code.data(), &ps);
    check(SUCCEEDED(hr) && ps != nullptr, "CreatePixelShader failed in DXVK");
    std::printf("[test_dxvk_draw_pipeline] Pixel Shader created in Vulkan (%zu bytes)\n", ps_code.size());

    // 5. Create Vertex Declaration
    D3DVERTEXELEMENT9 decl_elems[] = {
        {0, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        D3DDECL_END()
    };
    IDirect3DVertexDeclaration9 *decl = nullptr;
    hr = dev->CreateVertexDeclaration(decl_elems, &decl);
    check(SUCCEEDED(hr) && decl != nullptr, "CreateVertexDeclaration failed");

    // 6. Create Vertex Buffer and fill triangle coordinates
    struct Vertex {
        float x, y, z, w;
    };
    Vertex triangle_vertices[3] = {
        { -0.5f, -0.5f, 0.0f, 1.0f },
        {  0.0f,  0.5f, 0.0f, 1.0f },
        {  0.5f, -0.5f, 0.0f, 1.0f }
    };

    IDirect3DVertexBuffer9 *vb = nullptr;
    hr = dev->CreateVertexBuffer(sizeof(triangle_vertices), 0, 0, D3DPOOL_DEFAULT, &vb, nullptr);
    check(SUCCEEDED(hr) && vb != nullptr, "CreateVertexBuffer failed");

    void *vb_ptr = nullptr;
    hr = vb->Lock(0, sizeof(triangle_vertices), &vb_ptr, 0);
    check(SUCCEEDED(hr) && vb_ptr != nullptr, "vb->Lock failed");
    std::memcpy(vb_ptr, triangle_vertices, sizeof(triangle_vertices));
    vb->Unlock();

    // 7. Render Triangle Frame through full pipeline
    hr = dev->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(12, 28, 16), 1.0f, 0);
    check(SUCCEEDED(hr), "dev->Clear failed");

    hr = dev->BeginScene();
    check(SUCCEEDED(hr), "dev->BeginScene failed");

    // Bind state
    dev->SetVertexDeclaration(decl);
    dev->SetVertexShader(vs);
    dev->SetPixelShader(ps);
    dev->SetStreamSource(0, vb, 0, sizeof(Vertex));

    // Upload 4x4 matrix constant (Identity matrix) to c0..c3
    float identity_matrix[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    hr = dev->SetVertexShaderConstantF(0, identity_matrix, 4);
    check(SUCCEEDED(hr), "SetVertexShaderConstantF failed");

    // Draw Primitive
    hr = dev->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 1);
    check(SUCCEEDED(hr), "DrawPrimitive failed");

    hr = dev->EndScene();
    check(SUCCEEDED(hr), "dev->EndScene failed");

    hr = dev->Present(nullptr, nullptr, nullptr, nullptr);
    check(SUCCEEDED(hr), "dev->Present failed");

    std::printf("[test_dxvk_draw_pipeline] DrawPrimitive(TRIANGLELIST) executed and presented via Vulkan!\n");

    // 8. Clean up
    vb->Release();
    decl->Release();
    ps->Release();
    vs->Release();
    dev->Release();
    d3d->Release();
    SDL_DestroyWindow(win);
    SDL_Quit();

    std::printf("[PASS] test_dxvk_draw_pipeline completed successfully with zero errors.\n\n");
    return 0;
}
