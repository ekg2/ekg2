#!/usr/bin/scons
# vim:set fileencoding=utf-8
#  Alternate build system for EKG2, unstable and unfinished yet
#  (C) 2008 Michał Górny

EnsureSConsVersion(0, 98)

consts = {
	'VERSION':		'SVN',
	'SHARED_LIBS':	True
	}
dirs = {
	'PREFIX':		'/usr',
	'SYSCONFDIR':	'/etc',
	'LOCALEDIR':	'$PREFIX/share/locale',
	'DATADIR':		'$PREFIX/share',
	'PLUGINDIR':	'$PREFIX/lib/ekg2/plugins'
	}
mapped = {
	'UNICODE':		'USE_UNICODE'
	}

plugin_states = ['nocompile', 'deprecated', 'unknown', 'experimental', 'unstable', 'stable']
plugin_symbols = ['!', '!', '?', '*', '~', '']

import glob, sys, subprocess

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

def StupidPythonExec(cmd):
	p		= subprocess.Popen(cmd, shell = True, stdout = subprocess.PIPE, stderr = subprocess.PIPE, stdin = subprocess.PIPE)
	stdout	= p.stdout.read().strip()
	stderr	= p.stderr.read().strip()
	ret		= p.wait()
	
	return ret, stdout, stderr

def PkgConfig(context, pkg, libs, ccflags, linkflags):
	context.Message('Asking pkg-config about %s... ' % (pkg))
	res = StupidPythonExec('pkg-config --libs-only-l "%s"' % (pkg))
	ret = not res[0]
	if ret:
		libs.extend([s[2:] for s in res[1].split()])
		res = StupidPythonExec('pkg-config --libs-only-L "%s"' % (pkg))
		ret = not res[0]
		if ret:
			libs.extend([s[2:] for s in res[1].split()])
			res = StupidPythonExec('pkg-config --libs-only-other "%s"' % (pkg))
			ret = not res[0]
			if ret:
				linkflags.append(res[1])
				res = StupidPythonExec('pkg-config --cflags "%s"' % (pkg))
				ret = not res[0]
				if ret:
					ccflags.append(res[1])
	context.Result(ret)
	return ret

def ExtTest(name, addexports = []):
	exports = ['conf', 'defines', 'env']
	exports.extend(addexports)
	ret = SConscript('scons.d/%s' % (name), exports)
	return ret

opts = Options('options.cache')

avplugins = [elem.split('/')[1] for elem in glob.glob('plugins/*/')]
avplugins.extend(plugin_states)
opts.Add(ListOption('PLUGINS', 'List of plugins to build', 'unstable', avplugins))
opts.Add(BoolOption('UNICODE', 'Whether to build unicode version of ekg2', True))

for var,path in dirs.items():
	opts.Add(PathOption(var, '', path, PathOption.PathAccept))

env = Environment()
env.Append(CCFLAGS = ' -I.')
opts.Update(env)
opts.Save('options.cache', env)
env.Help(opts.GenerateHelpText(env))

defines = {}

for var in dirs.keys():
	if var == 'PREFIX':
		continue
	env[var] = env.subst(env[var])
	defines[var] = env[var]
for var,val in consts.items():
	defines[var] = val
for var,val in mapped.items():
	defines[val] = env[var]

conf = env.Configure(custom_tests = {'CheckStructMember': CheckStructMember, 'PkgConfig': PkgConfig})
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
	ccflags = []
	linkflags = []
	for dep in info['depends']:
		if not ExtTest(dep, ['libs', 'ccflags', 'linkflags']):
			print '[%s] Dependency not satisfied: %s' % (plugin, dep)
			info['fail'] = True
	if 'fail' in info:
		del plugins[plugin]
		continue

	optdeps = []
	for dep in info['optdepends']:
		have_it = ExtTest(dep, ['libs', 'ccflags', 'linkflags'])
		if not have_it:
			print '[%s] Optional dependency not satisfied: %s' % (plugin, dep)
			optdeps.append('-%s' % (dep))
		else:
			optdeps.append('%s' % (dep))

	if not ccflags:
		ccflags = ['']
	if not linkflags:
		linkflags = ['']
	plugins[plugin] = {
		'libs':			libs,
		'ccflags':		ccflags.pop(),
		'linkflags':	linkflags.pop()
		}

	type = info['type']
	if not pl.has_key(type):
		pl[type] = []

	if optdeps:
		optdeps = ' [%s]' % (' '.join(optdeps))
	else:
		optdeps = ''

	pl[type].append('%s%s%s' % (plugin_symbols[plugin_states.index(info['state'])], plugin, optdeps))

# some fancy output

print
if pl:
	print 'Enabled plugins:'
	for type, plugs in pl.items():
		print '- %s: %s' % (type, ', '.join(plugs))
	print

print 'Options:'
print '- unicode: %s' % (env['UNICODE'])
print
print 'Paths:'
for k in dirs.keys():
	print '- %s: %s' % (k, env[k])
print
print 'Build environment:'
for k in ['CC', 'CCFLAGS', 'LIBS', 'LINK', 'LINKFLAGS']:
	print '- %s: %s' % (k, env[k])

conf.Finish()

writedefines()

cenv = env.Clone()
cenv.Append(LIBS = ekg_libs)
cenv.Append(LIBPATH = 'compat')
cenv.Append(LINKFLAGS = '-Wl,--export-dynamic')
cenv.Program('ekg/ekg2', Glob('ekg/*.c'))

for plugin, data in plugins.items():
	plugpath = 'plugins/%s' % (plugin)
	Mkdir('%s/.libs' % (plugin))

	penv = env.Clone()
	penv.Append(LIBS = data['libs'])
	penv.Append(CCFLAGS = data['ccflags'])
	penv.Append(LINKFLAGS = data['linkflags'])
	penv.SharedLibrary('%s/.libs/%s' % (plugpath, plugin), Glob('%s/*.c' % (plugpath)), LIBPREFIX = '')

# vim:ts=4:sts=4:syntax=python
