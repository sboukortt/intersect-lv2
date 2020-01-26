# Maintainer: Sami Boukortt <sami@boukortt.com>
pkgname=intersect-lv2
pkgver=1.3
pkgrel=1
pkgdesc="LV2 plugin to split 2 audio channels into 3."
arch=("$CARCH")
url="https://github.com/sboukortt/$pkgname"
license=('GPL')
groups=('lv2-plugins')
depends=('fftw')
makedepends=('git' 'meson' 'lv2')
optdepends=('lv2proc: for the `intersect` script')
source=("git+https://github.com/sboukortt/intersect-lv2.git#tag=$pkgver")
sha512sums=('SKIP')

build() {
	mkdir -p build
	cd build
	meson --buildtype=release --prefix=/usr "$srcdir/$pkgname"
	ninja
}

package() {
	cd build
	DESTDIR="$pkgdir" ninja install
}
