#!/usr/bin/scons
# vim:set fileencoding=utf-8
#  Alternate build system for EKG2, unstable and unfinished yet
#  (C) 2008 Michał Górny

EnsureSConsVersion(0,97)
EnsurePythonVersion(2,4)

consts = {
	'VERSION':		'SVN',
	'SCONS':		True
	}
indirs = [ # pseudo-hash, 'coz we want to keep order
	['DESTDIR',		'',							'Virtual installation root'],
	['PREFIX',		'/usr/local',				'Prefix for arch-independent data'],
	['EPREFIX',		'$PREFIX',					'Prefix for arch-dependent data'],

	['BINDIR',		'$EPREFIX/bin',				'User executables'],
	['LIBEXECDIR',	'$EPREFIX/libexec',			'Program executables'],
	['LIBDIR',		'$EPREFIX/lib',				'Libraries'],
#	['INCLUDEDIR',	'$PREFIX/include',			'Headers'],
	['DATAROOTDIR',	'$PREFIX/share',			'Arch-independent data'],
	['SYSCONFDIR',	'/etc',						'System-wide configuration'],

	['LOCALEDIR',	'$DATAROOTDIR/locale',		'Locales'],
	['DATADIR',		'$DATAROOTDIR/ekg2',		'EKG2 data'],
	['MANDIR',		'$DATAROOTDIR/man',			'EKG2 manfiles'],
	['DOCDIR',		'$DATAROOTDIR/doc/ekg2',	'EKG2 additional documentation'],
	['PLUGINDIR',	'$LIBDIR/ekg2/plugins',		'EKG2 plugins'],
	['IOCTLD_PATH',	'$LIBEXECDIR/ekg2',			'EKG2 ioctld']
	]
envs = {
	'CCFLAGS':		['CFLAGS', 'Compiler flags'],
	'LINKFLAGS':	['LDFLAGS', 'LIBS', 'Linker flags']
	}

	# first and last one are special keywords
plugin_states = ['all', 'nocompile', 'deprecated', 'unknown', 'experimental', 'def', 'unstable', 'stable', 'none']
plugin_symbols = ['', '!', '!', '?', '*', '', '~', '', '']

import glob, subprocess, codecs, os, os.path, sys, SCons

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

def WriteDefines(target, source, env):
	""" Write list 'defines' to definefile. """
	defines = env['EKG_DEFINES']
	definefile = open(str(target[0]), 'w')
	for k, v in defines.items():
		writedef(definefile, k, v)
	definefile.close()
	return None

def WriteStatic(target, source, env):
	""" Write static plugin loader """
	static_inc = open('ekg2-static.c', 'w')
	for p in plugins:
		static_inc.write('extern int %s_plugin_init(int prio);\n' % p)
	static_inc.write('\nvoid *plugin_load_static(const char *name) {\n')
	for p in plugins:
		static_inc.write('\tif (!xstrcmp(name, "%s")) return &%s_plugin_init;\n' % (p, p))
	static_inc.write('}\n')
	static_inc.close()
	return None

warnings = []

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

def PkgConfig(context, pkg, flags, version = None, pkgconf = 'pkg-config',
			versionquery = '--version', flagquery = '--cflags --libs'):
	""" Ask pkg-config (or *-config) about given package, and put results in flags.
		If specific -config (pkg-config, for example) needs package name, then give it as pkg. If not,
		just set pkgconf correctly. When using something incompatible with *-config, give correct
		[versionquery, ccflagquery, ldflagquery] """
	if pkg is None:
		context.Message('Querying %s... ' % pkgconf)
		pkg = ''
	else:
		context.Message('Asking %s about %s... ' % (pkgconf, pkg))
		pkg = ' "%s"' % (pkg)

	queries = []
	if flags is not None:
		queries.append(flagquery)
	if version is not None:
		if pkgconf == 'pkg-config' and versionquery == '--version': # defaults
			versionquery = '--mod%s' % versionquery[2:]
		queries.append(versionquery)

	res = []
	ret = StupidPythonExec(None, ['"%s" %s%s' % (pkgconf, q, pkg) for q in queries], res)
	if ret:
		if flags is not None:
			flags.append(res[1])
			if version is not None:
				version.append(res[4])
		else:
			if version is not None:
				version.append(res[1])

	context.Result(ret)
	if int(res[0]) == 127 and pkgconf == 'pkg-config':
		warnings.append('%s not found!' % pkgconf)
	if int(res[0]) != 0:
		print res[2]
	return ret

def SetFlags(env, new):
	""" Set new (temporary) flags and return dict with old flag values """
	if not isinstance(new, dict):
		new	= env.ParseFlags(new)

	old		= {}
	for k in new.keys():
		try:
			old[k] = env[k]
		except KeyError:
			old[k] = []

	env.MergeFlags(new)
	return old

def RestoreFlags(env, old):
	""" Restore flags saved by BackupFlags() """
		# We don't use env.MergeFlags(), 'cause we want to remove new ones
	for k, v in old.items():
		env[k] = v
	return

def ExtTest(name, addexports = []):
	""" Execute external test from scons.d/. """
	exports = ['conf', 'defines', 'env', 'warnings', 'SetFlags', 'RestoreFlags']
	exports.extend(addexports)
	ret = SConscript('scons.d/%s' % (name), exports)
	return ret


def RecodeHelpEmitter(target, source, env):
	""" Fill targets to match sources. """
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

def RecodeHelp(target, source, env):
	""" Recode help file from correct encoding to UTF-8. """
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

recoder = Builder(action = RecodeHelp, emitter = RecodeHelpEmitter, suffix = '-utf.txt', src_suffix = '.txt')
msgfmt = Builder(generator = CompileMsgGen, emitter = CompileMsgEmitter, suffix = '.mo', src_suffix = '.po')
defgen = Builder(action = WriteDefines, suffix = '.h')
stagen = Builder(action = WriteStatic, suffix = '.c')

env = Environment(
	BUILDERS = {'RecodeHelp': recoder, 'CompileMsg': msgfmt, 'Defines': defgen, 'StaticLoader': stagen}
)
opts = Options('options.cache')

avplugins = [elem.split('/')[1] for elem in glob.glob('plugins/*/')]
xplugins = ['-%s' % elem for elem in avplugins]
xplugins.extend(['@%s' % elem for elem in plugin_states])
opts.Add(ListOption('PLUGINS', 'List of plugins or @sets to build', '@def', avplugins + xplugins))
opts.Add(BoolOption('HARDDEPS', 'Fail if specified plugin could not be built due to unfulfilled depends', False))
opts.Add(BoolOption('UNICODE', 'Enable unicode support', True))
opts.Add(BoolOption('RESOLV', 'Use libresolv-based domain resolver with SRV support', True))
opts.Add(BoolOption('IDN', 'Support Internation Domain Names if libidn is found', True))
opts.Add(BoolOption('NLS', 'Enable l10n in core (requires gettext)', True))
opts.Add(BoolOption('STATIC', 'Whether to build static plugins instead of shared', 0))
opts.Add(EnumOption('DEBUG', 'Internal debug level', 'std', ['none', 'std', 'stderr']))
opts.Add('DISTNOTES', 'Additional info to /version for use with distributed packages')

for p in avplugins:
	info = SConscript('plugins/%s/SConscript' % p, ['env', 'opts'])
	if not info:
		continue
	values = []
	if 'depends' in info:
		for a in info['depends']:
			if isinstance(a, list):
				values.extend(a)
	if 'optdepends' in info:
		for a in info['optdepends']:
			if isinstance(a, list):
				values.extend(a)
			else:
				values.append(a)

	if values:
		xvals = ['-%s' % elem for elem in values]
		xvals.extend(['+%s' % elem for elem in values])
		opts.Add(ListOption('%s_DEPS' % p.upper(), 'Optional/exclusive dependencies of %s plugin to disable or force' % p,
			'none', values + xvals))

dirs = []
for k,v,d in indirs:
	dirs.append(k)
	opts.Add(PathOption(k, d, v, PathOption.PathAccept))

for k,v in envs.items():
	desc = v.pop()
	var = []
	for val in v:
		if val in os.environ:
			var.append(os.environ[val])
	var = ' '.join(var)
	opts.Add(k, desc, var)

opts.Update(env)
opts.Save('options.cache', env)
env.Help(opts.GenerateHelpText(env))

defines = {}

if env['STATIC']:
	defines['STATIC_LIBS'] = True
else:
	defines['SHARED_LIBS'] = True
defines['USE_UNICODE'] = env['UNICODE']
try:
	defines['VER_DISTNOTES'] = env['DISTNOTES']
except KeyError:
	defines['VER_DISTNOTES'] = False
defines['SCONS_NOTES'] = 'built using SCons %s' % SCons.__version__

for var in dirs:
	env[var] = env.subst(env[var])
	defines[var] = env[var]
for var,val in consts.items():
	defines[var] = val

# We get CCFLAGS and some of LINKFLAGS into flags, to pass them to env.MergeFlags(),
# but we keep rest of LINKFLAGS alone, because Gentoo by default uses -O1 to linker, and -O2 to compiler.
flags = [env['CCFLAGS']]
linkflags = []
for arg in env['LINKFLAGS'].split():
	if arg[0:2] in ['-l', '-L']:
		flags.append(arg)
	else:
		linkflags.append(arg)

env['CCFLAGS']		= []
env['LINKFLAGS']	= []
env.MergeFlags(flags)
env.Append(LINKFLAGS = linkflags)

conf = env.Configure(custom_tests = {
		'CheckStructMember':	CheckStructMember,
		'PkgConfig':			PkgConfig,
		'External':				StupidPythonExec
	})
ekg_libs = []
ekg_staticlibs = []

ExtTest('standard', ['ekg_libs'])
ExtTest('compat', ['ekg_staticlibs'])

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
	xlibs = []
	xflags = []

	for dep in info['depends'] + info['optdepends']:
		isopt = dep in info['optdepends']

		if not isinstance(dep, list):
			dep = [dep]
		try:
			prefs = env['%s_DEPS' % plugin.upper()]
		except KeyError:
			prefs = []

		for a in prefs:
			if a[0] == '-':
				pass
			elif a[0] == '+':
				if '-%s' % a[1:] in prefs:
					raise ValueError('More than one variant of depend %s specified (%s and -%s)' % (a[1:],a,a[1:]))
			else:
				if '-%s' % a in prefs or '+%s' % a in prefs:
					raise ValueError('More than one variant of depend %s specified (%s, +%s and/or -%s)' % (a,a,a,a))

		forced = False
		have_it = True # if empty set is given, don't complain
		sdep = []
		for xdep in dep:
			if '+%s' % xdep in prefs: # if user forces dep, then forget about the rest
				for a in dep:
					if a != xdep and '+%s' % a in prefs:
						raise ValueError('More than one exclusive dependency forced (%s and %s)' % (xdep, a))
				forced = True
				sdep = [xdep]
				break
			elif xdep in prefs: # if he prefers it, place it before others
				sdep.append(xdep)
		if not forced:
			for xdep in dep:
				if not xdep in sdep and not '-%s' % xdep in prefs: # and rest of possibilities
					sdep.append(xdep)

		for xdep in sdep:	# 'flags' will be nicely split by SCons, but 'libs' are nicer to write
			libs = []
			flags = []

			have_it = ExtTest(xdep, ['libs', 'flags'])
			if have_it:
				if isopt or len(dep) > 1: # pretty-print optional and selected required (if more than one possibility)
					optdeps.append('%s' % (xdep))

				xflags.extend(flags)
				xlibs.extend(libs)
				break

		if not have_it:
			if isopt:
				if forced:
					print '[%s] User-forced optional dependency not satisfied: %s' % (plugin, sdep)
					if env['HARDDEPS']:
						print 'HARDDEPS specified, aborting.'
						sys.exit(1)
					info['fail'] = True
					break
				else:
					print '[%s] Optional dependency not satisfied: %s' % (plugin, sdep)
				optdeps.append('-%s' % (' -'.join(dep)))
			else:
				print '[%s] Dependency not satisfied: %s' % (plugin, sdep)
				if env['HARDDEPS']:
					print 'HARDDEPS specified, aborting.'
					sys.exit(1)
				info['fail'] = True
				break

	if 'fail' in info:
		warnings.append("Plugin '%s' disabled due to unsatisfied dependency: %s" % (plugin, sdep))
		del plugins[plugin]
		continue

	SConscript('%s/SConscript' % (plugpath), ['defines', 'optdeps'])
	plugins[plugin] = {
		'info':			info,
		'libs':			xlibs,
		'flags':		xflags
		}

	type = info['type']
	if not pl.has_key(type):
		pl[type] = []

	if optdeps:
		optdeps = ' [%s]' % (' '.join(optdeps))
		defines['SCONS_NOTES'] += '; %s%s' % (plugin, optdeps)
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

	if not 'ui' in pl:
		warnings.append('No UI-plugin selected. EKG2 might be unusable to you.')
	if not 'protocol' in pl:
		warnings.append("No protocol plugin selected. EKG2 won't be instant messenger anymore.")
else:
	warnings.append('You are compiling ekg2 without any plugin. Are you sure this is what you mean to do?')

print 'Options:'
print '- unicode: %s' % (env['UNICODE'])
print '- nls: %s' % (defines['ENABLE_NLS'])
print
print 'Paths:'
for k in dirs:
	print '- %s: %s' % (k, env[k])
print
print 'User-specified build environment:'
for k in ['CC', 'CCFLAGS', 'CPPPATH', 'LIBS', 'LINK', 'LINKFLAGS', 'LIBPATH']:
	try:
		print '- %s: %s' % (k, env[k])
	except KeyError:
		pass
print

if warnings:
	print 'Warnings:'
	for w in {}.fromkeys(warnings).keys():
		print '- %s' % w.replace('\n', '\n  ')
	print

conf.Finish()

env['EKG_DEFINES'] = defines
env.Defines('ekg2-config.h', [])
env.AlwaysBuild('ekg2-config.h', 'ekg2-static.c')
env.Append(CPPPATH = ['.'])

for k in dirs:
	if k != 'DESTDIR':
		env[k] = '%s%s' % (env['DESTDIR'], env[k])

docglobs = ['commands*', 'vars*', 'session*']

env.Alias('install', [env['PREFIX'], env['EPREFIX']])

cenv = env.Clone()

docfiles = []
for doc in docglobs:
	docfiles.extend(glob.glob('docs/%s.txt' % doc))
if env['UNICODE']:
	cenv.RecodeHelp('docs/', docfiles)
	docfiles = []
	for doc in docglobs:
		docfiles.extend(glob.glob('docs/%s.txt' % doc))

if defines['ENABLE_NLS']:
	cenv.CompileMsg('po/', glob.glob('po/*.po'))
	for f in glob.glob('po/*.mo'):
		lang = str(f)[str(f).rindex('/') + 1:-3]
		cenv.InstallAs(target = '%s/%s/LC_MESSAGES/ekg2.mo' % (env['LOCALEDIR'], lang), source = f)

cenv.Install(env['BINDIR'], 'ekg/%sekg2%s' % (env['PROGPREFIX'], env['PROGSUFFIX']))
#cenv.Install(env['INCLUDEDIR'], glob.glob('ekg/*.h', 'ekg2-config.h', 'gettext.h'))
cenv.Install(env['DATADIR'], docfiles)
cenv.Install('%s/themes' % env['DATADIR'], glob.glob('contrib/themes/*.theme'))

adddocs = [elem for elem in glob.glob('docs/*.txt') if not elem in docfiles]
for a in glob.glob('*') + glob.glob('docs/*'):
	i = a.find('.')
	if i != -1:
		b = a[:i]
	else:
		b = a
	i = b.find('/')
	if b != -1:
		b = b[i + 1:]

	if b.upper() == b: # all uppercase
		if a != 'docs/README': # what ekg1 readme does there? O O
			adddocs.append(a)
	else:
		i = b.rfind('.')
		if b[i:] == '.txt' and not b in docfiles:
			adddocs.append(a)
cenv.Install(env['DOCDIR'], adddocs)

for f in glob.glob('docs/*.[12345678]'):
	i = f.rindex('.')
	cat = f[i+1:]
	j = f.rindex('.', 0, i)
	lng = f[j+1:i]
	i = f.rindex('/')
	fn = f[i + 1:].replace('%s.' % lng, '')

	if lng == 'en':
		lng = ''
	else:
		lng = '%s/' % lng

	fn = '%s/%sman%s/%s' % (env['MANDIR'], lng, cat, fn)
	cenv.InstallAs(target = fn, source = f)

ekg_libpath = []

for plugin, data in plugins.items():
	plugpath = 'plugins/%s' % (plugin)

	penv = env.Clone()
	penv.Append(LIBS = data['libs'])
	penv.MergeFlags(data['flags'])

	SConscript('%s/SConscript' % plugpath, ['penv'])

	libfile = '%s/%s' % (plugpath, plugin)
	if env['STATIC']:
		penv.StaticLibrary(libfile, glob.glob('%s/*.c' % (plugpath)), LIBPREFIX = '')
		ekg_libs.extend(penv['LIBS'])
		try:
			ekg_libpath.extend(penv['LIBPATH'])
		except KeyError:
			pass
		ekg_staticlibs.append('%s/%s%s' % (plugpath, plugin, env['LIBSUFFIX']))
	else:
		penv.SharedLibrary(libfile, glob.glob('%s/*.c' % (plugpath)), LIBPREFIX = '')
		penv.Install(env['PLUGINDIR'], libfile + env['SHLIBSUFFIX']) 

	docfiles = []
	for doc in docglobs:
		docfiles.extend(glob.glob('%s/%s.txt' % (plugpath, doc)))
	if env['UNICODE']:
		penv.RecodeHelp(plugpath, docfiles)
		docfiles = []					# we must glob twice to include *utf*
		for doc in docglobs:
			docfiles.extend(glob.glob('%s/%s.txt' % (plugpath, doc)))
	
	for f in data['info']['extradist']:
		docfiles.extend(glob.glob('%s/%s' % (plugpath, f)))

	penv.Install('%s/plugins/%s' % (env['DATADIR'], plugin), docfiles)

if env['STATIC']:
	env.StaticLoader('ekg2-static.c', [])
	ekg_staticlibs.insert(0, 'ekg2-static.c') # well, it ain't exactly static lib, but no need to panic

cenv.Append(LIBS = ekg_libs)
cenv.Append(LIBPATH = ekg_libpath)
cenv.Append(LINKFLAGS = ['-Wl,--export-dynamic'])
cenv.Program('ekg/ekg2', glob.glob('ekg/*.c') + ekg_staticlibs)

# vim:ts=4:sts=4:sw=4:syntax=python
