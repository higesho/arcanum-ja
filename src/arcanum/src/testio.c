/* Test harness (ARCJP_TEST=1 only): a shared-memory command queue that tools/win.py fills.
 * Commands run on the game thread inside tig_ping, so input never depends on which window
 * has focus, and screenshots come from the game's own back buffer. */
#include "arcjp.h"
#include <stdio.h>

#define SHM_NAME "ARCJP_TESTIO"
typedef struct {
    uint32_t magic;           /* 'AJPT' */
    volatile int wr, rd;
    int cmd[256][4];          /* 4108 bytes so far */
    volatile int shot_req, shot_done;
    char shot_path[260];
    volatile int frame, busy;
} Shm;

static Shm *g_shm;
static DWORD g_wait_until;

static void key_msg(int dik, int pressed)
{
    struct { int ts, type, key; uint8_t pressed; uint8_t pad[3]; int pad2[4]; } m;
    memset(&m, 0, sizeof m);
    m.ts = *(int *)0x739f34;
    m.type = 5;
    m.key = dik;
    m.pressed = (uint8_t)pressed;
    ((int (__cdecl *)(void *))A_MSG_ENQUEUE)(&m);
}

static void do_shot(void)
{
    char src[MAX_PATH];
    DeleteFileA("screen0000.bmp");
    ((int (__cdecl *)(int))A_SHOT_INTERNAL)(G_SHOT_KEY);
    GetFullPathNameA("screen0000.bmp", MAX_PATH, src, NULL);
    DeleteFileA(g_shm->shot_path);
    if (!MoveFileA(src, g_shm->shot_path)) dlog("shot: move failed %lu", GetLastError());
}

/* the game minimises itself when it loses the focus; in test mode it must keep drawing
 * while other windows are in front, so deactivation messages are swallowed */
static WNDPROC g_oldproc;
static LRESULT CALLBACK test_wndproc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == WM_ACTIVATEAPP && !w) return 0;
    if (m == WM_ACTIVATE && LOWORD(w) == WA_INACTIVE) return 0;
    if (m == WM_KILLFOCUS) return 0;
    if (m == WM_SYSCOMMAND && (w & 0xfff0) == SC_MINIMIZE) return 0;
    return CallWindowProcA(g_oldproc, h, m, w, l);
}
static BOOL CALLBACK find_wnd(HWND h, LPARAM l)
{
    char cls[32];
    GetClassNameA(h, cls, sizeof cls);
    if (!strcmp(cls, "TIGClass")) { *(HWND *)l = h; return FALSE; }
    return TRUE;
}
static BOOL WINAPI my_showwindow(HWND h, int cmd)
{
    if (cmd == SW_MINIMIZE || cmd == SW_SHOWMINIMIZED || cmd == SW_SHOWMINNOACTIVE || cmd == SW_FORCEMINIMIZE)
        return TRUE;
    return ShowWindow(h, cmd);
}

void testio_ping(void)
{
    if (!g_shm) return;
    if (!g_oldproc) {
        HWND h = NULL;
        EnumThreadWindows(GetCurrentThreadId(), find_wnd, (LPARAM)&h);
        if (h) {
            g_oldproc = (WNDPROC)SetWindowLongPtrA(h, GWLP_WNDPROC, (LONG_PTR)test_wndproc);
            if (IsIconic(h)) ShowWindow(h, SW_SHOWNOACTIVATE);
        }
    }
    g_shm->frame++;
    if (g_shm->shot_req != g_shm->shot_done) {
        do_shot();
        g_shm->shot_done = g_shm->shot_req;
    }
    if (g_wait_until) {
        if ((int)(GetTickCount() - g_wait_until) < 0) return;
        g_wait_until = 0;
    }
    while (g_shm->rd != g_shm->wr) {
        int *c = g_shm->cmd[g_shm->rd & 255];
        g_shm->busy = 1;
        switch (c[0]) {
        case 1: key_msg(c[1], 1); *(uint8_t *)(0x62b09c + (c[1] & 255)) = 0x80; break;
        case 2: key_msg(c[1], 0); *(uint8_t *)(0x62b09c + (c[1] & 255)) = 0; break;
        case 3: ((void (__cdecl *)(int, int))A_MOUSE_SETPOS)(c[1], c[2]); break;
        case 4: ((void (__cdecl *)(int, int))A_MOUSE_SETBTN)(c[1], 1); break;
        case 5: ((void (__cdecl *)(int, int))A_MOUSE_SETBTN)(c[1], 0); break;
        case 6: g_wait_until = GetTickCount() + c[1]; break;
        case 7: ((void (__cdecl *)(int, int))0x56bc00)(c[1], c[2]); break;   /* written_ui_start_type */
        }
        g_shm->rd++;
        if (g_wait_until) return;
    }
    g_shm->busy = 0;
}

__declspec(naked) static void my_ping(void)
{
    __asm {
        call testio_ping
        push 0x739f34
        mov eax, 0x51f225
        jmp eax
    }
}

/* keep the game running while another window has the focus */
static void __cdecl my_set_active(int a)
{
    (void)a;
    *(int *)0x60f244 = 1;
    ((void (__cdecl *)(int))0x4ff350)(1);
    ((void (__cdecl *)(int))0x533c90)(1);
}

void testio_init(void)
{
    HANDLE h = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(Shm), SHM_NAME);
    if (!h) return;
    g_shm = (Shm *)MapViewOfFile(h, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(Shm));
    if (!g_shm) return;
    memset(g_shm, 0, sizeof(Shm));
    g_shm->magic = 0x54504a41;
    patch_jmp(A_TIG_PING, my_ping);
    patch_jmp(0x51f300, my_set_active);
    if (!iat_hook("USER32.dll", "ShowWindow", my_showwindow)) dlog("testio: ShowWindow not imported");
    dlog("testio: shm %d bytes", (int)sizeof(Shm));
}
