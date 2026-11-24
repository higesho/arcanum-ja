/* Arcanum (Steam 1.0.7.4, Arcanum.exe image base 0x400000, no ASLR) Japanese patch - shared declarations */
#pragma once
#include <windows.h>
#include <stdint.h>

/* ---- game structures (layouts from arcanum-ce, checked against the exe) ---- */
typedef struct { int x, y, width, height; } TRect;

typedef struct {
    uint32_t flags;          /* 1 scale, 2 underline, 8 shadow, 0x10 centered, 0x20 strike, 0x80 no alpha, 0x100 add */
    uint32_t art_id;
    uint32_t color, underline_color;
    int field_10;
    uint32_t strike_color;
    float scale;
    const char *str;
    int width, height;
    int field_28;
} TFont;

typedef struct {
    uint32_t flags; uint32_t art_id; TRect *src_rect; void *palette; uint32_t color;
    uint32_t *f14; TRect *f18; uint8_t alpha[4]; void *dst_vb; TRect *dst_rect; void *scratch;
} TArtBlit;

typedef struct { int width, height, hot_x, hot_y, off_x, off_y; } TFrame;

typedef struct { uint32_t flags; int width, height, pitch, bg, key, bpp; void *pixels; } TVBData;

typedef struct { int num; char *str; } MesEntry;
typedef struct {
    char path[260]; int refcount, num, max; MesEntry *entries; char *data; int size;
} MesFile;

typedef struct { int num; char *str; char *female; int iq; char *cond; int resp; char *act; } DlgEntry;

#define G_FONT_IDX     (*(int *)0x630cfc)
#define G_FONT_STACK   ((TFont **)0x630d00)
#define G_SHADOW_COLOR (*(uint32_t *)0x630d50)

#define A_FONT_MEASURE  0x535390
#define A_FONT_WRITE    0x535570
#define A_FONT_LINE     0x535850   /* static int(vb, str, len, TArtBlit*, bool shadow) */
#define A_GLYPH_DATA    0x535d10   /* static bool(art_id, ch, &w, &h, &dx, &dy) */
#define A_ART_FRAMESET  0x502c40
#define A_ART_FRAMEDATA 0x5031c0
#define A_VB_DATA       0x5203e0
#define A_VB_LOCK       0x5204b0
#define A_VB_UNLOCK     0x520500
#define A_VB_FILL       0x520630
#define A_COLOR_MASKS   0x52c2b0
#define A_MEM_ALLOC     0x58a716   /* exe CRT malloc (mes data uses it) */
#define A_MEM_FREE      0x58a347   /* exe CRT free */
#define A_MES_LOAD_INT  0x4d4920
#define A_DLG_PARSE     0x417860
#define A_TIG_PING      0x51f220
#define A_MSG_ENQUEUE   0x52b890
#define A_MOUSE_SETPOS  0x4ff3b0
#define A_MOUSE_SETBTN  0x4ff410
#define A_SHOT_INTERNAL 0x525250
#define G_SHOT_KEY      (*(int *)0x5bf3d8)

typedef int  (__cdecl *FontLineFn)(void *vb, const char *s, int len, TArtBlit *bi, int shadow);
typedef int  (__cdecl *GlyphDataFn)(uint32_t art, int ch, int *w, int *h, int *dx, int *dy);
typedef int  (__cdecl *VbDataFn)(void *vb, TVBData *d);
typedef int  (__cdecl *VbLockFn)(void *vb);
typedef int  (__cdecl *VbFillFn)(void *vb, TRect *r, uint32_t color);
typedef void (__cdecl *MasksFn)(uint32_t *r, uint32_t *g, uint32_t *b);
typedef void *(__cdecl *AllocFn)(size_t n);
typedef void (__cdecl *FreeFn)(void *p);

/* ---- main.c ---- */
#ifdef ARCJP_TEST
extern int g_test;
void dlog(const char *fmt, ...);
#else
/* 配布版には試験の仕組みも記録も入れない */
#define g_test 0
#define dlog(...) ((void)0)
#endif
void patch_jmp(uint32_t at, void *to);
void patch_call(uint32_t at, void *to);
void patch_bytes(uint32_t at, const void *b, int n);
int  check_bytes(uint32_t at, const char *hex);
void *iat_hook(const char *dll, const char *func, void *repl);

/* ---- font.c ---- */
void font_init(HMODULE self);
int  jp_lead(const uint8_t *s);

/* ---- text.c ---- */
void text_init(HMODULE self);
const char *tr_lookup(const char *en);
uint64_t fnv64(const uint8_t *s, int n);
#ifdef ARCJP_TEST
void log_untranslated(const char *s);

/* ---- testio.c（試験版だけ） ---- */
void testio_init(void);
void testio_ping(void);
#endif
