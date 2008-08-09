#!/usr/bin/scons
# vim:set fileencoding=utf-8
#  Alternate build system for EKG2, unstable and unfinished yet
#  (C) 2008 Michał Górny

EnsureSConsVersion(0, 98)

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

plugin_states = ['nocompile', 'deprecated', 'unknown', 'experimental', 'unstable', 'stable']
plugin_symbols = ['!', '!', '?', '*', '~', '']

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

ExtTestsCache = {}

def ExtTest(name, addexports = []):
	if name in ExtTestsCache.keys():
		return ExtTestsCache[name]
	exports = ['conf', 'defines', 'env']
	exports.extend(addexports)
	ret = SConscript('scons.d/%s' % (name), exports)
	ExtTestsCache[name] = ret
	return ret

opts = Options('options.cache')

avplugins = [elem.split('/')[1] for elem in glob.glob('plugins/*/')]
avplugins.extend(plugin_states)
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
ExtTest('compat', ['ekg_libs'])

plugin_def = {
	'type':			'misc',
	'state':		'unknown',
	'depends':		[],
	'optdepends':	[]
	}

plugins = env['PLUGINS']
plugins_state = 0

for st in plugin_states:
	if st in avplugins:
		avplugins.remove(st)

for st in reversed(plugin_states):
	while st in plugins:
		plugins.remove(st)
		plugins_state = plugin_states.index(st)
	plugins.extend(avplugins)

plugins.sort()
plugins = {}.fromkeys(plugins)

pl = {}

for plugin in list(plugins.keys()):
	plugpath = 'plugins/%s' % (plugin)
	info = SConscript('%s/SConscript' % (plugpath))
	if not info:
		info = plugin_def
	else:
		for k in plugin_def.keys():
			if not k in info:
				info[k] = plugin_def[k]
	if plugin_states.index(info['state']) < plugins_state:
		del plugins[plugin]
		continue

	if 'nocompile' in info:
		del plugins[plugin]
		print '[%s] Disabling due to build system incompatibility (probably junk in srcdir).' % (plugin)
		continue

	libs = []
	for dep in info['depends']:
		if not ExtTest(dep, ['libs']):
			print '[%s] Dependency not satisfied: %s' % (plugin, dep)
			info['fail'] = True
	if 'fail' in info:
		del plugins[plugin]
		continue

	optdeps = []
	for dep in info['optdepends']:
		have_it = ExtTest(dep, ['libs'])
		if not have_it:
			print '[%s] Optional dependency not satisfied: %s' % (plugin, dep)
			optdeps.append('-%s' % (dep))
		else:
			optdeps.append('%s' % (dep))

	plugins[plugin] = {
		'libs':	libs
		}

	type = info['type']
	if not pl.has_key(type):
		pl[type] = []

	if optdeps:
		optdeps = ' [%s]' % (' '.join(optdeps))
	else:
		optdeps = ''

	pl[type].append('%s%s%s' % (plugin_symbols[plugin_states.index(info['state'])], plugin, optdeps))

print
if pl:
	print 'Enabled plugins:'
	for type, plugs in pl.items():
		print '- %s: %s' % (type, ', '.join(plugs))

conf.Finish()

writedefines()

env.Program('ekg/ekg2', Glob('ekg/*.c'), LIBS = ekg_libs, LIBPATH = './compat')

for plugin, data in plugins.items():
	plugpath = 'plugins/%s' % (plugin)
	Mkdir('%s/.libs' % (plugin))
	env.SharedLibrary('%s/.libs/%s' % (plugpath, plugin), Glob('%s/*.c' % (plugpath)), LIBPREFIX = '', LIBS = data['libs'])

# vim:ts=4:sts=4:syntax=python
