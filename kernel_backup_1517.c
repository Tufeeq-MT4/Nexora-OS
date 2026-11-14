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
#define TEAL_COLOR       0x00363D   /* aqua dark */

/* ======================================================================
 *  PLATFORM LAYER  (real hardware  /  host simulator for testing)
 * ====================================================================== */
extern const u8 logo_bin[], font0_bin[], font1_bin[], font2_bin[];

static int  W, H;
static u32 *BACK, *WALL, *SCR, *WALL_D, *WALL_N;   /* WALL = wallpaper in use, _D desert, _N night city */
static u32 gl_fill = 118, gl_edge = 54;            /* glass transparency (lower = more transparent) */
static int wp_choice;                              /* 0 night, 1 desert, 2.. photo wallpapers */
static int nwp;                                    /* photo wallpapers found on the disk (walls.bin) */
static u32 *WALL_P, *THUMBS;                       /* WALL_P = photo wallpaper, THUMBS = settings previews */
static int pf_ok, pf_vis;                          /* hardware page flip (QEMU std VGA) */
static u32 *TMP;                                   /* off-screen buffer for smooth transitions */
static int glass_lvl = 2, show_w = 1, clock24;      /* taskbar controls */
static const u8 GLA[5] = { 90, 120, 150, 190, 225 };  /* glass opacity levels */
static int mspeed = 2;                             /* mouse speed level 0..4 */
static const u8 MSP[5] = { 2, 3, 4, 6, 8 };        /* speed multiplier x2: 1x 1.5x 2x 3x 4x */

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
      WALL_D = (u32 *)__builtin_malloc((unsigned)W * H * 4);
      WALL_N = (u32 *)__builtin_malloc((unsigned)W * H * 4);
      SCR  = (u32 *)__builtin_malloc((unsigned)W * H * 4);
      TMP = (u32 *)__builtin_malloc((unsigned)W * H * 4);
      WALL_P = (u32 *)__builtin_malloc((unsigned)W * H * 4);
      THUMBS = (u32 *)__builtin_malloc(9 * 128 * 72 * 4);
      WALL = WALL_D; mx = W / 2; my = H / 2;
  }
  static u32  ms_now(void)            { return host_ms(); }
  static void put_row(int y, int x, const u32 *s, int n) { host_put_row(y, x, s, n); }
  extern int host_mouse(int *x, int *y, int *b);
  static int  plat_key(void) {
      int x, y, b;
      if (host_mouse(&x, &y, &b)) { mx = x; my = y; if ((b & 1) && !(mbtn & 1)) mclick = 1; mbtn = b; }
      return host_key();
  }
  static void plat_wait(u32 ms)       { host_advance(ms); }
  static void mouse_reset(void)       { }
  static void mouse_drain(void)       { }
  extern int host_ata(unsigned lba, unsigned n, unsigned char *dst);
  static int  ata_read(u32 lba, u32 n, u8 *dst) { return host_ata(lba, n, dst); }
  extern int host_ata_write(unsigned lba, const unsigned char *src);
  static int  ata_write(u32 lba, const u8 *src) { return host_ata_write(lba, src); }
  static void plat_blit_page(const u32 *s, int p) { (void)s; (void)p; }
  static void plat_flip(int p)        { (void)p; }
  static u32 *plat_page(int p)        { (void)p; return 0; }
#else
  #define SNAP(n) ((void)0)

  struct bootinfo { u32 fb, pitch, width, height, bpp, redpos; };
  static u8 *fb_base, *fb_wr;                         /* fb_wr = page that put_row() writes to */
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
  static void ps2_wait_w(void) { int g = 100000; while ((inb(0x64) & 2) && --g) { } }
  static int  ps2_wait_r(void) { int g = 100000; while (!(inb(0x64) & 1) && --g) { } return g > 0; }
  static void ps2_cmd(u8 c)    { ps2_wait_w(); outb(0x64, c); }
  static void ps2_data(u8 d)   { ps2_wait_w(); outb(0x60, d); }
  static void mouse_write(u8 v) {
      ps2_cmd(0xD4); ps2_data(v);
      if (ps2_wait_r()) inb(0x60);                    /* ACK (0xFA) */
  }
  static int mrx, mry;                                /* sub-pixel remainders (smooth 1.5x speed) */
  static void mouse_reset(void) {                     /* drop old bytes, restart packet sync */
      int g = 600;
      while ((inb(0x64) & 1) && g--) inb(0x60);
      mcyc = 0; mbtn = 0; mclick = 0; mrx = mry = 0;
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
      if (mcyc == 0 && !(b & 8)) return;              /* first byte must have bit3 set (sync) */              /* first byte must have bit3 set */
      mpk[mcyc++] = b;
      if (mcyc < 3) return;
      mcyc = 0;
      if (mpk[0] & 0xC0) return;                      /* overflow -> ignore packet */
      {
          int dx = mpk[1], dy = mpk[2], nb = mpk[0] & 7;
          if (mpk[0] & 0x10) dx -= 256;
          if (mpk[0] & 0x20) dy -= 256;
          {   /* speed setting + acceleration curve (like macOS / MateBook) */
              int sp = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy), g = 8 + (sp > 3 ? (sp - 3) * 2 : 0);
              if (g > 16) g = 16;
              mrx += dx * MSP[mspeed] * g; mry += dy * MSP[mspeed] * g;
          }
          mx += mrx / 16; my -= mry / 16; mrx %= 16; mry %= 16;
          if (mx < 0) mx = 0;
          if (my < 0) my = 0;
          if (mx > W - 1) mx = W - 1;
          if (my > H - 1) my = H - 1;
          if ((nb & 1) && !(mbtn & 1)) mclick = 1;
          mbtn = nb;
      }
  }

  /* read EVERY waiting mouse byte at once (the 3-byte packets used to pile up while the screen was redrawing) */
  static void mouse_drain(void) {
      int g = 200;
      while (g--) { u8 st = inb(0x64); if (!(st & 1) || !(st & 0x20)) break; mouse_feed(inb(0x60)); }
  }

  /* ---- hardware double buffering (Bochs/QEMU VBE): two pages, flip = tear-free ---- */
  static inline void outw(u16 p, u16 v) { __asm__ volatile("outw %0,%1" : : "a"(v), "Nd"(p)); }
  static inline u16  inw(u16 p)         { u16 v; __asm__ volatile("inw %1,%0" : "=a"(v) : "Nd"(p)); return v; }
  static void dispi_w(u16 i, u16 v) { outw(0x1CE, i); outw(0x1CF, v); }
  static u16  dispi_r(u16 i)        { outw(0x1CE, i); return inw(0x1CF); }
  static void pf_init(void) {
      u16 id = dispi_r(0);
      pf_ok = 0; pf_vis = 0;
      if (id >= 0xB0C4 && id <= 0xB0CF && dispi_r(7) >= (u16)(2 * H) && dispi_r(6) == (u16)W && fb_bpp == 32 && fb_pitch == (u32)W * 4) {
          dispi_w(9, 0); pf_ok = 1;                      /* page 0 visible */
      }
  }
  static void put_row(int y, int x, const u32 *s, int n);
  static void plat_blit_page(const u32 *src, int page) {
      u8 *sv = fb_wr; int y;
      fb_wr = fb_base + (u32)page * (u32)H * fb_pitch;
      for (y = 0; y < H; y++) put_row(y, 0, src + (u32)y * (u32)W, W);
      fb_wr = sv;
  }
  static void plat_flip(int page) { dispi_w(9, page ? (u16)H : 0); pf_vis = page; }
  static u32 *plat_page(int page) { return (fb_bpp == 32 && !fb_swap) ? (u32 *)(fb_base + (u32)page * (u32)H * fb_pitch) : 0; }

  /* ---- ATA PIO (primary master) : used to read the wallpaper pack from the disk image ---- */
  static int ata_read(u32 lba, u32 n, u8 *dst) {
      while (n) {
          u32 c = n > 128 ? 128 : n, k; int g;
          g = 1000000; while ((inb(0x1F7) & 0x80) && --g) { }
          if (!g || inb(0x1F7) == 0xFF) return -1;
          outb(0x1F6, (u8)(0xE0 | ((lba >> 24) & 0x0F)));
          outb(0x1F2, (u8)c); outb(0x1F3, (u8)lba); outb(0x1F4, (u8)(lba >> 8)); outb(0x1F5, (u8)(lba >> 16));
          outb(0x1F7, 0x20);
          for (k = 0; k < c; k++) {
              u32 words = 256;
              inb(0x3F6); inb(0x3F6); inb(0x3F6); inb(0x3F6);
              g = 1000000; while ((inb(0x1F7) & 0x80) && --g) { }
              if (!g) return -1;
              if (inb(0x1F7) & 0x21) return -1;
              if (!(inb(0x1F7) & 0x08)) return -1;
              __asm__ volatile("rep insw" : "+D"(dst), "+c"(words) : "d"((u16)0x1F0) : "memory");
          }
          n -= c; lba += c;
      }
      return 0;
  }

  static int ata_write(u32 lba, const u8 *src) {      /* one sector, used to save the settings */
      u32 words = 256; int g;
      g = 1000000; while ((inb(0x1F7) & 0x80) && --g) { }
      if (!g || inb(0x1F7) == 0xFF) return -1;
      outb(0x1F6, (u8)(0xE0 | ((lba >> 24) & 0x0F)));
      outb(0x1F2, 1); outb(0x1F3, (u8)lba); outb(0x1F4, (u8)(lba >> 8)); outb(0x1F5, (u8)(lba >> 16));
      outb(0x1F7, 0x30);
      inb(0x3F6); inb(0x3F6); inb(0x3F6); inb(0x3F6);
      g = 1000000; while ((inb(0x1F7) & 0x80) && --g) { }
      if (!g || !(inb(0x1F7) & 0x08)) return -1;
      __asm__ volatile("rep outsw" : "+S"(src), "+c"(words) : "d"((u16)0x1F0) : "memory");
      g = 1000000; while ((inb(0x1F7) & 0x80) && --g) { }
      outb(0x1F7, 0xE7);                              /* flush cache */
      g = 1000000; while ((inb(0x1F7) & 0x80) && --g) { }
      return (inb(0x1F7) & 0x21) ? -1 : 0;
  }

  static void plat_init(void) {
      struct bootinfo *b = (struct bootinfo *)0x5800;
      W = (int)b->width; H = (int)b->height;
      fb_base = (u8 *)b->fb; fb_wr = fb_base; fb_bpp = b->bpp; fb_pitch = b->pitch;
      if (fb_bpp != 24) fb_bpp = 32;
      if (fb_pitch < (u32)W * (fb_bpp >> 3)) fb_pitch = (u32)W * (fb_bpp >> 3);
      fb_swap = (b->redpos == 0);
      BACK = (u32 *)0x00400000; WALL_D = (u32 *)0x01000000; SCR = (u32 *)0x02000000; WALL_N = (u32 *)0x03000000; TMP = (u32 *)0x04000000; WALL_P = (u32 *)0x05000000; THUMBS = (u32 *)0x06000000; WALL = WALL_D;
      time_init();
      mouse_init();
      pf_init();
  }
  static void put_row(int y, int x, const u32 *s, int n) {
      u8 *d = fb_wr + (u32)y * fb_pitch + (u32)x * (fb_bpp >> 3);
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
static void present(int x, int y, int w, int h) {
    int x1 = x + w, y1 = y + h, r;
    if (x < 0) x = 0; if (y < 0) y = 0; if (x1 > W) x1 = W; if (y1 > H) y1 = H;
    if (x1 <= x || y1 <= y) return;
    for (r = y; r < y1; r++) put_row(r, x, BACK + r * W + x, x1 - x);
}
static void frame_wait(u32 t0, u32 min_ms) {
    u32 e = ms_now() - t0;
    if (e < min_ms) plat_wait(min_ms - e);
}
static void show_tmp(void) {                         /* TMP -> screen (hidden page + flip when available) */
    int y;
    if (pf_ok) { int tgt = 1 - pf_vis; plat_blit_page(TMP, tgt); plat_flip(tgt); }
    else for (y = 0; y < H; y++) put_row(y, 0, TMP + (u32)y * (u32)W, W);
}
static void present_dip(const u32 *a, u32 t) {       /* picture `a` darkened by t/256 (256 = fully blank) */
    u32 i, n = (u32)W * (u32)H, k = 256 - t, *dst = TMP, *pg = pf_ok ? plat_page(1 - pf_vis) : 0;
    if (pg) dst = pg;                                /* write straight into the hidden video page */
    for (i = 0; i < n; i++) { u32 p = a[i]; dst[i] = ((((p & 0xFF00FF) * k) >> 8) & 0xFF00FF) | ((((p & 0x00FF00) * k) >> 8) & 0x00FF00); }
    if (pg) plat_flip(1 - pf_vis); else show_tmp();
}
static void crossfade_dip(u32 ms) {                  /* SCR (old) -> blank screen -> BACK (new), used for the first boot step */
    u32 t0 = ms_now(), e, ma = ms * 40 / 100, mb = ms - ma;
    while ((e = ms_now() - t0) < ma) {               /* 1) old screen fades to blank */
        u32 f0 = ms_now(), t = e * 256 / ma;
        t = (t * t) >> 8;
        present_dip(SCR, t);
        frame_wait(f0, 16);
    }
    present_dip(SCR, 256); plat_wait(70);            /* 2) a moment of blank screen */
    t0 = ms_now();
    while ((e = ms_now() - t0) < mb) {               /* 3) new screen opens out of the blank */
        u32 f0 = ms_now(), u = 256 - e * 256 / mb;
        u = (u * u) >> 8;
        present_dip(BACK, u);
        frame_wait(f0, 16);
    }
    if (pf_ok) {                                     /* final frame: show it, and end on page 0 */
        int tgt = 1 - pf_vis;
        plat_blit_page(BACK, tgt); plat_flip(tgt);
        if (pf_vis) { plat_blit_page(BACK, 0); plat_flip(0); }
    } else present(0, 0, W, H);
}
static void present_mix(const u32 *a, const u32 *b, u32 t) {   /* a -> b, t/256, straight into the hidden page */
    u32 i, n = (u32)W * (u32)H, ia = 256 - t, *dst = TMP, *pg = pf_ok ? plat_page(1 - pf_vis) : 0;
    if (pg) dst = pg;
    for (i = 0; i < n; i++) {
        u32 pa = a[i], pb = b[i];
        dst[i] = (pa == pb) ? pa : (((((pa & 0xFF00FF) * ia + (pb & 0xFF00FF) * t) >> 8) & 0xFF00FF) | ((((pa & 0x00FF00) * ia + (pb & 0x00FF00) * t) >> 8) & 0x00FF00));
    }
    if (pg) plat_flip(1 - pf_vis); else show_tmp();
}
static void finish_on_page0(void) {                  /* show BACK for good and end on page 0 */
    if (pf_ok) {
        int tgt = 1 - pf_vis;
        plat_blit_page(BACK, tgt); plat_flip(tgt);
        if (pf_vis) { plat_blit_page(BACK, 0); plat_flip(0); }
    } else present(0, 0, W, H);
}
static void crossfade(u32 ms) {                      /* smooth dissolve SCR -> BACK (wallpaper stays, items melt into each other) */
    u32 t0 = ms_now(), e;
    while ((e = ms_now() - t0) < ms) {
        u32 f0 = ms_now(), t = e * 256 / ms;
        t = (t * t * (768 - 2 * t)) >> 16;               /* smoothstep: soft start, soft end */
        present_mix(SCR, BACK, t);
        frame_wait(f0, 8);
    }
    finish_on_page0();
}
static void restore(int x, int y, int w, int h) {      /* WALL -> BACK region */
    int x1 = x + w, y1 = y + h, j;
    if (x < 0) x = 0; if (y < 0) y = 0; if (x1 > W) x1 = W; if (y1 > H) y1 = H;
    for (j = y; j < y1; j++) memcpy(BACK + j * W + x, WALL + j * W + x, (unsigned)(x1 - x) * 4);
}

/* ---------------- mouse cursor ---------------- */
#define CUR_W 26
#define CUR_H 40
static const short CPX[7] = { 0, 0, 76, 124, 169, 122, 216 };        /* arrow outline, tenths of a pixel */
static const short CPY[7] = { 0, 306, 239, 351, 331, 223, 221 };
static u16 cur_a[CUR_W * CUR_H];                    /* alpha 0..256 (outline + fill) */
static u8  cur_g[CUR_W * CUR_H];                    /* grey: 255 = white fill, 0 = black outline */
static int cur_ready, cur_x, cur_y, cur_on;
static u32 cbuf[(CUR_W * 2) * (CUR_H * 2)];
static int in_poly(int px, int py) {                /* even-odd test, coordinates in tenths of a pixel */
    int i, j = 6, c = 0;
    for (i = 0; i < 7; i++) {
        int xi = CPX[i] + 10, yi = CPY[i] + 10, xj = CPX[j] + 10, yj = CPY[j] + 10;
        if ((yi > py) != (yj > py)) {
            int xint = xi + (py - yi) * (xj - xi) / (yj - yi);
            if (px < xint) c ^= 1;
        }
        j = i;
    }
    return c;
}
static void cursor_build(void) {                    /* smooth arrow with a thin dark outline */
    static u16 fa[CUR_W * CUR_H];
    int x, y, sx, sy, k;
    for (y = 0; y < CUR_H; y++)
        for (x = 0; x < CUR_W; x++) {
            int n = 0;
            for (sy = 0; sy < 4; sy++) for (sx = 0; sx < 4; sx++) n += in_poly(x * 10 + sx * 10 / 4 + 1, y * 10 + sy * 10 / 4 + 1);
            fa[y * CUR_W + x] = (u16)(n * 16);
        }
    for (y = 0; y < CUR_H; y++)
        for (x = 0; x < CUR_W; x++) {
            u32 o = 0, a = fa[y * CUR_W + x]; int dx, dy;
            for (dy = -1; dy <= 1; dy++) for (dx = -1; dx <= 1; dx++) {
                int xx = x + dx, yy = y + dy;
                if (xx >= 0 && xx < CUR_W && yy >= 0 && yy < CUR_H && fa[yy * CUR_W + xx] > o) o = fa[yy * CUR_W + xx];
            }
            k = y * CUR_W + x;
            cur_a[k] = (u16)o;
            cur_g[k] = o ? (u8)(a * 255 / o) : 0;
        }
    cur_ready = 1;
}
/* draws region (x0,y0,w,h) of BACK to the screen with the cursor sprite on top at (cx,cy) */
static void cur_blit(int x0, int y0, int w, int h, int cx, int cy) {
    int i, j;
    if (!cur_ready) cursor_build();
    for (j = 0; j < h; j++) {
        int py = y0 + j, xs = x0 < 0 ? -x0 : 0, xe = (x0 + w > W) ? W - x0 : w;
        if (py < 0 || py >= H) continue;
        for (i = xs; i < xe; i++) {
            int ci = x0 + i - cx, cj = py - cy;
            u32 c = BACK[py * W + x0 + i];
            if (ci >= 0 && ci < CUR_W && cj >= 0 && cj < CUR_H) {
                u32 a = cur_a[cj * CUR_W + ci];
                if (a) { u32 g = cur_g[cj * CUR_W + ci]; c = mix(c, g * 0x010101u, a > 256 ? 256 : a); }
            }
            cbuf[j * w + i] = c;
        }
        if (xe > xs) put_row(py, x0 + xs, cbuf + j * w + xs, xe - xs);
    }
}
static void cursor_draw(void) {                     /* (re)draw cursor at current mouse position (tip = mx,my) */
    cur_blit(mx - 1, my - 1, CUR_W, CUR_H, mx - 1, my - 1);
    cur_x = mx; cur_y = my; cur_on = 1;
}
static void cursor_update(void) {                   /* call after the mouse moved */
    int dx = mx - cur_x, dy = my - cur_y;
    if (!cur_on) { cursor_draw(); return; }
    if (!dx && !dy) return;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    if (dx < CUR_W && dy < CUR_H) {                 /* close: one combined redraw, no flicker */
        int x0 = (cur_x < mx ? cur_x : mx) - 1, y0 = (cur_y < my ? cur_y : my) - 1;
        cur_blit(x0, y0, CUR_W + dx, CUR_H + dy, mx - 1, my - 1);
        cur_x = mx; cur_y = my;
    } else {                                        /* far: erase old, draw new */
        cur_blit(cur_x - 1, cur_y - 1, CUR_W, CUR_H, -1000, -1000);
        cursor_draw();
    }
}

/* ======================================================================
 *  wallpaper (generated - orange dunes + warm sky, like the concept art)
 * ====================================================================== */
static s16 ycurve[5][2048];
static void build_wall_part(int y0, int y1) {
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
    if (y0 == 0)
    for (i = 0; i < 5; i++)
        for (x = 0; x < W; x++) {
            s32 ang = LP[i] + (x * LF[i]) / W;
            ycurve[i][x] = (s16)(H * LB[i] / 1000 + (H * LA[i] / 1000) * isin(ang) / 1024
                                 + (H * LT[i] / 1000) * (x - W / 2) / W);
        }
    for (y = y0; y < y1; y++) {
        u32 *row = WALL_D + y * W, skyc;
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
 *  night-city wallpaper (generated - dark blurred skyline, cyan light bar,
 *  reflective floor - like the Nexora concept image)
 * ====================================================================== */
static u32 hash32(u32 x) { x ^= x >> 16; x *= 0x7FEB352Du; x ^= x >> 15; x *= 0x846CA68Bu; x ^= x >> 16; return x; }
static u32 dimc(u32 c, u32 f) { return ((((c & 0xFF00FF) * f) >> 8) & 0xFF00FF) | ((((c & 0x00FF00) * f) >> 8) & 0x00FF00); }
static s16 nbt[2][2048];
static int n_hz;
static void night_prep(void) {
    int l, x;
    n_hz = H * 66 / 100;
    for (l = 0; l < 2; l++) {
        int xs = 0; u32 seed = (u32)l * 977u + 13u;
        while (xs < W) {
            u32 hh = hash32(seed++ * 2654435u);
            int bw = W / 22 + (int)(hh % (u32)(W / 12));
            int ht = (l ? 20 : 12) + (int)((hh >> 8) % 32u);
            int top = n_hz - H * ht / 100;
            if (l == 1 && ((hh >> 20) % 10u) < 3u) top = n_hz;            /* gap in the near row */
            for (x = xs; x < xs + bw && x < W; x++) nbt[l][x] = (s16)top;
            xs += bw;
        }
    }
}
static u32 scene_px(int x, int y) {
    u32 c, t; int l;
    if (y < 0) y = 0;
    if (y >= n_hz) y = n_hz - 1;
    t = (u32)y * 256u / (u32)n_hz;
    c = mix(0x0A1118, 0x26394A, t);
    if (t > 150) c = mix(c, 0x3A5560, (t - 150) * 3 / 4);
    for (l = 0; l < 2; l++) {
        if (y >= nbt[l][x]) {
            u32 col = l ? 0x0B151B : 0x1A2C37, a = (u32)(y - nbt[l][x]) * 40u;
            u32 hv = hash32((u32)((x / 12) * 7919 + (y / 16) * 104729 + l * 31));
            if (a > 256) a = 256;
            if ((hv & 63) < 4 && (x % 12) < 5 && (y % 16) < 7)
                col = mix(col, (hv & 64) ? 0x6FD6D0 : 0xE8D9A8, l ? 70 : 45);   /* lit windows */
            c = mix(c, col, a);
        }
    }
    {   /* vertical cyan light bar */
        int cx = W * 815 / 1000, hw = W / 100, d = x - cx, g = W / 24;
        if (d < 0) d = -d;
        if (d < hw) c = mix(c, 0x3CF2E8, 235);
        else if (d < hw + g) { u32 f = (u32)(hw + g - d) * 256u / (u32)g; f = (f * f) >> 8; c = mix(c, 0x1FC4BC, f * 120 / 256); }
    }
    return c;
}
static void build_wall_night_part(u32 *dst, int y0, int y1) {
    int x, y;
    if (y0 == 0) night_prep();
    for (y = y0; y < y1; y++) {
        u32 *row = dst + y * W;
        for (x = 0; x < W; x++) {
            u32 c, vf; int ax_ = x - W / 2;
            if (y < n_hz) c = scene_px(x, y);
            else {
                int d = y - n_hz; u32 f = (u32)d * 256u / (u32)(H - n_hz), ra = 100 - f * 85 / 256;
                c = mix(0x0B1116, 0x020305, f);
                c = mix(c, scene_px(x, n_hz - 1 - d * 2), ra);                   /* reflection */
                if (d == 10 || d == 34 || d == 80 || d == 150) c = mix(c, 0x4A6772, 26);   /* gloss lines */
                {   /* cyan light streak on the right side of the floor */
                    int ys = H * 755 / 1000, dy = y - ys, xs = W * 86 / 100;
                    if (dy < 0) dy = -dy;
                    if (dy < 14 && x > xs) {
                        u32 g = (u32)(14 - dy) * 256u / 14u, a = (u32)(x - xs) * 256u / (u32)(W - xs);
                        g = (g * g) >> 8;
                        c = mix(c, 0x35E8DC, (g * a >> 8) * 210 / 256);
                    }
                }
            }
            if (ax_ < 0) ax_ = -ax_;
            vf = 256 - (u32)ax_ * 80u / (u32)(W / 2);
            if (y < H / 8) vf = vf * (190u + (u32)y * 66u / (u32)(H / 8)) >> 8;
            row[x] = dimc(c, vf);
        }
    }
}

/* ======================================================================
 *  photo wallpapers: read from the disk image (walls.bin at sector 4096)
 * ====================================================================== */
#define WP_LBA 4096
#define WP_W 800
#define WP_H 450
#define TW_ 128
#define TH_ 72
static const char *WPN[9] = { "Night City", "Desert", "Glass Spheres", "Blue Curves", "Twin Orbs", "Silk", "Fuji Autumn", "Sunset Cat", "Fantasy Falls" };
static void thumb_from(u32 *dst, const u32 *src) {
    int x, y;
    for (y = 0; y < TH_; y++) for (x = 0; x < TW_; x++) dst[y * TW_ + x] = src[(y * H / TH_ + H / (2 * TH_)) * W + x * W / TW_ + W / (2 * TW_)];
}
static void wp_init(void) {                         /* find the wallpaper pack and load the previews */
    u8 *raw = (u8 *)TMP; int i, k;
    thumb_from(THUMBS, WALL_N); thumb_from(THUMBS + TW_ * TH_, WALL_D);
    nwp = 0;
    if (ata_read(WP_LBA, 1, raw) == 0 && raw[0] == 'N' && raw[1] == 'X' && raw[2] == 'W' && raw[3] == 'P') {
        nwp = raw[4]; if (nwp > 7) nwp = 7;
        for (i = 0; i < nwp; i++) {
            if (ata_read(WP_LBA + 1 + (u32)i * 64, 54, raw)) { nwp = i; break; }
            for (k = 0; k < TW_ * TH_; k++) THUMBS[(2 + i) * TW_ * TH_ + k] = ((u32)raw[k * 3] << 16) | ((u32)raw[k * 3 + 1] << 8) | raw[k * 3 + 2];
        }
    }
}
static void wp_load_photo(int p) {                  /* disk -> smooth upscale to the screen size */
    static int xi[2048], yi_; static u8 xf[2048];
    u8 *raw = (u8 *)TMP; u32 sx = ((u32)WP_W << 16) / (u32)W, sy = ((u32)WP_H << 16) / (u32)H; int x, y;
    if (ata_read(WP_LBA + 1 + 8 * 64 + (u32)p * 2112, 2110, raw)) return;
    for (x = 0; x < W; x++) {
        u32 u = (u32)x * sx + (sx >> 1); u = u >= 0x8000 ? u - 0x8000 : 0;
        xi[x] = (int)(u >> 16); xf[x] = (u8)((u >> 8) & 255);
        if (xi[x] > WP_W - 2) { xi[x] = WP_W - 2; xf[x] = 255; }
    }
    for (y = 0; y < H; y++) {
        u32 v = (u32)y * sy + (sy >> 1), fy; const u8 *r0, *r1; u32 *row = WALL_P + (u32)y * (u32)W;
        v = v >= 0x8000 ? v - 0x8000 : 0; yi_ = (int)(v >> 16); fy = (v >> 8) & 255;
        if (yi_ > WP_H - 2) { yi_ = WP_H - 2; fy = 255; }
        r0 = raw + (u32)yi_ * WP_W * 3; r1 = r0 + WP_W * 3;
        for (x = 0; x < W; x++) {
            int i0 = xi[x] * 3; u32 fx = xf[x], c, out = 0;
            for (c = 0; c < 3; c++) {
                u32 top = r0[i0 + c] * (256 - fx) + r0[i0 + 3 + c] * fx, bot = r1[i0 + c] * (256 - fx) + r1[i0 + 3 + c] * fx;
                out = (out << 8) | ((top * (256 - fy) + bot * fy) >> 16);
            }
            row[x] = out;
        }
    }
}
static u32 *wp_wall(void) { return wp_choice >= 2 ? WALL_P : (wp_choice ? WALL_D : WALL_N); }
static void wp_select(int i) {
    if (i >= 2) { if (i - 2 >= nwp) return; wp_load_photo(i - 2); }
    wp_choice = i; WALL = wp_wall();
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
    rrect(x, y, w, h, r, 0xFFFFFF, gl_edge);
    rrect(x + 1, y + 1, w - 2, h - 2, r - 1, 0x0B1018, gl_fill);
}
static void time_str(const struct tm_ *t, char *out, char *ampm) {
    int h12 = t->h % 12 ? t->h % 12 : 12;
    if (clock24) { char mm[4]; out[0] = 0; u2s(out, (u32)t->h, 2); scat(out, ":"); u2s(mm, (u32)t->m, 2); scat(out, mm); ampm[0] = 0; return; }
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
static int shift_sym(int c) {
    switch (c) {
    case '1': return '!'; case '2': return '@'; case '3': return '#'; case '4': return '$'; case '5': return '%';
    case '6': return '^'; case '7': return '&'; case '8': return '*'; case '9': return '('; case '0': return ')';
    case '-': return '_'; case '=': return '+'; case '[': return '{'; case ']': return '}'; case ';': return ':';
    case '\'': return '"'; case ',': return '<'; case '.': return '>'; case '/': return '?'; case '\\': return '|'; case '`': return '~';
    }
    return c;
}
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
            if (kb_shift && c >= 'a' && c <= 'z') c -= 32; else if (kb_shift) c = shift_sym(c);
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
/* ---- settings are saved on the disk (sector 2800) so they survive a restart ---- */
#define ST_LBA 2800
static int st_dirty;
static void settings_save(void) {
    static u8 sb[512]; int i;
    for (i = 0; i < 512; i++) sb[i] = 0;
    sb[0] = 'N'; sb[1] = 'X'; sb[2] = 'S'; sb[3] = 'T';
    sb[4] = (u8)wp_choice; sb[5] = (u8)mspeed; sb[6] = (u8)glass_lvl; sb[7] = (u8)show_w; sb[8] = (u8)clock24;
    ata_write(ST_LBA, sb);
}
static void settings_load(void) {
    static u8 sb[512];
    if (ata_read(ST_LBA, 1, sb) != 0 || sb[0] != 'N' || sb[1] != 'X' || sb[2] != 'S' || sb[3] != 'T') return;
    if (sb[5] <= 4) mspeed = sb[5];
    if (sb[6] <= 4) glass_lvl = sb[6];
    show_w = sb[7] ? 1 : 0; clock24 = sb[8] ? 1 : 0;
    if (sb[4] == 1) { wp_choice = 1; WALL = WALL_D; }
    else if (sb[4] >= 2 && sb[4] - 2 < nwp) { wp_load_photo(sb[4] - 2); wp_choice = sb[4]; WALL = WALL_P; }
}

/* ---- start-up work, done in small slices while the logo / loading bar is on screen ---- */
static int bw_dune, bw_night, bw_disk;
static int boot_work(u32 budget_ms) {                /* returns 1 when everything is ready */
    u32 t0 = ms_now();
    do {
        if (bw_dune < H)       { int n = H - bw_dune < 8 ? H - bw_dune : 8; build_wall_part(bw_dune, bw_dune + n); bw_dune += n; }
        else if (bw_night < H) { int n = H - bw_night < 8 ? H - bw_night : 8; build_wall_night_part(WALL_N, bw_night, bw_night + n); bw_night += n; }
        else if (bw_disk == 0) { wp_init(); bw_disk = 1; }
        else if (bw_disk == 1) { settings_load(); bw_disk = 2; }
        else return 1;
    } while (ms_now() - t0 < budget_ms);
    return bw_dune >= H && bw_night >= H && bw_disk >= 2;
}
static u32 boot_progress(void) {                     /* 0..1000 */
    u32 p = (u32)(bw_dune + bw_night) * 900u / (2u * (u32)H);
    return p + (bw_disk >= 2 ? 100u : bw_disk ? 50u : 0u);
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

    /* 1) aqua-dark screen for half a second (the wallpapers already start building) */
    cv = BACK; fill_rect(0, 0, W, H, TEAL_COLOR, 256); present(0, 0, W, H);
    t0 = ms_now(); SNAP("01_teal");
    while (ms_now() - t0 < 500) { if (boot_work(8)) plat_wait(8); }

    /* 2) blank, then the dark splash background opens */
    memcpy(SCR, BACK, (unsigned)W * H * 4);
    splash_bg_rect(0, 0, W, H);
    SNAP("02_before_fade");
    crossfade_dip(500);

    /* 3) logo fades in */
    t0 = ms_now();
    while ((e = ms_now() - t0) < 600) {
        u32 f0 = ms_now(), a = e * 256 / 600;
        a = (a * a) >> 8;                                    /* ease-in */
        boot_work(6);
        splash_bg_rect(lx - 4, ly - 4, dw + 8, dh + 8);
        draw_logo(lx, ly, dw, dh, a);
        present(lx - 4, ly - 4, dw + 8, dh + 8);
        if (e > 300) SNAP("03_logo_fade");
        frame_wait(f0, 16);
    }
    splash_bg_rect(lx - 4, ly - 4, dw + 8, dh + 8);
    draw_logo(lx, ly, dw, dh, 256); present(lx - 4, ly - 4, dw + 8, dh + 8);

    /* 4) loading bar: follows the real work, at least 1.2 s so it is smooth */
    t0 = ms_now();
    for (;;) {
        u32 f0 = ms_now(), p, pt, pw;
        char pct[8];
        boot_work(10);
        e = f0 - t0; if (e > 1200) e = 1200;
        pt = e * 1000 / 1200; pw = boot_progress();
        p = pt < pw ? pt : pw;
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
        present(bar_x - 10, bar_y - 20, bar_w + 20, 120);
        if (p >= 500) SNAP("04_progress");
        if (p >= 1000) break;
        frame_wait(f0, 16);
    }
    plat_wait(120);
}

/* ======================================================================
 *  NEW UI : small helpers
 * ====================================================================== */
static u32 ui_acc = 0x2FD8D0;      /* accent colour (cyan on desktop, blue on lock screen) */
static int ui_dark = 1;            /* 1 = dark cyan glass (desktop), 0 = light glass (lock) */

static void panel(int x, int y, int w, int h, int r) {
    if (ui_dark) { rrect(x, y, w, h, r, 0x4FE3DC, 64); rrect(x + 1, y + 1, w - 2, h - 2, r - 1, 0x08161C, GLA[glass_lvl]); }
    else glass(x, y, w, h, r);
}
static void line(int x0, int y0, int x1, int y1, int t, u32 col, u32 a) {
    int dx = x1 - x0, dy = y1 - y0, adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy, n = adx > ady ? adx : ady, i;
    if (!n) n = 1;
    for (i = 0; i <= n; i++) fill_rect(x0 + dx * i / n - t / 2, y0 + dy * i / n - t / 2, t, t, col, a);
}
static void diamond(int cx, int cy, int r, u32 col, u32 a) {            /* password symbol */
    int x, y;
    for (y = -r - 1; y <= r + 1; y++)
        for (x = -r - 1; x <= r + 1; x++) {
            int dx2 = 2 * x + 1, dy2 = 2 * y + 1, d2, t; u32 cov;
            if (dx2 < 0) dx2 = -dx2;
            if (dy2 < 0) dy2 = -dy2;
            d2 = dx2 + dy2; t = 2 * r - d2;
            if (t < -2) continue;
            cov = (t >= 2) ? 256u : (u32)(128 + t * 64);
            px_blend(cx + x, cy + y, col, (a * cov) >> 8);
        }
}
static int text_r(const u8 *f, int rx, int yb, const char *s, u32 col, u32 a) { return draw_text(f, rx - text_w(f, s), yb, s, col, a); }
static int lc(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }
static int streq(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return *a == *b; }
static int starts(const char *s, const char *p) { while (*p) { if (*s++ != *p++) return 0; } return 1; }
static void itoa_s(char *o, int v) { if (v < 0) { *o++ = '-'; v = -v; } u2s(o, (u32)v, 1); }
static int inrect(int px, int py, int x, int y, int w, int h) { return px >= x && px < x + w && py >= y && py < y + h; }

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

/* ======================================================================
 *  widgets (used on the lock screen AND on the desktop)
 * ====================================================================== */
static void wifi_icon(int x, int y, u32 col, u32 a) {
    int i; for (i = 0; i < 4; i++) { int hh = i * 5 + 5; fill_rect(x + i * 7, y + 18 - hh, 5, hh, col, a); }
}
static void bt_icon(int cx, int y, u32 col, u32 a) {
    line(cx, y, cx, y + 20, 2, col, a);
    line(cx, y, cx + 7, y + 6, 2, col, a);   line(cx + 7, y + 6, cx - 7, y + 15, 2, col, a);
    line(cx - 7, y + 5, cx + 7, y + 14, 2, col, a); line(cx + 7, y + 14, cx, y + 20, 2, col, a);
}
static void battery_icon(int x, int y) {
    rrect(x, y, 32, 16, 4, 0xFFFFFF, 230);
    rrect(x + 2, y + 2, 28, 12, 3, 0x0A1A22, 256);
    rrect(x + 3, y + 3, 21, 10, 2, 0x7CF29C, 256);
    fill_rect(x + 32, y + 5, 3, 6, 0xFFFFFF, 230);
}
static void w_status(int x, int y, int w, int h) {
    char b[24], n[12];
    panel(x, y, w, h, 18);
    draw_text(F_UI, x + 18, y + 34, "System Status", 0xFFFFFF, 245);
    fill_rect(x + 18, y + 46, w - 36, 1, 0xFFFFFF, 40);
    draw_text(F_UI, x + 18, y + 80, "Status", 0xC9CED8, 220);  text_r(F_UI, x + w - 18, y + 80, "Online", 0x7CF29C, 256);
    u2s(n, ms_now() / 60000u, 1); b[0] = 0; scat(b, n); scat(b, " min");
    draw_text(F_UI, x + 18, y + 112, "Uptime", 0xC9CED8, 220);  text_r(F_UI, x + w - 18, y + 112, b, 0xFFFFFF, 245);
    u2s(n, (u32)W, 1); b[0] = 0; scat(b, n); scat(b, "x"); u2s(n, (u32)H, 1); scat(b, n);
    draw_text(F_UI, x + 18, y + 144, "Display", 0xC9CED8, 220); text_r(F_UI, x + w - 18, y + 144, b, 0xFFFFFF, 245);
    (void)h;
}
static void w_calendar(int x, int y, int w, int h) {
    static const char *WD[7] = { "S", "M", "T", "W", "T", "F", "S" };
    struct tm_ t; char hd[40], n[12]; int cw = (w - 24) / 7, rh = (h - 96) / 6, i, first, nd;
    rtc_time(&t);
    panel(x, y, w, h, 18);
    hd[0] = 0; scat(hd, MONTHS[t.mon - 1]); scat(hd, " "); u2s(n, (u32)t.year, 1); scat(hd, n);
    draw_text(F_UI, x + 18, y + 34, hd, 0xFFFFFF, 245);
    text_r(F_UI, x + w - 16, y + 34, ">", 0xC9CED8, 220);
    text_r(F_UI, x + w - 40, y + 34, "<", 0xC9CED8, 220);
    for (i = 0; i < 7; i++) text_c(F_UI, x + 12 + i * cw + cw / 2, y + 68, WD[i], 0xAEB8BE, 220);
    first = dow(t.year, t.mon, 1); nd = dim(t.year, t.mon);
    for (i = 1; i <= nd; i++) {
        int idx = first + i - 1, col = idx % 7, row = idx / 7, cx = x + 12 + col * cw + cw / 2, yb = y + 96 + row * rh;
        if (i == t.day) disc(cx, yb - 8, 13, 0, ui_acc, ui_acc, 256);
        u2s(n, (u32)i, 1);
        text_c(F_UI, cx, yb, n, i == t.day ? 0xFFFFFF : 0xDCE4E8, 240);
    }
}
static void w_network(int x, int y, int w, int h) {
    panel(x, y, w, h, 18);
    draw_text(F_UI, x + 18, y + 34, "Network", 0xFFFFFF, 245);
    fill_rect(x + 18, y + 46, w - 36, 1, 0xFFFFFF, 40);
    wifi_icon(x + 20, y + 62, ui_acc, 256);
    draw_text(F_UI, x + 62, y + 82, "Wi-Fi", 0xFFFFFF, 245);
    draw_text(F_UI, x + 18, y + 118, "Not connected", 0xC9CED8, 230);
    draw_text(F_UI, x + 18, y + 146, "No driver yet", ui_dark ? 0x8E99A2 : 0xE6ECEF, 220);
    (void)h;
}

/* ======================================================================
 *  PASSWORD / LOCK SCREEN (2 steps: 1 = widgets + "press Enter", 2 = password)
 * ====================================================================== */
static void login_rect(int *x, int *y, int *w, int *h) { *x = W / 2 - 330; *y = H / 2 - 132; *w = 660; *h = 450; }
static void draw_avatar_name(int cx, int ay) {
    disc(cx, ay, 66, 62, 0xFFFFFF, 0xFFFFFF, 120);
    disc(cx, ay, 58, 0, 0x4C8DFF, 0x35D0FF, 256);
    disc(cx, ay - 12, 15, 0, 0xFFFFFF, 0xFFFFFF, 245);
    rrect(cx - 32, ay + 12, 64, 36, 18, 0xFFFFFF, 245);
    text_c_sh(F_BIG, cx, ay + 58 + 52, USER_NAME, 0xFFFFFF, 256);
}
static void draw_login(int shake, int npw, int msg) {
    int cx = W / 2, ty = H / 2 - 120, ay = ty + 62, fx, fy, fw = 440, i, vis, from;
    int rx, ry, rw, rh; login_rect(&rx, &ry, &rw, &rh);
    restore(rx, ry, rw, rh);
    draw_avatar_name(cx, ay);
    /* password field (moved lower) */
    fx = cx - fw / 2 + shake; fy = ty + 272;
    rrect(fx, fy, fw, 58, 29, 0xFFFFFF, 70);
    rrect(fx + 1, fy + 1, fw - 2, 56, 28, 0x0B1018, 130);
    if (npw == 0) draw_text(F_UI, fx + 30, fy + 37, "Password", 0xFFFFFF, 150);
    vis = npw > 12 ? 12 : npw; from = npw - vis;
    for (i = 0; i < vis; i++) diamond(fx + 38 + i * 28, fy + 29, 9, 0xFFFFFF, 256);   /* diamonds, not dots */
    (void)from;
    disc(fx + fw - 31, fy + 29, 23, 0, ui_dark ? 0x35E0D8 : 0x4C8DFF, ui_dark ? 0x1AA6B8 : 0x3466D6, 256);
    text_c(F_BIG, fx + fw - 31, fy + 29 + 12, ">", ui_dark ? 0x06141A : 0xFFFFFF, 256);
    if (msg == 1 || msg == 2) {
        const char *m = (msg == 1) ? "Incorrect password. Try again." : "Welcome!";
        int pw = text_w(F_UI, m) + 44;
        rrect(cx - pw / 2, fy + 76, pw, 40, 20, msg == 1 ? 0xB3261E : 0x1E8E3E, 235);
        text_c(F_UI, cx, fy + 103, m, 0xFFFFFF, 256);
    } else {
        text_c_sh(F_UI, cx, fy + 100, "Type your password and press Enter", 0xFFFFFF, 200);
    }
}
static void lock_common(void) {
    memcpy(BACK, WALL, (unsigned)W * H * 4);
    cv = BACK;
    if (WALL != WALL_D) { ui_dark = 1; ui_acc = 0x2FD8D0; gl_fill = 118; gl_edge = 64; }   /* night: same look as desktop */
    else                { ui_dark = 0; ui_acc = 0x4C8DFF; gl_fill = 76;  gl_edge = 60; }   /* desert: transparent glass */
    clock_panel();
    corner_brand(44, H - 100);
    text_sh(F_UI, W - 190, H - 48, "Nexora OS  v1.0", 0xFFFFFF, 220);
}
static void compose_lock1(void) {
    int cx = W / 2, ay = H / 2 - 70, px = cx - 175, py = ay + 150;
    lock_common();
    w_status(56, 56, 300, 170);
    w_calendar(56, 240, 300, 270);
    w_network(56, 524, 300, 160);
    draw_avatar_name(cx, ay);
    rrect(px, py, 350, 58, 29, 0xFFFFFF, 80);
    rrect(px + 1, py + 1, 348, 56, 28, 0x0B1018, gl_fill);
    text_c(F_UI, cx, py + 37, "Press Enter to unlock", 0xFFFFFF, 245);
}
static void compose_lock2(void) { lock_common(); draw_login(0, 0, 0); }
static void lock_screen(void) {
    char pw[24]; int n = 0, k, lastm = -1, rx, ry, rw, rh; u32 tc = 0;
    login_rect(&rx, &ry, &rw, &rh);
    WALL = wp_wall();                                       /* same wallpaper as the desktop */
    memcpy(SCR, BACK, (unsigned)W * H * 4);
    compose_lock1();
    crossfade(500);
    SNAP("lock1");
    mouse_reset();
    while (plat_key() >= 0) { }                             /* flush old key presses */
    kb_shift = 0; kb_ext = 0;
    for (;;) {                                              /* step 1: wait for Enter */
        int c; mouse_drain(); c = key_char();
        if (c == 10 || mclick) break;
        if (ms_now() - tc > 400) {
            struct tm_ t; tc = ms_now(); rtc_time(&t);
            if (lastm >= 0 && t.m != lastm) redraw_clock();
            lastm = t.m;
        }
        plat_wait(8);
    }
    mclick = 0;
    memcpy(SCR, BACK, (unsigned)W * H * 4);                 /* step 2: password screen */
    compose_lock2();
    crossfade(300);
    SNAP("lock2");
    for (;;) {
        int c; mouse_drain(); c = key_char();
        mclick = 0;
        if (c == 8 && n > 0) { n--; draw_login(0, n, 0); present(rx, ry, rw, rh); }
        else if (c == 27)    { n = 0; draw_login(0, n, 0); present(rx, ry, rw, rh); }
        else if (c >= 32 && c < 127 && n < 20) {
            pw[n++] = (char)c; draw_login(0, n, 0); present(rx, ry, rw, rh);
            if (n == 5) SNAP("lock2_typing");
        } else if (c == 10) {
            pw[n] = 0;
            { const char *p = NEXORA_PASSWORD; int ok = (n == (int)slen(p)); for (k = 0; ok && k < n; k++) if (pw[k] != p[k]) ok = 0;
              if (ok) {
                  draw_login(0, n, 2); present(rx, ry, rw, rh);
                  SNAP("lock2_ok"); plat_wait(250); return;
              } }
            { static const s8 sh[] = { -18, 18, -14, 14, -9, 9, -4, 4, 0 };
              for (k = 0; k < 9; k++) { u32 f0 = ms_now(); draw_login(sh[k], n, 1); present(rx, ry, rw, rh); frame_wait(f0, 32); } }
            n = 0; draw_login(0, 0, 1); present(rx, ry, rw, rh);
            SNAP("lock2_wrong");
        }
        if (ms_now() - tc > 400) {
            struct tm_ t; tc = ms_now(); rtc_time(&t);
            if (lastm >= 0 && t.m != lastm) redraw_clock();
            lastm = t.m;
        }
        plat_wait(8);
    }
}

/* ======================================================================
 *  SCENE 4 : DESKTOP  (top bar, widgets, windows, start menu, taskbar, apps)
 * ====================================================================== */
#define NAPPS 5
#define AW 780
#define AH 520
static const char *APPN[NAPPS] = { "Explorer", "Notepad", "Calculator", "Terminal", "Settings" };
static const char *APPS[NAPPS] = { "Files", "Notepad", "Calc", "Terminal", "Settings" };
static const u32 IC1[NAPPS] = { 0x4FA3FF, 0xFFC371, 0x7CF29C, 0x3A3F4A, 0xFF7AA8 };
static const u32 IC2[NAPPS] = { 0x2B6BE0, 0xF08A24, 0x1FA2A0, 0x0E1014, 0x7A5CFF };

/* ---- desktop state ---- */
static int open_app = -1, menu_open = 1, ax, ay, drag, dgx, dgy, hov_menu = -1, hov_tb = -1;
static int sfocus, want_lock;
static char sbuf[24]; static int sn;
static int dr_x, dr_y, dr_w;                       /* last drawn app window (for erasing) */
/* notepad */
static char npbuf[2048]; static int nplen;
/* terminal */
#define TL_N 13
static char tl[TL_N][96]; static int tln; static char tin[72]; static int tinn;
/* calculator */
static s32 cacc, cval; static int cop, cnew = 1, cerr;

/* ---- geometry ---- */
static void lb_geom(int *cx, int *cy, int *r) { *cx = 56 + 62; *cy = H - 112; *r = 62; }
static void menu_rect(int *x, int *y, int *w, int *h) { *x = 56; *w = 290; *h = NAPPS * 64 + 24; *y = (H - 112 - 62 + 24) - *h; }
static void tb_rect(int *x, int *y, int *w, int *h) { *w = 1500; if (*w > W - 420) *w = W - 420; *x = (W - *w) / 2; *y = H - 140; *h = 96; }
static void tb_search(int *x, int *y, int *w, int *h) { int px, py, pw, ph; tb_rect(&px, &py, &pw, &ph); *x = px + 140; *y = py + 22; *w = 250; *h = 52; }
static void tb_icon(int i, int *cx, int *cy) { int px, py, pw, ph; tb_rect(&px, &py, &pw, &ph); *cx = px + 414 + 48 + i * 96; *cy = py + 34; }
static void tb_tog(int i, int *cx, int *cy)  { int px, py, pw, ph; tb_rect(&px, &py, &pw, &ph); *cx = px + 914 + 52 + i * 104; *cy = py + 36; }
static void tb_sld(int i, int *x, int *y)    { int px, py, pw, ph; tb_rect(&px, &py, &pw, &ph); *x = px + 1138 + i * 124 + 13; *y = py + 36; }   /* track is 98 px wide */
static void tb_desk(int *cx, int *cy)        { int px, py, pw, ph; tb_rect(&px, &py, &pw, &ph); *cx = px + 1402 + 48; *cy = py + 34; }
/* what is under (qx,qy): 0..4 app, 5 desktop, 6 search, 7 clock, 8/9 toggles, 10/11 sliders, -1 nothing */
static int tb_hit(int qx, int qy) {
    int px, py, pw, ph, rx;
    tb_rect(&px, &py, &pw, &ph);
    if (!inrect(qx, qy, px, py, pw, ph)) return -1;
    rx = qx - px;
    if (rx < 120) return 7;
    if (rx >= 140 && rx < 390) return (qy >= py + 22 && qy < py + 74) ? 6 : -1;
    if (rx >= 414 && rx < 894) return (rx - 414) / 96;
    if (rx >= 914 && rx < 1122) return 8 + (rx - 914) / 104;
    if (rx >= 1138 && rx < 1386) return 10 + (rx - 1138) / 124;
    if (rx >= 1402) return 5;
    return -1;
}

/* ---- icons ---- */
static void app_icon(int i, int cx, int cy, int r) {
    disc(cx, cy, r, 0, IC1[i], IC2[i], 256);
    disc(cx, cy, r, r - 3, 0xFFFFFF, 0xFFFFFF, 55);
    switch (i) {
    case 0:
        rrect(cx - 13, cy - 11, 13, 9, 3, 0xFFD36E, 256);
        rrect(cx - 13, cy - 6, 26, 18, 4, 0xFFE08F, 256);
        break;
    case 1:
        rrect(cx - 10, cy - 13, 20, 26, 3, 0xFFFFFF, 250);
        fill_rect(cx - 6, cy - 6, 12, 2, 0xC77B1E, 256); fill_rect(cx - 6, cy, 12, 2, 0xC77B1E, 256); fill_rect(cx - 6, cy + 6, 8, 2, 0xC77B1E, 256);
        break;
    case 2: {
        int a, b;
        rrect(cx - 11, cy - 13, 22, 26, 3, 0xFFFFFF, 250);
        fill_rect(cx - 8, cy - 10, 16, 6, 0x1FA2A0, 256);
        for (a = 0; a < 3; a++) for (b = 0; b < 2; b++) fill_rect(cx - 8 + a * 6, cy - 1 + b * 7, 4, 4, 0x6B7A84, 256);
        break; }
    case 3:
        text_c(F_BIG, cx, cy + 12, ">_", 0x7CF29C, 256);
        break;
    default:
        disc(cx, cy, 12, 7, 0xFFFFFF, 0xFFFFFF, 250);
        fill_rect(cx - 3, cy - 17, 6, 7, 0xFFFFFF, 250); fill_rect(cx - 3, cy + 10, 6, 7, 0xFFFFFF, 250);
        fill_rect(cx - 17, cy - 3, 7, 6, 0xFFFFFF, 250); fill_rect(cx + 10, cy - 3, 7, 6, 0xFFFFFF, 250);
        break;
    }
}

/* ---- top bar ---- */
static void topbar(void) {
    struct tm_ t; char ts[12], ap[4], full[20]; int rx = W - 24, nx;
    rtc_time(&t); time_str(&t, ts, ap);
    full[0] = 0; scat(full, ts); if (ap[0]) { scat(full, " "); scat(full, ap); }
    fill_rect(0, 0, W, 48, 0x03060A, 150);
    fill_rect(0, 48, W, 1, 0x4FE3DC, 46);
    draw_logo(18, 7, 30, 35, 256);
    nx = draw_text(F_UI, 60, 32, "NEXORA OS", 0xFFFFFF, 245);
    draw_text(F_UI, nx + 36, 32, "v1.0", 0xAEB8BE, 220);
    text_r(F_UI, rx, 32, full, 0xFFFFFF, 250);
    rx -= text_w(F_UI, full) + 34;
    battery_icon(rx - 35, 16);
    rx -= 35 + 30;
    wifi_icon(rx - 26, 15, 0xFFFFFF, 240);
    rx -= 26 + 30;
    bt_icon(rx - 4, 14, 0xFFFFFF, 240);
}

/* ---- window frames (concept style: title left, controls right) ---- */
static void win_controls(int x, int y, int w) {
    int cy = y + 22;
    fill_rect(x + w - 92, cy + 4, 12, 2, 0xFFFFFF, 220);
    fill_rect(x + w - 60, cy - 6, 12, 2, 0xFFFFFF, 220); fill_rect(x + w - 60, cy + 4, 12, 2, 0xFFFFFF, 220);
    fill_rect(x + w - 60, cy - 6, 2, 12, 0xFFFFFF, 220); fill_rect(x + w - 50, cy - 6, 2, 12, 0xFFFFFF, 220);
    line(x + w - 28, cy - 6, x + w - 16, cy + 6, 2, 0xFFFFFF, 230);
    line(x + w - 28, cy + 6, x + w - 16, cy - 6, 2, 0xFFFFFF, 230);
}
static void win_frame(int x, int y, int w, int h, const char *title, u32 fill) {
    rrect(x, y, w, h, 14, 0x4FE3DC, 70);
    rrect(x + 1, y + 1, w - 2, h - 2, 13, 0x08161C, fill);
    fill_rect(x + 12, y + 44, w - 24, 1, 0xFFFFFF, 36);
    draw_text(F_UI, x + 18, y + 30, title, 0xDDE6EA, 235);
    win_controls(x, y, w);
}
/* ---- the static desktop (everything that never changes between clicks) ---- */
static void build_desk(void) {
    memcpy(BACK, WALL, (unsigned)W * H * 4);
    cv = BACK; ui_dark = 1; ui_acc = 0x2FD8D0;
    topbar();
    if (show_w) {
        w_status(W - 276, 68, 250, 170);
        w_calendar(W - 276, 252, 250, 246);
        w_network(W - 276, 512, 250, 160);
    }
}

/* ---- start menu / logo button / taskbar ---- */
static void draw_menu(void) {
    int x, y, w, h, i; menu_rect(&x, &y, &w, &h);
    panel(x, y, w, h, 22);
    for (i = 0; i < NAPPS; i++) {
        int iy = y + 12 + i * 64, sel = (open_app == i), hv = (hov_menu == i);
        if (sel || hv) {
            rrect(x + 8, iy, w - 16, 56, 14, ui_acc, sel ? 230 : 130);
            rrect(x + 10, iy + 2, w - 20, 52, 12, 0x0A1D24, 235);
        }
        app_icon(i, x + 40, iy + 28, 22);
        draw_text(F_UI, x + 78, iy + 36, APPN[i], 0xFFFFFF, 250);
        draw_text(F_UI, x + w - 34, iy + 36, ">", 0xAEB8BE, 220);
    }
}
static void lb_rect(int *x, int *y, int *w, int *h) { int cx, cy, r; lb_geom(&cx, &cy, &r); *x = cx - r - 10; *y = cy - r - 10; *w = 2 * r + 20; *h = 2 * r + 20; }
static void draw_logo_btn(void) {
    int cx, cy, r; lb_geom(&cx, &cy, &r);
    disc(cx, cy, r + 8, r, ui_acc, ui_acc, menu_open ? 110 : 55);
    disc(cx, cy, r, 0, 0x0A2229, 0x04121A, 245);
    disc(cx, cy, r, r - 4, ui_acc, 0x1A8FA8, 256);
    draw_logo(cx - 26, cy - 30, 52, 60, 256);
}
static void draw_taskbar(void) {
    int px, py, pw, ph, i, cx, cy, sx, sy, sw, sh; struct tm_ t;
    static const short DV[5] = { 120, 402, 906, 1130, 1394 };
    tb_rect(&px, &py, &pw, &ph);
    panel(px, py, pw, ph, 48);
    rrect(px + 70, py, pw - 140, 2, 1, ui_acc, 200);                 /* glowing edges like the concept */
    rrect(px + 70, py + ph - 2, pw - 140, 2, 1, ui_acc, 110);
    for (i = 0; i < 5; i++) fill_rect(px + DV[i], py + 20, 1, 56, 0xFFFFFF, 40);
    /* analog clock button */
    cx = px + 60; cy = py + 48; rtc_time(&t);
    if (hov_tb == 7) disc(cx, cy, 34, 0, 0xFFFFFF, 0xFFFFFF, 28);
    disc(cx, cy, 30, 0, 0x0B242B, 0x08161C, 240);
    disc(cx, cy, 30, 28, ui_acc, ui_acc, 220);
    {
        int am = t.m * 1024 / 60, ah = ((t.h % 12) * 60 + t.m) * 1024 / 720;
        line(cx, cy, cx + isin(ah) * 13 / 1024, cy - icos(ah) * 13 / 1024, 3, 0xFFFFFF, 256);
        line(cx, cy, cx + isin(am) * 21 / 1024, cy - icos(am) * 21 / 1024, 2, 0xFFFFFF, 256);
        disc(cx, cy, 3, 0, ui_acc, ui_acc, 256);
    }
    /* search bar */
    tb_search(&sx, &sy, &sw, &sh);
    rrect(sx, sy, sw, sh, 26, sfocus ? ui_acc : 0xFFFFFF, sfocus ? 230 : (hov_tb == 6 ? 110 : 60));
    rrect(sx + 1, sy + 1, sw - 2, sh - 2, 25, 0x06121A, 225);
    disc(sx + 25, sy + 24, 9, 6, 0xFFFFFF, 0xFFFFFF, 235);
    line(sx + 31, sy + 30, sx + 38, sy + 37, 3, 0xFFFFFF, 256);
    if (!sn) draw_text(F_UI, sx + 54, sy + 34, "Search apps...", 0x8E99A2, 230);
    else {
        int tx = draw_text(F_UI, sx + 54, sy + 34, sbuf, 0xFFFFFF, 250), m = -1;
        for (i = 0; i < NAPPS && m < 0; i++) { int k = 0; while (k < sn && lc(APPN[i][k]) == lc(sbuf[k])) k++; if (k == sn) m = i; }
        if (sfocus) fill_rect(tx + 2, sy + 14, 2, 24, 0xFFFFFF, 230);
        if (m >= 0 && text_w(F_UI, APPN[m]) < sw - 54 - (tx - sx - 54) - 40) text_r(F_UI, sx + sw - 18, sy + 34, APPN[m], ui_acc, 240);
    }
    if (sfocus && !sn) fill_rect(sx + 54, sy + 14, 2, 24, 0xFFFFFF, 230);
    /* apps */
    for (i = 0; i < NAPPS; i++) {
        tb_icon(i, &cx, &cy);
        if (hov_tb == i) rrect(cx - 44, py + 6, 88, 84, 16, 0xFFFFFF, 28);
        app_icon(i, cx, cy, 26);
        text_c(F_UI, cx, py + 86, APPS[i], 0xFFFFFF, 235);
        if (open_app == i) rrect(cx - 10, py + 2, 20, 3, 1, ui_acc, 256);
    }
    /* toggle switches */
    for (i = 0; i < 2; i++) {
        int on = i ? clock24 : show_w;
        tb_tog(i, &cx, &cy);
        rrect(cx - 26, cy - 14, 52, 28, 14, on ? ui_acc : 0x3A4650, 256);
        disc(cx + (on ? 12 : -12), cy, 10, 0, 0xFFFFFF, 0xFFFFFF, 256);
        text_c(F_UI, cx, py + 86, i ? "24h" : "Widgets", 0xFFFFFF, 235);
    }
    /* sliders: mouse speed + glass opacity */
    for (i = 0; i < 2; i++) {
        int v = i ? glass_lvl : mspeed, kx;
        tb_sld(i, &sx, &sy);
        kx = sx + v * 98 / 4;
        rrect(sx, sy - 3, 98, 6, 3, 0xFFFFFF, 70);
        rrect(sx, sy - 3, kx - sx + 2, 6, 3, ui_acc, 256);
        disc(kx, sy, 11, 0, 0xFFFFFF, 0xFFFFFF, 256);
        disc(kx, sy, 5, 0, ui_acc, ui_acc, 256);
        text_c(F_UI, sx + 49, py + 86, i ? "Glass" : "Speed", 0xFFFFFF, 235);
    }
    /* desktop button */
    tb_desk(&cx, &cy);
    if (hov_tb == 5) rrect(cx - 44, py + 6, 88, 84, 16, 0xFFFFFF, 28);
    rrect(cx - 15, cy - 11, 30, 22, 5, ui_acc, 256);
    rrect(cx - 12, cy - 8, 24, 16, 3, 0x08161C, 256);
    text_c(F_UI, cx, py + 86, "Desktop", 0xFFFFFF, 235);
}

/* ---- applications ---- */
static void term_print(const char *s) {
    int i;
    if (tln == TL_N) { int r; for (r = 1; r < TL_N; r++) for (i = 0; i < 96; i++) tl[r - 1][i] = tl[r][i]; tln--; }
    for (i = 0; s[i] && i < 90; i++) tl[tln][i] = s[i];
    tl[tln][i] = 0; tln++;
}
static void term_init(void) {
    tln = 0; tinn = 0; tin[0] = 0;
    term_print("Nexora OS Terminal v1.0");
    term_print("Type 'help' to see the commands.");
}
static void launch(int i);
static void term_exec(void) {
    char out[96]; struct tm_ t; char a[16], ap[4];
    out[0] = 0; scat(out, "nexora> "); scat(out, tin); term_print(out);
    if (!tinn) { }
    else if (streq(tin, "help")) { term_print("help clear ver time date cpu res lock"); term_print("echo <text>  notepad calc files settings exit"); }
    else if (streq(tin, "clear")) { tln = 0; }
    else if (streq(tin, "ver")) term_print("Nexora OS v1.0 (32-bit, BIOS legacy)");
    else if (streq(tin, "time")) { rtc_time(&t); time_str(&t, out, ap); scat(out, " "); scat(out, ap); term_print(out); }
    else if (streq(tin, "date")) { rtc_time(&t); date_str(&t, out); u2s(a, (u32)t.year, 1); scat(out, " "); scat(out, a); term_print(out); }
    else if (streq(tin, "cpu")) { cpu_brand(out); term_print(out); }
    else if (streq(tin, "res")) { u2s(a, (u32)W, 1); out[0] = 0; scat(out, a); scat(out, " x "); u2s(a, (u32)H, 1); scat(out, a); term_print(out); }
    else if (streq(tin, "lock")) want_lock = 1;
    else if (starts(tin, "echo ")) term_print(tin + 5);
    else if (streq(tin, "notepad")) launch(1);
    else if (streq(tin, "calc")) launch(2);
    else if (streq(tin, "files")) launch(0);
    else if (streq(tin, "settings")) launch(4);
    else if (streq(tin, "exit")) open_app = -1;
    else term_print("Unknown command. Type 'help'.");
    tinn = 0; tin[0] = 0;
}
static void calc_apply(void) {
    s32 a = cacc < 0 ? -cacc : cacc, b = cval < 0 ? -cval : cval;
    switch (cop) {
    case '+': cacc += cval; break;
    case '-': cacc -= cval; break;
    case '*': if (b && a > 999999999 / b) cerr = 1; else cacc *= cval; break;
    case '/': if (cval == 0) cerr = 1; else cacc /= cval; break;
    default: cacc = cval; break;
    }
    if (cacc > 999999999 || cacc < -999999999) cerr = 1;
}
static void calc_key(int k) {
    if (k == 'C') { cacc = cval = 0; cop = 0; cnew = 1; cerr = 0; return; }
    if (cerr) { if (k >= '0' && k <= '9') { cerr = 0; cacc = cval = 0; cop = 0; cnew = 1; } else return; }
    if (k >= '0' && k <= '9') { if (cnew) { cval = 0; cnew = 0; } if (cval < 100000000) cval = cval * 10 + (k - '0'); return; }
    if (k == '<') { cval /= 10; return; }
    if (k == '=') { if (cop) { calc_apply(); cval = cacc; cop = 0; } cnew = 1; return; }
    if (cop && !cnew) { calc_apply(); cval = cacc; } else if (!cop) cacc = cval;
    cop = k; cnew = 1;
}
static const char CB[4][4] = { { '7', '8', '9', '/' }, { '4', '5', '6', '*' }, { '1', '2', '3', '-' }, { 'C', '0', '=', '+' } };
#define CBW 100
#define CBH 62
#define CBG 12

static void app_notepad(void) {
    int x = ax + 16, y = ay + 56, w = AW - 32, h = AH - 72 - 34, i, pad = 16, maxw = w - 2 * pad, lh = 30;
    int vis = (h - 2 * pad) / lh, nl = 0, lx = 0, k;
    static int ls[260]; char tmp[200]; char n[12], st[40];
    rrect(x, y, w, h, 10, 0x04080B, 215);
    if (!nplen) draw_text(F_UI, x + pad, y + pad + 22, "Start typing...", 0x6C7A84, 230);
    ls[0] = 0;
    for (i = 0; i < nplen; i++) {
        char s[2]; int cw_;
        if (npbuf[i] == '\n') { if (nl < 258) ls[++nl] = i + 1; lx = 0; continue; }
        s[0] = npbuf[i]; s[1] = 0; cw_ = text_w(F_UI, s);
        if (lx + cw_ > maxw) { if (nl < 258) ls[++nl] = i; lx = 0; }
        lx += cw_;
    }
    ls[nl + 1] = nplen + 1;
    {
        int first = nl + 1 > vis ? nl + 1 - vis : 0, ln;
        for (ln = first; ln <= nl; ln++) {
            int a = ls[ln], b = (ln == nl) ? nplen : ls[ln + 1]; int yy = y + pad + 22 + (ln - first) * lh, tx;
            for (k = 0; a < b && k < 198; a++) if (npbuf[a] != '\n') tmp[k++] = npbuf[a];
            tmp[k] = 0;
            tx = draw_text(F_UI, x + pad, yy, tmp, 0xFFFFFF, 250);
            if (ln == nl) fill_rect(tx + 1, yy - 18, 2, 24, ui_acc, 256);
        }
    }
    u2s(n, (u32)nplen, 1); st[0] = 0; scat(st, "Characters: "); scat(st, n);
    draw_text(F_UI, x + 4, ay + AH - 14, st, 0x8E99A2, 230);
    text_r(F_UI, x + w, ay + AH - 14, "Esc = close", 0x6C7A84, 220);
}
static void app_terminal(void) {
    int x = ax + 16, y = ay + 56, w = AW - 32, h = AH - 72, i, tx; char p[100];
    rrect(x, y, w, h, 10, 0x03070A, 240);
    for (i = 0; i < tln; i++) draw_text(F_UI, x + 16, y + 34 + i * 29, tl[i], starts(tl[i], "nexora>") ? 0x7CF29C : 0xDDE6EA, 245);
    tx = draw_text(F_UI, x + 16, y + 34 + tln * 29, "nexora> ", 0x7CF29C, 250);
    p[0] = 0; scat(p, tin);
    tx = draw_text(F_UI, tx, y + 34 + tln * 29, p, 0xFFFFFF, 250);
    fill_rect(tx + 2, y + 34 + tln * 29 - 18, 11, 3 + 20, 0xFFFFFF, 200);
}
static void app_calc(void) {
    int gx = ax + (AW - (4 * CBW + 3 * CBG)) / 2, gy = ay + 150, r, c; char ds[16];
    rrect(gx, ay + 60, 4 * CBW + 3 * CBG, 76, 14, 0x04080B, 225);
    if (cerr) { ds[0] = 0; scat(ds, "Error"); } else itoa_s(ds, cval);
    text_r(F_BIG, gx + 4 * CBW + 3 * CBG - 22, ay + 112, ds, cerr ? 0xFF8A80 : 0xFFFFFF, 256);
    if (cop && !cerr) { char o[2]; o[0] = (char)cop; o[1] = 0; draw_text(F_UI, gx + 20, ay + 90, o, ui_acc, 256); }
    for (r = 0; r < 4; r++)
        for (c = 0; c < 4; c++) {
            int bx = gx + c * (CBW + CBG), by = gy + r * (CBH + CBG), k = CB[r][c];
            int isop = (k == '/' || k == '*' || k == '-' || k == '+'), hit = inrect(mx, my, bx, by, CBW, CBH);
            char lab[2]; lab[0] = (char)k; lab[1] = 0;
            if (k == '=') rrect(bx, by, CBW, CBH, 14, ui_acc, hit ? 256 : 225);
            else if (k == 'C') rrect(bx, by, CBW, CBH, 14, 0xFF6F61, hit ? 210 : 150);
            else if (isop) rrect(bx, by, CBW, CBH, 14, ui_acc, hit ? 130 : 80);
            else rrect(bx, by, CBW, CBH, 14, 0xFFFFFF, hit ? 56 : 28);
            text_c(F_BIG, bx + CBW / 2, by + CBH / 2 + 12, lab, k == '=' ? 0x06141A : 0xFFFFFF, 256);
        }
}
static void app_explorer(void) {
    static const char *FN[6] = { "Documents", "Downloads", "Pictures", "Music", "Videos", "Desktop" };
    int i, x = ax + 16, y = ay + 56;
    rrect(x, y, AW - 32, 40, 10, 0xFFFFFF, 16);
    draw_text(F_UI, x + 16, y + 28, "This PC  >  Home", 0xDDE6EA, 245);
    for (i = 0; i < 6; i++) {
        int cx = ax + 40 + (i % 3) * 244, cy = ay + 120 + (i / 3) * 170;
        rrect(cx, cy, 224, 150, 14, 0xFFFFFF, 14);
        rrect(cx + 62, cy + 22, 44, 20, 6, 0xF2B84B, 256);
        rrect(cx + 62, cy + 32, 100, 66, 8, 0xFFD36E, 256);
        text_c(F_UI, cx + 112, cy + 130, FN[i], 0xFFFFFF, 245);
    }
    draw_text(F_UI, x + 4, ay + AH - 14, "File system support is coming soon.", 0x8E99A2, 230);
}
/* settings geometry (relative to window) */
#define ST_SX 40
#define ST_SW 500
#define ST_SY 100
#define WT_X 40
#define WT_Y 206
static void app_settings(void) {
    int sx = ax + ST_SX, sy = ay + ST_SY, i, kx, total = 2 + nwp; char v[16], n[12];
    draw_text(F_BIG, ax + 40, ay + 80, "Mouse speed", 0xFFFFFF, 250);
    u2s(n, (u32)MSP[mspeed] * 5 / 10, 1); v[0] = 0; scat(v, n); scat(v, "."); u2s(n, (u32)MSP[mspeed] * 5 % 10, 1); scat(v, n); scat(v, "x");
    text_r(F_UI, ax + 40 + ST_SW, ay + 78, v, ui_acc, 250);
    kx = sx + mspeed * ST_SW / 4;
    rrect(sx, sy, ST_SW, 8, 4, 0xFFFFFF, 55);
    rrect(sx, sy, kx - sx + 2, 8, 4, ui_acc, 256);
    for (i = 0; i < 5; i++) fill_rect(sx + i * ST_SW / 4 - 1, sy + 14, 2, 8, 0xFFFFFF, 90);
    disc(kx, sy + 4, 15, 0, 0xFFFFFF, 0xFFFFFF, 256);
    disc(kx, sy + 4, 9, 0, ui_acc, ui_acc, 256);
    draw_text(F_UI, sx - 2, sy + 46, "Slow", 0x8E99A2, 230);
    text_r(F_UI, sx + ST_SW + 2, sy + 46, "Fast", 0x8E99A2, 230);
    draw_text(F_BIG, ax + 40, ay + 192, "Wallpaper", 0xFFFFFF, 250);
    text_r(F_UI, ax + 740, ay + 190, WPN[wp_choice], ui_acc, 250);
    for (i = 0; i < total; i++) {
        int tx = ax + WT_X + (i % 5) * 143, ty = ay + WT_Y + (i / 5) * 88, x, y;
        rrect(tx - 3, ty - 3, 134, 78, 9, i == wp_choice ? ui_acc : 0xFFFFFF, i == wp_choice ? 256 : 40);
        for (y = 0; y < TH_; y++) for (x = 0; x < TW_; x++) BACK[(ty + y) * W + tx + x] = THUMBS[i * TW_ * TH_ + y * TW_ + x];
    }
    if (!nwp) draw_text(F_UI, ax + 40, ay + 344, "Photo wallpapers: put walls.bin in the project folder and run make.", 0x8E99A2, 230);
    rrect(ax + 40, ay + 400, 200, 52, 26, 0xFFFFFF, 70);
    rrect(ax + 41, ay + 401, 198, 50, 25, 0x08161C, 235);
    text_c(F_UI, ax + 140, ay + 434, "Lock screen", 0xFFFFFF, 250);
    {
        char cpu[64], r[40]; cpu_brand(cpu);
        u2s(n, (u32)W, 1); r[0] = 0; scat(r, n); scat(r, " x "); u2s(n, (u32)H, 1); scat(r, n);
        text_fit(F_UI, ax + 280, ay + 426, cpu, AW - 280 - 30, 0xAEB8BE, 230);
        draw_text(F_UI, ax + 280, ay + 456, "Nexora OS v1.0", 0xAEB8BE, 230);
        text_r(F_UI, ax + AW - 30, ay + 456, r, 0xAEB8BE, 230);
    }
}
static void draw_app(void) {
    win_frame(ax, ay, AW, AH, APPN[open_app], 256);
    switch (open_app) {
    case 0: app_explorer(); break;
    case 1: app_notepad(); break;
    case 2: app_calc(); break;
    case 3: app_terminal(); break;
    default: app_settings(); break;
    }
}

/* ---- composing ---- */
static void draw_dyn_layers(void) {
    cv = BACK; ui_dark = 1; ui_acc = 0x2FD8D0;
    if (menu_open) draw_menu();
    draw_logo_btn();
    draw_taskbar();
    if (open_app >= 0) draw_app();
}
static void restore_desk(int x, int y, int w, int h) {   /* static desktop (SCR) -> BACK region */
    int x1 = x + w, y1 = y + h, j;
    if (x < 0) x = 0; if (y < 0) y = 0; if (x1 > W) x1 = W; if (y1 > H) y1 = H;
    if (x1 <= x) return;
    for (j = y; j < y1; j++) memcpy(BACK + j * W + x, SCR + j * W + x, (unsigned)(x1 - x) * 4);
}
static u32 arow[2048];
static void overlay_cursor_row(u32 *row, int rx, int y, int w) {        /* keep the pointer visible during animations */
    int cj = y - (my - 1), i;
    if (!cur_on || !cur_ready || cj < 0 || cj >= CUR_H) return;
    for (i = 0; i < w; i++) {
        int ci = rx + i - (mx - 1);
        if (ci >= 0 && ci < CUR_W) { u32 al = cur_a[cj * CUR_W + ci]; if (al) row[i] = mix(row[i], cur_g[cj * CUR_W + ci] * 0x010101u, al > 256 ? 256 : al); }
    }
}
/* animate one rectangle from the saved picture `from` (stride w) to the new picture in BACK */
static void anim_rect(const u32 *from, int rx, int ry, int w, int h, u32 t) {
    int i, j;
    for (j = 0; j < h; j++) {
        const u32 *fa = from + (u32)j * (u32)w, *fb_ = BACK + (u32)(ry + j) * (u32)W + (u32)rx;
        u32 k = 256 - t;
        for (i = 0; i < w; i++) { u32 p = fa[i], q = fb_[i]; arow[i] = ((((p & 0xFF00FF) * k + (q & 0xFF00FF) * t) >> 8) & 0xFF00FF) | ((((p & 0x00FF00) * k + (q & 0x00FF00) * t) >> 8) & 0x00FF00); }
        overlay_cursor_row(arow, rx, ry + j, w);
        put_row(ry + j, rx, arow, w);
    }
}
static void dyn_redraw_ex(int anim_app, int anim_menu) {
    int mx_, my_, mw_, mh_, lx, ly, lw, lh, tx, ty, tw, th, ar_x = 0, ar_y = 0, ar_w = AW, ar_h = AH;
    u32 *sv_app = TMP, *sv_menu = TMP + 500000; int j;
    menu_rect(&mx_, &my_, &mw_, &mh_); lb_rect(&lx, &ly, &lw, &lh); tb_rect(&tx, &ty, &tw, &th);
    mx_ -= 4; my_ -= 4; mw_ += 8; mh_ += 8;
    if (anim_app) {                                  /* remember how the window area looks now */
        if (open_app >= 0) { ar_x = ax; ar_y = ay; } else if (dr_w) { ar_x = dr_x; ar_y = dr_y; } else anim_app = 0;
        if (ar_x < 0 || ar_y < 0 || ar_x + ar_w > W || ar_y + ar_h > H) anim_app = 0;
    }
    if (anim_menu && (mx_ < 0 || my_ < 0 || mx_ + mw_ > W || my_ + mh_ > H)) anim_menu = 0;
    if (anim_app)  for (j = 0; j < ar_h; j++) memcpy(sv_app + (u32)j * (u32)ar_w, BACK + (u32)(ar_y + j) * (u32)W + (u32)ar_x, (unsigned)ar_w * 4);
    if (anim_menu) for (j = 0; j < mh_; j++) memcpy(sv_menu + (u32)j * (u32)mw_, BACK + (u32)(my_ + j) * (u32)W + (u32)mx_, (unsigned)mw_ * 4);
    restore_desk(mx_, my_, mw_, mh_);
    restore_desk(lx, ly, lw, lh);
    restore_desk(tx - 4, ty - 4, tw + 8, th + 8);
    if (dr_w) restore_desk(dr_x, dr_y, dr_w, AH);
    if (open_app >= 0) restore_desk(ax, ay, AW, AH);
    draw_dyn_layers();
    if (!anim_menu) present(mx_, my_, mw_, mh_);
    present(lx, ly, lw, lh);
    present(tx - 4, ty - 4, tw + 8, th + 8);
    if (dr_w && !anim_app) present(dr_x, dr_y, dr_w, AH);
    if (open_app >= 0 && !anim_app) present(ax, ay, AW, AH);
    if (anim_app || anim_menu) {                     /* ease-out fade, about 170 ms */
        u32 t0 = ms_now(), e;
        while ((e = ms_now() - t0) < 170) {
            u32 f0 = ms_now(), u = 256 - e * 256 / 170, t;
            t = 256 - ((u * u) >> 8);
            if (anim_app)  anim_rect(sv_app, ar_x, ar_y, ar_w, ar_h, t);
            if (anim_menu) anim_rect(sv_menu, mx_, my_, mw_, mh_, t);
            frame_wait(f0, 8);
        }
        if (anim_app)  present(ar_x, ar_y, ar_w, ar_h);
        if (anim_menu) present(mx_, my_, mw_, mh_);
    }
    if (open_app >= 0) { dr_x = ax; dr_y = ay; dr_w = AW; } else dr_w = 0;
    cursor_draw();
}
static void dyn_redraw(void) { dyn_redraw_ex(0, 0); }
static void scene_full(void) {
    memcpy(BACK, SCR, (unsigned)W * H * 4);
    draw_dyn_layers();
    present(0, 0, W, H);
    if (open_app >= 0) { dr_x = ax; dr_y = ay; dr_w = AW; } else dr_w = 0;
    cursor_draw();
}
static void refresh_all(void) { build_desk(); memcpy(SCR, BACK, (unsigned)W * H * 4); scene_full(); }

/* ---- actions ---- */
static void launch(int i) {
    if (open_app < 0) { ax = (W - AW) / 2; ay = 110; }
    { int mo = menu_open; open_app = i; menu_open = 0; sfocus = 0; sn = 0; sbuf[0] = 0; dyn_redraw_ex(1, mo); }
}
static void show_desktop(void) { int mo = menu_open; open_app = -1; menu_open = 0; sfocus = 0; dyn_redraw_ex(1, mo); }
static int search_match(void) {
    int i, k;
    if (!sn) return -1;
    for (i = 0; i < NAPPS; i++) { k = 0; while (k < sn && lc(APPN[i][k]) == lc(sbuf[k])) k++; if (k == sn) return i; }
    for (i = 0; i < NAPPS; i++) { const char *a = APPN[i]; int s; for (s = 0; a[s]; s++) { k = 0; while (k < sn && a[s + k] && lc(a[s + k]) == lc(sbuf[k])) k++; if (k == sn) return i; } }
    if (sn >= 4 && lc(sbuf[0]) == 'f' && lc(sbuf[1]) == 'i' && lc(sbuf[2]) == 'l' && lc(sbuf[3]) == 'e') return 0;
    return -1;
}
static void app_click(int rx, int ry) {
    if (open_app == 2) {
        int gx = (AW - (4 * CBW + 3 * CBG)) / 2, gy = 150, c = (rx - gx) / (CBW + CBG), r = (ry - gy) / (CBH + CBG);
        if (rx >= gx && ry >= gy && c < 4 && r < 4 && (rx - gx) % (CBW + CBG) < CBW && (ry - gy) % (CBH + CBG) < CBH) { calc_key(CB[r][c]); dyn_redraw(); }
    } else if (open_app == 4) {
        if (ry >= ST_SY - 24 && ry < ST_SY + 36 && rx >= ST_SX - 24 && rx <= ST_SX + ST_SW + 24) {
            int v = ((rx - ST_SX) * 4 + ST_SW / 2) / ST_SW;
            if (v < 0) v = 0; if (v > 4) v = 4;
            mspeed = v; st_dirty = 1; dyn_redraw();
        } else if (ry >= WT_Y && ry < WT_Y + 2 * 88 && rx >= WT_X && rx < WT_X + 5 * 143) {
            int c = (rx - WT_X) / 143, r = (ry - WT_Y) / 88, i = r * 5 + c;
            if ((rx - WT_X) % 143 < 128 && (ry - WT_Y) % 88 < 72 && i < 2 + nwp && i != wp_choice) { wp_select(i); st_dirty = 1; refresh_all(); }
        } else if (ry >= 400 && ry < 452 && rx >= 40 && rx < 240) want_lock = 1;
    }
}
static void desk_click(int px, int py) {
    int i, cx, cy, r;
    if (open_app >= 0 && inrect(px, py, ax, ay, AW, AH)) {
        sfocus = 0;
        if (py < ay + 44) {
            if (px >= ax + AW - 40) { open_app = -1; dyn_redraw_ex(1, 0); }
            else if (px >= ax + AW - 72) { }
            else if (px >= ax + AW - 104) { open_app = -1; dyn_redraw_ex(1, 0); }
            else { drag = 1; dgx = px - ax; dgy = py - ay; }
        } else app_click(px - ax, py - ay);
        return;
    }
    i = tb_hit(px, py);
    if (i >= 0) {
        if (i < NAPPS) launch(i);
        else if (i == 5) show_desktop();
        else if (i == 6) { sfocus = 1; menu_open = 0; dyn_redraw(); }
        else if (i == 8) { show_w ^= 1; st_dirty = 1; refresh_all(); }
        else if (i == 9) { clock24 ^= 1; st_dirty = 1; refresh_all(); }
        else if (i == 10 || i == 11) {
            int sx, sy, v; tb_sld(i - 10, &sx, &sy);
            v = ((px - sx) * 4 + 49) / 98; if (px < sx) v = 0; if (v > 4) v = 4;
            st_dirty = 1;
            if (i == 10) { mspeed = v; dyn_redraw(); } else { glass_lvl = v; refresh_all(); }
        }
        return;
    }
    lb_geom(&cx, &cy, &r);
    if ((px - cx) * (px - cx) + (py - cy) * (py - cy) <= r * r) { menu_open ^= 1; sfocus = 0; dyn_redraw_ex(0, 1); return; }
    if (menu_open) {
        int x, y, w, h; menu_rect(&x, &y, &w, &h);
        for (i = 0; i < NAPPS; i++) if (inrect(px, py, x + 8, y + 12 + i * 64, w - 16, 56)) { launch(i); return; }
        menu_open = 0; sfocus = 0; dyn_redraw_ex(0, 1); return;
    }
    if (sfocus) { sfocus = 0; dyn_redraw(); }
}
static int pend;                                    /* a redraw is waiting (hover / drag) */
static u32 ldm;
static void on_move(void) {
    int nm = -1, nt = tb_hit(mx, my), i, ch = 0;
    if (menu_open) { int x, y, w, h; menu_rect(&x, &y, &w, &h); for (i = 0; i < NAPPS; i++) if (inrect(mx, my, x + 8, y + 12 + i * 64, w - 16, 56)) nm = i; }
    if (open_app >= 0 && inrect(mx, my, ax, ay, AW, AH)) { nm = -1; nt = -1; }
    if (nm != hov_menu || nt != hov_tb) { hov_menu = nm; hov_tb = nt; ch = 1; }
    if (drag) {
        int nx = mx - dgx, ny = my - dgy;
        if (nx < 0) nx = 0; if (nx > W - AW) nx = W - AW; if (ny < 52) ny = 52; if (ny > H - AH - 8) ny = H - AH - 8;
        if (nx != ax || ny != ay) { ax = nx; ay = ny; ch = 1; }
    }
    if (open_app == 2 && inrect(mx, my, ax, ay, AW, AH)) ch = 1;      /* calculator button hover */
    if (ch) pend = 1;
}
/* returns 1 when the user asked to lock the screen */
static int desk_key(int c) {
    if (c == 27) {
        { int aa = 0, am = 0;
          if (sfocus) { sfocus = 0; sn = 0; sbuf[0] = 0; }
          else if (open_app >= 0) { open_app = -1; aa = 1; }
          else if (menu_open) { menu_open = 0; am = 1; }
          dyn_redraw_ex(aa, am); }
        return 0;
    }
    if (sfocus) {
        if (c == 10) { int m = search_match(); if (m >= 0) { launch(m); return 0; } }
        else if (c == 8) { if (sn) sbuf[--sn] = 0; }
        else if (c >= 32 && c < 127 && sn < 22) { sbuf[sn++] = (char)c; sbuf[sn] = 0; }
        dyn_redraw(); return 0;
    }
    if (open_app == 1) {
        if (c == 8) { if (nplen) nplen--; }
        else if (c == 10 || (c >= 32 && c < 127)) { if (nplen < (int)sizeof(npbuf) - 1) npbuf[nplen++] = (char)c; }
        else return 0;
        dyn_redraw(); return 0;
    }
    if (open_app == 3) {
        if (c == 10) term_exec();
        else if (c == 8) { if (tinn) tin[--tinn] = 0; }
        else if (c >= 32 && c < 127 && tinn < 60) { tin[tinn++] = (char)c; tin[tinn] = 0; }
        else return 0;
        if (want_lock) return 1;
        dyn_redraw(); return 0;
    }
    if (open_app == 2) {
        if (c >= '0' && c <= '9') calc_key(c);
        else if (c == '+' || c == '-' || c == '*' || c == '/') calc_key(c);
        else if (c == 10 || c == '=') calc_key('=');
        else if (c == 'c' || c == 'C') calc_key('C');
        else if (c == 8) calc_key('<');
        else return 0;
        dyn_redraw(); return 0;
    }
    if (c == 'l' || c == 'L') return 1;
    if (c == 9 || c == '/') { sfocus = 1; menu_open = 0; dyn_redraw(); }
    return 0;
}

static void desktop_screen(void) {
    u32 tc = 0; int lastm = -1, k;
    memcpy(SCR, BACK, (unsigned)W * H * 4);                  /* old frame for the crossfade */
    WALL = wp_wall();
    open_app = -1; menu_open = 1; sfocus = 0; drag = 0; sn = 0; sbuf[0] = 0; dr_w = 0; hov_menu = hov_tb = -1; want_lock = 0;
    if (!tln) term_init();
    build_desk(); draw_dyn_layers();
    crossfade(380);
    refresh_all();                                           /* SCR = clean desktop, then draw layers */
    kb_shift = 0; kb_ext = 0; cur_on = 0; mouse_reset();
    mx = W / 2; my = H / 2; cursor_draw();
    SNAP("desk0");
    for (;;) {
        mouse_drain();                                       /* take ALL waiting mouse bytes first */
        for (k = 0; k < 6; k++) {
            int c = key_char();
            if (c) { if (desk_key(c)) return; break; }
        }
        if (want_lock) { want_lock = 0; return; }
        if (drag && !(mbtn & 1)) drag = 0;
        if (mx != cur_x || my != cur_y) { on_move(); cursor_update(); }
        if (mclick) { mclick = 0; desk_click(mx, my); SNAP("click"); }
        if (st_dirty && !(mbtn & 1)) { st_dirty = 0; settings_save(); }
        if (pend && ms_now() - ldm >= 16) { pend = 0; ldm = ms_now(); dyn_redraw(); }   /* at most ~60 redraws / second */
        if (want_lock) { want_lock = 0; return; }
        if (ms_now() - tc > 400) {
            struct tm_ t; tc = ms_now(); rtc_time(&t);
            if (lastm >= 0 && t.m != lastm) refresh_all();
            lastm = t.m;
        }
        plat_wait(1);
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
        desktop_screen();
    }
}
