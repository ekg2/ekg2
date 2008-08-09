#!/usr/bin/scons
# vim:set fileencoding=utf-8
#  Alternate build system for EKG2, unstable and unfinished yet
#  (C) 2008 Michał Górny
#
#  configure.ac ported to: idn

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

def CheckStructMember(context, struct, member, headers):
	context.Message('Checking for %s.%s... ' % (struct, member))
	testprog = ''
	for header in headers:
		testprog += '#include <%s>\n' % (header)
	testprog += '\nint main(void) {\n\tstatic %s tmp;\n\tif (tmp.%s)\n\t\treturn 0;\n\treturn 0;\n}\n' % (struct, member)
	
	result = context.TryCompile(testprog, 'C')
	context.Result(result)
	return not not result

opts = Options('options.cache')

avplugins = [elem.split('/')[1] for elem in glob.glob('plugins/*/')]
avplugins.extend(['stable', 'unstable', 'experimental'])
opts.Add(ListOption('PLUGINS', 'List of plugins to build', 'unstable', avplugins))
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

conf = env.Configure(custom_tests = {'CheckStructMember': CheckStructMember})

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


	# XXX: needs testing
struct_members = {
	'struct kinfo_proc':	['ki_size', 'sys/param.h', 'sys/user.h']
	}

for struct, headers in struct_members.items():
	member = headers.pop(0)
	writedef('HAVE_%s_%s' % (struct.upper().replace(' ', '_'), member.upper().replace('.', '_')),
			conf.CheckStructMember(struct, member, headers))

sys_types = {
	'socklen_t':			['sys/types.h', 'sys/socket.h']
	}

for type, headers in sys_types.items():
	includes = ''
	for inc in headers:
		includes += '#include <%s>\n' % (inc)
	writedef('HAVE_%s' % (type.upper()), conf.CheckType(type, includes))

possibly_libbized = {
	'dlopen':	'dl',
	'iconv':	'iconv'
	}

for func, lib in possibly_libbized.items():
	if conf.CheckFunc(func):
		ret = True
	elif conf.CheckLib(lib, func):
		ret = True
		ekg_libs.append(lib)
	else:
		ret = False
	writedef('HAVE_%s' % (func.upper()), ret)

have_idn = conf.CheckLibWithHeader('idn', ['stringprep.h'], 'C', 'stringprep_check_version(NULL);')
writedef('LIBIDN', have_idn)
ekg_libs.append('idn')

plugin_def = {
	'type':			'misc',
	'state':		'experimental',
	'depends':		[]
	}

plugins = env['PLUGINS']
plugin_states = ['nocompile', 'deprecated', 'experimental', 'unstable', 'stable']
plugins_state = plugin_states.index('experimental')

for st in plugin_states:
	if st in avplugins:
		avplugins.remove(st)

for st in reversed(plugin_states):
	while st in plugins:
		plugins.remove(st)
		plugins_state = plugin_states.index(st)
	plugins.extend(avplugins)

plugins = {}.fromkeys(plugins).keys() # uniq()
plugins.sort()

pl = {}

for plugin in list(plugins):
	plugpath = 'plugins/%s' % (plugin)
	info = SConscript('%s/SConscript' % (plugpath))
	if not info:
		info = plugin_def
	if plugin_states.index(info['state']) < plugins_state:
		plugins.remove(plugin)
		continue

	if 'nocompile' in info:
		plugins.remove(plugin)
		print '[%s] Disabling due to build system incompatibility (probably junk in srcdir).' % (plugin)
		continue
	if info['depends']:
		plugins.remove(plugin)
		print '[%s] Unknown dependencies: %s' % (plugin, ', '.join(info['depends']))
		continue

	type = info['type']
	if not pl.has_key(type):
		pl[type] = []
	pl[type].append(plugin)

if pl:
	print 'Enabled plugins:'
	for type, plugs in pl.items():
		print '- %s: %s' % (type, ', '.join(plugs))

conf.Finish()

definefile.close()

StaticLibrary('compat/compat', compat)

env.Program('ekg/ekg2', Glob('ekg/*.c'), LIBS = ekg_libs, LIBPATH = './compat')

for plugin in plugins:
	plugpath = 'plugins/%s' % (plugin)
	Mkdir('%s/.libs' % (plugin))
	env.SharedLibrary('%s/.libs/%s' % (plugpath, plugin), Glob('%s/*.c' % (plugpath)), LIBPREFIX = '', LIBS = [])

# vim:ts=4:sts=4:syntax=python
