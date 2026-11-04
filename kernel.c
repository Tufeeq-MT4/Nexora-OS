#define WIDTH 320
#define HEIGHT 200
#define FRAMEBUFFER 0xA0000

unsigned char* framebuffer = (unsigned char*) FRAMEBUFFER;

// VGA Colors
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

// POORA FONT DATA
unsigned char font[128][8] = {
    ['A'] = {0b00011000, 0b00111100, 0b01100110, 0b01100110, 0b01111110, 0b01100110, 0b01100110, 0b00000000},
    ['B'] = {0b01111100, 0b01100110, 0b01100110, 0b01111100, 0b01100110, 0b01100110, 0b01111100, 0b00000000},
    ['C'] = {0b00111100, 0b01100110, 0b01100000, 0b01100000, 0b01100000, 0b01100110, 0b00111100, 0b00000000},
    ['D'] = {0b01111100, 0b01100110, 0b01100110, 0b01100110, 0b01100110, 0b01100110, 0b01111100, 0b00000000},
    ['E'] = {0b01111110, 0b01100000, 0b01100000, 0b01111100, 0b01100000, 0b01100000, 0b01111110, 0b00000000},
    ['F'] = {0b01111110, 0b01100000, 0b01100000, 0b01111100, 0b01100000, 0b01100000, 0b01100000, 0b00000000},
    ['G'] = {0b00111100, 0b01100110, 0b01100000, 0b01101110, 0b01100110, 0b01100110, 0b00111100, 0b00000000},
    ['H'] = {0b01100110, 0b01100110, 0b01100110, 0b01111110, 0b01100110, 0b01100110, 0b01100110, 0b00000000},
    ['I'] = {0b00111100, 0b00011000, 0b00011000, 0b00011000, 0b00011000, 0b00011000, 0b00111100, 0b00000000},
    ['J'] = {0b00000110, 0b00000110, 0b00000110, 0b00000110, 0b01100110, 0b01100110, 0b00111100, 0b00000000},
    ['K'] = {0b01100110, 0b01101100, 0b01111000, 0b01110000, 0b01111000, 0b01101100, 0b01100110, 0b00000000},
    ['L'] = {0b01100000, 0b01100000, 0b01100000, 0b01100000, 0b01100000, 0b01100000, 0b01111110, 0b00000000},
    ['M'] = {0b01100011, 0b01110111, 0b01111111, 0b01101011, 0b01100011, 0b01100011, 0b01100011, 0b00000000},
    ['N'] = {0b01100011, 0b01110011, 0b01111011, 0b01101111, 0b01100111, 0b01100011, 0b01100011, 0b00000000},
    ['O'] = {0b00111100, 0b01100110, 0b01100110, 0b01100110, 0b01100110, 0b01100110, 0b00111100, 0b00000000},
    ['P'] = {0b01111100, 0b01100110, 0b01100110, 0b01111100, 0b01100000, 0b01100000, 0b01100000, 0b00000000},
    ['Q'] = {0b00111100, 0b01100110, 0b01100110, 0b01100110, 0b01101110, 0b01100110, 0b00111101, 0b00000000},
    ['R'] = {0b01111100, 0b01100110, 0b01100110, 0b01111100, 0b01111000, 0b01101100, 0b01100110, 0b00000000},
    ['S'] = {0b00111100, 0b01100110, 0b01100000, 0b00111100, 0b00000110, 0b01100110, 0b00111100, 0b00000000},
    ['T'] = {0b01111110, 0b00011000, 0b00011000, 0b00011000, 0b00011000, 0b00011000, 0b00011000, 0b00000000},
    ['U'] = {0b01100110, 0b01100110, 0b01100110, 0b01100110, 0b01100110, 0b01100110, 0b00111100, 0b00000000},
    ['V'] = {0b01100110, 0b01100110, 0b01100110, 0b01100110, 0b01100110, 0b00111100, 0b00011000, 0b00000000},
    ['W'] = {0b01100011, 0b01100011, 0b01100011, 0b01101011, 0b01111111, 0b01110111, 0b01100011, 0b00000000},
    ['X'] = {0b01100110, 0b01100110, 0b00111100, 0b00011000, 0b00111100, 0b01100110, 0b01100110, 0b00000000},
    ['Y'] = {0b01100110, 0b01100110, 0b01100110, 0b00111100, 0b00011000, 0b00011000, 0b00011000, 0b00000000},
    ['Z'] = {0b01111110, 0b00000110, 0b00001100, 0b00011000, 0b00110000, 0b01100000, 0b01111110, 0b00000000},
    ['0'] = {0b00111100, 0b01100110, 0b01101110, 0b01110110, 0b01100110, 0b01100110, 0b00111100, 0b00000000},
    ['1'] = {0b00011000, 0b00111000, 0b00011000, 0b00011000, 0b00011000, 0b00011000, 0b00111100, 0b00000000},
    ['2'] = {0b00111100, 0b01100110, 0b00000110, 0b00001100, 0b00110000, 0b01100000, 0b01111110, 0b00000000},
    ['3'] = {0b00111100, 0b01100110, 0b00000110, 0b00011100, 0b00000110, 0b01100110, 0b00111100, 0b00000000},
    ['4'] = {0b00001100, 0b00011100, 0b00101100, 0b01001100, 0b01111110, 0b00001100, 0b00001100, 0b00000000},
    ['5'] = {0b01111110, 0b01100000, 0b01111100, 0b00000110, 0b00000110, 0b01100110, 0b00111100, 0b00000000},
    ['6'] = {0b00111100, 0b01100000, 0b01100000, 0b01111100, 0b01100110, 0b01100110, 0b00111100, 0b00000000},
    ['7'] = {0b01111110, 0b00000110, 0b00001100, 0b00011000, 0b00110000, 0b00110000, 0b00110000, 0b00000000},
    ['8'] = {0b00111100, 0b01100110, 0b01100110, 0b00111100, 0b01100110, 0b01100110, 0b00111100, 0b00000000},
    ['9'] = {0b00111100, 0b01100110, 0b01100110, 0b00111110, 0b00000110, 0b00000110, 0b00111100, 0b00000000},
    [':'] = {0b00000000, 0b00011000, 0b00011000, 0b00000000, 0b00000000, 0b00011000, 0b00011000, 0b00000000},
    [' '] = {0, 0, 0, 0, 0, 0, 0, 0}
};

void draw_pixel(int x, int y, unsigned char color) {
    if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) framebuffer[y * WIDTH + x] = color;
}

void draw_rect(int x, int y, int w, int h, unsigned char color) {
    for (int i = 0; i < h; i++) for (int j = 0; j < w; j++) draw_pixel(x + j, y + i, color);
}

void draw_text(char* str, int x, int y, unsigned char color) {
    int i = 0;
    while (str[i] != '\0') {
        for (int row = 0; row < 8; row++) {
            unsigned char bits = font[(int)str[i]][row];
            for (int col = 0; col < 8; col++) {
                if (bits & (1 << (7 - col))) {
                    draw_pixel(x + (i * 8) + col, y + row, color);
                }
            }
        }
        i++;
    }
}

void draw_line(int x1, int y1, int x2, int y2, unsigned char color) {
    int dx = x2 - x1, dy = y2 - y1;
    int steps = (dx > dy) ? dx : dy;
    if (steps == 0) return;
    float x_inc = (float)dx / steps, y_inc = (float)dy / steps;
    float x = x1, y = y1;
    for (int i = 0; i <= steps; i++) {
        draw_pixel((int)x, (int)y, color);
        x += x_inc; y += y_inc;
    }
}

void draw_mouse(int x, int y) {
    unsigned char cursor[16][12] = {
        {2,0,0,0,0,0,0,0,0,0,0,0},{2,2,0,0,0,0,0,0,0,0,0,0},{2,1,2,0,0,0,0,0,0,0,0,0},
        {2,1,1,2,0,0,0,0,0,0,0,0},{2,1,1,1,2,0,0,0,0,0,0,0},{2,1,1,1,1,2,0,0,0,0,0,0},
        {2,1,1,1,1,1,2,0,0,0,0,0},{2,1,1,1,1,1,1,2,0,0,0,0},{2,1,1,1,1,1,1,1,2,0,0,0},
        {2,1,1,1,1,1,1,1,1,2,0,0},{2,1,1,1,1,1,2,2,2,2,0,0},{2,1,1,1,2,1,1,2,0,0,0,0},
        {2,1,1,2,0,2,1,1,2,0,0,0},{2,1,2,0,0,2,1,1,2,0,0,0},{2,2,0,0,0,0,2,1,1,2,0,0},
        {0,0,0,0,0,0,0,2,2,0,0,0}
    };
    for (int i = 0; i < 16; i++) for (int j = 0; j < 12; j++) {
        if (cursor[i][j] == 1) draw_pixel(x + j, y + i, WHITE);
        else if (cursor[i][j] == 2) draw_pixel(x + j, y + i, BLACK);
    }
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

void kmain() {
    int state = 0; 
    int mouse_x = 160, mouse_y = 100;
    int window_open = 0; 
    int win_x = 60, win_y = 40; 
    int dragging = 0;
    int start_menu_open = 0; 

    // --- BOOT SCREEN ---
    draw_rect(0, 0, WIDTH, HEIGHT, CYAN);
    draw_text("NEXORA OS", 120, 90, WHITE);
    draw_rect(80, 170, 160, 10, WHITE);
    draw_rect(82, 172, 100, 6, BLUE);

    for(volatile int i = 0; i < 30000000; i++); 

    while(1) {
        unsigned char key = read_key();
        
        // --- LOGIN SCREEN ---
        if (state == 0) {
            state = 1;
            draw_rect(0, 0, WIDTH, HEIGHT, DARK_GREY);
            draw_rect(130, 40, 60, 60, LIGHT_GREY);
            draw_rect(140, 50, 40, 40, DARK_GREY);
            draw_text("NEXORA OS", 120, 110, CYAN);
            draw_text("USER: ADMIN", 100, 130, WHITE);
            draw_text("PRESS ENTER TO LOGIN", 70, 160, YELLOW);
        }
        
        // --- DESKTOP ---
        else if (state == 1) {
            if (key == 0x1C) { 
                state = 2; 
            }
        }
        
        // --- DESKTOP ACTIONS ---
        else if (state == 2) {
            if (key != 0) {
                // Mouse Move
                if (key == 0x4B && mouse_x > 0) mouse_x -= 10;       
                else if (key == 0x4D && mouse_x < WIDTH - 10) mouse_x += 10; 
                else if (key == 0x48 && mouse_y > 0) mouse_y -= 10;  
                else if (key == 0x50 && mouse_y < HEIGHT - 15) mouse_y += 10; 
                
                // Mouse Click
                else if (key == 0x39 || key == 0x1C) { 
                    // Start Menu
                    if (mouse_x >= 2 && mouse_x <= 42 && mouse_y >= HEIGHT - 20) {
                        start_menu_open = !start_menu_open;
                    }
                    // PC Icon
                    else if (!window_open && mouse_x >= 10 && mouse_x <= 40 && mouse_y >= 10 && mouse_y <= 50) {
                        window_open = 1; 
                    }
                    // Close Button
                    else if (window_open && mouse_x >= win_x + 185 && mouse_x <= win_x + 195 && mouse_y >= win_y + 3 && mouse_y <= win_y + 13) {
                        window_open = 0; 
                    }
                    // Drag Window
                    else if (window_open && mouse_x >= win_x && mouse_x <= win_x + 200 && mouse_y >= win_y && mouse_y <= win_y + 15) {
                        dragging = 1; 
                    }
                    else {
                        dragging = 0;
                        start_menu_open = 0; 
                    }
                }
                
                // Dragging logic
                if (dragging && window_open) {
                    win_x = mouse_x - 100;
                    win_y = mouse_y - 7; 
                    if (win_x < 0) win_x = 0;
                    if (win_x > WIDTH - 200) win_x = WIDTH - 200;
                    if (win_y < 0) win_y = 0;
                    if (win_y > HEIGHT - 100) win_y = HEIGHT - 100;
                }
            }

            // --- SCREEN REDRAW ---
            draw_rect(0, 0, WIDTH, HEIGHT, BLUE); 
            
            // Icons
            draw_rect(10, 10, 24, 24, LIGHT_GREY);
            draw_text("PC", 12, 36, WHITE);
            draw_rect(10, 60, 24, 24, YELLOW);
            draw_text("DOC", 12, 86, WHITE);

            // Taskbar
            draw_rect(0, HEIGHT - 20, WIDTH, 20, LIGHT_GREY);
            draw_rect(2, HEIGHT - 18, 40, 16, DARK_GREY);
            draw_text("START", 4, HEIGHT - 15, WHITE);

            // Start Menu
            if (start_menu_open) {
                draw_rect(2, HEIGHT - 80, 80, 60, LIGHT_GREY);
                draw_rect(2, HEIGHT - 80, 80, 10, DARK_GREY);
                draw_text("NEXORA", 10, HEIGHT - 78, WHITE);
                draw_text("ABOUT", 10, HEIGHT - 60, BLACK);
                draw_text("EXIT", 10, HEIGHT - 45, BLACK);
            }

            // Window
            if (window_open) {
                draw_rect(win_x, win_y, 200, 100, WHITE);
                draw_rect(win_x, win_y, 200, 15, DARK_GREY);
                draw_text("WINDOW", win_x + 5, win_y + 3, WHITE);
                draw_line(win_x + 185, win_y + 3, win_x + 195, win_y + 13, RED);
                draw_line(win_x + 195, win_y + 3, win_x + 185, win_y + 13, RED);
                draw_text("HELLO WORLD", win_x + 20, win_y + 40, BLACK);
            }

            // Mouse
            draw_mouse(mouse_x, mouse_y);
        }
    }
}
