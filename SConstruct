opts=Options()

env = Environment(options = opts)
Help(opts.GenerateHelpText(env))

# Extra places we need to look for includes
env.Append(CPPPATH = ['/opt/local/include', '/usr/local/include'])
env.Append(LIBPATH = ['/opt/local/lib', '/usr/local/lib'])

if ARGUMENTS.get('USE_ASSERT', 0):
	env.Append(CCFLAGS = '-DUSE_ASSERT=1')

env.Append(CCFLAGS = '-g -O2 -Wall -Werror')
env.Append(LINKFLAGS = '-g')

env.conf = Configure(env)

if env.conf.CheckCHeader('alloca.h'):
	env.Append(CCFLAGS = '-DHAVE_ALLOCA_H')
if not env.conf.CheckLibWithHeader('boost_regex',
	'boost/regex.hpp', 'c++'):
	print 'Boost regex is required'
	Exit(1)

env = env.conf.Finish()
env.Program('logmerge', ['logmerge.cpp', 'logfiles.cpp'],
	LIBS=['z', 'boost_regex'])

# vim: syntax=python
