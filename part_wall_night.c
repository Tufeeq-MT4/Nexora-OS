
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
    c = mix(0x060A0E, 0x1B2A32, t);
    if (t > 170) c = mix(c, 0x2B4048, (t - 170) * 2 / 3);
    for (l = 0; l < 2; l++) {
        if (y >= nbt[l][x]) {
            u32 col = l ? 0x0A1116 : 0x14212A, a = (u32)(y - nbt[l][x]) * 40u;
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
static void build_wall_night(u32 *dst) {
    int x, y;
    night_prep();
    for (y = 0; y < H; y++) {
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
