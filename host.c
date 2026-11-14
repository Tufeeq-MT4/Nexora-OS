#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef unsigned char u8; typedef unsigned int u32;
#define HW 1920
#define HH 1080
static u32 *fb; static u32 vms = 0, snapn = 0;
void host_init(int *w, int *h) { *w = HW; *h = HH; fb = calloc(HW * HH, 4); }
void host_put_row(int y, int x, const u32 *s, int n) { memcpy(fb + y * HW + x, s, (size_t)n * 4); }
u32 host_ms(void) { return vms; }
void host_advance(u32 ms) { vms += ms; }
void host_rtc(u8 *s, u8 *m, u8 *h, u8 *d, u8 *mo, u8 *y) { *s = 30; *m = 4; *h = 12; *d = 12; *mo = 11; *y = 26; }
void host_cpu(char *o) { strcpy(o, "Intel(R) Core(TM) i5-8250U CPU @ 1.60GHz"); }
/* ---- scripted input ---- */
static const char SC[] = "\0\033" "1234567890-=\b\tqwertyuiop[]\n\0asdfghjkl;'`\0\\zxcvbnm,./\0*\0 ";
typedef struct { u32 t; char type; int a, b; } Ev;
#include "script.h"
static int ei = 0, pend_break = -1, mdirty = 0, mxx = 960, myy = 540, mb = 0;
static int keyq[256], kq_h = 0, kq_t = 0;
static void feed(void) {
    while (ei < (int)(sizeof(EV) / sizeof(EV[0])) && EV[ei].t <= vms) {
        Ev *e = &EV[ei++];
        if (e->type == 'k') {
            int sc = -1, i; char c = (char)e->a;
            if (c == '\n') sc = 0x1C; else if (c == 27) sc = 0x01; else if (c == 8) sc = 0x0E;
            else for (i = 2; i < (int)sizeof(SC) - 1; i++) if (SC[i] == c) { sc = i; break; }
            if (sc >= 0) { keyq[kq_t++ & 255] = sc; keyq[kq_t++ & 255] = sc | 0x80; }
        } else if (e->type == 'm') { mxx = e->a; myy = e->b; mdirty = 1; }
        else if (e->type == 'd') { mb |= 1; mdirty = 1; }
        else if (e->type == 'u') { mb &= ~1; mdirty = 1; }
    }
}
int host_key(void) { feed(); if (kq_h != kq_t) return keyq[kq_h++ & 255]; return -1; }
int host_mouse(int *x, int *y, int *b) { feed(); if (!mdirty) return 0; mdirty = 0; *x = mxx; *y = myy; *b = mb; return 1; }
void host_snap(const char *name) {
    const char *want = getenv("SNAPS"); char key[80];
    snprintf(key, sizeof key, ",%s,", name);
    if (want) { char w[2048]; snprintf(w, sizeof w, ",%s,", want); if (!strstr(w, key)) return; }
    char fn[160]; snprintf(fn, sizeof fn, "shots/%02u_%s.ppm", snapn++, name);
    FILE *f = fopen(fn, "wb"); if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", HW, HH);
    for (int i = 0; i < HW * HH; i++) { u32 c = fb[i]; u8 p[3] = { (u8)(c >> 16), (u8)(c >> 8), (u8)c }; fwrite(p, 1, 3, f); }
    fclose(f);
}
extern void kmain(void);
int main(void) { kmain(); return 0; }
