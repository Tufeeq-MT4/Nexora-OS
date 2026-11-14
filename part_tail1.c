/* ======================================================================
 *  NEW UI : small helpers
 * ====================================================================== */
static u32 ui_acc = 0x2FD8D0;      /* accent colour (cyan on desktop, blue on lock screen) */
static int ui_dark = 1;            /* 1 = dark cyan glass (desktop), 0 = light glass (lock) */

static void panel(int x, int y, int w, int h, int r) {
    if (ui_dark) { rrect(x, y, w, h, r, 0x4FE3DC, 64); rrect(x + 1, y + 1, w - 2, h - 2, r - 1, 0x08161C, 150); }
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
    draw_text(F_UI, x + 18, y + 146, "No driver yet", 0x8E99A2, 220);
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
    disc(fx + fw - 31, fy + 29, 23, 0, 0x4C8DFF, 0x3466D6, 256);
    text_c(F_BIG, fx + fw - 31, fy + 29 + 12, ">", 0xFFFFFF, 256);
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
    cv = BACK; ui_dark = 0; ui_acc = 0x4C8DFF; gl_fill = 76; gl_edge = 60;      /* transparent glass */
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
    WALL = WALL_D;
    memcpy(SCR, BACK, (unsigned)W * H * 4);
    compose_lock1();
    crossfade(800);
    SNAP("lock1");
    mouse_reset();
    while (plat_key() >= 0) { }                             /* flush old key presses */
    kb_shift = 0; kb_ext = 0;
    for (;;) {                                              /* step 1: wait for Enter */
        int c = key_char();
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
    crossfade(350);
    SNAP("lock2");
    for (;;) {
        int c = key_char();
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
                  SNAP("lock2_ok"); plat_wait(450); return;
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
        if (e > 2500) SNAP("loading");
        frame_wait(f0, 40);
    }
}
