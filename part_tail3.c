
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
static const u8 MSP[5] = { 2, 3, 4, 6, 8 };

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
#define ST_SY 128
static void app_settings(void) {
    int sx = ax + ST_SX, sy = ay + ST_SY, i, kx; char v[16], n[12];
    draw_text(F_BIG, ax + 40, ay + 92, "Mouse speed", 0xFFFFFF, 250);
    u2s(n, (u32)MSP[mspeed] * 5 / 10, 1); v[0] = 0; scat(v, n); scat(v, "."); u2s(n, (u32)MSP[mspeed] * 5 % 10, 1); scat(v, n); scat(v, "x");
    text_r(F_UI, ax + 40 + ST_SW, ay + 90, v, ui_acc, 250);
    kx = sx + mspeed * ST_SW / 4;
    rrect(sx, sy, ST_SW, 8, 4, 0xFFFFFF, 55);
    rrect(sx, sy, kx - sx + 2, 8, 4, ui_acc, 256);
    for (i = 0; i < 5; i++) fill_rect(sx + i * ST_SW / 4 - 1, sy + 14, 2, 8, 0xFFFFFF, 90);
    disc(kx, sy + 4, 15, 0, 0xFFFFFF, 0xFFFFFF, 256);
    disc(kx, sy + 4, 9, 0, ui_acc, ui_acc, 256);
    draw_text(F_UI, sx - 2, sy + 52, "Slow", 0x8E99A2, 230);
    text_r(F_UI, sx + ST_SW + 2, sy + 52, "Fast", 0x8E99A2, 230);
    draw_text(F_BIG, ax + 40, ay + 238, "Wallpaper", 0xFFFFFF, 250);
    for (i = 0; i < 2; i++) {
        int bx = ax + 40 + i * 220, by = ay + 258;
        rrect(bx, by, 200, 52, 26, ui_acc, wp_choice == i ? 256 : 70);
        if (wp_choice != i) rrect(bx + 1, by + 1, 198, 50, 25, 0x08161C, 235);
        text_c(F_UI, bx + 100, by + 34, i == 0 ? "Night City" : "Desert", wp_choice == i ? 0x06141A : 0xFFFFFF, 250);
    }
    draw_text(F_BIG, ax + 40, ay + 360, "Session", 0xFFFFFF, 250);
    rrect(ax + 40, ay + 380, 200, 52, 26, 0xFFFFFF, 70);
    rrect(ax + 41, ay + 381, 198, 50, 25, 0x08161C, 235);
    text_c(F_UI, ax + 140, ay + 414, "Lock screen", 0xFFFFFF, 250);
    {
        char cpu[64], r[40]; cpu_brand(cpu);
        u2s(n, (u32)W, 1); r[0] = 0; scat(r, n); scat(r, " x "); u2s(n, (u32)H, 1); scat(r, n);
        text_fit(F_UI, ax + 280, ay + 408, cpu, AW - 280 - 30, 0xAEB8BE, 230);
        draw_text(F_UI, ax + 280, ay + 438, "Nexora OS v1.0", 0xAEB8BE, 230);
        text_r(F_UI, ax + AW - 30, ay + 438, r, 0xAEB8BE, 230);
    }
}
static void draw_app(void) {
    win_frame(ax, ay, AW, AH, APPN[open_app], 215);
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
static void dyn_redraw(void) {
    int mx_, my_, mw_, mh_, lx, ly, lw, lh, tx, ty, tw, th;
    menu_rect(&mx_, &my_, &mw_, &mh_); lb_rect(&lx, &ly, &lw, &lh); tb_rect(&tx, &ty, &tw, &th);
    restore_desk(mx_ - 4, my_ - 4, mw_ + 8, mh_ + 8);
    restore_desk(lx, ly, lw, lh);
    restore_desk(tx - 4, ty - 4, tw + 8, th + 8);
    if (dr_w) restore_desk(dr_x, dr_y, dr_w, AH);
    if (open_app >= 0) restore_desk(ax, ay, AW, AH);
    draw_dyn_layers();
    present(mx_ - 4, my_ - 4, mw_ + 8, mh_ + 8);
    present(lx, ly, lw, lh);
    present(tx - 4, ty - 4, tw + 8, th + 8);
    if (dr_w) present(dr_x, dr_y, dr_w, AH);
    if (open_app >= 0) present(ax, ay, AW, AH);
    if (open_app >= 0) { dr_x = ax; dr_y = ay; dr_w = AW; } else dr_w = 0;
    cursor_draw();
}
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
    open_app = i; menu_open = 0; sfocus = 0; sn = 0; sbuf[0] = 0;
    dyn_redraw();
}
static void show_desktop(void) { open_app = -1; menu_open = 0; sfocus = 0; dyn_redraw(); }
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
            mspeed = v; dyn_redraw();
        } else if (ry >= 258 && ry < 310 && rx >= 40 && rx < 240) { if (wp_choice != 0) { wp_choice = 0; WALL = WALL_N; refresh_all(); } }
        else if (ry >= 258 && ry < 310 && rx >= 260 && rx < 460) { if (wp_choice != 1) { wp_choice = 1; WALL = WALL_D; refresh_all(); } }
        else if (ry >= 380 && ry < 432 && rx >= 40 && rx < 240) want_lock = 1;
    }
}
static void desk_click(int px, int py) {
    int i, cx, cy, r;
    if (open_app >= 0 && inrect(px, py, ax, ay, AW, AH)) {
        sfocus = 0;
        if (py < ay + 44) {
            if (px >= ax + AW - 40) { open_app = -1; dyn_redraw(); }
            else if (px >= ax + AW - 72) { }
            else if (px >= ax + AW - 104) { open_app = -1; dyn_redraw(); }
            else { drag = 1; dgx = px - ax; dgy = py - ay; }
        } else app_click(px - ax, py - ay);
        return;
    }
    i = tb_hit(px, py);
    if (i >= 0) {
        if (i < NAPPS) launch(i);
        else if (i == 5) show_desktop();
        else if (i == 6) { sfocus = 1; menu_open = 0; dyn_redraw(); }
        return;
    }
    lb_geom(&cx, &cy, &r);
    if ((px - cx) * (px - cx) + (py - cy) * (py - cy) <= r * r) { menu_open ^= 1; sfocus = 0; dyn_redraw(); return; }
    if (menu_open) {
        int x, y, w, h; menu_rect(&x, &y, &w, &h);
        for (i = 0; i < NAPPS; i++) if (inrect(px, py, x + 8, y + 12 + i * 64, w - 16, 56)) { launch(i); return; }
        menu_open = 0; sfocus = 0; dyn_redraw(); return;
    }
    if (sfocus) { sfocus = 0; dyn_redraw(); }
}
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
    if (ch) dyn_redraw();
}
/* returns 1 when the user asked to lock the screen */
static int desk_key(int c) {
    if (c == 27) {
        if (sfocus) { sfocus = 0; sn = 0; sbuf[0] = 0; }
        else if (open_app >= 0) open_app = -1;
        else menu_open = 0;
        dyn_redraw(); return 0;
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
    WALL = wp_choice ? WALL_D : WALL_N;
    open_app = -1; menu_open = 1; sfocus = 0; drag = 0; sn = 0; sbuf[0] = 0; dr_w = 0; hov_menu = hov_tb = -1; want_lock = 0;
    if (!tln) term_init();
    build_desk(); draw_dyn_layers();
    crossfade(700);
    refresh_all();                                           /* SCR = clean desktop, then draw layers */
    kb_shift = 0; kb_ext = 0; cur_on = 0; mouse_reset();
    mx = W / 2; my = H / 2; cursor_draw();
    SNAP("desk0");
    for (;;) {
        for (k = 0; k < 6; k++) {
            int c = key_char();
            if (c) { if (desk_key(c)) return; break; }
        }
        if (want_lock) { want_lock = 0; return; }
        if (drag && !(mbtn & 1)) drag = 0;
        if (mx != cur_x || my != cur_y) { on_move(); cursor_update(); }
        if (mclick) { mclick = 0; desk_click(mx, my); SNAP("click"); }
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
        loading_screen();
        desktop_screen();
    }
}
