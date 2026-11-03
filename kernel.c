#define WIDTH 320
#define HEIGHT 200
#define FRAMEBUFFER 0xA0000

unsigned char* framebuffer = (unsigned char*) FRAMEBUFFER;

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

unsigned char font[128][8] = {
    ['N'] = {0b10000001, 0b11000001, 0b10100001, 0b10010001, 0b10001001, 0b10000101, 0b10000011, 0b10000001},
    ['E'] = {0b11111111, 0b10000000, 0b10000000, 0b11111110, 0b10000000, 0b10000000, 0b10000000, 0b11111111},
    ['X'] = {0b11000011, 0b01100110, 0b00111100, 0b00011000, 0b00011000, 0b00111100, 0b01100110, 0b11000011},
    ['O'] = {0b00111100, 0b01000010, 0b10000001, 0b10000001, 0b10000001, 0b10000001, 0b01000010, 0b00111100},
    ['R'] = {0b11111110, 0b10000001, 0b10000001, 0b11111110, 0b10010000, 0b10001000, 0b10000100, 0b10000010},
    ['A'] = {0b00011000, 0b00100100, 0b01000010, 0b10000001, 0b11111111, 0b10000001, 0b10000001, 0b10000001},
    ['S'] = {0b01111110, 0b10000001, 0b10000000, 0b01111110, 0b00000001, 0b00000001, 0b10000001, 0b01111110},
    ['W'] = {0b10000001, 0b10000001, 0b10000001, 0b10000001, 0b10011001, 0b10100101, 0b11000011, 0b10000001},
    ['C'] = {0b00111110, 0b01000001, 0b10000000, 0b10000000, 0b10000000, 0b10000000, 0b01000001, 0b00111110},
    ['L'] = {0b10000000, 0b10000000, 0b10000000, 0b10000000, 0b10000000, 0b10000000, 0b10000000, 0b11111111},
    ['M'] = {0b10000001, 0b11000011, 0b10100101, 0b10011001, 0b10000001, 0b10000001, 0b10000001, 0b10000001},
    ['T'] = {0b11111111, 0b00011000, 0b00011000, 0b00011000, 0b00011000, 0b00011000, 0b00011000, 0b00011000},
    ['P'] = {0b11111110, 0b10000001, 0b10000001, 0b11111110, 0b10000000, 0b10000000, 0b10000000, 0b10000000},
    ['U'] = {0b10000001, 0b10000001, 0b10000001, 0b10000001, 0b10000001, 0b10000001, 0b01000010, 0b00111100},
    ['D'] = {0b11111100, 0b10000010, 0b10000001, 0b10000001, 0b10000001, 0b10000001, 0b10000010, 0b11111100},
    ['I'] = {0b00111100, 0b00011000, 0b00011000, 0b00011000, 0b00011000, 0b00011000, 0b00011000, 0b00111100},
    ['G'] = {0b00111110, 0b01000001, 0b10000000, 0b10001111, 0b10000001, 0b10000001, 0b01000010, 0b00111100},
    ['Y'] = {0b10000001, 0b01000010, 0b00100100, 0b00011000, 0b00011000, 0b00011000, 0b00011000, 0b00011000},
    ['F'] = {0b11111111, 0b10000000, 0b10000000, 0b11111110, 0b10000000, 0b10000000, 0b10000000, 0b10000000},
    ['H'] = {0b10000001, 0b10000001, 0b10000001, 0b11111111, 0b10000001, 0b10000001, 0b10000001, 0b10000001},
    ['B'] = {0b11111100, 0b10000010, 0b10000010, 0b11111100, 0b10000010, 0b10000010, 0b10000010, 0b11111100},
    ['V'] = {0b10000001, 0b10000001, 0b10000001, 0b10000001, 0b01000010, 0b01000010, 0b00100100, 0b00011000},
    ['K'] = {0b10000001, 0b10000010, 0b10000100, 0b10001000, 0b11110000, 0b10001000, 0b10000100, 0b10000010},
    ['!'] = {0b00011000, 0b00011000, 0b00011000, 0b00011000, 0b00011000, 0b00000000, 0b00011000, 0b00011000},
    ['.'] = {0, 0, 0, 0, 0, 0, 0, 0b00011000},
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

// --- MOUSE DRAWING FUNCTION WAPAS ADD KIYA ---
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

// --- NAYA MOUSE CODE ---
int mouse_x = 160, mouse_y = 100;
unsigned char mouse_cycle = 0;
unsigned char mouse_byte[3];

void mouse_wait(unsigned char type) {
    unsigned int timeout = 100000;
    unsigned char status;
    
    if (type == 0) {
        while (timeout--) {
            __asm__ volatile("inb $0x64, %0" : "=a"(status));
            if ((status & 1) == 0) return;
        }
    } else {
        while (timeout--) {
            __asm__ volatile("inb $0x64, %0" : "=a"(status));
            if ((status & 2) == 0) return;
        }
    }
}
void mouse_write(unsigned char write) {
    mouse_wait(1);
    __asm__ volatile("outb %0, $0x64" : : "a"((unsigned char)0xD4));
    mouse_wait(1);
    __asm__ volatile("outb %0, $0x60" : : "a"(write));
}

unsigned char mouse_read() {
    mouse_wait(0);
    unsigned char data;
    __asm__ volatile("inb $0x60, %0" : "=a"(data));
    return data;
}

void init_mouse() {
    unsigned char status;
    mouse_wait(1);
    __asm__ volatile("outb %0, $0x64" : : "a"((unsigned char)0xA8)); // Enable Mouse
    
    mouse_write(0xF6); // Set Defaults
    mouse_read();      // Acknowledge
    
    mouse_write(0xF4); // Enable Data Reporting
    mouse_read();      // Acknowledge
}

// Mouse se data parhne ka function
void update_mouse() {
    unsigned char status;
    __asm__ volatile("inb $0x64, %0" : "=a"(status));
    
    if ((status & 0x20) && (status & 1)) { // Agar data mouse ka hai
        unsigned char data;
        __asm__ volatile("inb $0x60, %0" : "=a"(data));
        
        switch (mouse_cycle) {
            case 0:
                mouse_byte[0] = data;
                if (data & 0x08) mouse_cycle++; // Valid packet
                break;
            case 1:
                mouse_byte[1] = data;
                mouse_cycle++;
                break;
            case 2:
                mouse_byte[2] = data;
                
                // X aur Y movement calculate karo
                int dx = mouse_byte[1];
                int dy = mouse_byte[2];
                
                if (mouse_byte[0] & 0x10) dx |= 0xFFFFFF00; // Negative X
                if (mouse_byte[0] & 0x20) dy |= 0xFFFFFF00; // Negative Y
                
                mouse_x += dx / 2;
                mouse_y -= dy / 2;
                
                // Screen boundaries check
                if (mouse_x < 0) mouse_x = 0;
                if (mouse_x > WIDTH - 12) mouse_x = WIDTH - 12;
                if (mouse_y < 0) mouse_y = 0;
                if (mouse_y > HEIGHT - 16) mouse_y = HEIGHT - 16;
                
                mouse_cycle = 0;
                break;
        }
    }
}

int game_target = 5;
char game_msg[15] = "GUESS 1 TO 9";

void draw_desktop(int selected_icon, int app_open, int start_menu_open) {
    draw_rect(0, 0, WIDTH, HEIGHT, BLUE);

    for (int i = 0; i < 4; i++) {
        int ix = 10;
        int iy = 10 + (i * 50);
        
        if (selected_icon == i && !app_open) {
            draw_rect(ix - 2, iy - 2, 28, 28, WHITE);
        }
        
        if (i == 0) draw_rect(ix, iy, 24, 24, LIGHT_GREY);
        if (i == 1) draw_rect(ix, iy, 24, 24, YELLOW);    
        if (i == 2) draw_rect(ix, iy, 24, 24, RED);       
        if (i == 3) draw_rect(ix, iy, 24, 24, GREEN);     

        if (i == 0) draw_text("PC", ix + 2, iy + 26, WHITE);
        if (i == 1) draw_text("DOC", ix + 2, iy + 26, WHITE);
        if (i == 2) draw_text("SET", ix + 2, iy + 26, WHITE);
        if (i == 3) draw_text("GAME", ix, iy + 26, WHITE);
    }

    draw_rect(0, HEIGHT - 20, WIDTH, 20, LIGHT_GREY);
    draw_rect(2, HEIGHT - 18, 40, 16, DARK_GREY);
    draw_text("START", 4, HEIGHT - 15, WHITE);

    if (start_menu_open) {
        draw_rect(2, HEIGHT - 80, 80, 60, LIGHT_GREY);
        draw_rect(2, HEIGHT - 80, 80, 10, DARK_GREY);
        draw_text("NEXORA", 10, HEIGHT - 78, WHITE);
        draw_text("ABOUT", 10, HEIGHT - 60, BLACK);
        draw_text("EXIT", 10, HEIGHT - 45, BLACK);
    }

    if (app_open) {
        draw_rect(100, 40, 180, 110, WHITE);
        draw_rect(100, 40, 180, 15, DARK_GREY);
        
        if (selected_icon == 0) {
            draw_text("MY COMPUTER", 105, 43, WHITE);
            draw_text("MY COMPUTER", 120, 70, BLACK);
            draw_text("PRESS ESC TO CLOSE", 110, 120, DARK_GREY);
        }
        if (selected_icon == 1) {
            draw_text("NOTEPAD", 105, 43, WHITE);
            draw_text("WELCOME TO NEXORA", 110, 70, BLACK);
            draw_text("PRESS ESC TO CLOSE", 110, 120, DARK_GREY);
        }
        if (selected_icon == 2) {
            draw_text("SETTINGS", 105, 43, WHITE);
            draw_text("SYSTEM OK", 120, 70, BLACK);
            draw_text("PRESS ESC TO CLOSE", 110, 120, DARK_GREY);
        }
        if (selected_icon == 3) {
            draw_text("GUESS GAME", 105, 43, WHITE);
            draw_text(game_msg, 120, 70, BLACK);
            draw_text("PRESS 1 TO 9", 120, 90, DARK_GREY);
            draw_text("ESC TO QUIT", 120, 120, DARK_GREY);
        }
        
        draw_line(270, 43, 278, 51, RED);
        draw_line(278, 43, 270, 51, RED);
    }

    // MOUSE DRAW KARO (Sab se upar taake chhupe nahi)
     draw_mouse(mouse_x, mouse_y);
}

void kmain() {
    int state = 0; 
    int selected_icon = 0;
    int app_open = 0;
    int start_menu_open = 0;

    draw_rect(0, 0, WIDTH, HEIGHT, CYAN);
    draw_text("NEXORA OS", 120, 90, WHITE);
    draw_text("LOADING...", 120, 110, WHITE);

    for(volatile int i = 0; i < 30000000; i++); 
        init_mouse(); // Mouse ko start karo	
    while(1) {
        unsigned char key = read_key();
        
        if (state == 0) {
            state = 1;
            draw_rect(0, 0, WIDTH, HEIGHT, DARK_GREY);
            draw_text("WELCOME", 120, 80, CYAN);
            draw_text("PRESS ENTER TO LOGIN", 70, 120, WHITE);
        }
        else if (state == 1) {
            if (key == 0x1C) { 
                state = 2;
                draw_desktop(selected_icon, app_open, start_menu_open);
            }
        }
        else if (state == 2) {
            if (key != 0) {
                if (start_menu_open) {
                    if (key == 0x01) start_menu_open = 0; 
                    if (key == 0x1C) start_menu_open = 0; 
                } 
                else if (app_open) {
                    if (key == 0x01) { 
                        app_open = 0;
                        if (selected_icon == 3) {
                            game_msg[0]='G'; game_msg[1]='U'; game_msg[2]='E'; 
                            game_msg[3]='S'; game_msg[4]='S'; game_msg[5]=' '; 
                            game_msg[6]='1'; game_msg[7]=' '; game_msg[8]='T'; 
                            game_msg[9]='O'; game_msg[10]=' '; game_msg[11]='9'; game_msg[12]='\0';
                        }
                    }
                    if (selected_icon == 3) {
                        if (key >= 0x02 && key <= 0x0A) { 
                            int guess = key - 0x01;
                            if (guess == game_target) {
                                game_msg[0]='Y'; game_msg[1]='O'; game_msg[2]='U'; 
                                game_msg[3]=' '; game_msg[4]='W'; game_msg[5]='I'; 
                                game_msg[6]='N'; game_msg[7]='!'; game_msg[8]='\0';
                            } else {
                                game_msg[0]='T'; game_msg[1]='R'; game_msg[2]='Y'; 
                                game_msg[3]=' '; game_msg[4]='A'; game_msg[5]='G'; 
                                game_msg[6]='A'; game_msg[7]='I'; game_msg[8]='N'; game_msg[9]='\0';
                            }
                        }
                    }
                }
                else {
                    if (key == 0x50 && selected_icon < 3) selected_icon++; 
                    if (key == 0x48 && selected_icon > 0) selected_icon--; 
                    if (key == 0x1C) app_open = 1; 
                    if (key == 0x39) start_menu_open = 1; 
                }
                draw_desktop(selected_icon, app_open, start_menu_open);
            }
        }
    }
}
