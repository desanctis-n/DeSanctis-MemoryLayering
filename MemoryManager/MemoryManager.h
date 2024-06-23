#pragma once
#include <iostream>
#include <functional>
#include <climits>
#include <cmath>
#include <cstring>
#include <iterator>
#include <map>
#include <queue>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

class MemoryManager {

    struct block {
        uint16_t size = 0;
        bool is_hole = true;
        block();
        block(uint16_t size, bool is_hole);
    };

    unsigned wordSize;
    std::function<int(int, void *)> allocator;
    uint8_t *memblock;
    size_t sizeInBytes;
    uint16_t sizeInWords;
    std::map<uint16_t, block> blocks;
    void combineHole(std::map<uint16_t, block>::iterator iter);

public:
    MemoryManager(unsigned wordSize, std::function<int(int, void *)> allocator);
    ~MemoryManager();

    void initialize(size_t sizeInWords);
    void shutdown();

    void *allocate(size_t sizeInBytes);
    void free(void *address);

    void setAllocator(std::function<int(int, void *)>);
    int dumpMemoryMap(char *filename);

    void *getList();
    void *getBitmap();
    unsigned getWordSize();
    void *getMemoryStart();
    unsigned getMemoryLimit();
};

int bestFit(int sizeInWords, void *list);
int worstFit(int sizeInWords, void *list);
