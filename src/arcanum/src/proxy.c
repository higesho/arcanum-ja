/* dinput.dll proxy: forwards every export to the system dinput.dll */
#include <windows.h>

static FARPROC pf[7];
static const char *names[7] = { "DirectInputCreateA", "DirectInputCreateEx", "DirectInputCreateW",
    "DllCanUnloadNow", "DllGetClassObject", "DllRegisterServer", "DllUnregisterServer" };

void proxy_init(void)
{
    char path[MAX_PATH];
    UINT n = GetSystemDirectoryA(path, MAX_PATH);
    HMODULE h;
    int i;
    lstrcpyA(path + n, "\\dinput.dll");
    h = LoadLibraryA(path);
    if (!h) return;
    for (i = 0; i < 7; i++) pf[i] = GetProcAddress(h, names[i]);
}

__declspec(naked) void p_DirectInputCreateA(void) { __asm { jmp dword ptr [pf + 0] } }
__declspec(naked) void p_DirectInputCreateEx(void) { __asm { jmp dword ptr [pf + 4] } }
__declspec(naked) void p_DirectInputCreateW(void) { __asm { jmp dword ptr [pf + 8] } }
__declspec(naked) void p_DllCanUnloadNow(void) { __asm { jmp dword ptr [pf + 12] } }
__declspec(naked) void p_DllGetClassObject(void) { __asm { jmp dword ptr [pf + 16] } }
__declspec(naked) void p_DllRegisterServer(void) { __asm { jmp dword ptr [pf + 20] } }
__declspec(naked) void p_DllUnregisterServer(void) { __asm { jmp dword ptr [pf + 24] } }
