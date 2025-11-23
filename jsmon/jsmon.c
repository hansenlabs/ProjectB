// jsmon_ncurses_full.c
// Complete ncurses-based joystick monitor with hotplug (inotify),
// one reader thread per device, gesture detection and a clean UI.
// Save as jsmon_ncurses_full.c

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <linux/joystick.h>
#include <sys/stat.h>
#include <sys/inotify.h>
#include <sys/select.h>
#include <time.h>
#include <ncurses.h>

#define DEV_DIR "/dev/input"
#define JS_PREFIX "js"
#define MAX_DEVS 64
#define AXIS_THRESHOLD 16000
#define RENDER_INTERVAL_MS 50
#define GESTURE_DUR 2.0

// ncurses color pairs
#define CP_DIM 1
#define CP_ACTIVE 2
#define CP_HEADER 3
#define CP_TITLE 4

/*
#define SYM_UP    "\u25B2"
#define SYM_DOWN  "\u25BC"
#define SYM_LEFT  "\u25C0"
#define SYM_RIGHT "\u25B6"
*/


#define SYM_UP    "^"
#define SYM_DOWN  "v"
#define SYM_LEFT  "<"
#define SYM_RIGHT ">"

static int global_exit_code = 0;
static int toggle_help = 0;

struct jsdev {
    char path[256];
    char name[128];
    int fd;
    pthread_t thread;
    int running; // 1 if thread should run, 0 otherwise

    int16_t axis[8];
    uint8_t btn[16];

    struct timespec gesture_start;
    int g_up1, g_down1, g_left1, g_right1;
    int g_up2, g_down2, g_left2, g_right2;
    int gesture;
};

static struct jsdev *devs[MAX_DEVS];
static int dev_count = 0;
//static pthread_mutex_t dev_lock = PTHREAD_MUTEX_INITIALIZER;

// forward
static void draw_ui();

static void msleep(int ms) {
    struct timespec ts = { ms/1000, (ms%1000)*1000000 };
    nanosleep(&ts, NULL);
}

static int file_exists(const char *p) {
    struct stat st;
    return stat(p, &st) == 0;
}

static void get_js_name(int fd, char *dst, size_t len) {
    if (ioctl(fd, JSIOCGNAME(len), dst) < 0) {
        strncpy(dst, "Joystick", len-1);
        dst[len-1] = '\0';
    }
}

static void *reader_thread(void *arg) {
    struct jsdev *d = arg;
    struct js_event e;
    d->running = 1;

    while (d->running) {
        ssize_t r = read(d->fd, &e, sizeof(e));
        if (r != sizeof(e)) {
            break; // device removed or error
        }
        if (e.type & JS_EVENT_INIT) continue;

        //pthread_mutex_lock(&dev_lock);
        if (e.type == JS_EVENT_AXIS && e.number >= 0 && e.number < 8)
            d->axis[e.number] = e.value;
        if (e.type == JS_EVENT_BUTTON && e.number >= 0 && e.number < 16)
            d->btn[e.number] = e.value ? 1 : 0;

        // gesture detection: when axis + button combination occurs record time
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);

        #define GTEST(cond, flag) \
            do { if (cond) { if (!flag) clock_gettime(CLOCK_MONOTONIC, &d->gesture_start); flag = 1; } else flag = 0; } while(0)

        GTEST((d->axis[1] < -AXIS_THRESHOLD && d->btn[0]), d->g_up1);
        GTEST((d->axis[1] > AXIS_THRESHOLD && d->btn[0]), d->g_down1);
        GTEST((d->axis[0] < -AXIS_THRESHOLD && d->btn[0]), d->g_left1);
        GTEST((d->axis[0] > AXIS_THRESHOLD && d->btn[0]), d->g_right1);

        GTEST((d->axis[1] < -AXIS_THRESHOLD && d->btn[1]), d->g_up2);
        GTEST((d->axis[1] > AXIS_THRESHOLD && d->btn[1]), d->g_down2);
        GTEST((d->axis[0] < -AXIS_THRESHOLD && d->btn[1]), d->g_left2);
        GTEST((d->axis[0] > AXIS_THRESHOLD && d->btn[1]), d->g_right2);

        d->gesture = d->g_up1||d->g_down1||d->g_left1||d->g_right1||
                     d->g_up2||d->g_down2||d->g_left2||d->g_right2;

        //pthread_mutex_unlock(&dev_lock);
    }

    // mark stopped
    //pthread_mutex_lock(&dev_lock);
    d->running = 0;
    //pthread_mutex_unlock(&dev_lock);
    return NULL;
}

static void add_device(const char *path) {
    //pthread_mutex_lock(&dev_lock);
    for (int i = 0; i < dev_count; ++i) {
        if (strcmp(devs[i]->path, path) == 0) {
            //pthread_mutex_unlock(&dev_lock);
            return; // already present
        }
    }
    //pthread_mutex_unlock(&dev_lock);

    int fd = open(path, O_RDONLY);
    if (fd < 0) return;

    struct jsdev *d = calloc(1, sizeof(*d));
    if (!d) { close(fd); return; }
    strncpy(d->path, path, sizeof(d->path)-1);
    d->fd = fd;
    d->running=1; //Otherwise Deadlock...
    get_js_name(fd, d->name, sizeof(d->name));

    if (pthread_create(&d->thread, NULL, reader_thread, d) != 0) {
        close(fd); free(d); return;
    }

    //pthread_mutex_lock(&dev_lock);
    if (dev_count < MAX_DEVS) devs[dev_count++] = d;
    else { // too many
        // cleanup
        d->running = 0;
        close(d->fd);
        pthread_join(d->thread, NULL);
        free(d);
    }
    //pthread_mutex_unlock(&dev_lock);
}

static void remove_device(const char *path) {
    //pthread_mutex_lock(&dev_lock);
    for (int i = 0; i < dev_count; ++i) {
        if (strcmp(devs[i]->path, path) == 0) {
            struct jsdev *d = devs[i];
            // signal thread to stop
            d->running = 0;
            // close fd to interrupt read
            close(d->fd);
            //pthread_mutex_unlock(&dev_lock);
            // wait for thread
            pthread_join(d->thread, NULL);
            //pthread_mutex_lock(&dev_lock);
            free(d);
            devs[i] = devs[dev_count-1];
            dev_count--;
            //pthread_mutex_unlock(&dev_lock);
            return;
        }
    }
    //pthread_mutex_unlock(&dev_lock);
}

static void initial_scan() {
    DIR *dr = opendir(DEV_DIR);
    if (!dr) return;
    struct dirent *e;
    char full[512];
    while ((e = readdir(dr))) {
        if (strncmp(e->d_name, JS_PREFIX, strlen(JS_PREFIX)) == 0) {
            snprintf(full, sizeof(full), "%s/%s", DEV_DIR, e->d_name);
            add_device(full);
        }
    }
    closedir(dr);
}

static void cleanup_all() {
    //pthread_mutex_lock(&dev_lock);
    for (int i = 0; i < dev_count; ++i) {
        struct jsdev *d = devs[i];
        d->running = 0;
        close(d->fd);
    }
    //pthread_mutex_unlock(&dev_lock);
    // join outside lock
    for (int i = 0; i < dev_count; ++i) {
        pthread_join(devs[i]->thread, NULL);
        free(devs[i]);
    }
    dev_count = 0;
}

static void setup_ncurses() {
    initscr();
    noecho();
    cbreak();
    curs_set(0);
    start_color();
    use_default_colors();
    keypad(stdscr, TRUE); 

    init_color(8, 300, 300, 300); 
    init_pair(CP_DIM, 8, -1);
    init_pair(CP_ACTIVE, COLOR_WHITE, -1);
    init_pair(CP_HEADER, COLOR_CYAN, -1);
    init_pair(CP_TITLE, COLOR_YELLOW, -1);
}

static void draw_ui() {
    erase();
    attron(COLOR_PAIR(CP_TITLE) | A_BOLD);
    mvprintw(0, 2, "Connect Keyboards,Gamepads, check layout, hold Up+B1 to start! (F1 toggle Help)");
    attroff(COLOR_PAIR(CP_TITLE) | A_BOLD);
    int y = 3;
    if (toggle_help){
      attron(COLOR_PAIR(CP_HEADER));
      //mvprintw(2, 2, "Hotplug folder: %s", DEV_DIR);
      //mvprintw(3, 2, "Hold Up+B1 for gesture actions (configurable)");
      mvprintw(y++, 2, "Change Key2Joy Name by typing(hit and release): LCTRL,n,y,o,u,r,n,a,m,e,LCTRL");
      mvprintw(y++, 2, "Change Key2Joy Layout by typing(hit and release): LCTRL,up,down,left,right,B1,B2,LCTRL");
      mvprintw(y++, 2, "Default Key2Joy Layout: Arrow Keys, Number 1, Number 2 (classical numbers, not numpad)");
      mvprintw(y++, 2, "Toggle Key2Joy mute / unmute to prevent double events in game menu by pressing LCTRL");
      mvprintw(y++, 2, "Gestures Up+B1 - Atomic Bomberman; Down+B2 Safe Shutdown; Right+B2 Exit to Terminal");
      attroff(COLOR_PAIR(CP_HEADER));
      y++;
    }


    mvprintw(y++, 2, "Devices:");
    y++;
    //pthread_mutex_lock(&dev_lock);
    
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    for (int i = 0; i < dev_count; ++i) {
        struct jsdev *d = devs[i];
        double dt = (now.tv_sec - d->gesture_start.tv_sec) + (now.tv_nsec - d->gesture_start.tv_nsec)/1e9;

        mvprintw(y, 2, "%-20.20s", d->name);

        int up = d->axis[1] < -AXIS_THRESHOLD;
        int down = d->axis[1] > AXIS_THRESHOLD;
        int left = d->axis[0] < -AXIS_THRESHOLD;
        int right = d->axis[0] > AXIS_THRESHOLD;

        // arrows
        attron(COLOR_PAIR(up?CP_ACTIVE:CP_DIM)); mvprintw(y, 25, "%s",SYM_UP); attroff(COLOR_PAIR(up?CP_ACTIVE:CP_DIM));
        attron(COLOR_PAIR(down?CP_ACTIVE:CP_DIM)); mvprintw(y, 27, "%s",SYM_DOWN); attroff(COLOR_PAIR(down?CP_ACTIVE:CP_DIM));
        attron(COLOR_PAIR(left?CP_ACTIVE:CP_DIM)); mvprintw(y, 29, "%s",SYM_LEFT); attroff(COLOR_PAIR(left?CP_ACTIVE:CP_DIM));
        attron(COLOR_PAIR(right?CP_ACTIVE:CP_DIM)); mvprintw(y, 31, "%s",SYM_RIGHT); attroff(COLOR_PAIR(right?CP_ACTIVE:CP_DIM));

        // buttons
        attron(COLOR_PAIR(d->btn[0]?CP_ACTIVE:CP_DIM)); mvprintw(y, 35, "[B1]"); attroff(COLOR_PAIR(d->btn[0]?CP_ACTIVE:CP_DIM));
        attron(COLOR_PAIR(d->btn[1]?CP_ACTIVE:CP_DIM)); mvprintw(y, 39, "[B2]"); attroff(COLOR_PAIR(d->btn[1]?CP_ACTIVE:CP_DIM));

        // gesture indicator
        if (d->gesture&&(dt>0.5)) {
            mvprintw(y, 44, "Gesture Countdown %f",(GESTURE_DUR-dt));
        } else {
            mvprintw(y, 44, " ");
        }

        // gesture timeout actions
        if (d->gesture) {
            if (dt > GESTURE_DUR) {
                // perform exit codes like previous behavior
                //cleanup_all();
                //endwin();
                if (d->g_up1) global_exit_code = 10;
                if (d->g_down1) global_exit_code = 11;
                if (d->g_left1) global_exit_code = 12;
                if (d->g_right1) global_exit_code = 13;
                if (d->g_up2) global_exit_code = 14;
                if (d->g_down2) global_exit_code = 15;
                if (d->g_left2) global_exit_code = 16;
                if (d->g_right2) global_exit_code = 17;
            }
            //if (global_exit_code >0){
              //pthread_mutex_unlock(&lock);
            //  return; 
            //}
        }

        y++;
    }

    // fill rest
    for (; y < 18; ++y) mvprintw(y, 2, " ");

    //pthread_mutex_unlock(&dev_lock);

    // footer

    refresh();
}

int main(void) {
    // initialize ncurses
    setup_ncurses();

    // initial device scan
    initial_scan();

    // inotify init
    int inotify_fd = inotify_init1(0);
    if (inotify_fd < 0) {
        endwin();
        fprintf(stderr, "inotify_init1 failed: %s\n", strerror(errno));
        return 1;
    }

    int wd = inotify_add_watch(inotify_fd, DEV_DIR, IN_CREATE | IN_MOVED_TO | IN_ATTRIB | IN_DELETE | IN_MOVED_FROM);
    if (wd < 0) {
        endwin();
        fprintf(stderr, "inotify_add_watch failed: %s\n", strerror(errno));
        close(inotify_fd);
        return 1;
    }

    // main loop: wait for inotify or timeout to render
    fd_set readfds;
    struct timeval tv;

    nodelay(stdscr, TRUE); // make getch non-blocking

    int running = 1;
    while (running) {
        FD_ZERO(&readfds);
        FD_SET(inotify_fd, &readfds);
        int maxfd = inotify_fd;

        tv.tv_sec = 0;
        tv.tv_usec = RENDER_INTERVAL_MS * 1000;

        int sel = select(maxfd + 1, &readfds, NULL, NULL, &tv);
        if (sel > 0 && FD_ISSET(inotify_fd, &readfds)) {
            char buf[4096] __attribute__ ((aligned(__alignof__(struct inotify_event))));
            ssize_t len = read(inotify_fd, buf, sizeof(buf));
            if (len > 0) {
                ssize_t i = 0;
                while (i < len) {
                    struct inotify_event *ev = (struct inotify_event *)(buf + i);
                    if (ev->len > 0 && strncmp(ev->name, JS_PREFIX, strlen(JS_PREFIX)) == 0) {
                        char full[512];
                        snprintf(full, sizeof(full), "%s/%s", DEV_DIR, ev->name);
                        if (ev->mask & (IN_CREATE | IN_MOVED_TO | IN_ATTRIB)) {
                            if (file_exists(full)) add_device(full);
                        }
                        if (ev->mask & (IN_DELETE | IN_MOVED_FROM)) {
                            remove_device(full);
                        }
                    }
                    i += sizeof(struct inotify_event) + ev->len;
                }
            }
        }

        int ch = getch();
        if (ch == 'q' || ch == 'Q') {
            running = 0;
            break;
        }
        if (ch == KEY_F(1)) {
            toggle_help=!toggle_help;
            //break;
        }
        // cleanup devices that exited unexpectedly or vanished
        //pthread_mutex_lock(&dev_lock);
        
        for (int i = dev_count - 1; i >= 0; --i) {
            struct jsdev *d = devs[i];
            if (!d->running) {
                // thread stopped
                //pthread_mutex_unlock(&dev_lock);
                pthread_join(d->thread, NULL);
                //pthread_mutex_lock(&dev_lock);
                free(d);
                devs[i] = devs[dev_count-1];
                dev_count--;
                continue;
            }
            // if file vanished
            if (!file_exists(d->path)) {
                d->running = 0;
                close(d->fd);
                //pthread_mutex_unlock(&dev_lock);
                pthread_join(d->thread, NULL);
                //pthread_mutex_lock(&dev_lock);
                free(d);
                devs[i] = devs[dev_count-1];
                dev_count--;
                continue;
            }
        }
            
        //pthread_mutex_unlock(&dev_lock);

        draw_ui();
        if (global_exit_code != 0) {
          //sleep(1);
          //cleanup_all();
          endwin();
          exit (global_exit_code);
        }
    }

    // cleanup
    //cleanup_all();
    endwin();
    close(inotify_fd);
    return 0;
}
