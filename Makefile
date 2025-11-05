CXX      := g++
CXXFLAGS := -std=gnu++23 -O2 -Wall -Wextra -Wpedantic
INCLUDES := -Iinclude

# ядро (без main)
SRCS_CORE := src/Vfs.cpp src/JsonIO.cpp src/ui.cpp
OBJS_CORE := $(SRCS_CORE:.cpp=.o)

BIN := vfs_cli

TESTS := test_nav test_create test_rm test_rename_mv test_find_save test_bstar

TEST_OBJS := $(addprefix tests/,$(addsuffix .o,$(TESTS)))

.PHONY: all clean run tests run_tests dirs

all: dirs $(BIN)

dirs:
	mkdir -p bin

$(BIN): src/main.o $(OBJS_CORE)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ -o $@

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

tests/%.o: tests/%.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

bin/%: tests/%.o $(OBJS_CORE) | dirs
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ -o $@

run: $(BIN)
	./$(BIN)

tests: $(TEST_BINS)

run_tests: tests
	@for t in $(TEST_BINS); do \
	  echo "===== RUN $$t ====="; \
	  ./$$t || exit 1; \
	done
	@echo "All tests passed."

clean:
	rm -f src/*.o tests/*.o $(BIN) $(TEST_BINS) ./*.json
