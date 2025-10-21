CXX      := g++
CXXFLAGS := -std=gnu++17 -O2 -Wall -Wextra -Wpedantic
INCLUDES := -Iinclude
SRCS     := src/main.cpp src/Vfs.cpp src/JsonIO.cpp
OBJS     := $(SRCS:.cpp=.o)
BIN      := lab2_vfs_bstar

.PHONY: all clean run distclean

all: $(BIN)

$(BIN): $(OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

run: $(BIN)
	./$(BIN)

clean:
	rm -f $(OBJS) $(BIN) ./*.json