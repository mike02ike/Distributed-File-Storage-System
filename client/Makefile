CXX = g++
CXXFLAGS = -std=c++17
FILES = client.cpp
EXECUTABLE = client
LDFLAGS = -lz

client: $(FILES)
	$(CXX) $(CXXFLAGS) $(FILES) -o $(EXECUTABLE) $(LDFLAGS)

clean:
	rm -f $(EXECUTABLE)