#!/usr/bin/env sh
set -u -o pipefail

VERSION="1.0.0"
FSROOT_MOUNTPOINT="/fsroot"

alias dialog="dialog --backtitle \"btrroll $VERSION\""

BUTTON_YES=0
BUTTON_NO=1

BUTTON_OK=0
BUTTON_CANCEL=1

BUTTON_HELP=2
BUTTON_ITEM_HELP=2
BUTTON_EXTRA=3
BUTTON_ESC=255
BUTTON_ERROR=-1

# Get the version of a kernel image
# See https://www.kernel.org/doc/Documentation/x86/boot.txt
kernel_version() {
  local file="$1" # $1: Kernel image file path

  test -f "$file"

  local offset="$(od -j 0x20E -N 2 -t u2 -An "$file")"
  dd if="$file" skip=$((512 + offset)) bs=1 status=none | head -1 | awk '{print $1}'
}

did_cancel_or_escape() {
  local button=$1 # The return code of dialog (button pressed)

  [ $button -eq $BUTTON_CANCEL ] || \
  [ $button -eq $BUTTON_ESC ]
}

# Get an argument from a string of keys and/or key/value pairs, such as
# the kernel command line available at /proc/cmdline. NOTE: If multiple
# instances of the same key exist, only the last will be printed.
# - If the arg is not present, return 1
# - If the arg has no associated value (e.g. "rw"), return 0
# - If the arg has a value (e.g. "root=xxx"), return 0 and print the value
kvmap_get() {
  local arg="$1"  # The argument to query
  local sep="$2"  # The separator between key/value pairs
  local file="$3" # The file to read from (optional - reads stdin if absent)

  sed -En -e "/(^|$sep)$arg(=|\$|$sep)/!{q1}" \
          -e "s/.*(^|$sep)$arg=([^$sep]+).*/\1/p" "$file"
}

# Mount the root device with subvol=/, overriding any cmdline defaults
mount_root_subvol() {
  local root_device="$1"  # The device to mount (first positional argument of `mount`)
  local root_flags="$2"   # The value of "rootflags" from the kernel cmdline

  umount "$FSROOT_MOUNTPOINT" || true
  mkdir -p "$FSROOT_MOUNTPOINT" && \
  mount -t btrfs -o \
      "$(echo "$root_flags" | \
          sed -E -e '/(^|,)subvol=[^,]+/{s//\1subvol=\//g;q}' \
                 -e 's/^/subvol=\/,/')" \
      "$root_device" "$FSROOT_MOUNTPOINT"
}

# True if the provided path is a symlink to a sublovume
is_subvol_link() {
  local subvol_path="$1"  # The subvol's path relative to the FS root

  local path_abs="$FSROOT_MOUNTPOINT/$subvol_path"
  local path_abs_real="$FSROOT_MOUNTPOINT/$(readlink "$path_abs")"

  [ -L "$path_abs" ] && \
  [ -d "$path_abs" ] && \
  [ "$(stat -c %i "$path_abs_real")" = "256" ]
}

# Get the path of a root device's subvolume relative to the FS root
get_subvol_path() {
  local root_flags="$1"   # The value of "rootflags" from the kernel cmdline

  local path="$(echo "$root_flags" | kvmap_get subvol ',')"
  if [ -z "$path" ]; then
    path="$(btrfs subvolume get-default "$FSROOT_MOUNTPOINT" | cut -d' ' -f9)"
  fi
  echo "$path"
}

setup_root_subvol() {
  local subvol_path="$1" # The subvol's path relative to the FS root

  local path_abs="$FSROOT_MOUNTPOINT/$subvol_path"

  mkdir "${path_abs}.d" && \
  mv "${path_abs}" "${path_abs}.d/working" && \
  ln -s "${path_abs}" "${path_abs}.d/working" && \
  btrfs subvolume create "${path_abs}/snapshots"
}

setup_confirm() {
  local subvol_path="$1"  # The subvol's path relative to the FS root

  dialog --title 'Confirm Setup' --yesno \
"The following operations will be performed on the root device:

1. A directory '${subvol_path}.d/' will be created
2. The root subvolume '${subvol_path}' will be moved to '${subvol_path}.d/working'
3. A symlink will be created at '${subvol_path}' pointing to '${subvol_path}.d/working'
4. A new subvolume will be created at '${subvol_path}.d/snapshots'

These changes will not affect your normal boot process, but will allow btrroll
to change the boot target in order to boot into previous system states." \
  25 80
}

snapshot_menu() {
  local root_device="$(kvmap_get root ' ' /proc/cmdline)"
  local root_flags="$(kvmap_get rootflags ' ' /proc/cmdline)"

  if [ -z "$root_device" ]; then
    dialog --title 'Error' --msgbox \
'No root device is specifed in the kernel command line!
/proc/cmdline does not contain a `root=...` flag.' \
        12 34
    return 1
  fi

  local mount_output="$(mount_root_subvol "$root_device" "$root_flags" 2>&1)"
  if [ $? -ne 0 ]; then
    dialog --title 'Error' --msgbox \
        "Failed to mount the root device!\n\n$mount_output" \
        12 34
    return 1
  fi
  
  local subvol_path="$(get_subvol_path "$root_flags")"

  # TODO: Offer to fix
  if ! is_subvol_link "$subvol_path"; then
    dialog --title 'Error' --msgbox \
        "The root subvolume \'$subvol_path\' is not set up for use with btrroll." \
        12 34
    return 0
  fi

  local item="$(dialog \
      --title 'Snapshots' \
      --menu 'Select a snapshot from the list below.' \
      --cancel-label 'Back'
      12 34 5 \
      "$ITEM_SNAPSHOT"  'Restore from a snapsho' \
      2>&1 >/dev/tty)"
  local button=$?
}

main_menu() {
  local ITEM_SNAPSHOT=1
  local ITEM_SHELL=2
  local ITEM_REBOOT=3
  local ITEM_SHUTDOWN=4
  local ITEM_EXIT=5

  while true; do
    local item="$(dialog \
        --title 'Main Menu' \
        --menu 'What would you like to do?' \
        12 34 5 \
        "$ITEM_SNAPSHOT"  'Manage snapshots' \
        "$ITEM_SHELL"     'Launch a shell' \
        "$ITEM_REBOOT"    'Reboot' \
        "$ITEM_SHUTDOWN"  'Shutdown' \
        "$ITEM_EXIT"      'Exit (continue booting)' \
        2>&1 >/dev/tty)"

    did_cancel_or_escape $? && exit 0

    case "$item" in
      "$ITEM_SNAPSHOT")
        snapshot_menu
        ;;
      "$ITEM_SHELL")
        clear
        /bin/sh
        ;;
      "$ITEM_REBOOT")
        systemctl reboot
        ;;
      "$ITEM_SHUTDOWN")
        systemctl poweroff
        ;;
      "$ITEM_EXIT")
        exit 0
        ;;
    esac
  done
}

cleanup() {
  if mountpoint -q "$FSROOT_MOUNTPOINT"; then
    umount "$FSROOT_MOUNTPOINT"
  fi
}

trap cleanup EXIT
main_menu
