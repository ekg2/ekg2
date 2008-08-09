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

def writedef(definefile, var, val):
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

def writedefines():
	definefile = open('ekg2-config.h', 'w')
	for k, v in defines.items():
		writedef(definefile, k, v)
	definefile.close()

def CheckStructMember(context, struct, member, headers):
	context.Message('Checking for %s.%s... ' % (struct, member))
	testprog = ''
	for header in headers:
		testprog += '#include <%s>\n' % (header)
	testprog += '\nint main(void) {\n\tstatic %s tmp;\n\tif (tmp.%s)\n\t\treturn 0;\n\treturn 0;\n}\n' % (struct, member)
	
	result = context.TryCompile(testprog, 'C')
	context.Result(result)
	return not not result

def ExtTest(name, addexports = []):
	exports = ['conf', 'defines']
	exports.extend(addexports)
	ret = SConscript('scons.d/%s' % (name), exports)
	return ret

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

defines = {}

for var in dirs.keys():
	defines[var] = env[var]
for var,val in consts.items():
	defines[var] = val
for var,val in mapped.items():
	defines[val] = env[var]

conf = env.Configure(custom_tests = {'CheckStructMember': CheckStructMember})
ekg_libs = []

ExtTest('standard', ['ekg_libs'])
compat = []
ExtTest('compat', ['ekg_libs', 'compat'])

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

writedefines()

if compat:
	StaticLibrary('compat/compat', compat)

env.Program('ekg/ekg2', Glob('ekg/*.c'), LIBS = ekg_libs, LIBPATH = './compat')

for plugin in plugins:
	plugpath = 'plugins/%s' % (plugin)
	Mkdir('%s/.libs' % (plugin))
	env.SharedLibrary('%s/.libs/%s' % (plugpath, plugin), Glob('%s/*.c' % (plugpath)), LIBPREFIX = '', LIBS = [])

# vim:ts=4:sts=4:syntax=python
