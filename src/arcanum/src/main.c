/* Arcanum Japanese patch: entry point, logging, code patch helpers */
#include "arcjp.h"
#include <stdio.h>
#include <stdarg.h>

void proxy_init(void);

#ifdef ARCJP_TEST
/* 試験版だけ: ARCJP_TEST=1 で試験の操作口、ARCJP_LOG=<ファイル> で記録 */
int g_test;
static char g_logpath[MAX_PATH];
static CRITICAL_SECTION g_logcs;

void dlog(const char *fmt, ...)
{
    FILE *f;
    va_list ap;
    if (!g_logpath[0]) return;
    EnterCriticalSection(&g_logcs);
    f = fopen(g_logpath, "a");
    if (f) {
        va_start(ap, fmt);
        vfprintf(f, fmt, ap);
        va_end(ap);
        fputc('\n', f);
        fclose(f);
    }
    LeaveCriticalSection(&g_logcs);
}
#endif

void patch_bytes(uint32_t at, const void *b, int n)
{
    DWORD old;
    VirtualProtect((void *)at, n, PAGE_EXECUTE_READWRITE, &old);
    memcpy((void *)at, b, n);
    VirtualProtect((void *)at, n, old, &old);
    FlushInstructionCache(GetCurrentProcess(), (void *)at, n);
}

static void patch_rel(uint32_t at, uint8_t op, void *to)
{
    uint8_t b[5];
    int32_t d = (int32_t)((uint32_t)to - (at + 5));
    b[0] = op;
    memcpy(b + 1, &d, 4);
    patch_bytes(at, b, 5);
}

void patch_jmp(uint32_t at, void *to) { patch_rel(at, 0xe9, to); }
void patch_call(uint32_t at, void *to) { patch_rel(at, 0xe8, to); }

/* compare code bytes with a hex string ("8b442408 83ec7c"); 1 = match */
int check_bytes(uint32_t at, const char *hex)
{
    const uint8_t *p = (const uint8_t *)at;
    int v, n = 0;
    while (*hex) {
        if (*hex == ' ') { hex++; continue; }
        if (sscanf(hex, "%2x", &v) != 1) return 0;
        if (p[n++] != (uint8_t)v) return 0;
        hex += 2;
    }
    return 1;
}

/* replace an import of Arcanum.exe; returns the old function or NULL */
void *iat_hook(const char *dll, const char *func, void *repl)
{
    uint8_t *base = (uint8_t *)0x400000;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
    IMAGE_IMPORT_DESCRIPTOR *d = (IMAGE_IMPORT_DESCRIPTOR *)(base + nt->OptionalHeader.DataDirectory[1].VirtualAddress);
    for (; d->Name; d++) {
        IMAGE_THUNK_DATA *on, *th;
        if (_stricmp((char *)base + d->Name, dll)) continue;
        on = (IMAGE_THUNK_DATA *)(base + d->OriginalFirstThunk);
        th = (IMAGE_THUNK_DATA *)(base + d->FirstThunk);
        for (; on->u1.AddressOfData; on++, th++) {
            IMAGE_IMPORT_BY_NAME *ib;
            if (on->u1.Ordinal & IMAGE_ORDINAL_FLAG) continue;
            ib = (IMAGE_IMPORT_BY_NAME *)(base + on->u1.AddressOfData);
            if (!strcmp((char *)ib->Name, func)) {
                void *old = (void *)th->u1.Function;
                DWORD prot;
                VirtualProtect(&th->u1.Function, 4, PAGE_READWRITE, &prot);
                th->u1.Function = (DWORD)repl;
                VirtualProtect(&th->u1.Function, 4, prot, &prot);
                return old;
            }
        }
    }
    return NULL;
}

static int version_ok(void)
{
    /* tig_font_write / tig_font_glyph_data / mes_load_internal call / tig_ping */
    return check_bytes(A_FONT_WRITE, "8b442408 83ec7c 53 55 56 57 33ff")
        && check_bytes(A_GLYPH_DATA, "83ec18 8b4c241c 53 8b5c2424")
        && check_bytes(0x4d4304, "e817060000")
        && check_bytes(A_TIG_PING, "68349f7300 e876ed0000");
}

#ifdef ARCJP_TEST
static LONG WINAPI crash_filter(EXCEPTION_POINTERS *e)
{
    CONTEXT *c = e->ContextRecord;
    uint32_t *sp = (uint32_t *)c->Esp;
    int i;
    static int n;
    if (++n > 6) return EXCEPTION_CONTINUE_SEARCH;
    dlog("CRASH code %08lx at %08lx eax=%08lx ebx=%08lx ecx=%08lx edx=%08lx esi=%08lx edi=%08lx ebp=%08lx esp=%08lx",
         e->ExceptionRecord->ExceptionCode, (unsigned long)c->Eip, c->Eax, c->Ebx, c->Ecx, c->Edx, c->Esi, c->Edi, c->Ebp, c->Esp);
    for (i = 0; i < 24; i++) {
        uint32_t v = 0;
        if (!IsBadReadPtr(sp + i, 4)) v = sp[i];
        dlog("  [esp+%02x] %08x", i * 4, v);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID r)
{
#ifdef ARCJP_TEST
    char buf[16];
#endif
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    DisableThreadLibraryCalls(h);
#ifdef ARCJP_TEST
    InitializeCriticalSection(&g_logcs);
#endif
    proxy_init();
#ifdef ARCJP_TEST
    if (GetEnvironmentVariableA("ARCJP_TEST", buf, sizeof buf) && buf[0] == '1') g_test = 1;
    if (!GetEnvironmentVariableA("ARCJP_LOG", g_logpath, MAX_PATH)) g_logpath[0] = 0;
#endif
    if (!version_ok()) {
        dlog("Arcanum.exe version mismatch - patch disabled");
        return TRUE;
    }
#ifdef ARCJP_TEST
    if (!GetEnvironmentVariableA("ARCJP_NOTEXT", buf, sizeof buf)) text_init(h);
    if (!GetEnvironmentVariableA("ARCJP_NOFONT", buf, sizeof buf)) font_init(h);
    if (g_test) AddVectoredExceptionHandler(0, crash_filter);
    if (g_test && !GetEnvironmentVariableA("ARCJP_NOIO", buf, sizeof buf)) testio_init();
#else
    text_init(h);
    font_init(h);
#endif
    dlog("arcjp loaded (test=%d)", g_test);
    return TRUE;
}
