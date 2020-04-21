#include "console.h"
#include <stdio.h>
#include <termios.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include "regs.h"
#include "machine.h"
#include <sys/ioctl.h>

static struct termios oldtty;
static int old_fd0_flags;
static stdio_device_t *global_stdio_device;

static void term_exit(void)
{
  /* recovery old flags */
  tcsetattr(0, TCSANOW, &oldtty);
  fcntl(0, F_SETFL, old_fd0_flags);
}

static void term_init(bool allow_ctrlc)
{
  struct termios tty;

  memset(&tty, 0, sizeof(tty));
  tcgetattr(0, &tty);
  oldtty = tty;
  old_fd0_flags = fcntl(0, F_GETFL);

  tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP
                        |INLCR|IGNCR|ICRNL|IXON);
  tty.c_oflag |= OPOST;
  tty.c_lflag &= ~(ECHO|ECHONL|ICANON|IEXTEN);
  if (!allow_ctrlc)
      tty.c_lflag &= ~ISIG;
  tty.c_cflag &= ~(CSIZE|PARENB);
  tty.c_cflag |= CS8;
  tty.c_cc[VMIN] = 1;
  tty.c_cc[VTIME] = 0;
  
  tcsetattr(0, TCSANOW, &tty);
  /* call term_exit when call exit() */
  atexit(term_exit);
}

static void term_resize_handler(int sig)
{
  if (global_stdio_device)
    global_stdio_device->resize_pending = true;
}

static int_t console_write(void *opaque, const uint8_t *buf, int len)
{
    fwrite(buf, 1, len, stdout);
    fflush(stdout);
    return len;
}

static int_t console_read(void *opaque, uint8_t *buf, int len)
{
    stdio_device_t *s = opaque;
    int ret, i, j;
    uint8_t ch;
    
    if (len <= 0)
        return 0;

    ret = read(s->stdin_fd, buf, len);
    if (ret < 0)
        return 0;
    if (ret == 0) {
        /* EOF */
        exit(1);
    }

    j = 0;
    for(i = 0; i < ret; i++) {
        ch = buf[i];
        if (s->console_esc_state) {
            s->console_esc_state = 0;
            switch(ch) {
            case 'x':
                printf("Terminated\n");
                exit(0);
            case 'h':
                printf("\n"
                       "C-a h   print this help\n"
                       "C-a x   exit emulator\n"
                       "C-a C-a send C-a\n"
                       );
                break;
            case 1:
                goto output_char;
            default:
                break;
            }
        } else {
            if (ch == 1) {
                s->console_esc_state = 1;
            } else {
            output_char:
                buf[j++] = ch;
            }
        }
    }
    return j;
}

character_device_t *console_init(bool allow_ctrlc)
{
  character_device_t *cdev;
  stdio_device_t *stdio_device;
  struct sigaction sig;

  term_init(allow_ctrlc);

  cdev = malloc(sizeof(*cdev));
  memset(cdev, 0, sizeof(*cdev));

  stdio_device = malloc(sizeof(*stdio_device));
  memset(stdio_device, 0, sizeof(*stdio_device));
  stdio_device->stdin_fd = 0;

  fcntl(stdio_device->stdin_fd, F_SETFL, O_NONBLOCK);

  stdio_device->resize_pending = true;
  global_stdio_device = stdio_device;

  sig.sa_handler = term_resize_handler;
  sigemptyset(&sig.sa_mask);
  sig.sa_flags = 0;
  sigaction(SIGWINCH, &sig, NULL);

  cdev->opaque = stdio_device;
  cdev->write_data = console_write;
  cdev->read_data  = console_read;

  return cdev;
}

static int console_item_init(address_item_t *handler)
{
  return true;
}

static void console_release(address_item_t *handler)
{
  virtio_console_device_t *vcd = (virtio_console_device_t*)handler->entity;
  free(vcd);
}

void console_get_size(stdio_device_t *dev, int *pw, int *ph)
{
  struct winsize ws;
  int width, height;

  width = 80;
  height = 25;
  if (ioctl(dev->stdin_fd, TIOCGWINSZ, &ws) == 0 &&
      ws.ws_col >= 4 && ws.ws_row >= 4)
  {
    width = ws.ws_col;
    height = ws.ws_row;
  }
  *pw = width;
  *ph = height;
}

static address_item_t console_item =
{
  .name = "console",
  .init = console_item_init,
  .release = console_release
};

void virtual_console_device_init(cpu_state_t *state, virtual_io_bus_t *bus)
{
  virtio_console_device_t *vcd;

  vcd = malloc(sizeof(*vcd));
  memset(vcd, 0, sizeof(*vcd));
  vcd->cs = console_init(true);
  virtio_init(state, &console_item, &vcd->common, bus, 3, 4, virtio_console_recv_request);
  vcd->common.device_features = (1 << 0);
  vcd->common.queue[0].manual_recv = true;

  riscv_machine.console = vcd;

  return;
}
