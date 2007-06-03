opts=Options()

opts.Add(BoolOption('MYMALLOC', 'Use mymalloc for memory debug', 0))

env = Environment(options = opts)
Help(opts.GenerateHelpText(env))

# Extra places we need to look for includes
env.Append(CPPPATH = ['/opt/local/include', '/usr/local/include'])
env.Append(LIBPATH = ['/opt/local/lib', '/usr/local/lib'])

if ARGUMENTS.get('MYMALLOC', 0):
	env.Append(CCFLAGS = '-DUSE_MYMALLOC -DMYMALLOC')

if ARGUMENTS.get('USE_ASSERT', 0):
	env.Append(CCFLAGS = '-DUSE_ASSERT=1')

env.Append(CCFLAGS = '-g -O2 -Wall -Werror')
env.Append(LINKFLAGS = '-g')

env.conf = Configure(env)

if env.conf.CheckCHeader('alloca.h'):
	env.Append(CCFLAGS = '-DHAVE_ALLOCA_H')
if not env.conf.CheckLibWithHeader('pcre', 'pcre.h', 'c'):
	print 'PCRE is required.'
	Exit(1)

env = env.conf.Finish()
env.Program('logmerge', ['logmerge.cpp', 'logfiles.c', 'mymalloc.c'],
	LIBS=['z', 'pcre'])

# vim: syntax=python
