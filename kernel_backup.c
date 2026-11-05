#define WIDTH 1024
#define HEIGHT 768
#define FRAMEBUFFER 0xFD000000 // HD Framebuffer

unsigned int* framebuffer = (unsigned int*) FRAMEBUFFER;

// 32-bit Colors (ARGB) - Mac Style
#define BLACK 0xFF000000
#define WHITE 0xFFFFFFFF
#define BLUE 0xFF0078D7
#define DARK_GREY 0xFF1E1E1E
#define LIGHT_GREY 0xFFCCCCCC
#define RED 0xFFFF0000
#define CYAN 0xFF00FFFF
#define YELLOW 0xFFFFFF00
#define TEAL 0xFF008080
#define GREEN 0xFF00FF00

void draw_pixel(int x, int y, unsigned int color) {
    if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
        framebuffer[y * WIDTH + x] = color;
    }
}

void draw_rect(int x, int y, int w, int h, unsigned int color) {
    for (int i = 0; i < h; i++) for (int j = 0; j < w; j++) draw_pixel(x + j, y + i, color);
}

void draw_rounded_rect(int x, int y, int w, int h, int radius, unsigned int color) {
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            int dx = (j < radius) ? radius - j : (j > w - radius - 1 ? j - (w - radius - 1) : 0);
            int dy = (i < radius) ? radius - i : (i > h - radius - 1 ? i - (h - radius - 1) : 0);
            if (dx * dx + dy * dy <= radius * radius) {
                draw_pixel(x + j, y + i, color);
            }
        }
    }
}

// Aap ka 3D Nexora Logo (HD Style)
void draw_nexora_logo(int x, int y) {
    // Shadow
    draw_rounded_rect(x + 10, y + 10, 200, 200, 20, BLACK);
    
    // Left Blue Hexagon
    draw_rounded_rect(x, y, 90, 180, 10, BLUE);
    draw_rounded_rect(x + 15, y + 15, 60, 150, 10, CYAN);
    
    // Right White Hexagon
    draw_rounded_rect(x + 110, y, 90, 180, 10, LIGHT_GREY);
    draw_rounded_rect(x + 125, y + 15, 60, 150, 10, WHITE);
    
    // Center Connection
    draw_rect(x + 85, y + 80, 25, 25, BLUE);
}

unsigned char read_key() {
    unsigned char status;
    __asm__ volatile ("inb %1, %0" : "=a"(status) : "Nd"(0x64));
    if ((status & 1) == 0) return 0; 
    unsigned char scancode;
    __asm__ volatile ("inb %1, %0" : "=a"(scancode) : "Nd"(0x60));
    if (scancode & 0x80) return 0;
    return scancode;
}

void delay(int count) {
    for(volatile int i = 0; i < count; i++);
}

void kmain() {
    int state = 0; 

    // --- STEP 1: HD LOGO & LOADING BAR ---
    draw_rect(0, 0, WIDTH, HEIGHT, DARK_GREY);
    draw_nexora_logo(412, 200);
    
    // Loading Bar (HD)
    draw_rounded_rect(312, 500, 400, 20, 10, LIGHT_GREY);
    for (int i = 0; i <= 380; i += 10) {
        draw_rounded_rect(317, 505, i, 10, 5, BLUE);
        delay(500000);
    }
    delay(1000000);

    // --- STEP 2: MAC STYLE LOCK SCREEN ---
    state = 1;
    draw_rect(0, 0, WIDTH, HEIGHT, BLACK); // Black background
    
    // User Avatar
    draw_rounded_rect(462, 200, 100, 100, 50, LIGHT_GREY);
    draw_rounded_rect(482, 220, 60, 60, 30, DARK_GREY);
    
    // Password Box (Rounded)
    draw_rounded_rect(362, 400, 300, 50, 25, DARK_GREY);
    draw_rounded_rect(367, 405, 290, 40, 20, LIGHT_GREY);
    
    // Text "Enter Password"
    // (Abhi text nahi draw kar rahe kyunke HD font lamba hai, par box Mac jaisa lagega)

    // --- MAIN LOOP ---
    while(1) {
        unsigned char key = read_key();
        
        // LOCK SCREEN (Password Check)
        if (state == 1) {
            if (key == 0x1C) { // Enter dabaya
                // Password Correct! Transition to Desktop
                state = 2;
                draw_rect(0, 0, WIDTH, HEIGHT, TEAL); // Desktop background
                
                // Taskbar (Mac Style)
                draw_rounded_rect(212, 700, 600, 50, 25, LIGHT_GREY);
                
                // Icons on Taskbar
                draw_rounded_rect(230, 710, 30, 30, 10, BLUE);
                draw_rounded_rect(280, 710, 30, 30, 10, YELLOW);
                draw_rounded_rect(330, 710, 30, 30, 10, RED);
                
                // Window (Mac Style)
                draw_rounded_rect(200, 150, 600, 400, 15, WHITE);
                draw_rounded_rect(200, 150, 600, 40, 15, LIGHT_GREY); // Title Bar
                
                // Close Button
                draw_rounded_rect(215, 165, 15, 15, 7, RED);
                draw_rounded_rect(240, 165, 15, 15, 7, YELLOW);
                draw_rounded_rect(265, 165, 15, 15, 7, GREEN);
            }
        }
    }
}
