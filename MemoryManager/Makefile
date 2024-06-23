CXX = g++
CXXFLAGS = -std=c++17
SRCS = MemoryManager.cpp
OBJS = $(SRCS:.cpp=.o)
LIB = libMemoryManager.a
all: $(LIB)
$(LIB): $(OBJS)
	ar cr $@ $^
%.o: MemoryManager.cpp MemoryManager.h
	$(CXX) $(CXXFLAGS) -c $< -o $@
clean:
	rm -f $(OBJS) $(LIB)
.PHONY: all clean
