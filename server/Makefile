CXX = g++
CXXFLAGS = -std=c++17
FILES = server.cpp
EXECUTABLE = server
LDFLAGS = -lz

server: server.cpp
	$(CXX) $(CXXFLAGS) $(FILES) -o $(EXECUTABLE) $(LDFLAGS)
clean:
	rm -f $(EXECUTABLE)