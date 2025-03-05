CXX ?= g++

DEBUG ?= 1
ifeq ($(DEBUG), 1)
	CXXFLAGS += -g
else
	CXXFLAGS += -o2

endif

server: main.cpp webserver.cpp config.cpp ./http/http_conn.cpp ./CGImysql/sql_connection_pool.cpp ./timer/lst_timer.cpp
	$(CXX) -o server $^ $(CXXFLAGS) -lpthread -lmysqlclient

clean:
	rm -r server