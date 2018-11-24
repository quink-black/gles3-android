project('hdr-tonemap', 'c', 'cpp',
	default_options : ['c_std=c11', 'cpp_std=c++11'])

src = files('image.cpp',
	'linux-main.cpp',
	'opengl-helper.cpp',
	'perf-monitor.cpp',
	'tonemap.cpp')

egl_dep = dependency('egl', required : true)
glesv2_dep = dependency('glesv2', required : true)
glfw_dep = dependency('glfw3', required : true)

executable('tonemap', src, dependencies : [egl_dep, glesv2_dep, glfw_dep])
