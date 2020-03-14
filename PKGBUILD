# Maintainer: Spencer Michaels <spencer@spencermichaels.net>
_pkgname=btrroll
pkgname="${_pkgname}-git"
pkgver=xxx # TODO
pkgrel=1
license=("MIT")
pkgdesc="initramfs hook for system rollbacks via btrfs snapshots"
makedepends=("git")
depends=("dialog" "btrfs-progs")
optdepends=()
arch=("any")
provides=("btrroll")
conflicts=("btrroll")
source=("${_pkgname}::git+https://github.com/SpencerMichaels/btrroll.git")
sha512sums=("SKIP")

pkgver() {
    cd "$_pkgname"
    printf "r%s.%s" "$(git rev-list --count HEAD)" "$(git rev-parse --short HEAD)"
}

prepare() {
    cd "$_pkgname"
}

package() {
    cd "$_pkgname"
    cd btrroll
    make
    cd ..

    install -Dm0755 btrroll/bin/btrroll "${pkgdir}/usr/bin/btrroll"
    install -Dm0755 btrroll.hook "${pkgdir}/usr/lib/initcpio/hooks/btrroll"
    install -Dm0755 btrroll.install "${pkgdir}/usr/lib/initcpio/install/btrroll"
    install -Dm0644 btrroll.service "${pkgdir}/usr/lib/systemd/system/btrroll.service"
}
