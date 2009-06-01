opts=Variables()

opts.Add(BoolVariable('PROFILE', 'Compile with profiling.', 0))
opts.Add(BoolVariable('USE_ASSERT', 'Compile with assertions', 0))

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

build_libs=['z']

if not env.conf.CheckLibWithHeader('boost_regex',
                                   'boost/regex.hpp', 'c++'):
    if not env.conf.CheckLibWithHeader('boost_regex-mt',
                                       'boost/regex.hpp', 'c++'):
        print 'Boost regex is required'
        Exit(1)
    else:
        build_libs.append('boost_regex-mt')
else:
    build_libs.append('boost_regex')

env = env.conf.Finish()
env.Program('logmerge', ['logmerge.cpp', 'logfiles.cpp', 'outputters.cpp'],
            LIBS=build_libs)

# vim: syntax=python
