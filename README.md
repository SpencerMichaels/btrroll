# `btrroll`: Boot-Time Root Partition Rollbacks for BTRFS

`btrroll` is a lightweight interactive console that enables a computer whose
root partition is a BTRFS subvolume to easily roll back to, or temporarily boot
to, snapshots of that subvolume at boot time. With `btrroll`, you can choose at
boot time to boot straight into an ephemeral rescue image, or restore a
known-good version of your root partition when an update renders it inoperable
— all without having to boot from an external device. `btrroll` also provides a
handy rescue console, for those times when you want to drop to a shell right in
your `initrd`.

## Requirements

`btrroll` requires that your root partition be located on a BTRFS subvolume
(_not_ the volume root directory). A typical BTRFS-based Linux install will
generally fulfill this requirement: the BTRFS sysadmin guide already
[recommends](https://btrfs.wiki.kernel.org/index.php/SysadminGuide#Managing_Snapshots)
keeping the system root on a subvolume as opposed to the filesystem root.

`btrroll` supports both systemd-based and traditional initscript-based initial
ramdisk configurations.

## Installation

If you're using Arch Linux, you can install `btrroll` using the [btrroll](TKTK)
AUR package. Otherwise, you can clone this repository manually and run `make &&
sudo make install` in the root directory.

`btrroll` has only one external dependency: the `dialog` binary, which it
uses for its interface. A typical Arch install likely already has `dialog`
installed.

## Usage

### Enabling `btrroll`

Add `btrroll` to the `HOOKS` section of your `/etc/mkinitcpio.conf` and
[rebuild your `initrd`](
https://wiki.archlinux.org/index.php/Mkinitcpio#Image_creation_and_activation).

If your `initrd` is systemd-based, make sure the `btrroll` hook appears _after_
the `systemd` hook. For an initscript-based `intrd`, place the `btrroll` hook
_before_ the `filesystems` hook (TKTK).

### Using `btrroll`

To open the `btrroll` console, simply tap "enter" at a steady rhythm as the
system boots. During the boot process, `btrroll` waits for a short time for the
user to press "enter" — one second by default — and, when this occurs, pauses
the boot process to display an interactive TUI console.

TKTK: image

To provision your root partition for use with `btrroll`, select "Boot/restore
from a snapshot" at the main menu, and select "yes" when asked to provision.
Your root subvolume will be moved from `subvol` to `subvol.d/current`, and a
symlink will be created pointing from the former to the latter. Based on my own
testing, this should not affect any other aspects of your system, but as
always, be careful if you have a particularly unusual setup.

Once the root partition has been provisioned, the "Boot/restore from a
snapshot" option will present a list of snapshots. All of the other options can
be used whether or not the root partition has been provisioned.

### Snapshots

During provisioning, a snapshots directory will be created at
`subvol.d/snapshots`; you can also replace this with a symlink or subvolume as
desired. Any BTRFS snapshots in this directory _not_ starting with a `.` will
be available to boot/restore via the `btrroll` console.

`btrroll` is not itself a backup application, so it is up to you to populate
this directory however you want using something like [btrbk](
https://github.com/digint/btrbk). For this reason, `btrroll` comes with a
generic way to save and view arbitrary details along with your snapshots,
however you may wish to generate them: place a file named `.btrroll-info` at
the root of the snapshot, and the contents of that file will be displayed when
the snapshot is displayed in the `btrroll` console. This can help if you want
to record things like which packages were installed since the last snapshot or
the reason the snapshot occurred (system update, manual, time-based, etc).

If no `.btrroll-info` file exists, `btrroll` will still display the filename,
last modification date (as recorded by the filesystem), and latest kernel
version of each snapshot.

## Configuration

`btrroll` does not generally require configuration, but a few options are made
available at `/etc/btrroll.conf` to cover any edge cases you may run into. Note
that, due to the nature of how `initrd` images work, you will need to [rebuild
your `initrd`](https://wiki.archlinux.org/index.php/Mkinitcpio#Image_creation_and_activation)
in order for any new configuration changes to take effect.

TKTK: to be finalized

* `timeout`: The time (in seconds) to wait for the user to press "enter" to
  bring up the `btrroll` console before continuing to boot. If zero, the
  console will always appear. Defaults to `1`.
* `mountpoint`: The directory to which `btrroll` will mount the BTRFS root
  partition within the `initrd` when symlink/snapshot manipulation is required.
  Defaults to `/btrfs_root`.

## How it Works

`btrroll` sits in the [initial ramdisk (`initrd`)](
https://en.wikipedia.org/wiki/Initial_ramdisk) and runs just before the system
root is mounted. It takes advantage of the fact that BTRFS subvolumes (and
symlinks to subvolumes) can be mounted just like regular partitions.

`btrroll` moves the original root subvolume — the `initrd`'s mount target —
into a parallel directory, replacing it with a symlink to its new location. It
can then manipulate symlink (or its target) at boot time to select alternate
boot targets on the fly before the `initrd` mounts the root partition, without
the need to alter kernel command line parameters.

# TODO

[x] ensure no directory traversal when entering backup filenames
[x] check for and reject toplevel root partition when provisioning
[x] handle a missing or invalid snapshots directory
[ ] use cmdline flags when mounting btrfs_root
[ ] move PKGBUILD install to "make install"
[ ] add config file
[ ] boot into different kernel versions with systemd-boot
