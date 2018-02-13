#include <fcntl.h>
#include <unistd.h>

#include <lib/util/nullfd.h>

static int nullfd_ = -1;

int nullfd_init() {
  nullfd_cleanup();
  nullfd_ = open("/dev/null", O_RDWR);
  if (nullfd_ < 0) {
    return -1;
  }

  return 0;
}

void nullfd_cleanup() {
  if (nullfd_ >= 0) {
    close(nullfd_);
    nullfd_ = -1;
  }
}

int nullfd_get() {
  return nullfd_;
}
