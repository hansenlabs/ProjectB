// gcc -pthread -O3 -o key2joy key2joy.c

// Changelog: Dont brick Apple keyboard:)

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#define DEV_INPUT_EVENT "/dev/input"
#define EVENT_DEV_NAME "event"

#define BITS_PER_LONG (sizeof(long) * 8)
#define NBITS(x) ((((x) - 1) / BITS_PER_LONG) + 1)
#define OFF(x) ((x) % BITS_PER_LONG)
#define BIT(x) (1UL << OFF(x))
#define LONG(x) ((x) / BITS_PER_LONG)
#define test_bit(bit, array) ((array[LONG(bit)] >> OFF(bit)) & 1)

static int product_id_increment = 0;

char getkeychar(int keycode) {
  // Default: alle ' ' (Leerzeichen)
  static const char keymap[256] = {
      [2] = '1',     [3] = '2',     [4] = '3',     [5] = '4',
      [6] = '5',     [7] = '6',     [8] = '7',     [9] = '8',
      [10] = '9',    [11] = '0',

      [30] = 'A',    [48] = 'B',    [46] = 'C',    [32] = 'D',
      [KEY_E] = 'E', [KEY_F] = 'F', [KEY_G] = 'G', [KEY_H] = 'H',
      [KEY_I] = 'I', [KEY_J] = 'J', [KEY_K] = 'K', [KEY_L] = 'L',
      [KEY_M] = 'M', [KEY_N] = 'N', [KEY_O] = 'O', [KEY_P] = 'P',
      [KEY_Q] = 'Q', [KEY_R] = 'R', [KEY_S] = 'S', [KEY_T] = 'T',
      [KEY_U] = 'U', [KEY_V] = 'V', [KEY_W] = 'W', [KEY_X] = 'X',
      [KEY_Y] = 'Z',  // :)
      [KEY_Z] = 'Y'   // :)
  };

  if (keycode < 0 || keycode >= 256) return ' ';
  return keymap[keycode] ? keymap[keycode] : ' ';
}

void handler(int sig) {
  printf("nexiting...(%d)n", sig);
  exit(0);
}

void perror_exit(char *error) {
  perror(error);
  handler(9);
}

void store_led(int fd, int *led_num, int *led_caps, int *led_scroll) {
  char led_b[LED_MAX];  // is too much
  *led_num = 0;
  *led_caps = 0;
  *led_scroll = 0;
  memset(led_b, 0, sizeof(led_b));
  ioctl(fd, EVIOCGLED(sizeof(led_b)), led_b);
  int yalv;
  for (yalv = 0; yalv < LED_MAX; yalv++) {
    if (test_bit(yalv, led_b)) {
      /* the bit is set in the LED state */
      // printf("  LED 0x%02x ", yalv);
      switch (yalv) {
        case LED_NUML:
          printf(" (Num Lock)\n");
          *led_num = 1;
          break;
        case LED_CAPSL:
          printf(" (Caps Lock)\n");
          *led_caps = 1;
          break;
        case LED_SCROLLL:
          printf(" (Scroll Lock)\n");
          *led_scroll = 1;
          break;
          /* other LEDs not shown here*/
          // default:
          // printf(" (Unknown LED: 0x%04hx)\n",
          //        yalv);
      }
    }
  }
}
void restore_led(int fd, int led_num, int led_caps, int led_scroll) {
  struct input_event event_out;
  if (led_num > -1) {
    event_out.type = EV_LED;
    event_out.value = led_num;
    event_out.code = LED_NUML;
    write(fd, &event_out, sizeof(struct input_event));
    event_out.type = EV_SYN;
    event_out.value = 0;
    event_out.code = 0;
    write(fd, &event_out, sizeof(struct input_event));
  }
  if (led_caps > -1) {
    event_out.type = EV_LED;
    event_out.value = led_caps;
    event_out.code = LED_CAPSL;
    write(fd, &event_out, sizeof(struct input_event));
    event_out.type = EV_SYN;
    event_out.value = 0;
    event_out.code = 0;
    write(fd, &event_out, sizeof(struct input_event));
  }
  if (led_scroll > -1) {
    event_out.type = EV_LED;
    event_out.value = led_scroll;
    event_out.code = LED_SCROLLL;
    write(fd, &event_out, sizeof(struct input_event));
    event_out.type = EV_SYN;
    event_out.value = 0;
    event_out.code = 0;
    write(fd, &event_out, sizeof(struct input_event));
  }
}

void lightall(int fd, int is_apple_wireless) {
  if (is_apple_wireless > 0)
    return restore_led(fd, 0, 1, -1);
  else
    return restore_led(fd, 1, 1, 1);
}

void *K2Jinstance(void *argument) {
  char *device = (char *)argument;
  printf("Launching thread for %s!\n", device);

  struct input_event ev[64];
  int fdk, rd, value, size = sizeof(struct input_event);
  char name[256] = "Unknown";

  int fdw;  // file descriptor to write to event device
  int led_num = 0, led_caps = 0,
      led_scroll = 0;  // back up values for LED states

  int code_up = 103;
  int code_down = 108;
  int code_left = 105;
  int code_right = 106;
  int code_but_a = 2;  // 97
  int code_but_b = 3;  // 54
  int code_mute = 29;

  int have_new_name = -1;
  int kb_ass_cnt = -1;
  int wasted = -1;  // user typed sth else after muting
  int mute_key_state = 0;

  // lite up capslock
  if ((fdw = open(device, O_RDWR)) == -1) {
    printf("%s does not allow writing to it.\n", device);
    pthread_exit(NULL);
  }

  // store_led (fdw, &led_num, &led_caps, &led_scroll);
  // lightall(fdw);
  // sleep(1);
  // restore_led (fdw, led_num, led_caps, led_scroll);

  if ((fdk = open(device, O_RDONLY)) == -1) {
    printf("%s is not a vaild device.\n", device);
    pthread_exit(NULL);
  }
  // Print Device Name
  ioctl(fdk, EVIOCGNAME(sizeof(name)), name);
  printf("Reading From : %s (%s)\n", device, name);
  int is_apple_wireless = 0;  // yes, the Apple wireless will brick if you send
                              // it the wrong LED code!!!
  if ((strstr(name, "Apple") > 0) && (strstr(name, "Wireless") > 0))
    is_apple_wireless = 1;
  if (is_apple_wireless) printf("This seems to be an Apple Wireless device\n");
  // now the uinput stuff
  int fd;
  struct uinput_user_dev uidev;
  struct input_event iev;
  struct input_event ievs;
  int dx, dy;
  int i;
  int xstate = 0, ystate = 0;
  int muted = 0;

  fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
  if (fd < 0) {
    fprintf(stderr, "error: open /dev/uinput");
    pthread_exit(NULL);
  }

  if (ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0) perror_exit("error: ioctl");
  if (ioctl(fd, UI_SET_KEYBIT, BTN_A) < 0) perror_exit("error: ioctl");
  if (ioctl(fd, UI_SET_KEYBIT, BTN_B) < 0) perror_exit("error: ioctl");

  if (ioctl(fd, UI_SET_EVBIT, EV_SYN) < 0) perror_exit("error: ioctl");

  if (ioctl(fd, UI_SET_EVBIT, EV_ABS) < 0) perror_exit("error: ioctl");
  if (ioctl(fd, UI_SET_ABSBIT, ABS_X) < 0) perror_exit("error: ioctl");
  if (ioctl(fd, UI_SET_ABSBIT, ABS_Y) < 0) perror_exit("error: ioctl");

  memset(&uidev, 0, sizeof(uidev));
  // snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "hansen");
  snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "%s", name);
  uidev.id.bustype = BUS_USB;
  uidev.id.vendor = 0x1;
  uidev.id.product =
      0x1 + product_id_increment++;  // Assign random Product Numbers as
                                     // otherwise patched Wine caches the name
  uidev.id.version = 1;
  uidev.absmax[0] = 32767;
  uidev.absmax[1] = 32767;
  uidev.absmin[0] = -32767;
  uidev.absmin[1] = -32767;

  if (write(fd, &uidev, sizeof(uidev)) < 0) {
    fprintf(stderr,
            "Device %s failed, write failed this thread will exit - dont "
            "wonder why you receive no events!!!!\n",
            device);
    pthread_exit(NULL);
  }
  if (ioctl(fd, UI_DEV_CREATE) < 0) {
    fprintf(stderr,
            "Device %s failed, ioctl failed this thread will exit - dont "
            "wonder why you receive no events!!!!\n",
            device);
    pthread_exit(NULL);
  }

  // sleep(1);

  srand(time(NULL));
  memset(&iev, 0, sizeof(struct input_event));
  memset(&ievs, 0, sizeof(struct input_event));
  ievs.type = EV_SYN;
  ievs.code = 0;
  ievs.value = 0;

  int sth_to_read;
  int offset = 0;  // will be counted in 2er steps
  // workaround for Apple devices
  if (is_apple_wireless) {
    restore_led(fdw, 0, 0, -1);
    code_but_a = 100;
  }
  while (1) {
    if ((rd = read(fdk, ev, size * 64)) < size) {
      // perror_exit ("read()");
      fprintf(stderr,
              "read() on device %s failed, this thread will exit - dont wonder "
              "why you receive no events!!!!\n",
              device);
      close(fd);
      pthread_exit(NULL);
    }
    offset = 0;
    // printf("read %d bytes\n",rd);
    while (rd > ((2 + offset) * sizeof(struct input_event))) {
      // printf("Looop offset %d, read %d \n",offset, rd);
      // check for mute
      if ((ev[1 + offset].code == code_mute) && (ev[1 + offset].value == 1)) {
        if ((ystate == 0) &&
            (xstate == 0)) {  // there is a BUG in a cherry keyboard sending
                              // RCTL if left + up + right are pressed
          mute_key_state = 1;
          if (muted) {
            if (have_new_name > 0) {
              fprintf(stdout, "Reregistering device with name %s!!!!\n", name);
              // reregister device
              if (ioctl(fd, UI_DEV_DESTROY) < 0) {
                fprintf(stderr, "Critical Error Cannot destroy device!!!!\n");
                pthread_exit(NULL);
              }
              close(fd);
              usleep(100000);

              snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "%s", name);
              fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
              if (fd < 0) {
                fprintf(stderr, "error: open /dev/uinput");
                pthread_exit(NULL);
              }

              if (ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0)
                perror_exit("error: ioctl");
              if (ioctl(fd, UI_SET_KEYBIT, BTN_A) < 0)
                perror_exit("error: ioctl");
              if (ioctl(fd, UI_SET_KEYBIT, BTN_B) < 0)
                perror_exit("error: ioctl");

              if (ioctl(fd, UI_SET_EVBIT, EV_SYN) < 0)
                perror_exit("error: ioctl");

              if (ioctl(fd, UI_SET_EVBIT, EV_ABS) < 0)
                perror_exit("error: ioctl");
              if (ioctl(fd, UI_SET_ABSBIT, ABS_X) < 0)
                perror_exit("error: ioctl");
              if (ioctl(fd, UI_SET_ABSBIT, ABS_Y) < 0)
                perror_exit("error: ioctl");

              if (write(fd, &uidev, sizeof(uidev)) < 0) {
                fprintf(stderr,
                        "Device %s failed, write failed this thread will exit "
                        "- dont wonder why you receive no events!!!!\n",
                        device);
                pthread_exit(NULL);
              }
              if (ioctl(fd, UI_DEV_CREATE) < 0) {
                fprintf(stderr,
                        "Device %s failed, ioctl failed this thread will exit "
                        "- dont wonder why you receive no events!!!!\n",
                        device);
                pthread_exit(NULL);
              }
              have_new_name = -1;
            }
            kb_ass_cnt = -1;
            wasted = -1;
            muted = 0;
            if (is_apple_wireless) led_scroll = led_num = -1;
            restore_led(fdw, led_num, led_caps, led_scroll);
          } else {
            muted = 1;
            wasted = -1;
            store_led(fdw, &led_num, &led_caps, &led_scroll);
            lightall(fdw, is_apple_wireless);
          }
        }
        offset = offset + 2;
        continue;
      } else if ((ev[1 + offset].code == code_mute) &&
                 (ev[1 + offset].value == 0)) {
        mute_key_state = 0;
        if (((ystate == 0) && (xstate == 0)) &&
            (rd > ((4 + offset) * sizeof(struct input_event)))) {
          muted = 0;  // there is a BUG in a cherry keyboard sending RCTL if
                      // left + up + right are pressed
          if (is_apple_wireless) led_scroll = led_num = -1;
          restore_led(fdw, led_num, led_caps, led_scroll);
          wasted = -1;
        }
        offset = offset + 2;
        continue;
      }
      if (muted && (ev[1 + offset].value == 1)) {
        if (mute_key_state == 1) {  // dont react to ctrl + c, just unmute
          muted = 0;
          if (is_apple_wireless) led_scroll = led_num = -1;
          restore_led(fdw, led_num, led_caps, led_scroll);
          wasted = -1;
        }
        if (wasted > -1) {
          ;
        } else if (have_new_name > -1) {
          if (have_new_name + 1 < sizeof(name))
            name[have_new_name] = getkeychar(ev[1 + offset].code);
          name[++have_new_name] = 0;
          // fprintf(stderr,"Current name is %s !!!!\n",name);
          // TODO
        } else if (kb_ass_cnt > -1) {
          int keyval = ev[1 + offset].code;
          if (kb_ass_cnt == 0)
            code_up = keyval;
          else if (kb_ass_cnt == 1)
            code_down = keyval;
          else if (kb_ass_cnt == 2)
            code_left = keyval;
          else if (kb_ass_cnt == 3)
            code_right = keyval;
          else if (kb_ass_cnt == 4)
            code_but_a = keyval;
          else if (kb_ass_cnt == 5)
            code_but_b = keyval;
          else
            kb_ass_cnt--;  // :)
          kb_ass_cnt++;
          // TODO
        } else {
          if (ev[1 + offset].code == KEY_N)
            have_new_name = 0;
          else if (ev[1 + offset].code == KEY_K)
            kb_ass_cnt = 0;
          else {
            printf("Setting wasted because received code %d\n",
                   ev[1 + offset].code);
            wasted = 1;
          }
        }
        offset = offset + 2;
        continue;
      } else if (muted) {
        offset = offset + 2;
        continue;
      }
      if ((ev[1 + offset].code == code_up) && (ev[1 + offset].value == 1)) {
        iev.type = EV_ABS;
        iev.code = ABS_Y;
        ystate = iev.value = -32767;
      } else if ((ev[1 + offset].code == code_up) &&
                 (ev[1 + offset].value == 0)) {
        if (ystate == 32767) {
          offset = offset + 2;
          continue;  // someone pressed down, so do not set to 0
        }
        iev.type = EV_ABS;
        iev.code = ABS_Y;
        ystate = iev.value = 0;
      } else if ((ev[1 + offset].code == code_down) &&
                 (ev[1 + offset].value == 1)) {
        iev.type = EV_ABS;
        iev.code = ABS_Y;
        ystate = iev.value = 32767;
      } else if ((ev[1 + offset].code == code_down) &&
                 (ev[1 + offset].value == 0)) {
        if (ystate == -32767) {
          offset = offset + 2;
          continue;  // someone pressed down, so do not set to 0
        }
        iev.type = EV_ABS;
        iev.code = ABS_Y;
        ystate = iev.value = 0;
      } else if ((ev[1 + offset].code == code_left) &&
                 (ev[1 + offset].value == 1)) {
        iev.type = EV_ABS;
        iev.code = ABS_X;
        xstate = iev.value = -32767;
      } else if ((ev[1 + offset].code == code_left) &&
                 (ev[1 + offset].value == 0)) {
        if (xstate == 32767) {
          offset = offset + 2;
          continue;  // someone pressed down, so do not set to 0
        }
        iev.type = EV_ABS;
        iev.code = ABS_X;
        xstate = iev.value = 0;
      } else if ((ev[1 + offset].code == code_right) &&
                 (ev[1 + offset].value == 1)) {
        iev.type = EV_ABS;
        iev.code = ABS_X;
        xstate = iev.value = 32767;
      } else if ((ev[1 + offset].code == code_right) &&
                 (ev[1 + offset].value == 0)) {
        if (xstate == -32767) {
          offset = offset + 2;
          continue;  // someone pressed down, so do not set to 0
        }
        iev.type = EV_ABS;
        iev.code = ABS_X;
        xstate = iev.value = 0;
      } else if ((ev[1 + offset].code == code_but_a) &&
                 (ev[1 + offset].value == 1)) {
        iev.type = EV_KEY;
        iev.code = BTN_A;
        iev.value = 1;
      } else if ((ev[1 + offset].code == code_but_a) &&
                 (ev[1 + offset].value == 0)) {
        iev.type = EV_KEY;
        iev.code = BTN_A;
        iev.value = 0;
      } else if ((ev[1 + offset].code == code_but_b) &&
                 (ev[1 + offset].value == 1)) {
        iev.type = EV_KEY;
        iev.code = BTN_B;
        iev.value = 1;
      } else if ((ev[1 + offset].code == code_but_b) &&
                 (ev[1 + offset].value == 0)) {
        iev.type = EV_KEY;
        iev.code = BTN_B;
        iev.value = 0;
      } else {
        if (0)
          printf("Skipping Value0 %d Ev1Value %d Ev1Type %d Code[%d]\n", value,
                 ev[1].value, ev[1].type, (ev[1].code));
        offset = offset + 2;
        continue;
      }
      if (write(fd, &iev, sizeof(struct input_event)) < 0)
        perror_exit("error: write");
      if (write(fd, &ievs, sizeof(struct input_event)) < 0)
        perror_exit("error: write");
      offset = offset + 2;
    }
  }
  free(device);
  pthread_exit(NULL);
}

int is_keyboard(char *devname) {
  int result = 0;
  int i, j;
  unsigned long bit[EV_MAX][NBITS(KEY_MAX)];
  int fd = -1;
  fd = open(devname, O_RDONLY);
  if (fd < 0) {
    fprintf(stderr, "Error opening %s\n", devname);
    return -1;
  }

  memset(bit, 0, sizeof(bit));
  ioctl(fd, EVIOCGBIT(0, EV_MAX), bit[0]);

  for (i = 0; i < EV_MAX; i++)
    if (test_bit(i, bit[0])) {
      // printf("  Event type %d\n", i);
      if (!i) continue;
      ioctl(fd, EVIOCGBIT(i, KEY_MAX), bit[i]);
      for (j = 0; j < KEY_MAX; j++)
        if (test_bit(j, bit[i])) {
          if (j == KEY_K) result = 1;
          // printf("    Event code %d\n", j);
          // if (i == EV_ABS)
          //	print_absdata(fd, j);
        }
    }
  close(fd);
  return result;
}

static int is_event_device(const struct dirent *dir) {
  return strncmp(EVENT_DEV_NAME, dir->d_name, 5) == 0;
}

int main(int argc, char **argv) {
  struct dirent **namelist;
  int i, ndev, devnum;
  char *filename;
  pthread_t threads[200];  // hopefully enough
  int rc = 0, t = 0;

  if (getuid() != 0) {
    fprintf(stderr, "Not running as root, no devices may be available.\n");
    return -1;
  }
  ndev = scandir(DEV_INPUT_EVENT, &namelist, is_event_device, alphasort);
  if (ndev <= 0) return -1;

  // initial Scan for devices
  fprintf(stderr, "Available devices:\n");

  for (i = 0; i < ndev; i++) {
    printf("Checking device %d\n", i);
    // char fname[64];
    char *fname = malloc(64);
    // char name[256] = "???";
    int is_keyboard_res = 0;

    // snprintf(fname, sizeof(fname),
    snprintf(fname, 64, "%s/%s", DEV_INPUT_EVENT, namelist[i]->d_name);
    // test if it is a keyboard
    is_keyboard_res = is_keyboard(fname);
    if (is_keyboard_res) {
      printf("%s is a keyboard\n", fname);
      rc = pthread_create(&threads[t], NULL, K2Jinstance, (void *)fname);
      // sleep(1);
      if (rc) {
        printf("ERROR; return code from pthread_create() is %d\n", rc);
        exit(-1);
      }
      t = (t + 1) % 200;
    } else
      printf("%s is not a keyboard\n", fname);
    free(namelist[i]);
  }

  // Scan for new devices with inotify
  int notify_fd = inotify_init();
  if (notify_fd < 0) {
    perror("inotify_init");
    return 1;
  }

  int wd = inotify_add_watch(notify_fd, DEV_INPUT_EVENT, IN_CREATE);

  if (wd < 0) {
    perror("inotify_add_watch");
    return 1;
  }

  char buf[4096];
  for (;;) {
    // Render

    // cusleep(100000);

    // Read inotify events
    int len = read(notify_fd, buf, sizeof(buf));
    if (len <= 0) continue;

    int pos = 0;
    while (pos < len) {
      struct inotify_event *ev = (void *)&buf[pos];

      if (42) {
        char full[256];
        snprintf(full, sizeof(full), "%s/%s", DEV_INPUT_EVENT, ev->name);
        printf("New Device Detected %s\n", ev->name);

        if (is_keyboard(full)) {
          printf("%s is a keyboard\n", full);
          rc = pthread_create(&threads[t], NULL, K2Jinstance, (void *)full);
          // sleep(1);
          if (rc) {
            printf("ERROR; return code from pthread_create() is %d\n", rc);
            exit(-1);
          }
          t = (t + 1) % 200;
        }
      }

      pos += sizeof(struct inotify_event) + ev->len;
    }
  }

  int *retval;
  for (i = 0; i < t; i++) pthread_join(threads[i], (void **)&retval);
  return 0;
}
