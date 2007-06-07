opts=Options()

opts.Add(BoolOption('PROFILE', 'Compile with profiling.', 0))
opts.Add(BoolOption('USE_ASSERT', 'Compile with assertions', 0))

env = Environment(options = opts)
Help(opts.GenerateHelpText(env))

# Extra places we need to look for includes
env.Append(CPPPATH = ['/opt/local/include', '/usr/local/include'])
env.Append(LIBPATH = ['/opt/local/lib', '/usr/local/lib'])

if ARGUMENTS.get('USE_ASSERT', 0):
	env.Append(CCFLAGS = '-DUSE_ASSERT=1')

env.Append(CCFLAGS = '-g -Wall -Werror')
env.Append(LINKFLAGS = '-g')

if ARGUMENTS.get('PROFILE', 0):
	env.Append(CCFLAGS = '-pg')
	env.Append(LINKFLAGS = '-pg')
else:
	# Turn on tons of optimization if we're not profiling.
	env.Append(CCFLAGS = '-O3')

env.conf = Configure(env)

if not env.conf.CheckLibWithHeader('boost_regex',
	'boost/regex.hpp', 'c++'):
	print 'Boost regex is required'
	Exit(1)

env = env.conf.Finish()
env.Program('logmerge', ['logmerge.cpp', 'logfiles.cpp'],
	LIBS=['z', 'boost_regex'])

# vim: syntax=python
