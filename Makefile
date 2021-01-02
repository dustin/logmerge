OBJS=logmerge.o logfiles.o outputters.o

logmerge: $(OBJS)
	$(CXX) -o logmerge $(OBJS) -lboost_regex -lboost_iostreams

clean:
	rm $(OBJS) logmerge
