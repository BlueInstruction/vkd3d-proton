#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>

static struct {
    int force_fl;
    int skip_cpu;
    int skip_avx;
    int debug;
    char log[260];
} g_cfg = {0xc200, 1, 1, 0, "dragon.log"};

static FILE* g_log = NULL;
static HMODULE g_dxgi = NULL;
static HMODULE g_d3d12 = NULL;

static void dlog(const char* fmt, ...) {
    if (!g_cfg.debug) return;
    if (!g_log) g_log = fopen(g_cfg.log, "a");
    if (!g_log) return;
    va_list a; va_start(a, fmt);
    vfprintf(g_log, fmt, a);
    fprintf(g_log, "\n");
    fflush(g_log);
    va_end(a);
}

static void load_cfg(void) {
    char p[260], m[260];
    GetModuleFileNameA(NULL, m, 260);
    char* s = strrchr(m, '\\'); if(s) *s = 0;
    snprintf(p, 260, "%s\\dxgi_proxy.ini", m);
    g_cfg.force_fl = GetPrivateProfileIntA("Dragon", "ForceFeatureLevel", 0xc200, p);
    g_cfg.skip_cpu = GetPrivateProfileIntA("Dragon", "SkipCPUCheck", 1, p);
    g_cfg.skip_avx = GetPrivateProfileIntA("Dragon", "SkipAVXCheck", 1, p);
    g_cfg.debug = GetPrivateProfileIntA("Dragon", "DebugLog", 0, p);
    GetPrivateProfileStringA("Dragon", "LogFile", "dragon.log", g_cfg.log, 260, p);
}

typedef HRESULT (WINAPI *PFN_CF)(REFIID, void**);
typedef HRESULT (WINAPI *PFN_CF2)(UINT, REFIID, void**);
typedef HRESULT (WINAPI *PFN_CD)(void*, int, REFIID, void**);

static PFN_CF p_CF = NULL;
static PFN_CF p_CF1 = NULL;
static PFN_CF2 p_CF2 = NULL;
static PFN_CD p_CD = NULL;

static BOOL load_real(void) {
    if (g_dxgi) return TRUE;
    char p[260];
    
    GetSystemDirectoryA(p, 260); strcat(p, "\\dxgi_vkd3d.dll");
    g_dxgi = LoadLibraryA(p);
    if (!g_dxgi) { GetSystemDirectoryA(p, 260); strcat(p, "\\dxgi.dll.real"); g_dxgi = LoadLibraryA(p); }
    
    if (g_dxgi) {
        p_CF = (PFN_CF)GetProcAddress(g_dxgi, "CreateDXGIFactory");
        p_CF1 = (PFN_CF)GetProcAddress(g_dxgi, "CreateDXGIFactory1");
        p_CF2 = (PFN_CF2)GetProcAddress(g_dxgi, "CreateDXGIFactory2");
    }
    
    GetSystemDirectoryA(p, 260); strcat(p, "\\d3d12_vkd3d.dll");
    g_d3d12 = LoadLibraryA(p);
    if (!g_d3d12) { GetSystemDirectoryA(p, 260); strcat(p, "\\d3d12.dll.real"); g_d3d12 = LoadLibraryA(p); }
    
    if (g_d3d12) p_CD = (PFN_CD)GetProcAddress(g_d3d12, "D3D12CreateDevice");
    
    dlog("Load DXGI:%p D3D12:%p", g_dxgi, g_d3d12);
    return g_dxgi != NULL;
}

__declspec(dllexport) HRESULT WINAPI CreateDXGIFactory(REFIID riid, void **pp) {
    dlog("CreateDXGIFactory");
    return (load_real() && p_CF) ? p_CF(riid, pp) : 0x887A0001;
}

__declspec(dllexport) HRESULT WINAPI CreateDXGIFactory1(REFIID riid, void **pp) {
    dlog("CreateDXGIFactory1");
    return (load_real() && p_CF1) ? p_CF1(riid, pp) : 0x887A0001;
}

__declspec(dllexport) HRESULT WINAPI CreateDXGIFactory2(UINT f, REFIID riid, void **pp) {
    dlog("CreateDXGIFactory2 f:0x%x", f);
    return (load_real() && p_CF2) ? p_CF2(f, riid, pp) : 0x887A0001;
}

__declspec(dllexport) HRESULT WINAPI D3D12CreateDevice(void* a, int fl, REFIID riid, void** pp) {
    dlog("D3D12CreateDevice FL:0x%x", fl);
    if (!load_real() || !p_CD) return 0x80004005;
    if (fl > g_cfg.force_fl) { dlog("Force FL 0x%x", g_cfg.force_fl); fl = g_cfg.force_fl; }
    return p_CD(a, fl, riid, pp);
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD r, LPVOID p) {
    if (r == DLL_PROCESS_ATTACH) { DisableThreadLibraryCalls(h); load_cfg(); dlog("Dragon Init"); }
    else if (r == DLL_PROCESS_DETACH) { if(g_log) fclose(g_log); if(g_dxgi) FreeLibrary(g_dxgi); if(g_d3d12) FreeLibrary(g_d3d12); }
    return TRUE;
}
