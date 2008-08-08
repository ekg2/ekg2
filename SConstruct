#!/usr/bin/scons
#  Alternate build system for EKG2, unstable and unfinished yet
#  (C) 2008 Michał Górny
#
#  configure.ac ported to: regex.h

consts = {
	'VERSION': 'SVN'
	}
dirs = {
	'SYSCONFDIR':	'/etc',
	'LOCALEDIR':	'/usr/share/locale',
	'DATADIR':		'/usr/share'
	}
mapped = {
	'UNICODE':		'USE_UNICODE'
	}

import glob, sys

def die(reason):
	print reason
	sys.exit(1)

def writedef(var, val):
	if val is None:
		definefile.write('#undef %s\n' % (var))
	if isinstance(val, str):
		definefile.write(('#define %s "%s"\n' % (var, val)))
	if isinstance(val, bool):
		if val:
			definefile.write(('#define %s 1\n' % (var)))
		else:
			definefile.write(('#undef %s\n' % (var)))
	elif isinstance(val, int):
		definefile.write(('#define %s %d\n' % (var, val)))

opts = Options('options.cache')

plugins = [elem.split('/')[1] for elem in glob.glob('plugins/*/')];
opts.Add(ListOption('PLUGINS', 'List of plugins to build', 'all', plugins))
opts.Add(BoolOption('UNICODE', 'Whether to build unicode version of ekg2', True))

for var,path in dirs.items():
	opts.Add(PathOption(var, '', path))

env = Environment()
env.Append(CCFLAGS = ' -I.')
opts.Update(env)
opts.Save('options.cache', env)
env.Help(opts.GenerateHelpText(env))

definefile = open('ekg2-config.h', 'w')
for var in dirs.keys():
	writedef(var, env[var])
for var,val in consts.items():
	writedef(var, val)
for var,val in mapped.items():
	writedef(val, env[var])

conf = env.Configure()

std_funcs = [
	'inet_aton', 'inet_ntop', 'inet_pton', 'getaddrinfo',
	'utimes', 'flock',
	'mkstemp'
	]

std_headers = [
	'regex.h'
	]

compat_funcs = ['strfry', 'strlcat', 'strlcpy', 'strndup', 'strnlen', 'scandir']

compat_spec = {
		'getopt_long': ['getopt', 'getopt1']
	}

for func in std_funcs:
	writedef('HAVE_%s' % (func.upper()), conf.CheckFunc(func))
for header in std_headers:
	writedef('HAVE_%s' % (header.upper().replace('.', '_')), conf.CheckHeader(header))

compat = []
for func in compat_funcs:
	have_it = conf.CheckFunc(func)
	writedef('HAVE_%s' % (func.upper()), have_it)
	if not have_it:
		compat.append('compat/%s.c' % (func))

for func, files in compat_spec.items():
	have_it = conf.CheckFunc(func)
	writedef('HAVE_%s' % (func.upper()), have_it)
	if not have_it:
		for file in files:
			compat.append('compat/%s.c' % (file))

ekg_libs = []
if compat:
	ekg_libs.append('compat')
if not conf.CheckFunc('dlopen'):
	if conf.CheckLib('dl', 'dlopen'):
		ekg_libs.append('dl')
	else:
		die('dlopen not found!') # XXX: on windows, we use LoadLibraryA() instead

platform_libs = {
	'kvm':		'kvm_openfiles', # bsd

	'nsl':		'gethostbyname', # sunos
	'socket':	'socket',
	'rt':		'sched_yield',

	'bind':		['inet_addr', '__inet_addr'],
	'wsock32':	None
	}

for lib, funcs in platform_libs.items():
	if not isinstance(funcs, list):
		funcs = [funcs]
	for func in funcs:
		if conf.CheckLib(lib, func):
			ekg_libs.append(lib)
			break

conf.Finish()

definefile.close()

StaticLibrary('compat/compat', compat)

env.Program('ekg/ekg2', Glob('ekg/*.c'), LIBS = ekg_libs, LIBPATH = './compat')

for plugin in env['PLUGINS']:
	plugpath = 'plugins/%s' % (plugin)
	Mkdir('%s/.libs' % (plugin))
	env.SharedLibrary('%s/.libs/%s' % (plugpath, plugin), Glob('%s/*.c' % (plugpath)), LIBPREFIX = '')
