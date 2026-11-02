#define WIDTH 320
#define HEIGHT 200
#define FRAMEBUFFER 0xA0000 // VGA Mode 13h address

unsigned char* framebuffer = (unsigned char*) FRAMEBUFFER;

// VGA Palette colors (0 to 255)
#define BLACK 0
#define BLUE 1
#define GREEN 2
#define CYAN 3
#define RED 4
#define MAGENTA 5
#define BROWN 6
#define LIGHT_GREY 7
#define DARK_GREY 8
#define LIGHT_BLUE 9
#define LIGHT_GREEN 10
#define LIGHT_CYAN 11
#define LIGHT_RED 12
#define LIGHT_MAGENTA 13
#define YELLOW 14
#define WHITE 15

void draw_pixel(int x, int y, unsigned char color) {
    if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
        framebuffer[y * WIDTH + x] = color;
    }
}

void draw_rect(int x, int y, int w, int h, unsigned char color) {
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            draw_pixel(x + j, y + i, color);
        }
    }
}

void draw_line(int x1, int y1, int x2, int y2, unsigned char color) {
    int dx = x2 - x1;
    int dy = y2 - y1;
    int steps = (dx > dy) ? dx : dy;
    if (steps == 0) return;
    
    float x_inc = (float)dx / steps;
    float y_inc = (float)dy / steps;
    float x = x1;
    float y = y1;
    
    for (int i = 0; i <= steps; i++) {
        draw_pixel((int)x, (int)y, color);
        x += x_inc;
        y += y_inc;
    }
}

void kmain() {
    // 1. Desktop Background (Blue)
    draw_rect(0, 0, WIDTH, HEIGHT, BLUE);

    // 2. Taskbar (Grey)
    draw_rect(0, HEIGHT - 20, WIDTH, 20, LIGHT_GREY);

    // 3. Start Button (Dark Grey)
    draw_rect(2, HEIGHT - 18, 40, 16, DARK_GREY);
    
    // 4. Window (White with Blue Title)
    draw_rect(40, 30, 200, 120, WHITE);       // Window Body
    draw_rect(40, 30, 200, 15, DARK_GREY);    // Title Bar

    // 5. Draw a Red 'X' (Close button)
    draw_line(225, 33, 235, 42, RED);
    draw_line(235, 33, 225, 42, RED);

    // 6. Draw a Yellow folder icon
    draw_rect(60, 60, 30, 20, YELLOW);
    draw_rect(60, 55, 15, 5, YELLOW);

    // 7. Draw Cyan Lines to make it look like a UI
    draw_rect(60, 90, 150, 2, CYAN);
    draw_rect(60, 110, 100, 2, CYAN);

    while(1) {}
}
