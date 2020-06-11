#ifndef __BOOT_H__
#define __BOOT_H__

typedef struct bootctl_entry {
  char *id,
       *title,
       *source,
       *kernel,
       *options;
} bootctl_entry_t;

void bootctl_entry_free(bootctl_entry_t *entry);

int bootctl_list(const char *esp_path, bootctl_entry_t *entries, size_t entries_len);

int bootctl_set_oneshot(const char *esp_path, const char *id);
int bootctl_set_default(const char *esp_path, const char *id);

int mount_esp(char *mountpoint);

void restart();
void shutdown();

#endif
