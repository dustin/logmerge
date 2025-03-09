OBJS=logmerge.o logfiles.o outputters.o

logmerge: $(OBJS)
	$(CXX) -o logmerge $(OBJS) $(LDFLAGS) -lboost_regex -lboost_iostreams

.PHONY: test

test: logmerge
	cd test && ./runtest

clean:
	rm $(OBJS) logmerge
