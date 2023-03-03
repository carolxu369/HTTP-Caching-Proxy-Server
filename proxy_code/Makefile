CCFLAGS=--std=gnu++98 -pedantic -Wall -Werror -ggdb3 -lpthread -pthread
DIR=httpparser
SOURCES=main.cpp request_create.cpp cache.cpp
PROGS=proxy

OBJS=$(patsubst %.cpp,%.o,$(SOURCES)) 
all: $(PROGS)
proxy: $(OBJS)
	g++ $(CCFLAGS) -o $@ $^

%.o: %.cpp $(DIR)/%.h
	g++ $(CCFLAGS) -c $<

.PHONY: all proxy clean
clean:
	rm -f *~ $(PROGS) $(OBJS)
