
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
static int sfocus, wp_choice, want_lock;
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
static void tb_rect(int *x, int *y, int *w, int *h) { *w = 1040; *x = (W - *w) / 2; if (*x < 220) *x = 220; *y = H - 140; *h = 96; }
static void tb_search(int *x, int *y, int *w, int *h) { int px, py, pw, ph; tb_rect(&px, &py, &pw, &ph); *x = px + 104; *y = py + 22; *w = 300; *h = 52; }
static void tb_icon(int i, int *cx, int *cy) { int px, py, pw, ph; tb_rect(&px, &py, &pw, &ph); *cx = px + 104 + 300 + 24 + 44 + i * 88; *cy = py + 34; }
static void tb_desk(int *x, int *y, int *w, int *h) { int px, py, pw, ph; tb_rect(&px, &py, &pw, &ph); *x = px + 104 + 300 + 24 + 440 + 24; *y = py + 22; *w = 112; *h = 52; }
/* what is under (px,py) in the taskbar: 0..4 app, 5 desktop, 6 search, 7 clock, -1 nothing */
static int tb_hit(int px, int py) {
    int x, y, w, h, i, cx, cy;
    tb_rect(&x, &y, &w, &h);
    if (!inrect(px, py, x, y, w, h)) return -1;
    for (i = 0; i < NAPPS; i++) { tb_icon(i, &cx, &cy); if (px >= cx - 44 && px < cx + 44) return i; }
    tb_search(&x, &y, &w, &h); if (inrect(px, py, x, y, w, h)) return 6;
    tb_desk(&x, &y, &w, &h);   if (inrect(px, py, x, y, w, h)) return 5;
    tb_rect(&x, &y, &w, &h);   if (px < x + 90) return 7;
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
    full[0] = 0; scat(full, ts); scat(full, " "); scat(full, ap);
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
static void showcase(void) {
    static const char *SB[5] = { "Dashboard", "System", "Display", "Storage", "Network" };
    char buf[64], cpu[64], n[12]; int x = W / 2 - 510, y = 300, i, px, pw;
    /* --- dashboard window --- */
    win_frame(x, y, 500, 340, "NEXORA OS", 150);
    for (i = 0; i < 5; i++) {
        int iy = y + 58 + i * 38;
        if (i == 0) { rrect(x + 12, iy, 126, 32, 10, ui_acc, 55); rrect(x + 12, iy + 6, 3, 20, 1, ui_acc, 256); }
        draw_text(F_UI, x + 26, iy + 23, SB[i], i == 0 ? 0xFFFFFF : 0xAEB8BE, 240);
    }
    px = x + 152; pw = 500 - 152 - 14;
    rrect(px, y + 58, pw, 138, 12, 0xFFFFFF, 16);
    draw_text(F_UI, px + 14, y + 86, "System Info", 0xFFFFFF, 245);
    draw_text(F_UI, px + 14, y + 118, "OS", 0xAEB8BE, 230);      draw_text(F_UI, px + 92, y + 118, "Nexora OS v1.0", 0xFFFFFF, 245);
    cpu_brand(cpu);
    draw_text(F_UI, px + 14, y + 148, "CPU", 0xAEB8BE, 230);     text_fit(F_UI, px + 92, y + 148, cpu, pw - 92 - 12, 0xFFFFFF, 245);
    u2s(n, (u32)W, 1); buf[0] = 0; scat(buf, n); scat(buf, " x "); u2s(n, (u32)H, 1); scat(buf, n);
    draw_text(F_UI, px + 14, y + 178, "Display", 0xAEB8BE, 230); draw_text(F_UI, px + 92, y + 178, buf, 0xFFFFFF, 245);
    rrect(px, y + 208, pw, 118, 12, 0xFFFFFF, 16);
    draw_text(F_UI, px + 14, y + 236, "Performance", 0xFFFFFF, 245);
    for (i = 0; i < 3; i++) fill_rect(px + 14, y + 256 + i * 20, pw - 28, 1, 0xFFFFFF, 22);
    for (i = 1; i < 40; i++) {
        int x0 = px + 14 + (i - 1) * (pw - 28) / 39, x1 = px + 14 + i * (pw - 28) / 39;
        int v0 = 52 + isin((i - 1) * 40 + 100) * 20 / 1024 + isin((i - 1) * 97) * 8 / 1024;
        int v1 = 52 + isin(i * 40 + 100) * 20 / 1024 + isin(i * 97) * 8 / 1024;
        line(x0, y + 318 - v0 * 54 / 100 - 10, x1, y + 318 - v1 * 54 / 100 - 10, 2, ui_acc, 256);
    }
    /* --- code editor window --- */
    {
        int ex = W / 2 + 90, ey = 330, tx, yy;
        static const char *NUM[8] = { "1", "2", "3", "4", "5", "6", "7", "8" };
        win_frame(ex, ey, 480, 320, "kernel.c - Nexora Editor", 150);
        for (i = 0; i < 8; i++) { yy = ey + 80 + i * 28; text_r(F_UI, ex + 46, yy, NUM[i], 0x6C7A84, 220); }
        tx = ex + 62; yy = ey + 80;
        tx = draw_text(F_UI, tx, yy, "void ", 0x4FE3DC, 250); draw_text(F_UI, tx, yy, "kmain(void) {", 0xF2D27A, 250);
        draw_text(F_UI, ex + 90, yy + 28, "plat_init();", 0xDDE6EA, 245);
        draw_text(F_UI, ex + 90, yy + 56, "boot_sequence();", 0xDDE6EA, 245);
        tx = draw_text(F_UI, ex + 90, yy + 84, "for ", 0x4FE3DC, 250); draw_text(F_UI, tx, yy + 84, "(;;) {", 0xDDE6EA, 245);
        draw_text(F_UI, ex + 118, yy + 112, "lock_screen();", 0xDDE6EA, 245);
        draw_text(F_UI, ex + 118, yy + 140, "desktop_screen();", 0xDDE6EA, 245);
        draw_text(F_UI, ex + 90, yy + 168, "}", 0xDDE6EA, 245);
        draw_text(F_UI, ex + 62, yy + 196, "}", 0xDDE6EA, 245);
    }
    /* --- small terminal window --- */
    {
        int tx = W / 2 + 420, ty = 520;
        win_frame(tx, ty, 270, 210, "Terminal", 175);
        draw_text(F_UI, tx + 16, ty + 80, "nexora> ver", 0x7CF29C, 245);
        draw_text(F_UI, tx + 16, ty + 110, "Nexora OS v1.0", 0xDDE6EA, 240);
        draw_text(F_UI, tx + 16, ty + 140, "nexora> help", 0x7CF29C, 245);
        draw_text(F_UI, tx + 16, ty + 170, "help ver time cpu", 0xAEB8BE, 230);
    }
}

/* ---- the static desktop (everything that never changes between clicks) ---- */
static void build_desk(void) {
    memcpy(BACK, WALL, (unsigned)W * H * 4);
    cv = BACK; ui_dark = 1; ui_acc = 0x2FD8D0;
    topbar();
    w_status(W - 236, 68, 210, 170);
    w_calendar(W - 236, 252, 210, 246);
    w_network(W - 236, 512, 210, 160);
    showcase();
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
    int px, py, pw, ph, i, cx, cy, sx, sy, sw, sh, dx, dy, dw, dh; struct tm_ t;
    tb_rect(&px, &py, &pw, &ph);
    panel(px, py, pw, ph, 48);
    rrect(px + 70, py, pw - 140, 2, 1, ui_acc, 150);
    /* analog clock button */
    cx = px + 56; cy = py + 48; rtc_time(&t);
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
        if (hov_tb == i) rrect(cx - 40, py + 6, 80, 84, 16, 0xFFFFFF, 28);
        app_icon(i, cx, cy, 26);
        text_c(F_UI, cx, py + 86, APPS[i], 0xFFFFFF, 235);
        if (open_app == i) rrect(cx - 10, py + 2, 20, 3, 1, ui_acc, 256);
    }
    /* separator + desktop button */
    tb_desk(&dx, &dy, &dw, &dh);
    fill_rect(dx - 14, py + 20, 1, 56, 0xFFFFFF, 44);
    rrect(dx, dy, dw, dh, 16, 0xFFFFFF, hov_tb == 5 ? 90 : 40);
    rrect(dx + 1, dy + 1, dw - 2, dh - 2, 15, 0x08161C, 215);
    rrect(dx + 14, dy + 18, 20, 14, 3, ui_acc, 256);
    rrect(dx + 17, dy + 21, 14, 8, 2, 0x08161C, 256);
    draw_text(F_UI, dx + 42, dy + 33, "Desktop", 0xFFFFFF, 245);
}
