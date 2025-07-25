#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#define MAX_LINES 1000
#define MAX_COLS 1000

// Contains all the state
typedef struct {
    char *lines[MAX_LINES];
    int num_lines;
    int cursor_x, cursor_y;
    int scroll_y;
    char *filename;
    int in_multiline_comment;
} Editor;

Editor ed = {0};

void init_editor() {
    initscr();
    raw();
    keypad(stdscr, TRUE);
    noecho();
    start_color();
    use_default_colors();
    
    // Initialize color pairs for C syntax highlighting
    init_pair(1, COLOR_BLUE, -1);    // keywords
    init_pair(2, COLOR_GREEN, -1);   // strings
    init_pair(3, COLOR_RED, -1);     // comments
    init_pair(4, COLOR_MAGENTA, -1); // numbers
    init_pair(5, COLOR_CYAN, -1);    // preprocessor
    
    ed.num_lines = 1;
    ed.lines[0] = malloc(MAX_COLS);
    ed.lines[0][0] = '\0';
}

void cleanup() {
    for (int i = 0; i < ed.num_lines; i++) {
        free(ed.lines[i]);
    }
    endwin();
}

void insert_char(int ch) {
    char *line = ed.lines[ed.cursor_y];
    int len = strlen(line);
    memmove(line + ed.cursor_x + 1, line + ed.cursor_x, len - ed.cursor_x + 1);
    line[ed.cursor_x] = ch;
    ed.cursor_x++;
}

void delete_char() {
    if (ed.cursor_x > 0) {
        char *line = ed.lines[ed.cursor_y];
        memmove(line + ed.cursor_x - 1, line + ed.cursor_x, strlen(line) - ed.cursor_x + 1);
        ed.cursor_x--;
    } else if (ed.cursor_y > 0) {
        // Backspace at beginning of line - merge with previous line
        char *prev_line = ed.lines[ed.cursor_y - 1];
        char *curr_line = ed.lines[ed.cursor_y];
        int prev_len = strlen(prev_line);
        
        // Check if merged line would fit
        if (prev_len + strlen(curr_line) < MAX_COLS - 1) {
            strcat(prev_line, curr_line);
            ed.cursor_x = prev_len;
            ed.cursor_y--;
            
            // Remove current line and shift remaining lines up
            free(curr_line);
            for (int i = ed.cursor_y + 1; i < ed.num_lines - 1; i++) {
                ed.lines[i] = ed.lines[i + 1];
            }
            ed.num_lines--;
        }
    }
}

void insert_line() {
    if (ed.num_lines < MAX_LINES - 1) {
        for (int i = ed.num_lines; i > ed.cursor_y + 1; i--) {
            ed.lines[i] = ed.lines[i - 1];
        }
        ed.lines[ed.cursor_y + 1] = malloc(MAX_COLS);
        char *curr_line = ed.lines[ed.cursor_y];
        strcpy(ed.lines[ed.cursor_y + 1], curr_line + ed.cursor_x);
        curr_line[ed.cursor_x] = '\0';
        ed.num_lines++;
        ed.cursor_y++;
        ed.cursor_x = 0;
    }
}

void save_file() {
    if (!ed.filename) ed.filename = "untitled.txt";
    FILE *f = fopen(ed.filename, "w");
    if (f) {
        for (int i = 0; i < ed.num_lines; i++) {
            fprintf(f, "%s\n", ed.lines[i]);
        }
        fclose(f);
    }
}

void load_file(char *filename) {
    ed.filename = filename;
    FILE *f = fopen(filename, "r");
    if (f) {
        ed.num_lines = 0;
        char buffer[MAX_COLS];
        while (fgets(buffer, sizeof(buffer), f) && ed.num_lines < MAX_LINES) {
            buffer[strcspn(buffer, "\n")] = '\0';
            ed.lines[ed.num_lines] = malloc(MAX_COLS);
            strcpy(ed.lines[ed.num_lines], buffer);
            ed.num_lines++;
        }
        fclose(f);
        if (ed.num_lines == 0) {
            ed.lines[0] = malloc(MAX_COLS);
            ed.lines[0][0] = '\0';
            ed.num_lines = 1;
        }
    } else {
        // File doesn't exist, create empty buffer with filename
        ed.lines[0] = malloc(MAX_COLS);
        ed.lines[0][0] = '\0';
        ed.num_lines = 1;
    }
}

int is_c_keyword(const char *word) {
    const char *keywords[] = {
        "auto", "break", "case", "char", "const", "continue", "default", "do",
        "double", "else", "enum", "extern", "float", "for", "goto", "if",
        "int", "long", "register", "return", "short", "signed", "sizeof", "static",
        "struct", "switch", "typedef", "union", "unsigned", "void", "volatile", "while"
    };
    for (int i = 0; i < 32; i++) {
        if (strcmp(word, keywords[i]) == 0) return 1;
    }
    return 0;
}

void draw_line_with_syntax(int line_num, int screen_row) {
    char *line = ed.lines[line_num];
    int len = strlen(line);
    int col = 0;
    int in_comment = 0;
    
    // Calculate comment state up to this line
    for (int prev_line = 0; prev_line < line_num; prev_line++) {
        char *prev = ed.lines[prev_line];
        int prev_len = strlen(prev);
        
        for (int j = 0; j < prev_len; j++) {
            if (in_comment) {
                if (prev[j] == '*' && j + 1 < prev_len && prev[j + 1] == '/') {
                    in_comment = 0;
                    j++;
                }
            } else {
                if (prev[j] == '/' && j + 1 < prev_len && prev[j + 1] == '/') {
                    break; // Rest of line is comment
                }
                if (prev[j] == '/' && j + 1 < prev_len && prev[j + 1] == '*') {
                    in_comment = 1;
                    j++;
                }
            }
        }
    }
    
    move(screen_row, 0);
    
    for (int i = 0; i < len; i++) {
        char ch = line[i];
        
        // Check if we're in a multiline comment
        if (in_comment) {
            attron(COLOR_PAIR(3));
            // Look for end of comment
            if (ch == '*' && i + 1 < len && line[i + 1] == '/') {
                addch(ch);
                addch(line[i + 1]);
                in_comment = 0;
                attroff(COLOR_PAIR(3));
                i++;
                continue;
            }
            addch(ch);
            continue;
        }
        
        // Handle single-line comments
        if (ch == '/' && i + 1 < len && line[i + 1] == '/') {
            attron(COLOR_PAIR(3));
            addstr(line + i);
            attroff(COLOR_PAIR(3));
            break;
        }
        
        // Handle start of multi-line comments
        if (ch == '/' && i + 1 < len && line[i + 1] == '*') {
            attron(COLOR_PAIR(3));
            addch(ch);
            addch(line[i + 1]);
            in_comment = 1;
            i++;
            continue;
        }
        
        // Handle strings
        if (ch == '"') {
            attron(COLOR_PAIR(2));
            addch(ch);
            i++;
            while (i < len && line[i] != '"') {
                if (line[i] == '\\' && i + 1 < len) {
                    addch(line[i]);
                    i++;
                }
                addch(line[i]);
                i++;
            }
            if (i < len) addch(line[i]);
            attroff(COLOR_PAIR(2));
            continue;
        }
        
        // Handle character literals
        if (ch == '\'') {
            attron(COLOR_PAIR(2));
            addch(ch);
            i++;
            while (i < len && line[i] != '\'') {
                if (line[i] == '\\' && i + 1 < len) {
                    addch(line[i]);
                    i++;
                }
                addch(line[i]);
                i++;
            }
            if (i < len) addch(line[i]);
            attroff(COLOR_PAIR(2));
            continue;
        }
        
        // Handle preprocessor directives
        if (ch == '#' && (i == 0 || isspace(line[i - 1]))) {
            attron(COLOR_PAIR(5));
            while (i < len && line[i] != ' ' && line[i] != '\t') {
                addch(line[i]);
                i++;
            }
            attroff(COLOR_PAIR(5));
            i--;
            continue;
        }
        
        // Handle numbers
        if (isdigit(ch) || (ch == '.' && i + 1 < len && isdigit(line[i + 1]))) {
            attron(COLOR_PAIR(4));
            while (i < len && (isdigit(line[i]) || line[i] == '.' || line[i] == 'f' || line[i] == 'l')) {
                addch(line[i]);
                i++;
            }
            attroff(COLOR_PAIR(4));
            i--;
            continue;
        }
        
        // Handle keywords and identifiers
        if (isalpha(ch) || ch == '_') {
            int start = i;
            while (i < len && (isalnum(line[i]) || line[i] == '_')) {
                i++;
            }
            
            char word[100];
            int word_len = i - start;
            if (word_len < 100) {
                strncpy(word, line + start, word_len);
                word[word_len] = '\0';
                
                if (is_c_keyword(word)) {
                    attron(COLOR_PAIR(1));
                    addstr(word);
                    attroff(COLOR_PAIR(1));
                } else {
                    addstr(word);
                }
            }
            i--;
            continue;
        }
        
        // Regular character
        addch(ch);
    }
}

void draw_screen() {
    clear();
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    
    for (int i = 0; i < max_y - 1 && i + ed.scroll_y < ed.num_lines; i++) {
        draw_line_with_syntax(i + ed.scroll_y, i);
    }
    
    mvprintw(max_y - 1, 0, "Ctrl-S: Save | Ctrl-Q: Quit | %s", 
             ed.filename ? ed.filename : "untitled.txt");
    
    if (ed.cursor_y - ed.scroll_y >= 0 && ed.cursor_y - ed.scroll_y < max_y - 1) {
        move(ed.cursor_y - ed.scroll_y, ed.cursor_x);
    }
    refresh();
}

int main(int argc, char *argv[]) {
    init_editor();
    
    // If an argument is passed, attempt to load it as a file
    if (argc > 1) { load_file(argv[1]); }
    
    draw_screen(); // Render the editor on startup
    
    int ch;
    while ((ch = getch()) != 17) { // Ctrl-Q
        int max_y, max_x;
        getmaxyx(stdscr, max_y, max_x);
        
        switch (ch) {
            case 19: // Ctrl-S
                save_file();
                break;
            case KEY_UP:
                if (ed.cursor_y > 0) {
                    ed.cursor_y--;
                    int len = strlen(ed.lines[ed.cursor_y]);
                    if (ed.cursor_x > len) ed.cursor_x = len;
                }
                break;
            case KEY_DOWN:
                if (ed.cursor_y < ed.num_lines - 1) {
                    ed.cursor_y++;
                    int len = strlen(ed.lines[ed.cursor_y]);
                    if (ed.cursor_x > len) ed.cursor_x = len;
                }
                break;
            case KEY_LEFT:
                if (ed.cursor_x > 0) ed.cursor_x--;
                break;
            case KEY_RIGHT:
                if (ed.cursor_x < strlen(ed.lines[ed.cursor_y])) ed.cursor_x++;
                break;
            case 10: // Enter
                insert_line();
                break;
            case 127: // Backspace
            case KEY_BACKSPACE:
                delete_char();
                break;
            default:
                if (ch >= 32 && ch <= 126) {
                    insert_char(ch);
                }
                break;
        }
        
        if (ed.cursor_y < ed.scroll_y) {
            ed.scroll_y = ed.cursor_y;
        } else if (ed.cursor_y >= ed.scroll_y + max_y - 1) {
            ed.scroll_y = ed.cursor_y - max_y + 2;
        }       
        draw_screen();
    }
    cleanup();
    return 0;
}
