#define WIDTH  1920
#define HEIGHT 1080

unsigned int* framebuffer;

#define BLACK      0xFF000000
#define WHITE      0xFFFFFFFF
#define BLUE       0xFF0078D7
#define DARK_GREY  0xFF1E1E1E
#define LIGHT_GREY 0xFFCCCCCC
#define RED        0xFFFF0000
#define CYAN       0xFF00FFFF
#define YELLOW     0xFFFFFF00
#define GREEN      0xFF00FF00
#define TEAL       0xFF008080

void draw_pixel(int x, int y, unsigned int color) {
    if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
        framebuffer[y * WIDTH + x] = color;
    }
}

void draw_rect(int x, int y, int w, int h, unsigned int color) {
    for (int i = 0; i < h; i++)
        for (int j = 0; j < w; j++)
            draw_pixel(x + j, y + i, color);
}

void draw_nexora_logo(int x, int y) {
    draw_rect(x + 10, y + 10, 200, 200, BLACK); // Shadow
    draw_rect(x, y, 90, 180, BLUE);
    draw_rect(x + 15, y + 15, 60, 150, CYAN);
    draw_rect(x + 110, y, 90, 180, LIGHT_GREY);
    draw_rect(x + 125, y + 15, 60, 150, WHITE);
    draw_rect(x + 85, y + 80, 25, 25, BLUE);
}

unsigned char read_key() {
    unsigned char status;
    __asm__ volatile ("inb %1, %0" : "=a"(status) : "Nd"((unsigned short)0x64));
    if ((status & 1) == 0) return 0;
    unsigned char scancode;
    __asm__ volatile ("inb %1, %0" : "=a"(scancode) : "Nd"((unsigned short)0x60));
    if (scancode & 0x80) return 0;
    return scancode;
}

void delay(int count) {
    for (volatile int i = 0; i < count; i++);
}

void kmain(unsigned int fb_addr) {
    framebuffer = (unsigned int*) (fb_addr ? fb_addr : 0xFD000000);
    int state = 0;
    int cx = WIDTH / 2;
    int cy = HEIGHT / 2;

    // Step 1: Teal screen
    draw_rect(0, 0, WIDTH, HEIGHT, TEAL);
    delay(30000000);

    // Step 2: Logo + loading bar
    draw_rect(0, 0, WIDTH, HEIGHT, DARK_GREY);
    draw_nexora_logo(cx - 100, cy - 200);

    draw_rect(cx - 200, cy + 150, 400, 10, LIGHT_GREY);
    for (int i = 0; i <= 380; i += 10) {
        draw_rect(cx - 195, cy + 152, i, 6, CYAN);
        delay(500000);
    }
    delay(1000000);

    // Step 3: Lock screen
    state = 1;
    draw_rect(0, 0, WIDTH, HEIGHT, DARK_GREY);
    draw_nexora_logo(cx - 100, cy - 400);

    draw_rect(cx - 50, cy - 100, 100, 100, LIGHT_GREY); // User box
    draw_rect(cx - 30, cy - 80, 60, 60, CYAN);

    draw_rect(cx - 150, cy + 60, 300, 40, BLACK);       // Password box
    draw_rect(cx - 146, cy + 64, 292, 32, DARK_GREY);
    draw_rect(cx - 142, cy + 68, 284, 24, BLACK);

    // Main loop
    while (1) {
        unsigned char key = read_key();

        if (state == 1 && key == 0x1C) {   // Enter
            state = 2;

            draw_rect(0, 0, WIDTH, HEIGHT, TEAL);                 // Desktop
            draw_rect(0, HEIGHT - 68, WIDTH, 68, DARK_GREY);      // Taskbar

            int wx = cx - 312;
            int wy = cy - 240;
            draw_rect(wx, wy, 624, 400, DARK_GREY);               // Window
            draw_rect(wx, wy, 624, 40, BLACK);                    // Title bar

            draw_rect(wx + 20, wy + 15, 15, 15, RED);
            draw_rect(wx + 50, wy + 15, 15, 15, YELLOW);
            draw_rect(wx + 80, wy + 15, 15, 15, GREEN);
        }
    }
}
