project('hdr-tonemap', 'c', 'cpp',
	default_options : ['c_std=c11', 'cpp_std=c++11'])

src = files('image.cpp',
	'linux-main.cpp',
	'opengl-helper.cpp',
	'perf-monitor.cpp',
	'tonemap.cpp')

egl_dep = dependency('egl', required : false)
glesv2_dep = dependency('glesv2', required : false)
gl_dep = dependency('OpenGL', required : false)
glfw_dep = dependency('glfw3', required : true)

if egl_dep.found() and glesv2_dep.found()
	executable('tonemap', src, dependencies : [egl_dep, glesv2_dep, glfw_dep])
elif gl_dep.found()
	executable('tonemap', src, dependencies : [gl_dep, glfw_dep])
endif

conf_data = configuration_data()
conf_data.set10('HAVE_EGL', egl_dep.found())
conf_data.set10('HAVE_GLES', glesv2_dep.found())
conf_data.set10('HAVE_GL', gl_dep.found())
configure_file(output : 'config.h', configuration : conf_data)
