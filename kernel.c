/* =====================================================================
 *  NEXORA OS  v1.0  -  kernel.c
 *  32-bit protected mode, VESA linear framebuffer, no libc.
 *  Flow:  dark-cyan (5s) -> logo fade + loading bar (4s) -> lock screen
 *         -> password -> loading (5s) -> desktop
 * ===================================================================== */
typedef unsigned char  u8;
typedef signed char    s8;
typedef unsigned short u16;
typedef short          s16;
typedef unsigned int   u32;
typedef int            s32;

/* ---------------- settings you can change ---------------- */
#define NEXORA_PASSWORD  "11223344"
#define USER_NAME        "Nexora User"
#define TZ_OFFSET_MIN    0          /* e.g. Pakistan = +300 if the PC clock is in UTC */
#define TEAL_COLOR       0x008B8B   /* dark cyan */

/* ======================================================================
 *  PLATFORM LAYER  (real hardware  /  host simulator for testing)
 * ====================================================================== */
extern const u8 logo_bin[], font0_bin[], font1_bin[], font2_bin[];

static int  W, H;
static u32 *BACK, *WALL, *SCR;

/* ---- mouse state (shared) ---- */
static int mx, my;            /* cursor position            */
static int mbtn;              /* bit0 left, bit1 right, bit2 middle */
static int mclick;            /* set to 1 on a new left click        */
static int mcyc;              /* position inside 3-byte packet       */
static u8  mpk[3];

#ifdef HOST
  extern void host_init(int *w, int *h);
  extern void host_put_row(int y, int x, const u32 *s, int n);
  extern u32  host_ms(void);
  extern void host_advance(u32 ms);
  extern int  host_key(void);
  extern void host_rtc(u8 *s, u8 *m, u8 *h, u8 *d, u8 *mo, u8 *y);
  extern void host_snap(const char *name);
  extern void host_cpu(char *out);
  #include <string.h>
  #define SNAP(n) host_snap(n)
  static void plat_init(void) {
      host_init(&W, &H);
      BACK = (u32 *)__builtin_malloc((unsigned)W * H * 4);
      WALL = (u32 *)__builtin_malloc((unsigned)W * H * 4);
      SCR  = (u32 *)__builtin_malloc((unsigned)W * H * 4);
  }
  static u32  ms_now(void)            { return host_ms(); }
  static void put_row(int y, int x, const u32 *s, int n) { host_put_row(y, x, s, n); }
  static int  plat_key(void)          { return host_key(); }
  static void plat_wait(u32 ms)       { host_advance(ms); }
  static void mouse_reset(void)       { }
#else
  #define SNAP(n) ((void)0)

  struct bootinfo { u32 fb, pitch, width, height, bpp, redpos; };
  static u8 *fb_base;
  static u32 fb_pitch, fb_bpp, fb_swap;
  static u32 cyc_per_ms, tsc_lo0, tsc_hi0;

  void *memcpy(void *d, const void *s, unsigned n) {
      void *r = d; unsigned q = n >> 2;
      __asm__ volatile("rep movsl" : "+D"(d), "+S"(s), "+c"(q) : : "memory");
      n &= 3;
      __asm__ volatile("rep movsb" : "+D"(d), "+S"(s), "+c"(n) : : "memory");
      return r;
  }
  void *memset(void *d, int c, unsigned n) {
      void *r = d;
      __asm__ volatile("rep stosb" : "+D"(d), "+c"(n) : "a"(c) : "memory");
      return r;
  }
  static inline void outb(u16 p, u8 v) { __asm__ volatile("outb %0,%1" : : "a"(v), "Nd"(p)); }
  static inline u8   inb(u16 p)        { u8 v; __asm__ volatile("inb %1,%0" : "=a"(v) : "Nd"(p)); return v; }
  static inline void rdtsc(u32 *lo, u32 *hi) { __asm__ volatile("rdtsc" : "=a"(*lo), "=d"(*hi)); }

  static void time_init(void) {
      u32 l0, h0, l1, h1, guard = 0x20000000;
      outb(0x61, (inb(0x61) & 0xFC) | 0x01);          /* PIT ch2 gate on, speaker off */
      outb(0x43, 0xB0);                               /* ch2, lobyte/hibyte, mode 0   */
      outb(0x42, 59659 & 0xFF); outb(0x42, 59659 >> 8);   /* ~50 ms */
      rdtsc(&l0, &h0);
      while (!(inb(0x61) & 0x20) && --guard) { }
      rdtsc(&l1, &h1);
      cyc_per_ms = (l1 - l0) / 50;
      if (cyc_per_ms < 1000) cyc_per_ms = 1000000;    /* sane fallback */
      tsc_lo0 = l1; tsc_hi0 = h1;
  }
  static u32 ms_now(void) {
      u32 lo, hi, q, r;
      rdtsc(&lo, &hi);
      u32 dlo = lo - tsc_lo0;
      u32 dhi = hi - tsc_hi0 - (lo < tsc_lo0);
      if (dhi >= cyc_per_ms) return 0xFFFFFFFFu;
      __asm__("divl %4" : "=a"(q), "=d"(r) : "a"(dlo), "d"(dhi), "r"(cyc_per_ms));
      return q;
  }
  static void plat_wait(u32 ms) { u32 t = ms_now(); while (ms_now() - t < ms) __asm__ volatile("pause"); }

  /* ---------------- PS/2 mouse (polling, no interrupts needed) ---------------- */
  static u32 mlast;
  static void ps2_wait_w(void) { int g = 100000; while ((inb(0x64) & 2) && --g) { } }
  static int  ps2_wait_r(void) { int g = 100000; while (!(inb(0x64) & 1) && --g) { } return g > 0; }
  static void ps2_cmd(u8 c)    { ps2_wait_w(); outb(0x64, c); }
  static void ps2_data(u8 d)   { ps2_wait_w(); outb(0x60, d); }
  static void mouse_write(u8 v) {
      ps2_cmd(0xD4); ps2_data(v);
      if (ps2_wait_r()) inb(0x60);                    /* ACK (0xFA) */
  }
  static void mouse_reset(void) {                     /* drop old bytes, restart packet sync */
      int g = 64;
      while ((inb(0x64) & 1) && g--) inb(0x60);
      mcyc = 0; mbtn = 0; mclick = 0;
  }
  static void mouse_init(void) {
      u8 cfg; int i;
      ps2_cmd(0xA8);                                  /* enable 2nd PS/2 port (mouse) */
      ps2_cmd(0x20); ps2_wait_r(); cfg = inb(0x60);   /* read controller config */
      cfg &= (u8)~0x20;                               /* mouse clock ON */
      ps2_cmd(0x60); ps2_data(cfg);
      ps2_cmd(0xD4); ps2_data(0xFF);                  /* mouse reset -> FA AA 00 */
      for (i = 0; i < 3; i++) if (ps2_wait_r()) inb(0x60);
      mouse_write(0xF6);                              /* defaults */
      mouse_write(0xF4);                              /* enable data reporting */
      mx = W / 2; my = H / 2; mouse_reset();
  }
  /* called for every byte that comes from the mouse */
  static void mouse_feed(u8 b) {
      u32 t = ms_now();
      if (mcyc && t - mlast > 60) mcyc = 0;           /* lost a byte -> resync */
      mlast = t;
      if (mcyc == 0 && !(b & 8)) return;              /* first byte must have bit3 set */
      mpk[mcyc++] = b;
      if (mcyc < 3) return;
      mcyc = 0;
      if (mpk[0] & 0xC0) return;                      /* overflow -> ignore packet */
      {
          int dx = mpk[1], dy = mpk[2], nb = mpk[0] & 7;
          if (mpk[0] & 0x10) dx -= 256;
          if (mpk[0] & 0x20) dy -= 256;
          mx += dx; my -= dy;
          if (mx < 0) mx = 0;
          if (my < 0) my = 0;
          if (mx > W - 1) mx = W - 1;
          if (my > H - 1) my = H - 1;
          if ((nb & 1) && !(mbtn & 1)) mclick = 1;
          mbtn = nb;
      }
  }

  static void plat_init(void) {
      struct bootinfo *b = (struct bootinfo *)0x5800;
      W = (int)b->width; H = (int)b->height;
      fb_base = (u8 *)b->fb; fb_bpp = b->bpp; fb_pitch = b->pitch;
      if (fb_bpp != 24) fb_bpp = 32;
      if (fb_pitch < (u32)W * (fb_bpp >> 3)) fb_pitch = (u32)W * (fb_bpp >> 3);
      fb_swap = (b->redpos == 0);
      BACK = (u32 *)0x00400000; WALL = (u32 *)0x01000000; SCR = (u32 *)0x02000000;
      time_init();
      mouse_init();
  }
  static void put_row(int y, int x, const u32 *s, int n) {
      u8 *d = fb_base + (u32)y * fb_pitch + (u32)x * (fb_bpp >> 3);
      int i;
      if (fb_bpp == 32 && !fb_swap) {
          __asm__ volatile("rep movsl" : "+D"(d), "+S"(s), "+c"(n) : : "memory");
      } else if (fb_bpp == 32) {
          u32 *dd = (u32 *)d;
          for (i = 0; i < n; i++) { u32 c = s[i]; dd[i] = ((c & 0xFF) << 16) | (c & 0xFF00) | ((c >> 16) & 0xFF); }
      } else {
          for (i = 0; i < n; i++) {
              u32 c = s[i]; u8 r = (u8)(c >> 16), g = (u8)(c >> 8), b = (u8)c;
              if (fb_swap) { d[0] = r; d[1] = g; d[2] = b; } else { d[0] = b; d[1] = g; d[2] = r; }
              d += 3;
          }
      }
  }
  /* ---- keyboard (PS/2 port 0x60, works with USB legacy emulation too) ---- */
  static int plat_key(void) {
      u8 st = inb(0x64);
      if (!(st & 1)) return -1;
      if (st & 0x20) { mouse_feed(inb(0x60)); return -1; }   /* mouse byte */
      return inb(0x60);
  }
#endif

/* ======================================================================
 *  small helpers
 * ====================================================================== */
static u32 isqrt(u32 n) {
    u32 r = 0, b = 1u << 30;
    while (b > n) b >>= 2;
    while (b) { if (n >= r + b) { n -= r + b; r = (r >> 1) + b; } else r >>= 1; b >>= 2; }
    return r;
}
/* sine: angle 0..1023 = full circle, result -1024..1024 (Bhaskara approximation) */
static s32 isin(s32 a) {
    int neg = 0; a &= 1023;
    if (a >= 512) { a -= 512; neg = 1; }
    s32 p = a * (512 - a);
    s32 v = (s32)(((u32)p * 16384u) / (u32)(5 * 512 * 512 - 4 * p));
    return neg ? -v : v;
}
static s32 icos(s32 a) { return isin(a + 256); }

static inline u32 mix(u32 a, u32 b, u32 t) {          /* t = 0..256 */
    u32 rb = (((a & 0xFF00FF) * (256 - t) + (b & 0xFF00FF) * t) >> 8) & 0xFF00FF;
    u32 g  = (((a & 0x00FF00) * (256 - t) + (b & 0x00FF00) * t) >> 8) & 0x00FF00;
    return rb | g;
}
static u32 slen(const char *s) { u32 n = 0; while (s[n]) n++; return n; }
static int u2s(char *o, u32 v, int minw) {            /* unsigned -> text, zero padded */
    char t[12]; int n = 0, i, k = 0;
    do { t[n++] = (char)('0' + v % 10); v /= 10; } while (v);
    while (n < minw) t[n++] = '0';
    for (i = n - 1; i >= 0; i--) o[k++] = t[i];
    o[k] = 0; return k;
}
static void scat(char *d, const char *s) { while (*d) d++; while ((*d++ = *s++)) { } }

/* ======================================================================
 *  drawing primitives (all draw into the current canvas "cv")
 * ====================================================================== */
static u32 *cv;

static inline void px_blend(int x, int y, u32 col, u32 a) {
    if ((unsigned)x >= (unsigned)W || (unsigned)y >= (unsigned)H || !a) return;
    u32 *p = &cv[y * W + x];
    *p = (a >= 256) ? col : mix(*p, col, a);
}
static void fill_rect(int x, int y, int w, int h, u32 c, u32 a) {
    int x1 = x + w, y1 = y + h, i, j;
    if (x < 0) x = 0; if (y < 0) y = 0; if (x1 > W) x1 = W; if (y1 > H) y1 = H;
    for (j = y; j < y1; j++) {
        u32 *p = cv + j * W;
        if (a >= 256) for (i = x; i < x1; i++) p[i] = c;
        else          for (i = x; i < x1; i++) p[i] = mix(p[i], c, a);
    }
}
static inline u32 cov4(s32 dx4, s32 dy4, s32 r4) {    /* 0..256 anti-aliased coverage */
    s32 d = (s32)isqrt((u32)(dx4 * dx4 + dy4 * dy4));
    s32 c = r4 - d + 2;
    if (c <= 0) return 0;
    if (c >= 4) return 256;
    return (u32)c * 64;
}
static void rrect(int x, int y, int w, int h, int r, u32 col, u32 a) {
    int i, j;
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;
    for (j = 0; j < h; j++) {
        int py = y + j, cornerRow = 0; s32 dy4 = 0;
        if ((unsigned)py >= (unsigned)H) continue;
        if (j < r)          { dy4 = (r - j) * 4 - 2;       cornerRow = 1; }
        else if (j >= h - r){ dy4 = (j - (h - r)) * 4 + 2; cornerRow = 1; }
        for (i = 0; i < w; i++) {
            int px = x + i; u32 aa = a;
            if ((unsigned)px >= (unsigned)W) continue;
            if (cornerRow && (i < r || i >= w - r)) {
                s32 dx4 = (i < r) ? (r - i) * 4 - 2 : (i - (w - r)) * 4 + 2;
                u32 cc = cov4(dx4, dy4, r * 4);
                if (!cc) continue;
                aa = (a * cc) >> 8;
            }
            px_blend(px, py, col, aa);
        }
    }
}
/* ring / disc.  ri = 0 -> filled circle.  c2 != c1 gives a diagonal gradient. */
static void disc(int cx, int cy, int ro, int ri, u32 c1, u32 c2, u32 a) {
    int x, y;
    for (y = cy - ro - 1; y <= cy + ro + 1; y++) {
        if ((unsigned)y >= (unsigned)H) continue;
        for (x = cx - ro - 1; x <= cx + ro + 1; x++) {
            if ((unsigned)x >= (unsigned)W) continue;
            s32 dx4 = (x - cx) * 4 + 2, dy4 = (y - cy) * 4 + 2;
            s32 d2 = dx4 * dx4 + dy4 * dy4, o4 = ro * 4;
            u32 cc;
            if (d2 >= (o4 + 3) * (o4 + 3)) continue;
            cc = (d2 <= (o4 - 3) * (o4 - 3)) ? 256 : cov4(dx4, dy4, o4);
            if (ri > 0) {
                s32 i4 = ri * 4; u32 ci;
                if (d2 <= (i4 - 3) * (i4 - 3)) continue;
                ci = (d2 >= (i4 + 3) * (i4 + 3)) ? 0 : cov4(dx4, dy4, i4);
                cc = (cc > ci) ? cc - ci : 0;
            }
            if (!cc) continue;
            u32 col = c1;
            if (c2 != c1) {
                s32 t = ((y - (cy - ro)) * 256 + (x - (cx - ro)) * 128) / (ro * 3 + 1);
                if (t < 0) t = 0; if (t > 256) t = 256;
                col = mix(c1, c2, (u32)t);
            }
            px_blend(x, y, col, (a * cc) >> 8);
        }
    }
}

/* ---------------- text ---------------- */
struct gl { u32 off; u16 w, h; s16 xo, yo; u16 adv, pad; };
#define F_UI    font0_bin    /* 22 px */
#define F_BIG   font1_bin    /* 34 px */
#define F_CLOCK font2_bin    /* 128 px digits */

static int draw_text(const u8 *f, int x, int yb, const char *s, u32 col, u32 a) {
    u32 n = *(const u32 *)f, first = *(const u32 *)(f + 4);
    const struct gl *g = (const struct gl *)(f + 8);
    for (; *s; s++) {
        u32 idx = (u32)(u8)*s - first;
        if (idx >= n) continue;
        const struct gl *q = &g[idx];
        if (q->off) {
            const u8 *bm = f + q->off; int i, j;
            for (j = 0; j < q->h; j++)
                for (i = 0; i < q->w; i++) {
                    u32 al = bm[j * q->w + i];
                    if (!al) continue;
                    al += al >> 7;
                    if (a < 256) al = (al * a) >> 8;
                    px_blend(x + q->xo + i, yb + q->yo + j, col, al);
                }
        }
        x += q->adv;
    }
    return x;
}
static int text_w(const u8 *f, const char *s) {
    u32 n = *(const u32 *)f, first = *(const u32 *)(f + 4);
    const struct gl *g = (const struct gl *)(f + 8); int w = 0;
    for (; *s; s++) { u32 idx = (u32)(u8)*s - first; if (idx < n) w += g[idx].adv; }
    return w;
}
static void text_c(const u8 *f, int cx, int yb, const char *s, u32 col, u32 a) {
    draw_text(f, cx - text_w(f, s) / 2, yb, s, col, a);
}
static void text_sh(const u8 *f, int x, int yb, const char *s, u32 col, u32 a) {
    draw_text(f, x + 1, yb + 2, s, 0x000000, a * 90 / 256);
    draw_text(f, x, yb, s, col, a);
}
static void text_c_sh(const u8 *f, int cx, int yb, const char *s, u32 col, u32 a) {
    text_sh(f, cx - text_w(f, s) / 2, yb, s, col, a);
}

/* ---------------- logo (indexed RGBA, area-averaged scaling) ---------------- */
static void draw_logo(int x, int y, int dw, int dh, u32 a) {
    const u8 *b = logo_bin;
    int sw = *(const u16 *)b, sh = *(const u16 *)(b + 2), i, j, xx, yy;
    const u8 *pal = b + 4, *pix = b + 4 + 1024;
    for (j = 0; j < dh; j++) {
        int sy0 = j * sh / dh, sy1 = (j + 1) * sh / dh; if (sy1 <= sy0) sy1 = sy0 + 1;
        if ((unsigned)(y + j) >= (unsigned)H) continue;
        for (i = 0; i < dw; i++) {
            int sx0 = i * sw / dw, sx1 = (i + 1) * sw / dw; if (sx1 <= sx0) sx1 = sx0 + 1;
            u32 sr = 0, sg = 0, sb = 0, sa = 0, cnt = 0, al, col;
            for (yy = sy0; yy < sy1; yy++)
                for (xx = sx0; xx < sx1; xx++) {
                    const u8 *p = pal + pix[yy * sw + xx] * 4; u32 q = p[3];
                    sr += p[0] * q; sg += p[1] * q; sb += p[2] * q; sa += q; cnt++;
                }
            if (!sa) continue;
            col = ((sr / sa) << 16) | ((sg / sa) << 8) | (sb / sa);
            al = sa / cnt; al += al >> 7; al = (al * a) >> 8;
            px_blend(x + i, y + j, col, al);
        }
    }
}

/* ---------------- present (copy BACK -> screen) ---------------- */
static u32 rowbuf[2600];
static void present(int x, int y, int w, int h) {
    int x1 = x + w, y1 = y + h, r;
    if (x < 0) x = 0; if (y < 0) y = 0; if (x1 > W) x1 = W; if (y1 > H) y1 = H;
    if (x1 <= x || y1 <= y) return;
    for (r = y; r < y1; r++) put_row(r, x, BACK + r * W + x, x1 - x);
}
static void present_mix(const u32 *a, const u32 *b, u32 t) {
    int x, y;
    for (y = 0; y < H; y++) {
        const u32 *pa = a + y * W, *pb = b + y * W;
        for (x = 0; x < W; x++) rowbuf[x] = mix(pa[x], pb[x], t);
        put_row(y, 0, rowbuf, W);
    }
}
static void frame_wait(u32 t0, u32 min_ms) {
    u32 e = ms_now() - t0;
    if (e < min_ms) plat_wait(min_ms - e);
}
static void crossfade(u32 ms) {                        /* SCR (old) -> BACK (new) */
    u32 t0 = ms_now(), e;
    while ((e = ms_now() - t0) < ms) {
        u32 f0 = ms_now();
        present_mix(SCR, BACK, e * 256 / ms);
        frame_wait(f0, 16);
    }
    present(0, 0, W, H);
}
static void restore(int x, int y, int w, int h) {      /* WALL -> BACK region */
    int x1 = x + w, y1 = y + h, j;
    if (x < 0) x = 0; if (y < 0) y = 0; if (x1 > W) x1 = W; if (y1 > H) y1 = H;
    for (j = y; j < y1; j++) memcpy(BACK + j * W + x, WALL + j * W + x, (unsigned)(x1 - x) * 4);
}

/* ---------------- mouse cursor ---------------- */
#define CUR_W 12
#define CUR_H 19
static const char *CUR[CUR_H] = {
    "X           ",
    "XX          ",
    "X.X         ",
    "X..X        ",
    "X...X       ",
    "X....X      ",
    "X.....X     ",
    "X......X    ",
    "X.......X   ",
    "X........X  ",
    "X.....XXXXX ",
    "X..X..X     ",
    "X.X X..X    ",
    "XX  X..X    ",
    "X    X..X   ",
    "     X..X   ",
    "      X..X  ",
    "      X..X  ",
    "       XX   "
};
static int cur_x, cur_y, cur_on;
static u32 cbuf[(CUR_W * 2) * (CUR_H * 2)];
/* draws region (x0,y0,w,h) of BACK to the screen with the cursor on top at (cx,cy) */
static void cur_blit(int x0, int y0, int w, int h, int cx, int cy) {
    int i, j;
    for (j = 0; j < h; j++) {
        int py = y0 + j, xs = x0 < 0 ? -x0 : 0, xe = (x0 + w > W) ? W - x0 : w;
        if (py < 0 || py >= H) continue;
        for (i = xs; i < xe; i++) {
            int ci = x0 + i - cx, cj = py - cy;
            u32 c = BACK[py * W + x0 + i];
            if (ci >= 0 && ci < CUR_W && cj >= 0 && cj < CUR_H) {
                char k = CUR[cj][ci];
                if (k == 'X') c = 0x000000; else if (k == '.') c = 0xFFFFFF;
            }
            cbuf[j * w + i] = c;
        }
        if (xe > xs) put_row(py, x0 + xs, cbuf + j * w + xs, xe - xs);
    }
}
static void cursor_draw(void) {                     /* (re)draw cursor at current mouse position */
    cur_blit(mx, my, CUR_W, CUR_H, mx, my);
    cur_x = mx; cur_y = my; cur_on = 1;
}
static void cursor_update(void) {                   /* call after the mouse moved */
    int dx = mx - cur_x, dy = my - cur_y;
    if (!cur_on) { cursor_draw(); return; }
    if (!dx && !dy) return;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    if (dx < CUR_W && dy < CUR_H) {                 /* close: one combined redraw, no flicker */
        int x0 = cur_x < mx ? cur_x : mx, y0 = cur_y < my ? cur_y : my;
        cur_blit(x0, y0, CUR_W + dx, CUR_H + dy, mx, my);
        cur_x = mx; cur_y = my;
    } else {                                        /* far: erase old, draw new */
        cur_blit(cur_x, cur_y, CUR_W, CUR_H, -1000, -1000);
        cursor_draw();
    }
}

/* ======================================================================
 *  wallpaper (generated - orange dunes + warm sky, like the concept art)
 * ====================================================================== */
static s16 ycurve[5][2048];
static void build_wall(void) {
    static const s16 LB[5] = { 590, 650, 730, 820, 910 };     /* base y   (1/1000 H) */
    static const s16 LA[5] = {  45,  55,  60,  55,  40 };     /* amplitude           */
    static const s16 LF[5] = { 922, 1280, 768, 1434, 1126 };  /* cycles*1024 / width */
    static const s16 LP[5] = { 100, 400, 700, 200, 900 };     /* phase               */
    static const s16 LT[5] = { -60, -40, 20, 60, 30 };        /* tilt                */
    static const u32 LC1[5] = { 0xF5CDA7, 0xF0AA72, 0xE48A4B, 0xC96A33, 0xA04C24 };
    static const u32 LC2[5] = { 0xEBB383, 0xDE8546, 0xC8692F, 0xA64F22, 0x7A3A1A };
    int x, y, i;
    int sx = W * 54 / 100, sy = H * 57 / 100, R = H * 40 / 100, core = H * 35 / 1000;
    s32 R2 = R * R, core2 = core * core, span = H * 22 / 100;
    for (i = 0; i < 5; i++)
        for (x = 0; x < W; x++) {
            s32 ang = LP[i] + (x * LF[i]) / W;
            ycurve[i][x] = (s16)(H * LB[i] / 1000 + (H * LA[i] / 1000) * isin(ang) / 1024
                                 + (H * LT[i] / 1000) * (x - W / 2) / W);
        }
    for (y = 0; y < H; y++) {
        u32 *row = WALL + y * W, skyc;
        s32 t = y * 256 / (H * 62 / 100);
        if (t > 256) t = 256;
        skyc = (t < 128) ? mix(0x3F8E9B, 0x8CC4C2, (u32)t * 2) : mix(0x8CC4C2, 0xF2DDB6, (u32)(t - 128) * 2);
        for (x = 0; x < W; x++) {
            int li = -1; u32 c; s32 dx = x - sx, dy = y - sy, d2 = dx * dx + dy * dy;
            for (i = 4; i >= 0; i--) if (y >= ycurve[i][x]) { li = i; break; }
            if (li < 0) c = skyc;
            else {
                s32 d = y - ycurve[li][x], tt = d * 256 / span;
                s32 st = isin(d * 1024 / 16) * 7 / 1024;     /* contour stripes */
                if (tt > 256) tt = 256;
                c = mix(LC1[li], LC2[li], (u32)tt);
                if (d < 2) c = mix(c, 0xFFE9C8, 60);
                else {
                    s32 r = (s32)((c >> 16) & 255) + st, g = (s32)((c >> 8) & 255) + st, b = (s32)(c & 255) + st;
                    if (r < 0) r = 0; if (r > 255) r = 255; if (g < 0) g = 0; if (g > 255) g = 255;
                    if (b < 0) b = 0; if (b > 255) b = 255;
                    c = ((u32)r << 16) | ((u32)g << 8) | (u32)b;
                }
            }
            if (d2 < R2) {
                u32 g = (u32)((R2 - d2) * 256 / R2), g2 = (g * g) >> 8;
                c = mix(c, 0xFFE9B8, (li < 0) ? g2 * 190 / 256 : g2 * 45 / 256);
                if (li < 0 && d2 < core2) c = mix(c, 0xFFF6DC, 200);
            }
            row[x] = c;
        }
    }
}

/* ======================================================================
 *  RTC clock
 * ====================================================================== */
struct tm_ { int h, m, s, day, mon, year; };
static const char *MONTHS[] = { "January","February","March","April","May","June","July",
                                "August","September","October","November","December" };
static const char *DAYS[]   = { "Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday" };

static int dim(int y, int m) {
    static const u8 d[] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
    return (m == 2 && ((y % 4 == 0 && y % 100) || y % 400 == 0)) ? 29 : d[m - 1];
}
static int dow(int y, int m, int d) {
    static const int t[] = { 0,3,2,5,0,3,5,1,4,6,2,4 };
    if (m < 3) y--;
    return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}
#ifndef HOST
static u8 cmos(u8 r) { outb(0x70, r); return inb(0x71); }
#endif
static void rtc_time(struct tm_ *t) {
    u8 s, m, h, d, mo, y;
#ifdef HOST
    host_rtc(&s, &m, &h, &d, &mo, &y);
#else
    u8 b, s2, m2, h2; int guard = 100000;
    do {
        while ((cmos(0x0A) & 0x80) && --guard) { }
        s = cmos(0); m = cmos(2); h = cmos(4); d = cmos(7); mo = cmos(8); y = cmos(9);
        s2 = cmos(0); m2 = cmos(2); h2 = cmos(4);
    } while ((s != s2 || m != m2 || h != h2) && --guard);
    b = cmos(0x0B);
    if (!(b & 4)) {                                       /* BCD -> binary */
        u8 pm = h & 0x80;
        s = (u8)((s & 15) + (s >> 4) * 10); m = (u8)((m & 15) + (m >> 4) * 10);
        h = (u8)(((h & 0x7F) & 15) + ((h & 0x7F) >> 4) * 10); d = (u8)((d & 15) + (d >> 4) * 10);
        mo = (u8)((mo & 15) + (mo >> 4) * 10); y = (u8)((y & 15) + (y >> 4) * 10);
        if (!(b & 2) && pm) h = (u8)((h % 12) + 12);
    } else if (!(b & 2) && (h & 0x80)) h = (u8)(((h & 0x7F) % 12) + 12);
#endif
    t->s = s; t->year = 2000 + y; t->mon = mo ? mo : 1; t->day = d ? d : 1;
    int mins = h * 60 + m + TZ_OFFSET_MIN;
    while (mins < 0)     { mins += 1440; if (--t->day < 1) { if (--t->mon < 1) { t->mon = 12; t->year--; } t->day = dim(t->year, t->mon); } }
    while (mins >= 1440) { mins -= 1440; if (++t->day > dim(t->year, t->mon)) { t->day = 1; if (++t->mon > 12) { t->mon = 1; t->year++; } } }
    t->h = mins / 60; t->m = mins % 60;
}

/* ======================================================================
 *  UI pieces
 * ====================================================================== */
static void glass(int x, int y, int w, int h, int r) {
    rrect(x, y, w, h, r, 0xFFFFFF, 54);
    rrect(x + 1, y + 1, w - 2, h - 2, r - 1, 0x0B1018, 118);
}
static void time_str(const struct tm_ *t, char *out, char *ampm) {
    int h12 = t->h % 12 ? t->h % 12 : 12;
    out[0] = 0; u2s(out, (u32)h12, 1); scat(out, ":");
    { char mm[4]; u2s(mm, (u32)t->m, 2); scat(out, mm); }
    ampm[0] = t->h >= 12 ? 'P' : 'A'; ampm[1] = 'M'; ampm[2] = 0;
}
static void date_str(const struct tm_ *t, char *out) {
    out[0] = 0; scat(out, DAYS[dow(t->year, t->mon, t->day)]); scat(out, ", ");
    { char n[6]; u2s(n, (u32)t->day, 1); scat(out, n); }
    scat(out, " "); scat(out, MONTHS[t->mon - 1]);
}
#define CLK_PW 500
#define CLK_PH 270
static void clock_rect(int *x, int *y, int *w, int *h) { *x = W - CLK_PW - 56 - 4; *y = 52; *w = CLK_PW + 8; *h = CLK_PH + 8; }
static void clock_panel(void) {
    struct tm_ t; char ts[12], ap[4], ds[40];
    int x = W - CLK_PW - 56, y = 56, base = y + 128, ex;
    rtc_time(&t); time_str(&t, ts, ap); date_str(&t, ds);
    glass(x, y, CLK_PW, CLK_PH, 30);
    ex = draw_text(F_CLOCK, x + 36, base, ts, 0xFFFFFF, 256);
    draw_text(F_BIG, ex + 14, base, ap, 0xFFFFFF, 215);
    draw_text(F_BIG, x + 38, base + 56, ds, 0xFFFFFF, 240);
    draw_text(F_UI, x + 40, base + 100,
              t.h < 12 ? "Good morning" : t.h < 18 ? "Good afternoon" : "Good evening", 0xFFFFFF, 170);
}
static void redraw_clock(void) {
    int x, y, w, h; clock_rect(&x, &y, &w, &h);
    restore(x, y, w, h); clock_panel(); present(x, y, w, h);
}
static void corner_brand(int x, int y) {
    draw_logo(x, y, 42, 49, 256);
    text_sh(F_BIG, x + 56, y + 36, "Nexora OS", 0xFFFFFF, 240);
}

/* ---- keyboard ---- */
static const char SC_NORM[] = "\0\033" "1234567890-=\b\tqwertyuiop[]\n\0asdfghjkl;'`\0\\zxcvbnm,./\0*\0 ";
static int  kb_shift, kb_ext;
/* returns an ASCII char, or 0 for nothing, 10 = Enter, 8 = Backspace, 27 = Esc */
static int key_char(void) {
    int sc = plat_key();
    if (sc < 0) return 0;
    if (sc == 0xE0) { kb_ext = 1; return 0; }
    if (sc == 0x2A || sc == 0x36) { kb_shift = 1; return 0; }
    if (sc == 0xAA || sc == 0xB6) { kb_shift = 0; return 0; }
    if (sc & 0x80) { kb_ext = 0; return 0; }
    {
        int ext = kb_ext; kb_ext = 0;
        if (!ext) {
            switch (sc) {
                case 0x47: return '7'; case 0x48: return '8'; case 0x49: return '9';
                case 0x4B: return '4'; case 0x4C: return '5'; case 0x4D: return '6';
                case 0x4F: return '1'; case 0x50: return '2'; case 0x51: return '3';
                case 0x52: return '0';
            }
        }
        if (sc < (int)sizeof(SC_NORM) - 1) {
            int c = SC_NORM[sc];
            if (kb_shift && c >= 'a' && c <= 'z') c -= 32;
            return c;
        }
    }
    return 0;
}

/* ======================================================================
 *  SCENE 1 : boot (teal, logo fade, loading bar)
 * ====================================================================== */
static void splash_bg_rect(int x, int y, int w, int h) {
    int j;
    for (j = y; j < y + h; j++) {
        if ((unsigned)j >= (unsigned)H) continue;
        fill_rect(x, j, w, 1, mix(0x1B1D23, 0x0D0E12, (u32)(j * 256 / H)), 256);
    }
}
static void boot_sequence(void) {
    int dh = H * 46 / 100, dw, top, bar_w, bar_x, bar_y, lx, ly;
    u32 t0, e;
    if (dh > 467) dh = 467;
    dw = 400 * dh / 467;
    top = (H - (dh + 170)) / 2; if (top < 10) top = 10;
    lx = (W - dw) / 2; ly = top;
    bar_w = 480; if (bar_w > W - 100) bar_w = W - 100;
    bar_x = (W - bar_w) / 2; bar_y = top + dh + 56;

    /* 1) dark cyan screen for 5 seconds (wallpaper is built meanwhile) */
    cv = BACK; fill_rect(0, 0, W, H, TEAL_COLOR, 256); present(0, 0, W, H);
    t0 = ms_now(); SNAP("01_teal");
    cv = WALL; build_wall(); cv = BACK;
    while (ms_now() - t0 < 5000) plat_wait(20);

    /* 2) crossfade to dark splash background */
    memcpy(SCR, BACK, (unsigned)W * H * 4);
    splash_bg_rect(0, 0, W, H);
    SNAP("02_before_fade");
    crossfade(700);

    /* 3) logo fades in smoothly */
    t0 = ms_now();
    while ((e = ms_now() - t0) < 1200) {
        u32 f0 = ms_now(), a = e * 256 / 1200;
        a = (a * a) >> 8;                                    /* ease-in */
        splash_bg_rect(lx - 4, ly - 4, dw + 8, dh + 8);
        draw_logo(lx, ly, dw, dh, a);
        present(lx - 4, ly - 4, dw + 8, dh + 8);
        if (e > 600) SNAP("03_logo_fade");
        frame_wait(f0, 16);
    }
    splash_bg_rect(lx - 4, ly - 4, dw + 8, dh + 8);
    draw_logo(lx, ly, dw, dh, 256); present(lx - 4, ly - 4, dw + 8, dh + 8);

    /* 4) loading bar 0 -> 100 % over 4 seconds */
    t0 = ms_now();
    for (;;) {
        u32 f0 = ms_now(), p;
        char pct[8], msg[40];
        e = f0 - t0; if (e > 4000) e = 4000;
        p = e * 1000 / 4000;                                 /* 0..1000 */
        splash_bg_rect(bar_x - 10, bar_y - 20, bar_w + 20, 120);
        rrect(bar_x, bar_y, bar_w, 8, 4, 0x2B2F38, 256);
        if (p > 0) {
            int fw = (int)((u32)bar_w * p / 1000);
            if (fw < 8) fw = 8;
            rrect(bar_x, bar_y, fw, 8, 4, 0x4C8DFF, 256);
            rrect(bar_x, bar_y, fw, 4, 2, 0xFFFFFF, 46);
        }
        u2s(pct, p / 10, 1); scat(pct, "%");
        text_c(F_BIG, W / 2, bar_y + 58, pct, 0xFFFFFF, 235);
        text_c(F_UI, W / 2, bar_y + 92,
               p < 250 ? "Initializing kernel..." : p < 500 ? "Loading drivers..." :
               p < 750 ? "Preparing display..." : "Starting Nexora OS...", 0xB8BCC8, 230);
        (void)msg;
        present(bar_x - 10, bar_y - 20, bar_w + 20, 120);
        if (p >= 500) SNAP("04_progress");
        if (e >= 4000) break;
        frame_wait(f0, 16);
    }
    plat_wait(400);
}

/* ======================================================================
 *  SCENE 2 : lock screen
 * ====================================================================== */
static void draw_login(int shake, int npw, int msg) {
    int cx = W / 2, ty = H / 2 - 120, ay = ty + 62, fx, fy, fw = 400, i;
    restore(cx - 330, ty - 12, 660, 450);
    /* avatar */
    disc(cx, ay, 66, 62, 0xFFFFFF, 0xFFFFFF, 120);
    disc(cx, ay, 58, 0, 0x4C8DFF, 0x35D0FF, 256);
    disc(cx, ay - 12, 15, 0, 0xFFFFFF, 0xFFFFFF, 245);
    rrect(cx - 32, ay + 12, 64, 36, 18, 0xFFFFFF, 245);
    text_c_sh(F_BIG, cx, ay + 58 + 52, USER_NAME, 0xFFFFFF, 256);
    /* password field */
    fx = cx - fw / 2 + shake; fy = ty + 232;
    rrect(fx, fy, fw, 58, 29, 0xFFFFFF, 70);
    rrect(fx + 1, fy + 1, fw - 2, 56, 28, 0x0B1018, 130);
    if (npw == 0) draw_text(F_UI, fx + 30, fy + 37, "Password", 0xFFFFFF, 150);
    for (i = 0; i < npw; i++) disc(fx + 34 + i * 22, fy + 29, 6, 0, 0xFFFFFF, 0xFFFFFF, 256);
    disc(fx + fw - 31, fy + 29, 23, 0, 0x4C8DFF, 0x3466D6, 256);
    text_c(F_BIG, fx + fw - 31, fy + 29 + 12, ">", 0xFFFFFF, 256);
    /* message line */
    if (msg == 1 || msg == 2) {
        const char *m = (msg == 1) ? "Incorrect password. Try again." : "Welcome!";
        int pw = text_w(F_UI, m) + 44;
        rrect(cx - pw / 2, fy + 76, pw, 40, 20, msg == 1 ? 0xB3261E : 0x1E8E3E, 235);
        text_c(F_UI, cx, fy + 103, m, 0xFFFFFF, 256);
    } else {
        text_c_sh(F_UI, cx, fy + 100, "Type your password and press Enter", 0xFFFFFF, 200);
    }
}
static void compose_lock(void) {
    memcpy(BACK, WALL, (unsigned)W * H * 4);
    cv = BACK;
    clock_panel();
    corner_brand(44, H - 100);
    text_sh(F_UI, W - 190, H - 48, "Nexora OS  v1.0", 0xFFFFFF, 220);
    draw_login(0, 0, 0);
}
static void lock_screen(void) {
    char pw[24]; int n = 0, k, lastm = -1; u32 tc = 0;
    memcpy(SCR, BACK, (unsigned)W * H * 4);
    compose_lock();
    crossfade(800);
    SNAP("05_lock_ready");
    while (plat_key() >= 0) { }                             /* flush old key presses */
    kb_shift = 0; kb_ext = 0;
    for (;;) {
        int c = key_char();
        if (c == 8 && n > 0) { n--; draw_login(0, n, 0); present(W / 2 - 330, H / 2 - 132, 660, 450); }
        else if (c == 27)    { n = 0; draw_login(0, n, 0); present(W / 2 - 330, H / 2 - 132, 660, 450); }
        else if (c >= 32 && c < 127 && n < 20) {
            pw[n++] = (char)c; draw_login(0, n, 0); present(W / 2 - 330, H / 2 - 132, 660, 450);
        } else if (c == 10) {
            pw[n] = 0;
            { const char *p = NEXORA_PASSWORD; int ok = (n == (int)slen(p)); for (k = 0; ok && k < n; k++) if (pw[k] != p[k]) ok = 0;
              if (ok) {
                  draw_login(0, n, 2); present(W / 2 - 330, H / 2 - 132, 660, 450);
                  SNAP("06_lock_ok"); plat_wait(450); return;
              } }
            { static const s8 sh[] = { -18, 18, -14, 14, -9, 9, -4, 4, 0 };
              for (k = 0; k < 9; k++) { u32 f0 = ms_now(); draw_login(sh[k], n, 1); present(W / 2 - 330, H / 2 - 132, 660, 450); frame_wait(f0, 32); } }
            n = 0; draw_login(0, 0, 1); present(W / 2 - 330, H / 2 - 132, 660, 450);
            SNAP("06_lock_wrong");
        }
        if (ms_now() - tc > 400) {                          /* keep the clock correct */
            struct tm_ t; tc = ms_now(); rtc_time(&t);
            if (lastm >= 0 && t.m != lastm) { redraw_clock(); }
            lastm = t.m;
        }
        plat_wait(8);
    }
}

/* ======================================================================
 *  SCENE 3 : loading after login (5 s)
 * ====================================================================== */
static void loading_screen(void) {
    int cx = W / 2, cy = H / 2, i, rx = cx - 260, ry = cy - 210, rw = 520, rh = 420;
    u32 t0, e;
    memcpy(SCR, BACK, (unsigned)W * H * 4);
    memcpy(BACK, WALL, (unsigned)W * H * 4);
    cv = BACK; fill_rect(0, 0, W, H, 0x05080E, 150);
    crossfade(500);
    t0 = ms_now();
    while ((e = ms_now() - t0) < 5000) {
        u32 f0 = ms_now(); int head = (int)(e / 90) % 12;
        restore(rx, ry, rw, rh); fill_rect(rx, ry, rw, rh, 0x05080E, 150);
        disc(cx, cy - 110, 46, 42, 0xFFFFFF, 0xFFFFFF, 110);
        disc(cx, cy - 110, 38, 0, 0x4C8DFF, 0x35D0FF, 256);
        disc(cx, cy - 120, 11, 0, 0xFFFFFF, 0xFFFFFF, 245);
        rrect(cx - 24, cy - 104, 48, 26, 13, 0xFFFFFF, 245);
        text_c_sh(F_BIG, cx, cy - 20, "Welcome, " USER_NAME, 0xFFFFFF, 256);
        for (i = 0; i < 12; i++) {
            int back = (head - i + 12) % 12; u32 al = 256 - (u32)back * 20; if (al < 40) al = 40;
            disc(cx + icos(i * 85) * 30 / 1024, cy + 62 + isin(i * 85) * 30 / 1024, 5, 0, 0xFFFFFF, 0xFFFFFF, al);
        }
        text_c(F_UI, cx, cy + 140, "Loading your desktop...", 0xFFFFFF, 210);
        present(rx, ry, rw, rh);
        if (e > 2500) SNAP("07_loading");
        frame_wait(f0, 40);
    }
}

/* ======================================================================
 *  SCENE 4 : desktop
 * ====================================================================== */
static void cpu_brand(char *out) {
#ifdef HOST
    host_cpu(out);
#else
    u32 a, b, c, d; int i, k = 0; char tmp[49];
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(0x80000000), "c"(0));
    if (a >= 0x80000004) {
        for (i = 0; i < 3; i++) {
            __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(0x80000002 + i), "c"(0));
            memcpy(tmp + i * 16, &a, 4); memcpy(tmp + i * 16 + 4, &b, 4);
            memcpy(tmp + i * 16 + 8, &c, 4); memcpy(tmp + i * 16 + 12, &d, 4);
        }
        tmp[48] = 0; i = 0; while (tmp[i] == ' ') i++;
        while (tmp[i]) out[k++] = tmp[i++];
        out[k] = 0;
    } else {
        __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(0), "c"(0));
        memcpy(out, &b, 4); memcpy(out + 4, &d, 4); memcpy(out + 8, &c, 4); out[12] = 0;
    }
#endif
}
static void text_fit(const u8 *f, int x, int yb, const char *s, int maxw, u32 col, u32 a) {
    char buf[64]; int n = 0;
    while (s[n] && n < 60) { buf[n] = s[n]; buf[n + 1] = 0; if (text_w(f, buf) > maxw) { buf[n] = 0; if (n > 3) { buf[n - 1] = buf[n - 2] = buf[n - 3] = '.'; } break; } n++; }
    draw_text(f, x, yb, buf, col, a);
}
static void tray_rect(int *x, int *y, int *w, int *h) { *x = W - 270; *y = H - 104; *w = 230; *h = 70; }
static void tray(void) {
    struct tm_ t; char ts[12], ap[4], full[20]; int x, y, w, h;
    tray_rect(&x, &y, &w, &h); rtc_time(&t); time_str(&t, ts, ap);
    full[0] = 0; scat(full, ts); scat(full, " "); scat(full, ap);
    glass(x + 4, y + 4, w - 8, h - 8, 24);
    text_c(F_BIG, x + w / 2, y + 46, full, 0xFFFFFF, 245);
}
static void redraw_tray(void) { int x, y, w, h; tray_rect(&x, &y, &w, &h); restore(x, y, w, h); tray(); present(x, y, w, h); }

static void compose_desktop(void) {
    static const u32 IC1[5] = { 0xFF7AA8, 0x35D0FF, 0x3A3F4A, 0xFFC371, 0x7CF29C };
    static const u32 IC2[5] = { 0x7A5CFF, 0x3D7BE0, 0x0E1014, 0xFF5F6D, 0x1FA2A0 };
    static const char *LAB[5] = { "Files", "Web", "Terminal", "Settings", "About" };
    static const char *GLY[5] = { "F", "W", ">_", "S", "i" };
    int i, dockw = 5 * 118 + 40, dx = (W - dockw) / 2, dy = H - 170, wx = 110, wy = 150, ww = 640, wh = 380;
    char buf[64], cpu[64];
    memcpy(BACK, WALL, (unsigned)W * H * 4);
    cv = BACK;
    clock_panel();
    corner_brand(44, 34);
    tray();
    /* System Info window */
    glass(wx, wy, ww, wh, 22);
    fill_rect(wx + 14, wy + 47, ww - 28, 1, 0xFFFFFF, 50);
    disc(wx + 30, wy + 24, 8, 0, 0xFF5F57, 0xFF5F57, 256);
    disc(wx + 56, wy + 24, 8, 0, 0xFEBC2E, 0xFEBC2E, 256);
    disc(wx + 82, wy + 24, 8, 0, 0x28C840, 0x28C840, 256);
    text_c(F_UI, wx + ww / 2, wy + 31, "System Info", 0xFFFFFF, 230);
    text_sh(F_BIG, wx + 36, wy + 108, "Nexora OS  v1.0", 0xFFFFFF, 256);
    draw_text(F_UI, wx + 36, wy + 168, "Status", 0xC9CED8, 230);  draw_text(F_UI, wx + 170, wy + 168, "Online", 0x7CF29C, 256);
    buf[0] = 0; { char n[8]; u2s(n, (u32)W, 1); scat(buf, n); scat(buf, " x "); u2s(n, (u32)H, 1); scat(buf, n); scat(buf, "  (32-bit colour)"); }
    draw_text(F_UI, wx + 36, wy + 212, "Display", 0xC9CED8, 230); draw_text(F_UI, wx + 170, wy + 212, buf, 0xFFFFFF, 245);
    cpu_brand(cpu);
    draw_text(F_UI, wx + 36, wy + 256, "CPU", 0xC9CED8, 230);     text_fit(F_UI, wx + 170, wy + 256, cpu, ww - 170 - 30, 0xFFFFFF, 245);
    draw_text(F_UI, wx + 36, wy + 300, "Boot", 0xC9CED8, 230);    draw_text(F_UI, wx + 170, wy + 300, "BIOS (Legacy), 32-bit protected mode", 0xFFFFFF, 245);
    draw_text(F_UI, wx + 36, wy + 344, "Hint", 0xC9CED8, 230);    draw_text(F_UI, wx + 170, wy + 344, "Press L to lock the screen", 0xFFFFFF, 245);
    /* dock */
    glass(dx, dy, dockw, 130, 44);
    for (i = 0; i < 5; i++) {
        int icx = dx + 20 + 59 + i * 118, icy = dy + 50;
        disc(icx, icy, 38, 0, IC1[i], IC2[i], 256);
        disc(icx, icy, 38, 35, 0xFFFFFF, 0xFFFFFF, 70);
        text_c(F_BIG, icx, icy + 12, GLY[i], i == 2 ? 0x7CF29C : 0xFFFFFF, 256);
        text_c(F_UI, icx, dy + 112, LAB[i], 0xFFFFFF, 225);
    }
}
static int open_app = -1;
static const char *APP_NAME[5] = { "Files", "Web", "Terminal", "Settings", "About" };
static void app_rect(int *x, int *y, int *w, int *h) { *w = 600; *h = 420; *x = (W - *w) / 2 + 120; *y = 170; }
static void draw_app_window(int i) {
    int x, y, w, h; char t[40];
    app_rect(&x, &y, &w, &h);
    glass(x, y, w, h, 22);
    fill_rect(x + 14, y + 47, w - 28, 1, 0xFFFFFF, 50);
    disc(x + 30, y + 24, 8, 0, 0xFF5F57, 0xFF5F57, 256);          /* click = close */
    disc(x + 56, y + 24, 8, 0, 0xFEBC2E, 0xFEBC2E, 256);
    disc(x + 82, y + 24, 8, 0, 0x28C840, 0x28C840, 256);
    text_c(F_UI, x + w / 2, y + 31, APP_NAME[i], 0xFFFFFF, 230);
    t[0] = 0; scat(t, APP_NAME[i]);
    text_c(F_BIG, x + w / 2, y + h / 2, t, 0xFFFFFF, 256);
    text_c(F_UI, x + w / 2, y + h / 2 + 50, "This app is coming soon", 0xC9CED8, 230);
}
/* returns dock icon index under (px,py), or -1 */
static int dock_hit(int px, int py) {
    int i, dockw = 5 * 118 + 40, dx = (W - dockw) / 2, dy = H - 170;
    for (i = 0; i < 5; i++) {
        int icx = dx + 20 + 59 + i * 118, icy = dy + 50;
        int ax = px - icx, ay = py - icy;
        if (ax * ax + ay * ay <= 38 * 38) return i;
    }
    return -1;
}
static void desktop_click(int px, int py) {
    int x, y, w, h, hit;
    if (open_app >= 0) {
        int cx, cy;
        app_rect(&x, &y, &w, &h); cx = px - (x + 30); cy = py - (y + 24);
        if (cx * cx + cy * cy <= 12 * 12) {                        /* red button: close */
            open_app = -1; compose_desktop(); present(0, 0, W, H); cursor_draw(); return;
        }
    }
    hit = dock_hit(px, py);
    if (hit >= 0) {
        open_app = hit; compose_desktop(); draw_app_window(hit);
        present(0, 0, W, H); cursor_draw();
    }
}
static void desktop_screen(void) {
    u32 tc = 0; int lastm = -1, shots = 0; u32 t0;
    memcpy(SCR, BACK, (unsigned)W * H * 4);
    compose_desktop();
    crossfade(700);
    t0 = ms_now();
    kb_shift = 0; kb_ext = 0;
    open_app = -1; cur_on = 0; mouse_reset();
    cursor_draw();
    for (;;) {
        int c = key_char();
        if (c == 'l' || c == 'L') return;                   /* lock again */
        if (mx != cur_x || my != cur_y) cursor_update();
        if (mclick) { mclick = 0; desktop_click(mx, my); }
        if (ms_now() - tc > 400) {
            struct tm_ t; tc = ms_now(); rtc_time(&t);
            if (lastm >= 0 && t.m != lastm) { redraw_clock(); redraw_tray(); cursor_draw(); }
            lastm = t.m;
        }
        if (!shots && ms_now() - t0 > 800) { SNAP("08_desktop"); shots = 1; }
        plat_wait(1);                                       /* 1 ms: fast enough for mouse packets */
    }
}

/* ======================================================================
 *  entry point (called from kernel_entry.S)
 * ====================================================================== */
void kmain(void) {
    plat_init();
    boot_sequence();
    for (;;) {
        lock_screen();
        loading_screen();
        desktop_screen();
    }
}
