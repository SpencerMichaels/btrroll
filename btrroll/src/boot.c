#include <linux/magic.h>
#include <linux/reboot.h>
#include <sys/reboot.h>
#include <unistd.h>

#include <boot.h>

void restart() {
  // TODO: Make sure this does not cause any data loss
  sync();
  reboot(LINUX_REBOOT_CMD_RESTART);
}

void shutdown() {
  // TODO: Make sure this does not cause any data loss
  sync();
  reboot(LINUX_REBOOT_CMD_POWER_OFF);
}
