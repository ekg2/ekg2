#!/usr/bin/scons
# vim:set fileencoding=utf-8
#  Alternate build system for EKG2, unstable and unfinished yet
#  (C) 2008 Michał Górny

consts = {
	'VERSION':		'SVN',
	'SHARED_LIBS':	True,
	'SCONS':		True
	}
indirs = [ # pseudo-hash, 'coz we want to keep order
	['DESTDIR',		'',							'Virtual installation root'],
	['PREFIX',		'/usr/local',				'Prefix for arch-independent data'],
	['EPREFIX',		'$PREFIX',				'Prefix for arch-dependent data'],

	['BINDIR',		'$EPREFIX/bin',				'User executables'],
	['LIBEXECDIR',	'$EPREFIX/libexec',			'Program executables'],
	['LIBDIR',		'$EPREFIX/lib',				'Libraries'],
#	['INCLUDEDIR',	'$PREFIX/include',			'Headers'],
	['DATAROOTDIR',	'$PREFIX/share',			'Arch-independent data'],
	['SYSCONFDIR',	'/etc',						'System-wide configuration'],

	['LOCALEDIR',	'$DATAROOTDIR/locale',		'Locales'],
	['DATADIR',		'$DATAROOTDIR/ekg2',		'EKG2 data'],
	['PLUGINDIR',	'$LIBDIR/ekg2/plugins',		'EKG2 plugins'],
	['IOCTLD_PATH',	'$LIBEXECDIR/ekg2',			'EKG2 ioctld']
	]
mapped = {
	'UNICODE':		['USE_UNICODE', 'Enable unicode support'],
	'NLS':			['ENABLE_NLS', 'Enable l10n in core'],
	}
envs = {
	'CCFLAGS':		['CFLAGS', 'Compiler flags'],
	'LINKFLAGS':	['LDFLAGS', 'LIBS', 'Linker flags']
	}

	# first and last one are special keywords
plugin_states = ['all', 'nocompile', 'deprecated', 'unknown', 'experimental', 'def', 'unstable', 'stable', 'none']
plugin_symbols = ['', '!', '!', '?', '*', '', '~', '', '']

import glob, subprocess, codecs, os, os.path, sys

def writedef(definefile, var, val):
	""" Write define to definefile, choosing correct format. """
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
	""" Write list 'defines' to definefile. """
	definefile = open('ekg2-config.h', 'w')
	for k, v in defines.items():
		writedef(definefile, k, v)
	definefile.close()


def CheckStructMember(context, struct, member, headers):
	""" Check whether struct with given member is available. """
	context.Message('Checking for %s.%s... ' % (struct, member))
	testprog = ''
	for header in headers:
		testprog += '#include <%s>\n' % (header)
	testprog += '\nint main(void) {\n\tstatic %s tmp;\n\tif (tmp.%s)\n\t\treturn 0;\n\treturn 0;\n}\n' % (struct, member)

	result = context.TryCompile(testprog, '.c')
	context.Result(result)
	return not not result

def StupidPythonExec(context, cmds, result):
	""" Execute given commands and put errorcode, data written to stdout and to stderr onto result. """
	if not isinstance(cmds, list):
		cmds = [cmds]

	if context is not None:
		context.Message('Querying %s... ' % cmds[0].split()[0])

	fret = 0
	for cmd in cmds:
		p		= subprocess.Popen(cmd, shell = True, stdout = subprocess.PIPE, stderr = subprocess.PIPE, stdin = subprocess.PIPE)
		stdout	= p.stdout.read().strip()
		stderr	= p.stderr.read().strip()
		ret		= p.wait()

		result.extend([ret, stdout, stderr])
		fret	+= int(ret)

	if context is not None:
		context.Result(not fret)

	return not fret

def PkgConfig(context, pkg, libs, ccflags, linkflags, version = None, pkgconf = 'pkg-config',
			versionquery = '--version', ccflagquery = '--cflags', ldflagquery = '--libs'):
	""" Ask pkg-config (or *-config) about given package, and put results in [libs, ccflags, linkflags].
		If specific -config (pkg-config, for example) needs package name, then give it as pkg. If not,
		just set pkgconf correctly. When using something incompatible with *-config, give correct
		[versionquery, ccflagquery, ldflagquery] """
	if pkg is None:
		context.Message('Querying %s... ' % pkgconf)
		pkg = ''
	else:
		context.Message('Asking %s about %s... ' % (pkgconf, pkg))
		pkg = ' "%s"' % (pkg)

	queries = [ldflagquery, ccflagquery]
	if version is not None:
		if pkgconf == 'pkg-config' and versionquery == '--version': # defaults
			versionquery = '--mod%s' % versionquery[2:]
		queries.append(versionquery)

	res = []
	ret = StupidPythonExec(None, ['"%s" %s%s' % (pkgconf, q, pkg) for q in queries], res)
	if ret:
		for arg in res[1].split():
			if arg[0:2] == '-l':
				libs.append(arg[2:])
			else:
				linkflags.append(arg)

		ccflags.append(res[4])
		if version is not None:
			version.append(res[7])

	context.Result(ret)
	return ret

def CheckThreads(context, variant):
	""" Check whether threads can be used with given flags and libs. """
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

def ExtTest(name, addexports = []):
	""" Execute external test from scons.d/. """
	exports = ['conf', 'defines', 'env']
	exports.extend(addexports)
	ret = SConscript('scons.d/%s' % (name), exports)
	return ret


def RecodeDocsEmitter(target, source, env):
	""" Fill targets to match source recoding. """
	src		= []
	target	= []
	for f in source:
#		if str(f)[-4:] != '.txt':
#			continue
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

def RecodeDocs(target, source, env):
	""" Recode documentation file from correct encoding to UTF-8. """
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

def CompileMsgEmitter(target, source, env):
	""" Fill targets to match source .po. """
	target	= []
	for f in source:
#		if str(f)[-3:] == '.po':
		target.append(str(f).replace('.po', '.mo'))
	
	return target, source

def CompileMsgGen(source, target, env, for_signature):
	""" Compile .po to .mo. """
	map = dict(zip(target, source))
	ret = []
	for d,s in map.items():
		ret.append('msgfmt -o "%s" "%s"' % (d,s))
	return ret


opts = Options('options.cache')

avplugins = [elem.split('/')[1] for elem in glob.glob('plugins/*/')]
xplugins = ['-%s' % elem for elem in avplugins]
xplugins.extend(['@%s' % elem for elem in plugin_states])
opts.Add(ListOption('PLUGINS', 'List of plugins or @sets to build', '@def', avplugins + xplugins))
opts.Add(BoolOption('HARDDEPS', 'Fail if specified plugin could not be built due to unfulfilled depends', False))
for k,v in mapped.items():
	opts.Add(BoolOption(k, v[1], True))

dirs = []
for k,v,d in indirs:
	dirs.append(k)
	try:
		opts.Add(PathOption(k, d, v, PathOption.PathAccept))
	except AttributeError:
		print 'Ancient SCons version detected, build may fail, please upgrade!'
		opts.Add(PathOption(k, d, v))

for k,v in envs.items():
	desc = v.pop()
	var = []
	for val in v:
		if val in os.environ:
			var.append(os.environ[val])
	var = ' '.join(var)
	opts.Add(k, desc, var)

recoder = Builder(action = RecodeDocs, emitter = RecodeDocsEmitter, suffix = '-utf.txt', src_suffix = '.txt')
msgfmt = Builder(generator = CompileMsgGen, emitter = CompileMsgEmitter, suffix = '.mo', src_suffix = '.po')

env = Environment(BUILDERS = {'RecodeDocs': recoder, 'CompileMsg': msgfmt})
opts.Update(env)
opts.Save('options.cache', env)
env.Help(opts.GenerateHelpText(env))

defines = {}

for var in dirs:
	env[var] = env.subst(env[var])
	defines[var] = env[var]
for var,val in consts.items():
	defines[var] = val
for var,val in mapped.items():
	defines[val[0]] = env[var]

linkflags = []
for arg in env['LINKFLAGS'].split():
	if arg[0:2] == '-l':
		env.Append(LIBS = [arg[2:]])
	else:
		linkflags.append(arg)
env['LINKFLAGS'] = ' '.join(linkflags)

conf = env.Configure(custom_tests = {
		'CheckStructMember':	CheckStructMember,
		'PkgConfig':			PkgConfig,
		'CheckThreads':			CheckThreads,
		'External':				StupidPythonExec
	})
ekg_libs = []

ExtTest('standard', ['ekg_libs'])
ExtTest('compat', ['ekg_libs'])

plugin_def = {
	'type':			'misc',
	'state':		'unknown',
	'depends':		[],
	'optdepends':	[],
	'extradist':	[]
	}

specplugins = env['PLUGINS']

plugins_state = plugin_states.index('none')

for st in reversed(plugin_states):
	while '@%s' % st in specplugins:
		specplugins.remove('@%s' % st)
		plugins_state = plugin_states.index(st)

avplugins = {}.fromkeys(avplugins)

pl = {}

plugins = avplugins
pllist = list(plugins.keys())
pllist.sort()
for plugin in pllist:
	plugpath = 'plugins/%s' % (plugin)
	info = SConscript('%s/SConscript' % (plugpath), ['env'])
	if not info:
		info = plugin_def
	else:
		for k in plugin_def.keys():
			if not k in info:
				info[k] = plugin_def[k]

	if plugin_states.index(info['state']) < plugins_state:
		if not plugin in specplugins:
			del plugins[plugin]
			continue
	elif '-%s' % plugin in specplugins and not plugin in specplugins:
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
			if env['HARDDEPS']:
				print 'HARDDEPS specified, aborting.'
				sys.exit(1)
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
		'info':			info,
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

env.Append(CCFLAGS = ' -I.')
# some fancy output

print
if pl:
	print 'Enabled plugins:'
	for type, plugs in pl.items():
		print '- %s: %s' % (type, ', '.join(plugs))
	print

print 'Options:'
print '- unicode: %s' % (env['UNICODE'])
print '- nls: %s' % (env['NLS'])
print
print 'Paths:'
for k in dirs:
	print '- %s: %s' % (k, env[k])
print
print 'Build environment:'
for k in ['CC', 'CCFLAGS', 'LIBS', 'LINK', 'LINKFLAGS']:
	print '- %s: %s' % (k, env[k])

conf.Finish()

writedefines()

for k in dirs:
	if k != 'DESTDIR':
		env[k] = '%s%s' % (env['DESTDIR'], env[k])

docglobs = ['commands*', 'vars*', 'session*']

env.Alias('install', '%s/' % env['DESTDIR'])
cenv = env.Clone()
cenv.Append(LIBS = ekg_libs)
cenv.Append(LIBPATH = 'compat')
cenv.Append(LINKFLAGS = '-Wl,--export-dynamic')
cenv.Program('ekg/ekg2', glob.glob('ekg/*.c'))

docfiles = []
for doc in docglobs:
	docfiles.extend(glob.glob('docs/%s.txt' % doc))
if env['UNICODE']:
	cenv.RecodeDocs('docs/', docfiles)
	docfiles = []
	for doc in docglobs:
		docfiles.extend(glob.glob('docs/%s.txt' % doc))

if env['NLS']:
	cenv.CompileMsg('po/', glob.glob('po/*.po'))
	for f in glob.glob('po/*.mo'):
		lang = str(f)[str(f).rindex('/') + 1:-3]
		cenv.InstallAs(target = '%s/%s/LC_MESSAGES/ekg2.mo' % (env['LOCALEDIR'], lang), source = str(f))

cenv.Install(env['BINDIR'], 'ekg/ekg2')
#cenv.Install(env['INCLUDEDIR'], glob.glob('ekg/*.h', 'ekg2-config.h', 'gettext.h'))
cenv.Install(env['DATADIR'], docfiles)
cenv.Install('%s/themes' % env['DATADIR'], glob.glob('contrib/themes/*.theme'))

for plugin, data in plugins.items():
	plugpath = 'plugins/%s' % (plugin)

	penv = env.Clone()
	penv.Append(LIBS = data['libs'])
	penv.Append(CCFLAGS = ' ' + data['ccflags'])
	penv.Append(LINKFLAGS = ' ' + data['linkflags'])

	sconshelper = '%s/SConscript' % plugpath
	if os.path.exists(sconshelper):
		SConscript([sconshelper], ['penv'])

	libfile = '%s/%s' % (plugpath, plugin)
	penv.SharedLibrary(libfile, glob.glob('%s/*.c' % (plugpath)), LIBPREFIX = '')

	docfiles = []
	for doc in docglobs:
		docfiles.extend(glob.glob('%s/%s.txt' % (plugpath, doc)))
	if env['UNICODE']:
		penv.RecodeDocs(plugpath, docfiles)
		docfiles = []					# we must glob twice to include *utf*
		for doc in docglobs:
			docfiles.extend(glob.glob('%s/%s.txt' % (plugpath, doc)))
	
	for f in data['info']['extradist']:
		docfiles.extend(glob.glob('%s/%s' % (plugpath, f)))

	penv.Install(env['PLUGINDIR'], libfile + env['SHLIBSUFFIX']) 
	penv.Install('%s/plugins/%s' % (env['DATADIR'], plugin), docfiles)

# vim:ts=4:sts=4:sw=4:syntax=python
