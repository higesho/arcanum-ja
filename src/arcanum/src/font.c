/* Text rendering: tig_font_measure / tig_font_write replaced with versions that understand
 * EUC-JP. ASCII runs still go through the game's own glyph blitter (0x535850); Japanese
 * characters are rasterised with GDI from the bundled font (resource ARCJP/2) and blended
 * straight into the destination video buffer. */
#include "arcjp.h"
#include <math.h>

static const FontLineFn  g_line  = (FontLineFn)A_FONT_LINE;
static const GlyphDataFn g_gdata = (GlyphDataFn)A_GLYPH_DATA;
static const VbDataFn    g_vbdata = (VbDataFn)A_VB_DATA;
static const VbLockFn    g_lock = (VbLockFn)A_VB_LOCK;
static const VbLockFn    g_unlock = (VbLockFn)A_VB_UNLOCK;
static const VbFillFn    g_fill = (VbFillFn)A_VB_FILL;

#define CUR_FONT (G_FONT_STACK[G_FONT_IDX])

/* ------------------------------------------------------------------ EUC-JP */
int jp_lead(const uint8_t *s) { return s[0] >= 0xa1 && s[0] != 0xff && s[1] >= 0xa1 && s[1] != 0xff; }

static WCHAR g_uni[94 * 94];
static WCHAR euc_to_uni(int a, int b)
{
    int idx = (a - 0xa1) * 94 + (b - 0xa1);
    if (!g_uni[idx]) {
        int j1 = a - 0x80, j2 = b - 0x80;
        char sj[2];
        WCHAR w = 0x30fb;
        sj[0] = (char)(((j1 + 1) >> 1) + (j1 <= 0x5e ? 0x70 : 0xb0));
        sj[1] = (char)(j2 + ((j1 & 1) ? (j2 >= 0x60 ? 0x20 : 0x1f) : 0x7e));
        if (MultiByteToWideChar(932, MB_ERR_INVALID_CHARS, sj, 2, &w, 1) != 1) w = 0x30fb;
        g_uni[idx] = w;
    }
    return g_uni[idx];
}

/* ------------------------------------------------------------------ per-font metrics */
typedef struct { uint32_t art; int ok, h, dy, sp_dx, em, lh, base; } FMet;
static FMet g_met[64];
static int g_nmet;

static int jp_em_for(int h)
{
    /* Arcanum's fonts are small; Japanese needs at least 12px to stay readable */
    int em = h;
    if (em < 12) em = 12;
    return em;
}

static FMet *metrics(TFont *f)
{
    uint32_t art = f->art_id & 0xfffff000u;   /* strip frame bits */
    int i, w, h, dx, dy;
    for (i = 0; i < g_nmet; i++) if (g_met[i].art == art) return &g_met[i];
    if (g_nmet == 64) g_nmet = 0;
    i = g_nmet++;
    g_met[i].art = art;
    g_met[i].ok = g_gdata(f->art_id, ' ', &w, &h, &dx, &dy);
    g_met[i].h = h; g_met[i].dy = dy; g_met[i].sp_dx = dx;
    g_met[i].em = jp_em_for(h);
    g_met[i].lh = g_met[i].em + 1 > h ? g_met[i].em + 1 : h;   /* Japanese lines get 1px of leading */
    g_met[i].base = dy;
    if (g_test) {
        int w2 = 0, h2 = 0, dx2 = 0, dy2 = 0, ok2 = g_gdata(f->art_id, '2', &w2, &h2, &dx2, &dy2);
        dlog("font art %08x: h=%d dy=%d space=%d -> em %d lh %d  ['2' ok=%d w=%d dx=%d]", art, h, dy, dx,
             g_met[i].em, g_met[i].lh, ok2, w2, dx2);
    }
    return &g_met[i];
}

static float fscale(TFont *f) { return (f->flags & 1) ? f->scale : 1.0f; }

/* line height that measure and write both use */
static int line_h(TFont *f, FMet *m, int has_jp)
{
    int h = has_jp ? m->lh : m->h;
    return h;
}

static int has_jp(const uint8_t *s)
{
    for (; *s; s++) if (s[0] >= 0xa1 && jp_lead(s)) return 1;
    return 0;
}

/* advance of one character (bytes consumed in *n) */
/* In Japanese text set in a font smaller than the Japanese em (newspaper body, 10px),
 * Latin letters and digits are drawn with the Japanese font too, or they look like superscripts. */
static int g_asc_gdi;
static int ascii_adv(int em, int ch);
static int gdi_char(const uint8_t *s) { return jp_lead(s) || (g_asc_gdi && *s >= 0x20 && *s < 0x7f); }

static int char_adv(TFont *f, FMet *m, const uint8_t *s, int *n, int *wlast)
{
    int w, h, dx, dy;
    if (jp_lead(s)) {
        *n = 2; *wlast = m->em;
        return m->em;
    }
    if (g_asc_gdi && *s >= 0x20 && *s < 0x7f) {
        *n = 1; *wlast = ascii_adv(m->em, *s);
        return *wlast;
    }
    *n = 1;
    if (!g_gdata(f->art_id, *s, &w, &h, &dx, &dy)) { *wlast = 0; return 0; }
    *wlast = w;
    return dx;
}

/* kinsoku */
static int no_start(const uint8_t *p)
{
    static const char *list = "、。，．・：；？！ー）」』】〕］｝〉》ゃゅょっぁぃぅぇぉャュョッァィゥェォ…～〜";
    if (!jp_lead(p)) return (*p == '.' || *p == ',' || *p == '!' || *p == '?' || *p == ')' || *p == ':' || *p == ';');
    {
        const uint8_t *l = (const uint8_t *)list;
        WCHAR c = euc_to_uni(p[0], p[1]);
        /* list is UTF-8 source; compare via conversion table once */
        static WCHAR codes[64]; static int nc = -1;
        int i;
        if (nc < 0) { nc = MultiByteToWideChar(CP_UTF8, 0, list, -1, codes, 64) - 1; }
        (void)l;
        for (i = 0; i < nc; i++) if (codes[i] == c) return 1;
    }
    return 0;
}
static int no_end(const uint8_t *p)
{
    static WCHAR codes[16]; static int nc = -1;
    WCHAR c;
    int i;
    if (!jp_lead(p)) return (*p == '(' || *p == '"');
    if (nc < 0) nc = MultiByteToWideChar(CP_UTF8, 0, "「『（【〔［｛〈《", -1, codes, 16) - 1;
    c = euc_to_uni(p[0], p[1]);
    for (i = 0; i < nc; i++) if (codes[i] == c) return 1;
    return 0;
}

/* Find the next line. Returns bytes of text in the line; *skip = bytes to drop after it
 * (1 for the space/newline the line broke at); *wout = pixel width of the line. */
static int next_line(TFont *f, FMet *m, const uint8_t *s, int maxw, int *skip, int *wout)
{
    int pos = 0, prev = -1, total = 0, brk = -1, brk_w = 0, brk_skip = 0;
    *skip = 0;
    while (s[pos]) {
        int n, wl, adv, need, cj;
        if (s[pos] == 10) { *skip = 1; *wout = total; return pos; }
        adv = char_adv(f, m, s + pos, &n, &wl);
        cj = (n == 2);
        if (s[pos] == ' ') { brk = pos; brk_w = total; brk_skip = 1; }
        else if (prev >= 0 && (cj || jp_lead(s + prev)) && s[prev] != ' '
                 && !no_start(s + pos) && !no_end(s + prev)) { brk = pos; brk_w = total; brk_skip = 0; }
        need = s[pos + n] ? adv : wl;
        if (total + need > maxw && pos > 0) {
            if (brk > 0 || (brk == 0 && brk_skip)) { *skip = brk_skip; *wout = brk_w; return brk; }
            *wout = total; return pos;           /* no break point: cut here */
        }
        total += adv;
        if (s[pos] == '-') { brk = pos + 1; brk_w = total; brk_skip = 0; }
        prev = pos;
        pos += n;
    }
    *wout = total;
    return pos;
}

/* ------------------------------------------------------------------ measure */
static void __cdecl my_font_measure(TFont *font)
{
    const char *str = font->str;
    int width = font->width, height = font->height;
    TFont *cur = CUR_FONT;
    FMet *m;
    memcpy(font, cur, sizeof(TFont));
    font->str = str; font->width = width; font->height = height;
    width = 0; height = 0;
    if (str) {
        const uint8_t *s = (const uint8_t *)str;
        int jp = has_jp(s), lines = 1;
        m = metrics(cur);
        if (!m->ok) { font->height = 0; return; }
        g_asc_gdi = jp && m->h < m->em;
        if (font->width != 0) {
            int pos = 0, skip, lw;
            for (;;) {
                int len = next_line(cur, m, s + pos, font->width, &skip, &lw);
                if (len < 0) { font->height = 0; return; }
                if (lw > width) width = lw;
                pos += len;
                if (!s[pos]) break;
                if (len == 0 && skip == 0) pos += jp_lead(s + pos) ? 2 : 1;  /* never loop */
                pos += skip;
                if (!s[pos]) break;
                lines++;
            }
            width = 0;      /* original leaves width as given when wrapping */
        } else {
            int pos = 0, n, wl, adv;
            while (s[pos]) {
                adv = char_adv(cur, m, s + pos, &n, &wl);
                width += s[pos + n] ? adv : wl;
                pos += n;
            }
        }
        height = line_h(cur, m, jp) * lines;
        if (font->flags & 8) { height++; width++; }
        if (font->flags & 1) {
            width = (int)(width * font->scale);
            height = (int)(height * font->scale);
        }
    }
    if (font->width == 0 || !str) font->width = width;
    font->height = height;
}

/* ------------------------------------------------------------------ glyph cache (GDI) */
typedef struct { uint32_t key; int16_t bw, bh, ox, oy, adv; uint8_t *cov; } Glyph;
#define GC_SIZE 32768
static Glyph *g_gc;
static HDC g_dc;
static HFONT g_hf[80];
static WCHAR g_face[32] = L"ArcJP Serif";
static uint32_t g_rmask, g_gmask, g_bmask;
static int g_rsh, g_gsh, g_bsh, g_rbits, g_gbits, g_bbits;

static Glyph *glyph(int em, WCHAR ch)
{
    uint32_t key = ((uint32_t)em << 16) | ch, h = (key * 2654435761u) & (GC_SIZE - 1);
    Glyph *g;
    GLYPHMETRICS gm;
    MAT2 mat = { {0, 1}, {0, 0}, {0, 0}, {0, 1} };
    DWORD sz;
    uint8_t *buf;
    TEXTMETRICW tm;
    int x, y, pitch;
    for (;;) {
        g = &g_gc[h];
        if (g->key == key) return g;
        if (!g->key) break;
        h = (h + 1) & (GC_SIZE - 1);
    }
    g->key = key; g->cov = NULL; g->bw = g->bh = 0; g->adv = (int16_t)(ch < 0x80 ? em / 2 : em);
    if (em >= 80) return g;
    if (!g_hf[em]) {
        g_hf[em] = CreateFontW(-em, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, SHIFTJIS_CHARSET, OUT_TT_PRECIS,
                               CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, g_face);
    }
    SelectObject(g_dc, g_hf[em]);
    GetTextMetricsW(g_dc, &tm);
    sz = GetGlyphOutlineW(g_dc, ch, GGO_GRAY8_BITMAP, &gm, 0, NULL, &mat);
    if (sz != GDI_ERROR && ch < 0x80) g->adv = (int16_t)gm.gmCellIncX;   /* proportional Latin */
    if (sz == GDI_ERROR || sz == 0) return g;
    buf = (uint8_t *)malloc(sz);
    if (!buf) return g;
    GetGlyphOutlineW(g_dc, ch, GGO_GRAY8_BITMAP, &gm, sz, buf, &mat);
    pitch = (gm.gmBlackBoxX + 3) & ~3;
    g->bw = (int16_t)gm.gmBlackBoxX; g->bh = (int16_t)gm.gmBlackBoxY;
    /* x offset inside the em cell; y = rows from the baseline up to the bitmap top */
    g->ox = (int16_t)(ch < 0x80 ? gm.gmptGlyphOrigin.x : gm.gmptGlyphOrigin.x + ((int)em - (int)gm.gmCellIncX) / 2);
    g->oy = (int16_t)gm.gmptGlyphOrigin.y;
    g->cov = (uint8_t *)malloc(g->bw * g->bh);
    if (g->cov) {
        for (y = 0; y < g->bh; y++)
            for (x = 0; x < g->bw; x++) {
                int v = buf[y * pitch + x] * 255 / 64;
                g->cov[y * g->bw + x] = (uint8_t)(v > 255 ? 255 : v);
            }
    }
    free(buf);
    return g;
}

static int ascii_adv(int em, int ch) { return glyph(em, (WCHAR)ch)->adv; }

static void masks_init(void);
static int mask_shift(uint32_t m) { int s = 0; if (!m) return 0; while (!(m & 1)) { m >>= 1; s++; } return s; }
static int mask_bits(uint32_t m) { int b = 0; while (m) { b += m & 1; m >>= 1; } return b; }

static void unpack(uint32_t c, int *r, int *g, int *b)
{
    *r = ((c & g_rmask) >> g_rsh) << (8 - g_rbits);
    *g = ((c & g_gmask) >> g_gsh) << (8 - g_gbits);
    *b = ((c & g_bmask) >> g_bsh) << (8 - g_bbits);
}
static uint32_t pack(int r, int g, int b)
{
    return ((uint32_t)(r >> (8 - g_rbits)) << g_rsh) | ((uint32_t)(g >> (8 - g_gbits)) << g_gsh)
         | ((uint32_t)(b >> (8 - g_bbits)) << g_bsh);
}

/* blend one glyph into a locked buffer: (x, base) = left of the em cell / baseline */
static void blit_glyph(TVBData *d, Glyph *gl, int x, int base, uint32_t color, uint32_t flags)
{
    int cr, cg, cb, gx, gy, x0 = x + gl->ox, y0 = base - gl->oy;
    if (!gl->cov || !d->pixels) return;
    if (!g_rmask) masks_init();
    unpack(color, &cr, &cg, &cb);
    for (gy = 0; gy < gl->bh; gy++) {
        int yy = y0 + gy;
        uint8_t *row;
        if (yy < 0 || yy >= d->height) continue;
        row = (uint8_t *)d->pixels + yy * d->pitch;
        for (gx = 0; gx < gl->bw; gx++) {
            int xx = x0 + gx, a = gl->cov[gy * gl->bw + gx], r, g, b;
            uint32_t px;
            if (!a || xx < 0 || xx >= d->width) continue;
            if (flags & 0x80) { if (a < 110) continue; a = 255; }
            if (d->bpp == 16) px = ((uint16_t *)row)[xx];
            else if (d->bpp == 32) px = ((uint32_t *)row)[xx];
            else continue;
            unpack(px, &r, &g, &b);
            if (flags & 0x100) {
                r += cr * a / 255; g += cg * a / 255; b += cb * a / 255;
                if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255;
            } else {
                r += (cr - r) * a / 255; g += (cg - g) * a / 255; b += (cb - b) * a / 255;
            }
            px = pack(r, g, b);
            if (d->bpp == 16) ((uint16_t *)row)[xx] = (uint16_t)px;
            else ((uint32_t *)row)[xx] = px | (((uint32_t *)row)[xx] & ~(g_rmask | g_gmask | g_bmask));
        }
    }
}

/* draw one line: ASCII runs through the game, Japanese through GDI glyphs */
static int draw_line(void *vb, TFont *f, FMet *m, const uint8_t *s, int len, TArtBlit *bi, int shadow,
                     int lh, uint32_t color)
{
    int pos = 0, rc = 0, x0 = bi->dst_rect->x;
    uint32_t saved = f->flags;
    int em = (int)(m->em * fscale(f) + 0.5f);
    /* Japanese em box centred in the line (some fonts have no usable baseline: hot_y = 0);
     * the glyph baseline sits 0.88 em below the top of the em box */
    int base = bi->dst_rect->y + (lh - em) / 2 + (int)(em * 0.88f + 0.5f);
    int asc_y = bi->dst_rect->y;
    f->flags &= ~0x22u;     /* underline / strike drawn here for the whole line */
    while (pos < len && s[pos]) {
        if (gdi_char(s + pos)) {
            TVBData d;
            int n = 0, k, ok = 0, locked;
            while (pos + n < len && s[pos + n] && gdi_char(s + pos + n)) n += jp_lead(s + pos + n) ? 2 : 1;
            memset(&d, 0, sizeof d);
            locked = g_lock(vb) == 0;
            if (locked) ok = g_vbdata(vb, &d) == 0 && d.pixels;
            for (k = 0; k < n; ) {
                int two = jp_lead(s + pos + k);
                Glyph *g = glyph(two ? em : m->em, two ? euc_to_uni(s[pos + k], s[pos + k + 1]) : s[pos + k]);
                if (ok) blit_glyph(&d, g, bi->dst_rect->x, base, color, f->flags);
                bi->dst_rect->x += two ? em : g->adv;
                k += two ? 2 : 1;
            }
            if (locked) g_unlock(vb);
            pos += n;
        } else {
            int n = 0;
            while (pos + n < len && s[pos + n] && !gdi_char(s + pos + n)) n++;
            bi->dst_rect->y = asc_y + (lh - m->h) / 2;
            rc = g_line(vb, (const char *)s + pos, n, bi, shadow);
            bi->dst_rect->y = asc_y;
            if (rc) break;
            pos += n;
        }
    }
    f->flags = saved;
    if (!rc && (saved & 0x22) && pos > 0) {
        TRect r;
        r.x = x0; r.width = bi->dst_rect->x - x0; r.height = 1;
        if (saved & 2) {
            r.y = asc_y + (lh - m->h) / 2 + m->dy;
            if (shadow) { TRect q = r; q.x++; q.y++; q.width--; g_fill(vb, &q, G_SHADOW_COLOR); }
            g_fill(vb, &r, f->underline_color);
        }
        if (saved & 0x20) {
            r.y = asc_y + (lh - m->h) / 2 + m->dy / 2;
            if (shadow) { TRect q = r; q.x++; q.y++; q.width--; g_fill(vb, &q, G_SHADOW_COLOR); }
            g_fill(vb, &r, f->strike_color);
        }
    }
    return rc;
}

/* ------------------------------------------------------------------ write */
static int __cdecl my_font_write(void *vb, const char *str, const TRect *rect, TRect *dirty)
{
    TArtBlit bi;
    TRect dst, src;
    TFont *f = CUR_FONT;
    FMet *m;
    const uint8_t *s = (const uint8_t *)str;
    int passes, pass, shadow, min_dx, max_w = 0, max_y = 0, lh, jp;
    if (!str) return 0;
    if (g_test) log_untranslated(str);
    m = metrics(f);
    if (!m->ok) return 4;
    jp = has_jp(s);
    g_asc_gdi = jp && m->h < m->em;
    lh = line_h(f, m, jp);
    shadow = (f->flags & 8) != 0;
    passes = shadow ? 2 : 1;
    memset(&bi, 0, sizeof bi);
    bi.dst_rect = &dst; bi.src_rect = &src; bi.dst_vb = vb;
    src.x = src.y = 0;
    min_dx = rect->width;
    for (pass = 0; pass < passes; pass++) {
        const uint8_t *rem = s;
        int x0;
        uint32_t color;
        bi.flags = 0;
        if (!shadow) {
            bi.flags |= 0x2000;  /* TIG_ART_BLT_BLEND_COLOR_CONST */
            if (f->flags & 0x100) bi.flags |= 0x10;     /* BLEND_ADD */
            if (!(f->flags & 0x80)) bi.flags |= 0x200;  /* BLEND_ALPHA_SRC */
            bi.color = f->color;
        }
        dst.x = rect->x; dst.y = rect->y;
        if (shadow) { bi.flags |= 0x2000; dst.x++; dst.y++; bi.color = G_SHADOW_COLOR; }
        color = bi.color;
        max_y = rect->y;
        x0 = dst.x;
        if (lh <= rect->height || m->h <= rect->height) {
            for (;;) {
                int skip, lw, dx, len, rc;
                len = next_line(f, m, rem, rect->width, &skip, &lw);
                if (len < 0) break;
                dx = (f->flags & 0x10) ? (rect->width - lw) / 2 : 0;
                if (dx < min_dx) min_dx = dx;
                if (lw > max_w) max_w = lw;
                dst.x = x0 + dx;
                rc = draw_line(vb, f, m, rem, len, &bi, shadow, lh, color);
                if (rc) return rc;
                dst.y += lh;
                dst.x = x0;
                max_y += lh;
                rem += len;
                if (!*rem) break;
                if (len == 0 && skip == 0) rem += jp_lead(rem) ? 2 : 1;
                rem += skip;
                if (!*rem) break;
                if (rect->y + rect->height - max_y < lh) break;
            }
        }
        shadow = 0;
    }
    if (dirty) {
        dirty->x = rect->x + min_dx;
        dirty->y = rect->y;
        dirty->width = max_w;
        dirty->height = max_y - rect->y + 1;
        if (f->flags & 8) { dirty->width++; dirty->height++; }
    }
    return 0;
}

/* ------------------------------------------------------------------ books / newspapers
 * written_ui_draw_paragraph (0x56C800) looks for the last space that still fits the page.
 * Japanese has no spaces, so the search loop (0x56C931..0x56C998) is replaced: a page may
 * also break before any Japanese character. Returns the byte count drawn on this page;
 * the next page continues at str + result (after the space, like the original). */
static int __cdecl find_trunc(char *str, TFont *fd, TRect *rect)
{
    int pos = 0, t = 0, len = (int)strlen(str);
    while (pos < len) {
        uint8_t *p = (uint8_t *)str + pos;
        int n = jp_lead(p) ? 2 : 1, cand = -1, cut = pos;
        if (*p == ' ' || *p == '\n') cand = pos + 1;
        else if (n == 2 && pos > 0 && !no_start(p)) {
            /* previous character start */
            int q = 0, prev = 0;
            while (q < pos) { prev = q; q += jp_lead((uint8_t *)str + q) ? 2 : 1; }
            if (!no_end((uint8_t *)str + prev)) cand = pos;
        }
        if (cand >= 0) {
            char save = str[cut];
            str[cut] = 0;
            fd->str = str;
            my_font_measure(fd);
            str[cut] = save;
            if (fd->height > rect->height) break;
            t = cand;
        }
        pos += n;
    }
    return t;
}

__declspec(naked) static void para_stub(void)
{
    __asm {
        push edi                    /* as the original code at 0x56C931 */
        lea eax, [esp + 0x14]       /* TigFont font_desc */
        mov ecx, [esp + 0x50]       /* TigRect *rect */
        push ecx
        push eax
        push ebp                    /* str */
        call find_trunc
        add esp, 12
        mov [esp + 0x4c], eax       /* truncate position */
        mov eax, 0x56c998
        jmp eax
    }
}

void font_init(HMODULE self)
{
    HRSRC r = FindResourceA(self, MAKEINTRESOURCEA(2), "ARCJP");
    DWORD n = 0;
    if (r) {
        void *p = LockResource(LoadResource(self, r));
        DWORD sz = SizeofResource(self, r);
        AddFontMemResourceEx(p, sz, NULL, &n);
    }
    if (!n) lstrcpyW(g_face, L"MS Gothic");
    dlog("font: %d face(s)", (int)n);
    g_gc = (Glyph *)calloc(GC_SIZE, sizeof(Glyph));
    g_dc = CreateCompatibleDC(NULL);
    patch_jmp(A_FONT_MEASURE, my_font_measure);
    patch_jmp(A_FONT_WRITE, my_font_write);
    /* speech bubbles: 200x200 buffer -> 300x300 buffer, text area 200 wide x 300 high
     * (Japanese needs more lines than English; long lines were cut off at the bottom) */
    /* the High Resolution Patch (UAP) widens the bubble to 235/300 for its bigger dialog fonts:
     * keep its width, make the buffer and text area 1.5x as tall (at least 300) */
    {
        uint8_t *p = (uint8_t *)0x4d5c7d;
        int w = *(int *)(p + 1), *rect = (int *)0x5b8eb0;
        if (p[0] == 0xb9 && w >= 200 && w <= 400 && rect[2] == w && rect[3] == w) {
            uint8_t mov_ecx[5] = { 0xb9 };
            int h = w * 3 / 2 < 300 ? 300 : w * 3 / 2;
            memcpy(mov_ecx + 1, &h, 4);
            patch_bytes(0x4d5c7d, mov_ecx, 5);
            patch_bytes(0x5b8eb0 + 12, &h, 4);
            dlog("text bubble %d -> %d", w, h);
        } else dlog("text bubble: unexpected code, not patched");
    }
    if (check_bytes(0x56c931, "57 8bfd 83c9ff 33c0 f2ae")) patch_jmp(0x56c931, para_stub);
    else dlog("paragraph splitter: unexpected code, not patched");
}

/* colour masks are only known once the video mode is up: fetch lazily */
static void masks_init(void)
{
    MasksFn masks = (MasksFn)A_COLOR_MASKS;
    masks(&g_rmask, &g_gmask, &g_bmask);
    g_rsh = mask_shift(g_rmask); g_gsh = mask_shift(g_gmask); g_bsh = mask_shift(g_bmask);
    g_rbits = mask_bits(g_rmask); g_gbits = mask_bits(g_gmask); g_bbits = mask_bits(g_bmask);
    if (g_rbits > 8) g_rbits = 8; if (g_gbits > 8) g_gbits = 8; if (g_bbits > 8) g_bbits = 8;
    dlog("masks %08x %08x %08x", g_rmask, g_gmask, g_bmask);
}
