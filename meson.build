project('intersect-lv2', 'cpp', version: '1.4.0', default_options: ['cpp_std=c++17'])

static = target_machine.system() == 'windows' or target_machine.system() == 'darwin'

fftw3_dep = dependency('fftw3f', static: static)
if static
	# Due to apparent issues when trying to statically link to system-wide Highway, bypass and use the subproject directly
	hwy_dep = subproject('highway', default_options: [
		'contrib=disabled',
		'examples=disabled',
		'tests=disabled',
	]).get_variable('hwy_dep')
else
	hwy_dep = dependency('libhwy', version: '1.3.0', default_options: [
		'contrib=disabled',
		'examples=disabled',
		'tests=disabled',
	])
endif
lv2_dep = dependency('lv2', static: static, default_options: [
	'docs=disabled',
	'old_headers=false',
	'plugins=disabled',
	'tests=disabled',
])

visibility_args = []
if meson.get_compiler('cpp').has_argument('-fvisibility=hidden')
	visibility_args = ['-fvisibility=hidden']
endif

link_args = []
if static
	foreach arg : ['-static', '-static-libstdc++']
		if meson.get_compiler('cpp').has_argument(arg)
			link_args += arg
		endif
	endforeach
endif

intersect = shared_library(
	'intersect',
	'src/init.h',
	'src/init.cc',
	'src/intersect.h',
	'src/intersect.cc',
	'src/main.cc',
	'src/types.h',
	cpp_args: visibility_args,
	link_args: link_args,
	include_directories: ['src'],
	dependencies: [hwy_dep, lv2_dep, fftw3_dep],
	name_prefix: '',
	install: true,
	install_dir: 'lib/lv2/intersect.lv2',
)

configuration = configuration_data()
if target_machine.system() == 'windows'
	configuration.set('EXT', 'dll')
elif target_machine.system() == 'darwin'
	configuration.set('EXT', 'dylib')
else
	configuration.set('EXT', 'so')
endif
configure_file(
	input: 'metadata/manifest.ttl.in',
	output: 'manifest.ttl',
	configuration: configuration,
	install: true,
	install_dir: 'lib/lv2/intersect.lv2',
	install_tag: 'runtime',
)

install_data(
	'metadata/intersect.ttl',
	install_dir: 'lib/lv2/intersect.lv2',
	install_tag: 'runtime',
)

install_data(
	'intersect',
	install_dir: 'bin',
)
