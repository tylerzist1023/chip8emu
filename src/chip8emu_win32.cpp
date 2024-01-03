#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#include "chip8emu_platform.h"
#include "chip8emu.h"

#include <d3d11.h>
#include <limits.h>
#include <tchar.h>
#include <shellapi.h>
#include <timeapi.h>
#include <commdlg.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "comdlg32.lib")

#include "imgui_impl_win32.cpp"
#include "imgui_impl_dx11.cpp"

static CRITICAL_SECTION g_critical_section;

static HINSTANCE g_hinstance;
static HWND g_hwnd;

namespace plat
{

bool show_file_prompt(FilePath* path)
{
    char* buf = (char*)malloc(MAX_PATH * sizeof(*buf));
    ZeroMemory(buf, MAX_PATH*sizeof(*buf));

    OPENFILENAMEA ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hInstance = g_hinstance;
    ofn.hwndOwner = g_hwnd;
    ofn.lpstrTitle = "Open File";
    ofn.lpstrFilter = "CHIP-8 Programs (*.ch8)\0*.ch8\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = buf;
    ofn.nMaxFile = MAX_PATH;

    if(GetOpenFileNameA(&ofn) && path)
    {
        path->data = buf;
        path->len = strlen(path->data);
        return true;
    }
    else
    {
        return false;
    }
}

void unload_path(FilePath path)
{
    if(path.data)
        free(path.data);
    path.data = 0;
    path.len = 0;
}

FileContents load_entire_file(FilePath path)
{
    FileContents contents = {0};

    HANDLE file = CreateFileA(path.data, GENERIC_READ, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

    LARGE_INTEGER size;

    if(file == INVALID_HANDLE_VALUE)
    {
        CloseHandle(file);
        return contents;
    }
    if(!GetFileSizeEx(file, &size))
    {
        CloseHandle(file);
        return contents;
    }
    if(size.QuadPart > INT32_MAX)
    {
        CloseHandle(file);
        return contents;
    }
    void* memory = VirtualAlloc(0, size.QuadPart, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if(!memory)
    {
        CloseHandle(file);
        return contents;
    }

    DWORD bytes_read;
    if(!ReadFile(file, memory, size.QuadPart, &bytes_read, 0))
    {
        unload_file(contents);
        CloseHandle(file);
        return contents;
    }
    if(bytes_read != size.QuadPart)
    {
        unload_file(contents);
        CloseHandle(file);
        return contents;
    }

    contents.data = memory;
    contents.len = size.QuadPart;
    CloseHandle(file);
    return contents;
}
void unload_file(FileContents contents)
{
    VirtualFree(contents.data, contents.len, MEM_DECOMMIT | MEM_RELEASE);
}

void update_input()
{
    ImGuiIO& io = ImGui::GetIO();
    io.WantCaptureKeyboard = true;

    EnterCriticalSection(&g_critical_section);
    {
        c8e::c8.keys[0]   = ImGui::IsKeyDown(ImGuiKey_X);
        c8e::c8.keys[1]   = ImGui::IsKeyDown(ImGuiKey_1);
        c8e::c8.keys[2]   = ImGui::IsKeyDown(ImGuiKey_2);
        c8e::c8.keys[3]   = ImGui::IsKeyDown(ImGuiKey_3);
        c8e::c8.keys[4]   = ImGui::IsKeyDown(ImGuiKey_Q);
        c8e::c8.keys[5]   = ImGui::IsKeyDown(ImGuiKey_W);
        c8e::c8.keys[6]   = ImGui::IsKeyDown(ImGuiKey_E);
        c8e::c8.keys[7]   = ImGui::IsKeyDown(ImGuiKey_A);
        c8e::c8.keys[8]   = ImGui::IsKeyDown(ImGuiKey_S);
        c8e::c8.keys[9]   = ImGui::IsKeyDown(ImGuiKey_D);
        c8e::c8.keys[0xA] = ImGui::IsKeyDown(ImGuiKey_Z);
        c8e::c8.keys[0xB] = ImGui::IsKeyDown(ImGuiKey_C);
        c8e::c8.keys[0xC] = ImGui::IsKeyDown(ImGuiKey_4);
        c8e::c8.keys[0xD] = ImGui::IsKeyDown(ImGuiKey_R);
        c8e::c8.keys[0xE] = ImGui::IsKeyDown(ImGuiKey_F);
        c8e::c8.keys[0xF] = ImGui::IsKeyDown(ImGuiKey_V);
    }
    LeaveCriticalSection(&g_critical_section);

    io.WantCaptureKeyboard = false;
}

};

// Data
static ID3D11Device*                g_pd3dDevice = NULL;
static ID3D11DeviceContext*         g_pd3dDeviceContext = NULL;
static IDXGISwapChain*              g_pSwapChain = NULL;
static ID3D11RenderTargetView*      g_mainRenderTargetView = NULL;
    
static ID3D11Texture2D*             g_display_texture = 0;
static ID3D11ShaderResourceView*    g_display_rec_view = 0;
static ID3D11InputLayout*           g_display_input_layout = 0;
static ID3D11VertexShader*          g_display_vertex_shader = 0;
static ID3D11PixelShader*           g_display_pixel_shader = 0;
static ID3D11Buffer*                g_display_vertex_buffer = 0;
static ID3D11SamplerState*          g_display_sampler_state = 0;

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static inline double get_elapsed(LARGE_INTEGER start, LARGE_INTEGER end, LARGE_INTEGER freq)
{
    LARGE_INTEGER elapsed_mus;
    elapsed_mus.QuadPart = end.QuadPart - start.QuadPart;
    elapsed_mus.QuadPart *= 1000000;
    elapsed_mus.QuadPart /= freq.QuadPart;
    return elapsed_mus.QuadPart / 1000000.0;
}

DWORD WINAPI ThreadProc(LPVOID param)
{
    LARGE_INTEGER start_second, start_timer;
    LARGE_INTEGER freq;
    LARGE_INTEGER end_second, end_timer;

    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start_timer);
    QueryPerformanceCounter(&start_second);

    static int debug_de_facto_ips = 0;

    HANDLE timer = CreateWaitableTimerA(0, TRUE, 0);
    LARGE_INTEGER due_time;
    due_time.QuadPart = -166666LL;

    SetWaitableTimer(timer, &due_time, 0, 0, 0, 0);

    for(;;)
    {
        EnterCriticalSection(&g_critical_section);
        {
            if(!c8e::c8.loaded)
            {
                LeaveCriticalSection(&g_critical_section);
                Sleep(1);
                continue;
            }
        }
        LeaveCriticalSection(&g_critical_section);

        EnterCriticalSection(&g_critical_section);
            c8e::update_timers();
        LeaveCriticalSection(&g_critical_section);

        double ipt_d = c8e::c8.ips / 60.0;
        static double carry = 0.0; 
        carry += ipt_d - (long long)ipt_d;
        long long ipt = (c8e::c8.ips / 60) + (carry >= 1.0);
        while(carry >= 1.0)
            carry -= 1.0;
        for(int i = 0; i < ipt; i++)
        {
            EnterCriticalSection(&g_critical_section);
                c8e::next_op();
            LeaveCriticalSection(&g_critical_section);
            debug_de_facto_ips++;
        }
        
        QueryPerformanceCounter(&end_second);
        if(get_elapsed(start_second, end_second, freq) >= 1.0)
        {
            char buf[64];
            snprintf(buf, 64, "Defacto ips this second: %d\n", debug_de_facto_ips);
            OutputDebugStringA(buf);
            debug_de_facto_ips = 0;
            QueryPerformanceCounter(&start_second);
        }

        WaitForSingleObject(timer, INFINITE);
        SetWaitableTimer(timer, &due_time, 0, 0, 0, 0);
    }

    ExitThread(0);
}

static const float display_vertex_quad[] = 
{
/*    x      y      z      r      g      b      a      u      v     */
    -1.0f,  1.0f,  0.0f,  1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f,
     1.0f,  1.0f,  0.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  0.0f,
     1.0f, -1.0f,  0.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,

    -1.0f,  1.0f,  0.0f,  1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f,
     1.0f, -1.0f,  0.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,
    -1.0f, -1.0f,  0.0f,  1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  1.0f,
};
static const UINT display_vertex_stride = 9 * sizeof(*display_vertex_quad);
static const UINT display_vertex_offset = 0;
static const UINT display_vertex_count  = 6;

static inline void display_init()
{
    // Create the display texture
    D3D11_TEXTURE2D_DESC texture2d_desc;
    texture2d_desc.Width = c8e::c8.display_w;
    texture2d_desc.Height = c8e::c8.display_h;
    texture2d_desc.MipLevels = 1;
    texture2d_desc.ArraySize = 1;
    texture2d_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texture2d_desc.SampleDesc.Count = 1;
    texture2d_desc.SampleDesc.Quality = 0;
    texture2d_desc.Usage = D3D11_USAGE_DYNAMIC;
    texture2d_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    texture2d_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    texture2d_desc.MiscFlags = 0;

    D3D11_SUBRESOURCE_DATA texture_subresource = {};
    texture_subresource.pSysMem = c8e::c8.display;
    texture_subresource.SysMemPitch = c8e::c8.display_w * sizeof(*c8e::c8.display);

    HRESULT hr = g_pd3dDevice->CreateTexture2D(&texture2d_desc, &texture_subresource, &g_display_texture);
    assert(SUCCEEDED(hr));
    hr = g_pd3dDevice->CreateShaderResourceView(g_display_texture, 0, &g_display_rec_view);
    assert(SUCCEEDED(hr));

    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = sizeof(display_vertex_quad);
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA sr_data = {0};
    sr_data.pSysMem = display_vertex_quad;

    hr = g_pd3dDevice->CreateBuffer(&bd, &sr_data, &g_display_vertex_buffer);
    assert(SUCCEEDED(hr));

    static const char* vertex_shader =
"struct VS_INPUT\n \
{\n \
    float3 pos : POSITION;\n \
    float4 col : COLOR0;\n \
    float2 uv  : TEXCOORD0;\n \
};\n \
\n \
struct PS_INPUT\n \
{\n \
    float4 pos : SV_POSITION;\n \
    float4 col : COLOR0;\n \
    float2 uv  : TEXCOORD0;\n \
};\n \
\n \
PS_INPUT main(VS_INPUT input)\n \
{\n \
    PS_INPUT output;\n \
    output.pos = float4(input.pos.xyz, 1.f);\n \
    output.col = input.col;\n \
    output.uv  = input.uv;\n \
    return output;\n \
}\n";
    ID3DBlob* vertex_shader_blob = 0;
    ID3DBlob* vertex_shader_errors = 0;
    hr = D3DCompile(vertex_shader, strlen(vertex_shader), 0, 0, 0, "main", "vs_5_0", 0, 0, &vertex_shader_blob, &vertex_shader_errors);
    if(vertex_shader_errors)
    {
        const char* error_string = (const char*)vertex_shader_errors->GetBufferPointer();
        vertex_shader_errors->Release();
        MessageBox(0, error_string, "Vertex Shader Compile Error", MB_ICONERROR | MB_OK);
    }
    assert(SUCCEEDED(hr));
    hr = g_pd3dDevice->CreateVertexShader(vertex_shader_blob->GetBufferPointer(), vertex_shader_blob->GetBufferSize(), 0, &g_display_vertex_shader);
    assert(SUCCEEDED(hr));

    static const char* pixel_shader =
"struct PS_INPUT\n \
{\n \
    float4 pos : SV_POSITION;\n \
    float4 col : COLOR0;\n \
    float2 uv  : TEXCOORD0;\n \
};\n \
Texture2D texture0 : register(t0);\n \
SamplerState sampler0 : register(s0);\n \
\n \
float4 main(PS_INPUT input) : SV_TARGET\n \
{\n \
    float4 out_col = input.col * texture0.Sample(sampler0, input.uv);\n \
    //float4 out_col = float4(1.0f, 1.0f, 0.0f, 1.0f);\n \
    return out_col;\n \
}\n";

    ID3DBlob* pixel_shader_blob = 0;
    ID3DBlob* pixel_shader_errors = 0;
    hr = D3DCompile(pixel_shader, strlen(pixel_shader), 0, 0, 0, "main", "ps_5_0", 0, 0, &pixel_shader_blob, &pixel_shader_errors);
    if(pixel_shader_errors)
    {
        const char* error_string = (const char*)pixel_shader_errors->GetBufferPointer();
        pixel_shader_errors->Release();
        MessageBox(0, error_string, "Pixel Shader Compile Error", MB_ICONERROR | MB_OK);
    }
    assert(SUCCEEDED(hr));
    hr = g_pd3dDevice->CreatePixelShader(pixel_shader_blob->GetBufferPointer(), pixel_shader_blob->GetBufferSize(), 0, &g_display_pixel_shader);
    assert(SUCCEEDED(hr));

    D3D11_INPUT_ELEMENT_DESC input_layout_desc[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,   0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,   0, D3D11_APPEND_ALIGNED_ELEMENT,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    hr = g_pd3dDevice->CreateInputLayout(input_layout_desc, ARRAYSIZE(input_layout_desc), vertex_shader_blob->GetBufferPointer(), vertex_shader_blob->GetBufferSize(), &g_display_input_layout);
    assert(SUCCEEDED(hr));

    vertex_shader_blob->Release();
    pixel_shader_blob->Release();

    D3D11_SAMPLER_DESC sample_desc = {};
    sample_desc.Filter         = D3D11_FILTER_MIN_MAG_MIP_POINT;
    sample_desc.AddressU       = D3D11_TEXTURE_ADDRESS_BORDER;
    sample_desc.AddressV       = D3D11_TEXTURE_ADDRESS_BORDER;
    sample_desc.AddressW       = D3D11_TEXTURE_ADDRESS_BORDER;
    sample_desc.BorderColor[0] = 1.0f;
    sample_desc.BorderColor[1] = 1.0f;
    sample_desc.BorderColor[2] = 1.0f;
    sample_desc.BorderColor[3] = 1.0f;
    sample_desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    hr = g_pd3dDevice->CreateSamplerState(&sample_desc, &g_display_sampler_state);
    assert(SUCCEEDED(hr));
}

static inline void display_render(RECT display_bounds)
{
    D3D11_VIEWPORT viewport =
    {
        (FLOAT)display_bounds.left,
        (FLOAT)display_bounds.top,
        (FLOAT)(display_bounds.right - display_bounds.left),
        (FLOAT)(display_bounds.bottom - display_bounds.top),
        0.0f,
        1.0f
    };

    g_pd3dDeviceContext->RSSetViewports(1, &viewport);

    g_pd3dDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    g_pd3dDeviceContext->IASetInputLayout(g_display_input_layout);
    g_pd3dDeviceContext->IASetVertexBuffers(0, 1, &g_display_vertex_buffer, &display_vertex_stride, &display_vertex_offset);

    g_pd3dDeviceContext->VSSetShader(g_display_vertex_shader, 0, 0);
    g_pd3dDeviceContext->PSSetShader(g_display_pixel_shader, 0, 0);
    g_pd3dDeviceContext->PSSetShaderResources(0, 1, &g_display_rec_view);
    g_pd3dDeviceContext->PSSetSamplers(0, 1, &g_display_sampler_state);

    EnterCriticalSection(&g_critical_section);
    {
        D3D11_SUBRESOURCE_DATA texture_subresource = {};
        texture_subresource.pSysMem = c8e::c8.display;
        texture_subresource.SysMemPitch = c8e::c8.display_w * sizeof(*c8e::c8.display);
        D3D11_MAPPED_SUBRESOURCE mapped_resource = {0};
        g_pd3dDeviceContext->Map(g_display_texture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_resource);
        memcpy(mapped_resource.pData, c8e::c8.display, c8e::c8.display_w * c8e::c8.display_h * sizeof(*c8e::c8.display));
        g_pd3dDeviceContext->Unmap(g_display_texture, 0);
    }
    LeaveCriticalSection(&g_critical_section);

    g_pd3dDeviceContext->Draw(display_vertex_count, 0);
}

// Main code
#if 0
int main(int, char**)
#else
int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
#endif
{
    wchar_t** argv;
    int argc;
    argv = CommandLineToArgvW(GetCommandLineW(), &argc); 

    InitializeCriticalSection(&g_critical_section);

    // Create application window
    //ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, _T("ImGui Example"), NULL };
    ::RegisterClassEx(&wc);
    HWND hwnd = ::CreateWindow(wc.lpszClassName, _T("Dear ImGui DirectX11 Example"), WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, NULL, NULL, wc.hInstance, NULL);

    g_hinstance = hInstance;
    g_hwnd = hwnd;

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClass(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show the window
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    ImGuiIO io;
    c8e::initialize(io);

    display_init();

    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    HANDLE cpu_thread = CreateThread(0, 0, ThreadProc, 0, CREATE_SUSPENDED, 0);

    // Our state
    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // EnterCriticalSection(&g_critical_section);
    // c8e::load_rom("program.ch8");
    // LeaveCriticalSection(&g_critical_section);

    SetThreadPriority(cpu_thread, THREAD_PRIORITY_TIME_CRITICAL);

    ResumeThread(cpu_thread);
    // Main loop
    bool done = false;
    while (!done)
    {
        // Poll and handle messages (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        MSG msg;
        int message_count = 0;
        while (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
        {
            if(msg.message != WM_TIMER)
                message_count++;
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        plat::update_input();

        // Start the Dear ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        EnterCriticalSection(&g_critical_section);
        {
            if(c8e::c8.vs > 0)
            {
                // ImGui::Begin("BEEP!");
                // ImGui::LabelText("BEEP!", "BEEP!");
                // ImGui::End();
                //OutputDebugStringA("BEEP!");
            }
        }
        LeaveCriticalSection(&g_critical_section);

        ImGui::BeginMainMenuBar();
        float menubar_h = ImGui::GetWindowHeight();
        ImGui::EndMainMenuBar();

        c8e::imgui_generic();
        ImGui::Render();

        EnterCriticalSection(&g_critical_section);
        if(c8e::c8.update_display || message_count > 0)
        {
            c8e::c8.update_display = false;
            LeaveCriticalSection(&g_critical_section);

            const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
            g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
            g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);

            RECT display_bounds;
            GetClientRect(hwnd, &display_bounds);
            display_bounds.top = (LONG)menubar_h;

            display_render(display_bounds);

            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

            g_pSwapChain->Present(1, 0);
        }
        else
        {
            LeaveCriticalSection(&g_critical_section);
            Sleep(1);
        }

        //g_pSwapChain->Present(1, 0); // Present with vsync
        //g_pSwapChain->Present(0, 0); // Present without vsync
    }

    // Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();

    //EnterCriticalSection(&g_critical_section);
    {
        TerminateThread(cpu_thread, 0);
    }
    //LeaveCriticalSection(&g_critical_section);
    DeleteCriticalSection(&g_critical_section);
    
    ::DestroyWindow(hwnd);
    ::UnregisterClass(wc.lpszClassName, wc.hInstance);

    return 0;
}

// Helper functions

bool CreateDeviceD3D(HWND hWnd)
{
    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    if (D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext) != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = NULL; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = NULL; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = NULL; }
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED)
        {
            EnterCriticalSection(&g_critical_section);
            {
                c8e::c8.update_display = true;
            }
            LeaveCriticalSection(&g_critical_section);

            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProc(hWnd, msg, wParam, lParam);
}
