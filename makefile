CXX ?= g++

DEBUG ?= 1
ifeq ($(DEBUG), 1)
	CXXFLAGS += -g
else
	CXXFLAGS += -o2

endif

server: main.cpp webserver.cpp config.cpp ./http/http_conn.cpp
	$(CXX) -o server $^ $(CXXFLAGS) -lmysqlclient

clean:
	rm -r server