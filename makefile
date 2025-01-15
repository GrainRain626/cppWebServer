CXX ?= g++

DEBUG ?= 1
ifeq ($(DEBUG), 1)
	CXXFLAGS += -g
else
	CXXFLAGS += -o2

endif

server: main.cpp webserver.cpp config.cpp 
	$(CXX) -o server $^ $(CXXFLAGS)

clean:
	rm -r server