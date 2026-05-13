#include "shell.h"
#include "common.h"
#include "terminal.h"
#include "filesystem.h"
#include "keyboard.h"

extern uint16_t          column;
extern uint8_t           line;
extern volatile uint64_t ticks;
extern bool              err;

#define LINE_MAX     256
#define ARGV_MAX     16
#define CWD_MAX      256
#define BLINK_TICKS  50      // 10ms/tick = 500ms
#define IDLE_TICKS   1000    // 10s of no keys = screen_saver starts

static char     cwd[CWD_MAX] = "/";
static char     line_buf[LINE_MAX];
static size_t   line_len = 0;

static bool     cursor_visible = false;
static uint64_t last_blink     = 0;

// libc why did you leave me
static size_t s_len(const char* s)              { size_t n = 0; while (s[n]) n++; return n; }
static void   s_cpy(char* d, const char* s)     { while ((*d++ = *s++)); }
static int    s_eq (const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a == (unsigned char)*b;
}

static inline void say(struct limine_framebuffer* fb, const char* s, uint32_t color) {
    draw_sentence(fb, (char*)s, color);
}

static void resolve_path(char* out, const char* arg) {
    if (arg[0] == '/') { s_cpy(out, arg); return; }

    s_cpy(out, cwd);
    size_t n = s_len(out);

    if (arg[0] == '.' && arg[1] == '\0') return;                // "."

    if (arg[0] == '.' && arg[1] == '.' && arg[2] == '\0') {     // ".."
        if (n <= 1) return;                                     // root stays root
        if (out[n-1] == '/') { n--; out[n] = '\0'; }            // strip trailing /
        while (n > 0 && out[n-1] != '/') n--;                   // back up to parent 
        if (n > 1) n--;                                         // keep root '/' 
        out[n] = '\0';
        return;
    }

    if (n == 0 || out[n-1] != '/') { out[n++] = '/'; }
    s_cpy(out + n, arg);
}

static int tokenize(char* line_in, char** argv, int max_argv) {
    int   argc = 0;
    char* p    = line_in;
    while (*p && argc < max_argv) {
        while (*p == ' ' || *p == '\t') *p++ = '\0';
        if (!*p) break;
        argv[argc++] = p;
        while (*p && *p != ' ' && *p != '\t') p++;
    }
    if (*p) *p = '\0';
    return argc;
}


static void cursor_draw (struct limine_framebuffer* fb) { draw_character(fb, column*8, line*16, '_', RGB32_WHITE); }
static void cursor_erase(struct limine_framebuffer* fb) { draw_character(fb, column*8, line*16, ' ', RGB32_BLACK); }

static void cursor_tick(struct limine_framebuffer* fb) {
    if (ticks >= last_blink && (ticks - last_blink) < BLINK_TICKS) return;
    last_blink = ticks;
    cursor_visible = !cursor_visible;
    if (cursor_visible) cursor_draw(fb);
    else                cursor_erase(fb);
}

static void cursor_reset(struct limine_framebuffer* fb) {
    cursor_erase(fb);
    cursor_visible = false;
    last_blink     = ticks;
}

static void print_prompt(struct limine_framebuffer* fb) {
    say(fb, cwd,  RGB32_GREEN);
    say(fb, "$ ", RGB32_GREEN);
}

typedef int (*cmd_fn)(int argc, char** argv, struct limine_framebuffer* fb);

static int cmd_help (int argc, char** argv, struct limine_framebuffer* fb);
static int cmd_clear(int argc, char** argv, struct limine_framebuffer* fb);
static int cmd_echo (int argc, char** argv, struct limine_framebuffer* fb);
static int cmd_pwd  (int argc, char** argv, struct limine_framebuffer* fb);
static int cmd_cd   (int argc, char** argv, struct limine_framebuffer* fb);
static int cmd_ls   (int argc, char** argv, struct limine_framebuffer* fb);
static int cmd_cat  (int argc, char** argv, struct limine_framebuffer* fb);
static int cmd_mkdir(int argc, char** argv, struct limine_framebuffer* fb);
static int cmd_rmdir(int argc, char** argv, struct limine_framebuffer* fb);
static int cmd_rm   (int argc, char** argv, struct limine_framebuffer* fb);
static int cmd_touch(int argc, char** argv, struct limine_framebuffer* fb);
static int cmd_write(int argc, char** argv, struct limine_framebuffer* fb);

static const struct { const char* name; cmd_fn fn; } cmds[] = {
    {"help",  cmd_help},
    {"clear", cmd_clear},
    {"echo",  cmd_echo},
    {"pwd",   cmd_pwd},
    {"cd",    cmd_cd},
    {"ls",    cmd_ls},
    {"cat",   cmd_cat},
    {"mkdir", cmd_mkdir},
    {"rmdir", cmd_rmdir},
    {"rm",    cmd_rm},
    {"touch", cmd_touch},
    {"write", cmd_write},
    {NULL, NULL}
};

static int cmd_help(int argc, char** argv, struct limine_framebuffer* fb) {
    (void)argc; (void)argv;
    say(fb, "Available commands:\n",                            RGB32_WHITE);
    say(fb, "  help                  - this message\n",         RGB32_WHITE);
    say(fb, "  clear                 - clear the screen\n",     RGB32_WHITE);
    say(fb, "  echo <text...>        - print text\n",           RGB32_WHITE);
    say(fb, "  pwd                   - print working dir\n",    RGB32_WHITE);
    say(fb, "  cd <path>             - change directory\n",     RGB32_WHITE);
    say(fb, "  ls [path]             - list directory\n",       RGB32_WHITE);
    say(fb, "  cat <file>            - print file contents\n",  RGB32_WHITE);
    say(fb, "  mkdir <path>          - create directory\n",     RGB32_WHITE);
    say(fb, "  rmdir <path>          - remove empty dir\n",     RGB32_WHITE);
    say(fb, "  rm <file>             - delete file\n",          RGB32_WHITE);
    say(fb, "  touch <file>          - create empty file\n",    RGB32_WHITE);
    say(fb, "  write <file> <text>   - write text to file\n",   RGB32_WHITE);
    return 0;
}

static int cmd_clear(int argc, char** argv, struct limine_framebuffer* fb) {
    (void)argc; (void)argv;
    reset(fb);
    return 0;
}

static int cmd_echo(int argc, char** argv, struct limine_framebuffer* fb) {
    for (int i = 1; i < argc; i++) {
        say(fb, argv[i], RGB32_WHITE);
        if (i < argc - 1) say(fb, " ", RGB32_WHITE);
    }
    say(fb, "\n", RGB32_WHITE);
    return 0;
}

static int cmd_pwd(int argc, char** argv, struct limine_framebuffer* fb) {
    (void)argc; (void)argv;
    say(fb, cwd,  RGB32_WHITE);
    say(fb, "\n", RGB32_WHITE);
    return 0;
}


static void cd_probe_cb(const char* n, uint8_t a, uint32_t s, void* u) {
    (void)n; (void)a; (void)s; (void)u;
}

static int cmd_cd(int argc, char** argv, struct limine_framebuffer* fb) {
    if (argc < 2) { cwd[0] = '/'; cwd[1] = '\0'; return 0; }

    char target[CWD_MAX];
    resolve_path(target, argv[1]);

    int r = fat32_listdir(target, cd_probe_cb, NULL);
    if (r < 0) {
        say(fb, "cd: no such directory\n", RGB32_RED);
        return r;
    }
    s_cpy(cwd, target);
    return 0;
}

static void ls_cb(const char* name, uint8_t attr, uint32_t size, void* user) {
    (void)size;
    struct limine_framebuffer* fb = (struct limine_framebuffer*)user;
    uint32_t color = (attr & ATTR_DIRECTORY) ? RGB32_BLUE : RGB32_WHITE;
    draw_sentence(fb, (char*)name, color);
    draw_sentence(fb, (char*)"\n", color);
}

static int cmd_ls(int argc, char** argv, struct limine_framebuffer* fb) {
    char target[CWD_MAX];
    if (argc < 2) s_cpy(target, cwd);
    else          resolve_path(target, argv[1]);

    int r = fat32_listdir(target, ls_cb, fb);
    if (r < 0) say(fb, "ls: error\n", RGB32_RED);
    return r;
}

static int cmd_cat(int argc, char** argv, struct limine_framebuffer* fb) {
    if (argc < 2) { say(fb, "cat: missing filename\n", RGB32_RED); return -1; }

    char target[CWD_MAX];
    resolve_path(target, argv[1]);

    int fd = fat32_open(target, O_RDONLY);
    if (fd < 0) { say(fb, "cat: cannot open\n", RGB32_RED); return fd; }

    // leave 128+1 for null terminator
    char buf[129];
    int  n;
    while ((n = fat32_read(fd, buf, sizeof(buf) - 1)) > 0) {
        buf[n] = '\0';
        draw_sentence(fb, buf, RGB32_WHITE);
    }
    fat32_close(fd);
    say(fb, "\n", RGB32_WHITE);
    return 0;
}

static int cmd_mkdir(int argc, char** argv, struct limine_framebuffer* fb) {
    if (argc < 2) { say(fb, "mkdir: missing arg\n", RGB32_RED); return -1; }
    char target[CWD_MAX]; resolve_path(target, argv[1]);
    int r = fat32_mkdir(target);
    if (r < 0) say(fb, "mkdir: error\n", RGB32_RED);
    return r;
}

static int cmd_rmdir(int argc, char** argv, struct limine_framebuffer* fb) {
    if (argc < 2) { say(fb, "rmdir: missing arg\n", RGB32_RED); return -1; }
    char target[CWD_MAX]; resolve_path(target, argv[1]);
    int r = fat32_rmdir(target);
    if (r < 0) say(fb, "rmdir: error\n", RGB32_RED);
    return r;
}

static int cmd_rm(int argc, char** argv, struct limine_framebuffer* fb) {
    if (argc < 2) { say(fb, "rm: missing arg\n", RGB32_RED); return -1; }
    char target[CWD_MAX]; resolve_path(target, argv[1]);
    int r = fat32_unlink(target);
    if (r < 0) say(fb, "rm: error\n", RGB32_RED);
    return r;
}

static int cmd_touch(int argc, char** argv, struct limine_framebuffer* fb) {
    if (argc < 2) { say(fb, "touch: missing arg\n", RGB32_RED); return -1; }
    char target[CWD_MAX]; resolve_path(target, argv[1]);
    int fd = fat32_open(target, O_WRONLY | O_CREATE);
    if (fd < 0) { say(fb, "touch: cannot create\n", RGB32_RED); return fd; }
    fat32_close(fd);
    return 0;
}

static int cmd_write(int argc, char** argv, struct limine_framebuffer* fb) {
    if (argc < 3) { say(fb, "write: usage: write <file> <text...>\n", RGB32_RED); return -1; }
    char target[CWD_MAX]; resolve_path(target, argv[1]);
    int fd = fat32_open(target, O_WRONLY | O_CREATE | O_TRUNC);
    if (fd < 0) { say(fb, "write: cannot open\n", RGB32_RED); return fd; }

    for (int i = 2; i < argc; i++) {
        fat32_write(fd, argv[i], s_len(argv[i]));
        if (i < argc - 1) fat32_write(fd, " ", 1);
    }
    fat32_close(fd);
    return 0;
}

static void execute_line(struct limine_framebuffer* fb, char* line_in) {
    char* argv[ARGV_MAX];
    int   argc = tokenize(line_in, argv, ARGV_MAX);
    if (argc == 0) return;

    for (int i = 0; cmds[i].name; i++) {
        if (s_eq(argv[0], cmds[i].name)) {
            cmds[i].fn(argc, argv, fb);
            return;
        }
    }
    say(fb, argv[0], RGB32_RED);
    say(fb, ": command not found\n", RGB32_RED);
}

static void handle_key(struct limine_framebuffer* fb, char c) {
    cursor_erase(fb);
    cursor_visible = false;
    last_blink     = ticks;

    if (c == '\n') {
        newLine(fb);
        line_buf[line_len] = '\0';
        if (line_len > 0) execute_line(fb, line_buf);
        line_len = 0;
        print_prompt(fb);
    } else if (c == '\b') {
        // don't need to do nothing if it's the first character
        if (line_len > 0 && column > 0) {
            line_len--;
            column--;
            draw_character(fb, column*8, line*16, ' ', RGB32_BLACK);
        }
    } else if (c >= 0x20 && c < 0x7F) {
        if (line_len < LINE_MAX - 1) {
            line_buf[line_len++] = c;
            uint16_t max_cols = fb->width / 8;
            if (column >= max_cols) newLine(fb);
            draw_character(fb, column*8, line*16, c, RGB32_WHITE);
            column++;
        }
    }
}

// actual main in the OS now
void shell_run(struct limine_framebuffer* fb) {
    keyboard_init();

    say(fb, "\nLucidiOS shell. Type 'help' for commands.\n", RGB32_YELLOW);
    line_len = 0;
    print_prompt(fb);
    cursor_reset(fb);

    for (;;) {
        // screen saver
        if (ticks > IDLE_TICKS && err) {
            copy_screen(fb);
            last_blink = ticks;
        }

        cursor_tick(fb);

        int c;
        while ((c = keyboard_pop()) >= 0) {
            handle_key(fb, (char)c);
        }

        // wait for next input
        asm volatile("hlt");
    }
}