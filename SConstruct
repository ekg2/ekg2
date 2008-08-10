#!/usr/bin/scons
# vim:set fileencoding=utf-8
#  Alternate build system for EKG2, unstable and unfinished yet
#  (C) 2008 Michał Górny

consts = {
	'VERSION':		'SVN',
	'SHARED_LIBS':	True
	}
dirs = {
	'DESTDIR':		'',
	'PREFIX':		'/usr/local',
	'SYSCONFDIR':	'/etc',
	'BINDIR':		'$PREFIX/bin',
#	'INCLUDEDIR':	'$PREFIX/include/ekg',
	'LOCALEDIR':	'$PREFIX/share/locale',
	'DATADIR':		'$PREFIX/share/ekg2',
	'PLUGINDIR':	'$PREFIX/lib/ekg2/plugins',
	'IOCTLD_PATH':	'$PREFIX/libexec/ekg2'
	}
mapped = {
	'UNICODE':		'USE_UNICODE'
	}
envs = {
	'CCFLAGS':		'Compiler flags',
	'LINKFLAGS':	'Linker flags'
	}

plugin_states = ['nocompile', 'deprecated', 'unknown', 'experimental', 'unstable', 'stable']
plugin_symbols = ['!', '!', '?', '*', '~', '']

import glob, subprocess, codecs

""" Write define to definefile, choosing correct format. """
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

""" Write list 'defines' to definefile. """
def writedefines():
	definefile = open('ekg2-config.h', 'w')
	for k, v in defines.items():
		writedef(definefile, k, v)
	definefile.close()


""" Check whether struct with given member is available. """
def CheckStructMember(context, struct, member, headers):
	context.Message('Checking for %s.%s... ' % (struct, member))
	testprog = ''
	for header in headers:
		testprog += '#include <%s>\n' % (header)
	testprog += '\nint main(void) {\n\tstatic %s tmp;\n\tif (tmp.%s)\n\t\treturn 0;\n\treturn 0;\n}\n' % (struct, member)

	result = context.TryCompile(testprog, '.c')
	context.Result(result)
	return not not result

""" Execute given cmd and return tuple with errorcode, data written to stdout and to stderr. """
def StupidPythonExec(cmd):
	p		= subprocess.Popen(cmd, shell = True, stdout = subprocess.PIPE, stderr = subprocess.PIPE, stdin = subprocess.PIPE)
	stdout	= p.stdout.read().strip()
	stderr	= p.stderr.read().strip()
	ret		= p.wait()
	
	return ret, stdout, stderr

""" Ask pkg-config (or *-config) about given package. """
def PkgConfig(context, pkg, libs, ccflags, linkflags, pkgconf = 'pkg-config', version = None):
	context.Message('Asking %s about %s... ' % (pkgconf, pkg))
	if pkgconf != 'pkg-config':
		pkg = ''
	else:
		pkg = ' "%s"' % (pkg)

	if version is not None:
		if pkgconf == 'pkg-config':
			vermod = 'mod'
		else:
			vermod = ''
		res = StupidPythonExec('"%s" --%sversion%s' % (pkgconf, vermod, pkg))
		if not res[0]:
			version.append(res[1])

	res = StupidPythonExec('"%s" --libs%s' % (pkgconf, pkg))
	ret = not res[0]
	if ret:
		for arg in res[1].split():
			if arg[0:2] == '-l':
				libs.append(arg[2:])
			else:
				linkflags.append(arg)

		res = StupidPythonExec('"%s" --cflags%s' % (pkgconf, pkg))
		ret = not res[0]
		if ret:
			ccflags.append(res[1])

	context.Result(ret)
	return ret

""" Check whether threads can be used with given flags and libs. """
def CheckThreads(context, variant):
	context.Message('Trying to get threading with %s... ' % variant)
	testprog = '''#include <pthread.h>
int main(void) {
	pthread_t th;
	pthread_join(th, 0);
	pthread_attr_init(0);
	pthread_cleanup_push(0, 0);
	pthread_create(0,0,0,0);
	pthread_cleanup_pop(0);
}'''
	ret = context.TryLink(testprog, '.c')
	context.Result(ret)
	return not not ret


""" Execute external test from scons.d/. """
def ExtTest(name, addexports = []):
	exports = ['conf', 'defines', 'env']
	exports.extend(addexports)
	ret = SConscript('scons.d/%s' % (name), exports)
	return ret

""" Fill targets to match source recoding. """
def RecodeDocsEmitter(target, source, env):
	src		= []
	target	= []
	for f in source:
		if str(f)[-4:] != '.txt':
			continue
		s = str(f)[:-4]
		if s[-4:] == '-utf':
			continue
		src.append(str(f))
		target.append(s + '-utf.txt')
	
	return target, src

langmap = {
	'en':	'iso-8859-1',
	'pl':	'iso-8859-2'
	}

""" Recode documentation file from correct encoding to UTF-8. """
def RecodeDocs(target, source, env):
	map = dict(zip(target, source))
	for d,s in map.items():
		lang = str(s)[str(s)[:-4].rindex('-') + 1:-4]
		if lang in langmap:
			enc = langmap[lang]
		else:
			continue

		sf = codecs.open(str(s), 'r', enc)
		df = codecs.open(str(d), 'w', 'utf-8')

		data = sf.read()
		df.write(data)

		sf.close()
		df.close()
	return None

opts = Options('options.cache')

avplugins = [elem.split('/')[1] for elem in glob.glob('plugins/*/')]
avplugins.extend(plugin_states)
opts.Add(ListOption('PLUGINS', 'List of plugins to build', 'unstable', avplugins))
opts.Add(BoolOption('UNICODE', 'Whether to build unicode version of ekg2', True))
opts.Add(BoolOption('NOCONF', 'Whether to skip configure', False))

for var,path in dirs.items():
	opts.Add(PathOption(var, '', path, PathOption.PathAccept))
for var,desc in envs.items():
	opts.Add(var, desc, '')

recoder = Builder(action = RecodeDocs, emitter = RecodeDocsEmitter, suffix = '-utf.txt', src_suffix = '.txt')

env = Environment(BUILDERS = {'RecodeDocs': recoder})
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

conf = env.Configure(custom_tests = {'CheckStructMember': CheckStructMember, 'PkgConfig': PkgConfig, 'CheckThreads': CheckThreads})
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

pllist = list(plugins.keys())
pllist.sort()
for plugin in pllist:
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
		print '[%s] Disabling due to build system incompatibility.' % (plugin)
		continue

	optdeps = []
	libs = []
	ccflags = []
	linkflags = []
	for dep in info['depends']:
		if not isinstance(dep, list):
			dep = [dep]
		for xdep in dep: # exclusive depends
			have_it = ExtTest(xdep, ['libs', 'ccflags', 'linkflags', 'plugin'])
			if have_it:
				if len(dep) > 1:
					optdeps.append('%s' % (xdep)) # well, it's not so optional, but pretty print it
				break

		if not have_it:
			print '[%s] Dependency not satisfied: %s' % (plugin, dep)
			info['fail'] = True
	if 'fail' in info:
		del plugins[plugin]
		continue

	for dep in info['optdepends']:
		if not isinstance(dep, list):
			dep = [dep]
		for xdep in dep: # exclusive depends
			have_it = ExtTest(xdep, ['libs', 'ccflags', 'linkflags', 'plugin'])
			if have_it:
				optdeps.append('%s' % (xdep))
				break

		if not have_it:
			print '[%s] Optional dependency not satisfied: %s' % (plugin, dep)
			optdeps.append('-%s' % (' -'.join(dep)))

	if not ccflags:
		ccflags = ['']
	if not linkflags:
		linkflags = ['']
	plugins[plugin] = {
		'libs':			libs,
		'ccflags':		' '.join(ccflags),
		'linkflags':	' '.join(linkflags)
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
env.Append(CCFLAGS = ' -I.')

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

for k in dirs.keys():
	if k != 'DESTDIR':
		env[k] = '%s%s' % (env['DESTDIR'], env[k])

docglobs = ['commands*.txt', 'vars*.txt', 'session*.txt']

env.Alias('install', '%s/' % env['DESTDIR'])
cenv = env.Clone()
cenv.Append(LIBS = ekg_libs)
cenv.Append(LIBPATH = 'compat')
cenv.Append(LINKFLAGS = '-Wl,--export-dynamic')
cenv.Program('ekg/ekg2', glob.glob('ekg/*.c'))

docfiles = []
for doc in docglobs:
	docfiles.extend(glob.glob('docs/%s' % doc))
cenv.RecodeDocs('docs/', docfiles)
docfiles = []					# we must glob twice to include *utf*
for doc in docglobs:
	docfiles.extend(glob.glob('docs/%s' % doc))

cenv.Install(env['BINDIR'], 'ekg/ekg2')
#cenv.Install(env['INCLUDEDIR'], glob.glob('ekg/*.h', 'ekg2-config.h', 'gettext.h'))
cenv.Install(env['DATADIR'], docfiles)
cenv.Install('%s/themes' % env['DATADIR'], glob.glob('contrib/themes/*.theme'))

for plugin, data in plugins.items():
	plugpath = 'plugins/%s' % (plugin)
	Mkdir('%s/.libs' % (plugin))

	penv = env.Clone()
	penv.Append(LIBS = data['libs'])
	penv.Append(CCFLAGS = ' ' + data['ccflags'])
	penv.Append(LINKFLAGS = ' ' + data['linkflags'])

	libfile = '%s/.libs/%s' % (plugpath, plugin)
	penv.SharedLibrary(libfile, glob.glob('%s/*.c' % (plugpath)), LIBPREFIX = '')

	docfiles = []
	for doc in docglobs:
		docfiles.extend(glob.glob('%s/%s' % (plugpath, doc)))
	penv.RecodeDocs(plugpath, docfiles)
	docfiles = []					# we must glob twice to include *utf*
	for doc in docglobs:
		docfiles.extend(glob.glob('%s/%s' % (plugpath, doc)))

	penv.Install(env['PLUGINDIR'], libfile + env['SHLIBSUFFIX']) 
	penv.Install('%s/plugins/%s' % (env['DATADIR'], plugin), docfiles)

# vim:ts=4:sts=4:sw=4:syntax=python
