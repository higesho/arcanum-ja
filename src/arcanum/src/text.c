/* Translation table (resource ARCJP/1) and the hooks that swap English text for Japanese
 * right after the game parses its .mes / .dlg files. */
#include "arcjp.h"
#include <stdio.h>

typedef struct { uint64_t h; uint32_t off; uint32_t pad; } TrEnt;
static const TrEnt *g_ent;
static const char *g_blob;
static int g_count;

uint64_t fnv64(const uint8_t *s, int n)
{
    uint64_t h = 0xcbf29ce484222325ULL;
    int i;
    for (i = 0; i < n; i++) { if (s[i] == 13) continue; h ^= s[i]; h *= 0x100000001b3ULL; }
    return h;
}

const char *tr_lookup(const char *en)
{
    uint64_t h;
    int lo = 0, hi = g_count - 1;
    if (!g_count || !en || !*en) return NULL;
    h = fnv64((const uint8_t *)en, (int)strlen(en));
    while (lo <= hi) {
        int mid = (lo + hi) >> 1;
        if (g_ent[mid].h == h) return g_blob + g_ent[mid].off;
        if (g_ent[mid].h < h) lo = mid + 1; else hi = mid - 1;
    }
    return NULL;
}

/* only player-visible message files are translated (rules\, sound\, art\ ... are data) */
static int mes_path_ok(const char *p)
{
    return (p[0] == 'm' || p[0] == 'M') && (p[1] == 'e' || p[1] == 'E') && (p[2] == 's' || p[2] == 'S')
        && (p[3] == '\\' || p[3] == '/');
}

/* main menu: item n has its hotkey letter in entry n+10 -> show it as "ラベル(S)" */
static const char *with_hotkey(MesFile *m, int i, const char *ja)
{
    int k, want = m->entries[i].num + 10;
    for (k = i + 1; k < m->num && m->entries[k].num <= want; k++) {
        const char *h = m->entries[k].str;
        if (m->entries[k].num == want && h[0] && !h[1] && ((h[0] | 0x20) >= 'a' && (h[0] | 0x20) <= 'z')) {
            size_t n = strlen(ja);
            char *s = (char *)malloc(n + 4);       /* kept for the whole session */
            if (!s) return ja;
            memcpy(s, ja, n);
            s[n] = '('; s[n + 1] = (char)(h[0] & ~0x20); s[n + 2] = ')'; s[n + 3] = 0;
            return s;
        }
    }
    return ja;
}

static void __cdecl my_mes_load_internal(MesFile *m)
{
    AllocFn alloc = (AllocFn)A_MEM_ALLOC;
    FreeFn mfree = (FreeFn)A_MEM_FREE;
    int i, total = 0, hits = 0, broadcast, mainmenu;
    const char **tr;
    char *nd, *p;

    ((void (__cdecl *)(MesFile *))A_MES_LOAD_INT)(m);
    if (g_test) dlog("mesload %s num=%d data=%p", m->path, m->num, m->data);
    if (m->num <= 0 || !m->data || !mes_path_ok(m->path)) return;
    tr = (const char **)malloc(sizeof(char *) * m->num);
    if (!tr) return;
    broadcast = !_stricmp(m->path, "mes\\broadcast.mes");
    mainmenu = !_stricmp(m->path, "mes\\mainmenu.mes");
    for (i = 0; i < m->num; i++) {
        /* follower commands (broadcast.mes < 1000) are typed by the player: keep English */
        tr[i] = (broadcast && m->entries[i].num < 1000) ? NULL : tr_lookup(m->entries[i].str);
        /* options tabs (4000+) have hotkey entries that the game never checks */
        if (tr[i] && mainmenu && m->entries[i].num < 4000) tr[i] = with_hotkey(m, i, tr[i]);
        if (tr[i]) hits++;
        total += (int)strlen(tr[i] ? tr[i] : m->entries[i].str) + 1;
    }
    if (hits) {
        nd = (char *)alloc(total);
        if (nd) {
            p = nd;
            for (i = 0; i < m->num; i++) {
                const char *s = tr[i] ? tr[i] : m->entries[i].str;
                size_t n = strlen(s) + 1;
                memcpy(p, s, n);
                m->entries[i].str = p;
                p += n;
            }
            mfree(m->data);
            m->data = nd;
            m->size = total;
        }
    }
    if (g_test) dlog("mes %s: %d/%d", m->path, hits, m->num);
    free(tr);
}

static int is_code_line(const char *s)
{
    while (*s == ' ' || *s == '\t') s++;
    return ((s[0] | 0x20) >= 'a' && (s[0] | 0x20) <= 'z' && s[1] == ':');
}

static void tr_into(char *buf, int cap)
{
    const char *t;
    size_t n;
    if (!buf || !*buf || is_code_line(buf)) return;
    t = tr_lookup(buf);
    if (!t) return;
    n = strlen(t);
    if ((int)n >= cap) n = cap - 1;
    memcpy(buf, t, n);
    buf[n] = 0;
}

static int __cdecl my_dlg_parse(void *stream, DlgEntry *e, int *line)
{
    int r = ((int (__cdecl *)(void *, DlgEntry *, int *))A_DLG_PARSE)(stream, e, line);
    if (g_test) { static int n; if (++n < 5) dlog("dlg parse %d: r=%d num=%d iq=%d str=%.40s", n, r, e->num, e->iq, e->str); }
    if (!r) return r;
    tr_into(e->str, 1000);
    if (e->iq == 0) tr_into(e->female, 1000);
    return r;
}

/* ---- untranslated text log (test builds only; ARCJP_LOG must be set) ---- */
void log_untranslated(const char *s)
{
    static uint32_t seen[16384];
    static int nseen;
    const uint8_t *p = (const uint8_t *)s;
    int run = 0, letters = 0, i;
    uint32_t h;
    for (; *p; p++) {
        if (*p >= 0xa1) return;     /* already Japanese */
        if ((*p | 0x20) >= 'a' && (*p | 0x20) <= 'z') { if (++run >= 3) letters = 1; }
        else run = 0;
    }
    if (!letters) return;
    h = (uint32_t)fnv64((const uint8_t *)s, (int)strlen(s));
    for (i = 0; i < nseen; i++) if (seen[i] == h) return;
    if (nseen < 16384) seen[nseen++] = h;
    dlog("UNTR|%s", s);
}

void text_init(HMODULE self)
{
    HRSRC r = FindResourceA(self, MAKEINTRESOURCEA(1), "ARCJP");
    const uint8_t *d;
    if (r) {
        d = (const uint8_t *)LockResource(LoadResource(self, r));
        if (d && !memcmp(d, "AJP1", 4)) {
            g_count = *(const int *)(d + 4);
            g_ent = (const TrEnt *)(d + 8);
            g_blob = (const char *)(d + 8 + g_count * sizeof(TrEnt));
        }
    }
    dlog("translations: %d", g_count);
    patch_call(0x4d4304, my_mes_load_internal);
    patch_call(0x417783, my_dlg_parse);
    patch_call(0x417811, my_dlg_parse);
}
