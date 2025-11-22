// jsmonitor_fixed.c

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
#include <stdint.h>
#include <time.h>

#define DEV_DIR "/dev/input"
#define JS_PREFIX "js"

#define DEVICE_COL_WIDTH 20
#define AXIS_ACTIVE_THRESHOLD 16000
#define SCAN_INTERVAL_MS 500
#define RENDER_INTERVAL_MS 10
#define GESTURE_DUR 3

#define CLR_RESET "\x1b[0m"
#define CLR_GRAY  "\x1b[90m"
#define CLR_WHITE "\x1b[97m"

#define SYM_UP    "\u25B2"
#define SYM_DOWN  "\u25BC"
#define SYM_LEFT  "\u25C0"
#define SYM_RIGHT "\u25B6"

typedef struct {
    char devpath[256];
    char name[128];
    int fd;

    pthread_t thread;
    int should_stop;
    int alive;

    int16_t axes[8];
    uint8_t buttons[16];
    struct timespec gesture_start;
    int gesture;
    int gesture_up_1;
    int gesture_down_1;
    int gesture_left_1;
    int gesture_right_1;    
    int gesture_up_2;
    int gesture_down_2;
    int gesture_left_2;
    int gesture_right_2;  
} jsdev_t;

static jsdev_t **list = NULL;
static size_t list_count = 0;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static void msleep(int ms) {
    struct timespec ts = { ms/1000, (ms%1000)*1000000 };
    nanosleep(&ts, NULL);
}

static int file_exists(const char *p) {
    struct stat st;
    return stat(p, &st) == 0;
}

static void get_dev_name(int fd, char *dst, size_t dstlen) {
    char tmp[128] = {0};
    if (ioctl(fd, JSIOCGNAME(sizeof(tmp)), tmp) >= 0) {
        strncpy(dst, tmp, dstlen-1);
    } else {
        strncpy(dst, "Joystick", dstlen-1);
    }
}

static void* js_thread(void *arg) {
    jsdev_t *d = arg;
    struct js_event e;

    d->alive = 1;


    

    while (!d->should_stop) {
        int r = read(d->fd, &e, sizeof(e));
        if (r == sizeof(e)) {
            pthread_mutex_lock(&lock);

            struct timespec now;
            clock_gettime(CLOCK_MONOTONIC, &now);
            double dt = (now.tv_sec - d->gesture_start.tv_sec)
              + (double)(now.tv_nsec - d->gesture_start.tv_nsec) / 1e9;


            if (e.type & JS_EVENT_AXIS) {
                if (e.number < 8) d->axes[e.number] = e.value;
            }
            if (e.type & JS_EVENT_BUTTON) {
                if (e.number < 16) d->buttons[e.number] = e.value;
            }
            if ((d->axes[1]<0)&&(d->buttons[0])) {
                if (!d->gesture_up_1)clock_gettime(CLOCK_MONOTONIC, &(d->gesture_start));
                d->gesture_up_1=1;
            }
            else d->gesture_up_1=0;

            if ((d->axes[1]>0)&&(d->buttons[0])) {
                if (!d->gesture_down_1)clock_gettime(CLOCK_MONOTONIC, &(d->gesture_start)); 
                d->gesture_down_1=1;
            }
            else d->gesture_down_1=0;

            if ((d->axes[1]<0)&&(d->buttons[1])) {
                if (!d->gesture_up_2)clock_gettime(CLOCK_MONOTONIC, &(d->gesture_start));
                d->gesture_up_2=1;
            }
            else d->gesture_up_2=0;

            if ((d->axes[1]>0)&&(d->buttons[1])) {
                if (!d->gesture_down_2)clock_gettime(CLOCK_MONOTONIC, &(d->gesture_start)); 
                d->gesture_down_2=1;
            }
            else d->gesture_down_2=0;

            if ((d->axes[0]<0)&&(d->buttons[0])) {
                if (!d->gesture_left_1)clock_gettime(CLOCK_MONOTONIC, &(d->gesture_start));
                d->gesture_left_1=1;
            }
            else d->gesture_left_1=0;

            if ((d->axes[0]>0)&&(d->buttons[0])) {
                if (!d->gesture_right_1)clock_gettime(CLOCK_MONOTONIC, &(d->gesture_start)); 
                d->gesture_right_1=1;
            }
            else d->gesture_right_1=0;

            if ((d->axes[0]<0)&&(d->buttons[1])) {
                if (!d->gesture_left_2)clock_gettime(CLOCK_MONOTONIC, &(d->gesture_start));
                d->gesture_left_2=1;
            }
            else d->gesture_left_2=0;

            if ((d->axes[0]>0)&&(d->buttons[1])) {
                if (!d->gesture_right_2)clock_gettime(CLOCK_MONOTONIC, &(d->gesture_start)); 
                d->gesture_right_2=1;
            }
            else d->gesture_right_2=0;

            d->gesture=(d->gesture_up_1 || d->gesture_down_1 || d->gesture_left_1 || d->gesture_right_1 || d->gesture_up_2 || d->gesture_down_2 || d->gesture_left_2 || d->gesture_right_2);

            pthread_mutex_unlock(&lock);
        } else {
            // device removed
            break;
        }
    }

    d->alive = 0;
    return NULL;
}

static void add_device(const char *path) {
    int fd = open(path, O_RDONLY);  // ← BLOCKIEREND wichtig!!
    if (fd < 0) return;

    jsdev_t *d = calloc(1, sizeof(jsdev_t));
    strcpy(d->devpath, path);
    d->fd = fd;

    get_dev_name(fd, d->name, sizeof(d->name));

    pthread_create(&d->thread, NULL, js_thread, d);

    list = realloc(list, (list_count+1)*sizeof(list[0]));
    list[list_count++] = d;
}

static void remove_device(size_t i) {
    jsdev_t *d = list[i];
    d->should_stop = 1;
    close(d->fd);
    pthread_join(d->thread, NULL);

    free(d);

    for (size_t j=i; j+1<list_count; ++j)
        list[j] = list[j+1];

    list_count--;
    list = realloc(list, list_count*sizeof(list[0]));
}

static void scan_devices() {
    DIR *dir = opendir(DEV_DIR);
    if (!dir) return;

    struct dirent *e;
    while ((e = readdir(dir)) != NULL) {
        if (strncmp(e->d_name, JS_PREFIX, 2) != 0) continue;

        char fullpath[300];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", DEV_DIR, e->d_name);

        int known = 0;
        for (size_t i=0; i<list_count; i++)
            if (strcmp(list[i]->devpath, fullpath) == 0) known = 1;

        if (!known)
            add_device(fullpath);
    }

    closedir(dir);
}

static void render() {
    printf("\x1b[2J\x1b[H");

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);


    pthread_mutex_lock(&lock);

    for (size_t i=0; i<list_count; i++) {
        jsdev_t *d = list[i];

        double dt = (now.tv_sec - d->gesture_start.tv_sec)
              + (double)(now.tv_nsec - d->gesture_start.tv_nsec) / 1e9;

        if ((d->gesture_up_1)&&(dt>GESTURE_DUR))exit(10);
        if ((d->gesture_down_1)&&(dt>GESTURE_DUR))exit(11);
        if ((d->gesture_left_1)&&(dt>GESTURE_DUR))exit(12);
        if ((d->gesture_right_1)&&(dt>GESTURE_DUR))exit(13);
        if ((d->gesture_up_2)&&(dt>GESTURE_DUR))exit(14);
        if ((d->gesture_down_2)&&(dt>GESTURE_DUR))exit(15);
        if ((d->gesture_left_2)&&(dt>GESTURE_DUR))exit(16);
        if ((d->gesture_right_2)&&(dt>GESTURE_DUR))exit(17);

        const char *name = d->name;
        char shortname[DEVICE_COL_WIDTH+1];
        if ((int)strlen(name) > DEVICE_COL_WIDTH) {
            strncpy(shortname, name, DEVICE_COL_WIDTH-3);
            strcpy(shortname + DEVICE_COL_WIDTH - 3, "...");
        } else {
            strcpy(shortname, name);
        }

        printf("%-*s ", DEVICE_COL_WIDTH, shortname);

        int up    = (d->axes[1] < -AXIS_ACTIVE_THRESHOLD);
        int down  = (d->axes[1] >  AXIS_ACTIVE_THRESHOLD);
        int left  = (d->axes[0] < -AXIS_ACTIVE_THRESHOLD);
        int right = (d->axes[0] >  AXIS_ACTIVE_THRESHOLD);

        printf("%s%s%s ", up    ? CLR_WHITE : CLR_GRAY, SYM_UP,    CLR_RESET);
        printf("%s%s%s ", down  ? CLR_WHITE : CLR_GRAY, SYM_DOWN,  CLR_RESET);
        printf("%s%s%s ", left  ? CLR_WHITE : CLR_GRAY, SYM_LEFT,  CLR_RESET);
        printf("%s%s%s ", right ? CLR_WHITE : CLR_GRAY, SYM_RIGHT, CLR_RESET);

        printf("%s[1]%s ", d->buttons[0] ? CLR_WHITE : CLR_GRAY, CLR_RESET);
        printf("%s[2]%s ", d->buttons[1] ? CLR_WHITE : CLR_GRAY, CLR_RESET);

        if (d->gesture && (dt>1))printf("%s %f %s ", CLR_WHITE ,(GESTURE_DUR-dt),CLR_RESET);

        printf("\n");
    }

    pthread_mutex_unlock(&lock);
    fflush(stdout);
}

int main() {
    for (;;) {
        scan_devices();

        pthread_mutex_lock(&lock);
        for (ssize_t i = list_count - 1; i >= 0; i--) {
            if (!file_exists(list[i]->devpath))
                remove_device(i);
        }
        pthread_mutex_unlock(&lock);

        render();
        msleep(RENDER_INTERVAL_MS);
    }

    return 0;
}

