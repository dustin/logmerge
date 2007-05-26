opts=Options()

opts.Add(BoolOption('MYMALLOC', 'Use mymalloc for memory debug', 0))

env = Environment(options = opts)
Help(opts.GenerateHelpText(env))

print "Mymalloc: %s" % ARGUMENTS.get('MYMALLOC', 0)
if ARGUMENTS.get('MYMALLOC', 0):
	env.Append(CCFLAGS = '-DUSE_MYMALLOC -DMYMALLOC')

env.Append(CCFLAGS = '-g -O2 -Wall -Werror')
env.Append(LDFLAGS = '-g')
env.conf = Configure(env)

if env.conf.CheckCHeader('alloca.h'):
	env.Append(CCFLAGS = '-DHAVE_ALLOCA_H')

env = env.conf.Finish()
env.Program('logmerge', ['logmerge.c', 'logfiles.c', 'mymalloc.c'],
	LIBS=['z'])
