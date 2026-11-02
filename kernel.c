#define VGA_ADDRESS 0xB8000

#define BLACK 0x00
#define WHITE 0x0F
#define YELLOW 0x0E
#define LIGHT_GREEN 0x0A
#define LIGHT_CYAN 0x0B
#define LIGHT_RED 0x0C

char* video_memory = (char*) VGA_ADDRESS;
int cursor_pos = 0;
char buffer[128];
int buf_idx = 0;

// Forward Declarations
void update_cursor();
void clear_screen();
void print_at(const char* str, int row, int col, int color);
void scroll_screen();

void print_logo() {
    print_at("  _   _  _______  __  __   ____   _____    _    ", 2, 10, LIGHT_CYAN);
    print_at(" | \\ | ||  _____|\\ \\/ /  / __ \\ |  _  |  / \\   ", 3, 10, LIGHT_CYAN);
    print_at(" |  \\| || |__    \\  /  | |  | || |_) | / _ \\  ", 4, 10, LIGHT_CYAN);
    print_at(" | . ` ||  __|   /  \\  | |  | ||  _ < / ___ \\ ", 5, 10, LIGHT_CYAN);
    print_at(" | |\\  || |____ / _/\\_\\ | |__| || | \\_/ |_| | ", 6, 10, LIGHT_CYAN);
    print_at(" |_| \\_||______/_/   \\_\\ \\____/ |_|  \\_____/  ", 7, 10, LIGHT_CYAN);

    print_at("The Next Generation Operating System", 9, 20, WHITE);
    print_at("C Language is running successfully!", 12, 20, LIGHT_GREEN);
}

void scroll_screen() {
    // Upar shift karo
    for (int i = 0; i < 24 * 80 * 2; i++) {
        video_memory[i] = video_memory[i + 160];
    }
    // Aakhri line saaf karo
    for (int i = 24 * 80 * 2; i < 25 * 80 * 2; i += 2) {
        video_memory[i] = ' ';
        video_memory[i+1] = (BLACK << 4) | WHITE;
    }
}

void clear_screen() {
    for (int i = 0; i < 80 * 25 * 2; i += 2) {
        video_memory[i] = ' ';
        video_memory[i+1] = (BLACK << 4) | WHITE;
    }
    cursor_pos = 0;
    update_cursor();
}

void print_at(const char* str, int row, int col, int color) {
    int offset = (row * 80 + col) * 2;
    int i = 0;
    while (str[i] != '\0') {
        video_memory[offset] = str[i];
        video_memory[offset + 1] = (BLACK << 4) | color;
        offset += 2;
        i++;
    }
}

void update_cursor() {
    // Agar cursor screen se bahar chala jaye, toh wapas andar lao
    if (cursor_pos < 0) cursor_pos = 0;
    if (cursor_pos >= 80 * 25 * 2) {
        scroll_screen();
        cursor_pos = (24 * 80) * 2; // Aakhri line par set karo
    }

    unsigned short pos = cursor_pos / 2;
    __asm__ volatile ("outb %0, %1" : : "a"((unsigned char)0x0F), "Nd"(0x3D4));
    __asm__ volatile ("outb %0, %1" : : "a"((unsigned char)(pos & 0xFF)), "Nd"(0x3D5));
    __asm__ volatile ("outb %0, %1" : : "a"((unsigned char)0x0E), "Nd"(0x3D4));
    __asm__ volatile ("outb %0, %1" : : "a"((unsigned char)((pos >> 8) & 0xFF)), "Nd"(0x3D5));
}

void put_char(char c, int color) {
    if (c == '\n') {
        // Next line ke shuru mein jao
        cursor_pos += (80 - (cursor_pos / 2) % 80) * 2;
    } else if (c == '\b') {
        if (cursor_pos > 0) {
            cursor_pos -= 2;
            video_memory[cursor_pos] = ' ';
            video_memory[cursor_pos + 1] = (BLACK << 4) | color;
        }
    } else {
        video_memory[cursor_pos] = c;
        video_memory[cursor_pos + 1] = (BLACK << 4) | color;
        cursor_pos += 2;
    }
    update_cursor(); // Har character ke baad cursor ko update karo
}

char get_key() {
    unsigned char status;
    do {
        __asm__ volatile ("inb %1, %0" : "=a"(status) : "Nd"(0x64));
    } while ((status & 1) == 0);

    unsigned char scancode;
    __asm__ volatile ("inb %1, %0" : "=a"(scancode) : "Nd"(0x60));

    if (scancode & 0x80) return 0; 

    static const char keymap[] = {
        0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
        '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
        0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
        0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
        '*', 0, ' '
    };

    if (scancode < sizeof(keymap)) return keymap[scancode];
    return 0;
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

int strncmp(const char* s1, const char* s2, int n) {
    while (n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

void process_command() {
    put_char('\n', WHITE);
    
    if (buf_idx == 0) return; 
    
    buffer[buf_idx] = '\0';
    
    if (strcmp(buffer, "help") == 0) {
        print_at("Commands: help, about, clear, echo", (cursor_pos / 160) + 1, 0, LIGHT_GREEN);
        cursor_pos += 160;
    } 
    else if (strcmp(buffer, "about") == 0) {
        print_at("Nexora OS v0.6 - Built by Tufeeq", (cursor_pos / 160) + 1, 0, LIGHT_CYAN);
        cursor_pos += 160;
    }
    else if (strcmp(buffer, "clear") == 0) {
        clear_screen();
        print_logo(); 
        cursor_pos = (15 * 80) * 2; 
    }
    else if (strncmp(buffer, "echo ", 5) == 0) {
        for(int i = 5; i < buf_idx; i++) {
            put_char(buffer[i], WHITE);
        }
    }
    else {
        print_at("Command not found!", (cursor_pos / 160) + 1, 0, LIGHT_RED);
        cursor_pos += 160;
    }

    // Naya Prompt dikhao
    print_at("Nexora> ", (cursor_pos / 160) + 1, 0, YELLOW);
    cursor_pos += 160 + (8 * 2); // Next line + prompt ka space
    buf_idx = 0; 
    update_cursor();
}

void kmain() {
    clear_screen();
    print_logo(); 

    print_at("Nexora> ", 15, 0, YELLOW);
    cursor_pos = (15 * 80 + 8) * 2;
    update_cursor();

    while(1) {
        char c = get_key();
        if (c != 0) {
            if (c == '\n') {
                process_command();
            } else if (c == '\b') {
                if (buf_idx > 0) {
                    buf_idx--;
                    put_char(c, WHITE);
                }
            } else {
                if (buf_idx < 127) { 
                    buffer[buf_idx] = c;
                    buf_idx++;
                    put_char(c, WHITE);
                }
            }
        }
    }
}
