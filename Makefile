CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
BIN      := downloader
SRC      := $(wildcard src/*.cpp)

$(BIN): $(SRC) src/downloader.h
	$(CXX) $(CXXFLAGS) -o $@ $(SRC)

clean:
	rm -f $(BIN)

.PHONY: clean
