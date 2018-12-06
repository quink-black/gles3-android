project('hdr-tonemap', 'c', 'cpp',
	default_options : ['c_std=c11', 'cpp_std=c++11'])

libhdr2sdr_proj = subproject('hdr2sdr')
libhdr2sdr_dep = libhdr2sdr_proj.get_variable('libhdr2sdr_dep')

src = files(
	'main.cpp',
	'opengl-helper.cpp',
	'perf-monitor.cpp',
	'render.cpp')

egl_dep = dependency('egl', required : false)
glesv2_dep = dependency('glesv2', required : false)
gl_dep = dependency('OpenGL', required : false)
glfw_dep = dependency('glfw3', required : true)

if egl_dep.found() and glesv2_dep.found()
	executable('tonemap', src, dependencies : [libhdr2sdr_dep, egl_dep, glesv2_dep, glfw_dep])
elif gl_dep.found()
	executable('tonemap', src, dependencies : [libhdr2sdr_dep, gl_dep, glfw_dep])
endif

conf_data = configuration_data()
conf_data.set10('HAVE_EGL', egl_dep.found())
conf_data.set10('HAVE_GLES', glesv2_dep.found())
conf_data.set10('HAVE_GL', gl_dep.found())
configure_file(output : 'config.h', configuration : conf_data)
