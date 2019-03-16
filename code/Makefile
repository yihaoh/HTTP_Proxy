SOURCES = proxy.cpp request.cpp response.cpp
OBJS=$(patsubst % cpp, %.o, $(SOURCES))
CPPFLAGS=-Wall -Werror -pedantic -std=gnu++11 -pthread

proxy: $(OBJS)
	g++ $(CPPFLAGS) -g -o proxy $(OBJS)

%.o: %.cpp proxy.h request.h parse.h response.h
	g++ $(CPPFLAGS) -c $<

clean:
	rm proxy 
