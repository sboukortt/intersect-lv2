# Maintainer: Sami Boukortt <sami@boukortt.com>
pkgname=intersect-lv2
pkgver=1.1
pkgrel=1
pkgdesc="LV2 plugin to split 2 audio channels into 3."
arch=("$CARCH")
url="https://github.com/sboukortt/$pkgname"
license=('GPL')
groups=('lv2-plugins')
depends=('fftw')
makedepends=('git' 'tup' 'clang' 'lv2')
optdepends=('lv2proc: for the `intersect` script')
options=('!ccache')
source=("git+https://github.com/sboukortt/intersect-lv2.git#tag=$pkgver")
sha512sums=('SKIP')

prepare() {
	cd $pkgname
	echo 'CONFIG_SKIP_TESTS=1' >> config/release.config
}

build() {
	cd $pkgname
	[ -d build-release ] || tup variant config/release.config
	tup
}

package() {
	cd $pkgname
	install -Dm755 intersect "$pkgdir"/usr/bin/intersect

	cd build-release
	install --directory "$pkgdir"/usr/lib/lv2
	cp -r intersect.lv2 "$pkgdir"/usr/lib/lv2/
}
