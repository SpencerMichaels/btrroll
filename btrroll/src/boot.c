#include <ctype.h>
#include <errno.h>
#include <linux/magic.h>
#include <linux/reboot.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <unistd.h>

#include <boot.h>
#include <macros.h>
#include <run.h>

// See https://www.freedesktop.org/wiki/Software/systemd/BootLoaderInterface/
#define EFI_VENDOR_ID "4a67b082-0a4c-41cf-b6c7-440b29bb8c4f"

void bootctl_entry_free(bootctl_entry_t *entry) {
  free(entry->id);
  free(entry->title);
  free(entry->source);
  free(entry->kernel);
  free(entry->options);
}

// TODO: Untested code!
int bootctl_list(const char *esp_path, bootctl_entry_t *entries, size_t entries_len) {
  const char *args[] = {
    "bootctl",
    "--esp-path", esp_path,
    "list",
    NULL
  };

  char buf[0x6000];
  int status = run_pipe("bootctl", args, buf, sizeof(buf), NULL, 0);
  if (status) {
    errno = status;
    return -1;
  }

  memset(entries, 0, entries_len * sizeof(bootctl_entry_t));
  bootctl_entry_t *entry = entries;
  char *p = buf;
  char *p_end;
  while ((p_end = strchr(p, '\n')) && entries_len) {
    *p_end = '\0';

    if (p == p_end) {
      ++entry;
      --entries_len;
    } else {
      // Strip leading whitespace
      char *key = p + strspn(p, " \t");

      // Isolate the key
      char *key_end = strchr(p, ':');
      if (!key_end)
        continue;
      *key_end = '\0';

      // Isolate the value
      char *value = key_end + 2;

      if (!strcmp("id", key))
        entry->id = strdup(value);
      else if (!strcmp("title", key))
        entry->title = strdup(value);
      else if (!strcmp("source", key))
        entry->source = strdup(value);
      else if (!strcmp("linux", key) || !strcmp("kernel", key))
        entry->kernel = strdup(value);
      else if (!strcmp("options", key))
        entry->options = strdup(value);
    }

    p = p_end+1;
  }

  // Count the last entry if one was found
  if (entry->id)
    ++entry;

  return entry - entries;
}

// TODO: validate id?
int bootctl_set_oneshot(const char *esp_path, const char *id) {
  const char *args[] = {
    "bootctl",
    "--esp-path", esp_path,
    "set-oneshot",
    id,
    NULL
  };

  eprintf("setting oneshot with %s and %s\n", esp_path, id);
  int err = run("bootctl", args);
  errno = err;
  return err;
}

// TODO: validate id?
int bootctl_set_default(const char *esp_path, const char *id) {
  const char *args[] = {
    "bootctl",
    "--esp-path", esp_path,
    "set-default",
    id,
    NULL
  };

  int err = run("bootctl", args);
  errno = err;
  return err;
}

// TODO: Untested code!
int mount_esp(char *mountpoint) {
  CLEANUP_DECLARE(ret);

  // Get the partition UUID of the ESP
  FILE * const fp = fopen(
      "/sys/firmware/efi/efivars/LoaderDevicePartUUID-" EFI_VENDOR_ID,
      "r");

  if (!fp) {
    perror("fopen");
    FAIL(ret);
  }

  // The EFI var in question is formatted with 2 leading shorts worth of metadata,
  // followed by shorts representing char values. We can't just read it a string;
  // have to do some conversion. TODO: Make sure this is not implementation-dependent.
  char partuuid[64];
  {
    short buf[128];
    size_t sz = fread(buf, 1, sizeof(buf), fp);
    if (!sz && ferror(fp)) {
      perror("fread");
      FAIL(ret);
    }
    char *p = partuuid;
    for (size_t i = 2; i < sz; ++i) {
      const short c = buf[i];
      if (c)
        *p++ = tolower((char) c);
    }
    *p = '\0';
  }

  // Mount the ESP by partition UUID
  char esp[64];
  snprintf(esp, sizeof(esp), "/dev/disk/by-partuuid/%s", partuuid);
  if (mount(esp, mountpoint, "vfat", MS_NOATIME, "")) {
    perror("mount");
    FAIL(ret);
  }

CLEANUP:
  if (fclose(fp))
    perror("fclose");

  return ret;
}

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
