project('intersect-lv2', 'c')

lv2_dep = dependency('lv2')
fftw3_dep = dependency('fftw3f')

visibility_args = []
if meson.get_compiler('c').has_argument('-fvisibility=hidden')
	visibility_args = ['-fvisibility=hidden']
endif

intersect = shared_library(
	'intersect',
	'src/init.h',
	'src/init.c',
	'src/intersect.h',
	'src/intersect.c',
	'src/main.c',
	'src/types.h',
	'src/util.h',
	c_args: visibility_args,
	dependencies: [lv2_dep, fftw3_dep],
	name_prefix: '',
	install: true,
	install_dir: 'lib/lv2/intersect.lv2',
)

configuration = configuration_data()
configuration.set('EXT', 'so')
configure_file(
	input: 'metadata/manifest.ttl.in',
	output: 'manifest.ttl',
	configuration: configuration,
	install: true,
	install_dir: 'lib/lv2/intersect.lv2',
)

install_data(
	'metadata/intersect.ttl',
	install_dir: 'lib/lv2/intersect.lv2',
)

install_data(
	'intersect',
	install_dir: 'bin',
)
