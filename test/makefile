CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -I../include
LDFLAGS := -lpthread
TARGET := test4
SOURCES := test4.cpp

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: all clean