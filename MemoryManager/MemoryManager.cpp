#include "MemoryManager.h"

// ----------------- CONSTRUCTION / DESTRUCTION ----------------- //

MemoryManager::block::block() = default;
MemoryManager::block::block(uint16_t size, bool is_hole) {
    this->size = size;
    this->is_hole = is_hole;
}
MemoryManager::MemoryManager(unsigned wordSize, std::function<int(int, void *)> allocator) {
    this->wordSize = wordSize;
    this->allocator = allocator;
    memblock = nullptr;
    sizeInBytes = 0;
    sizeInWords = 0;
}
MemoryManager::~MemoryManager() {
    shutdown();
}

// ----------------- INITIALIZATION / SHUTDOWN ----------------- //

void MemoryManager::initialize(size_t sizeInWords) {
    if (memblock != nullptr)
        shutdown();
    if (sizeInWords > UINT16_MAX)
        sizeInWords = UINT16_MAX;
    this->sizeInWords = sizeInWords;
    sizeInBytes = sizeInWords * wordSize;
    memblock = new uint8_t[sizeInBytes];
    blocks.emplace(0, block(sizeInWords, true));
}
void MemoryManager::shutdown() {
    if (memblock == nullptr)
        return;
    delete[] memblock;
    memblock = nullptr;
    sizeInWords = 0;
    sizeInBytes = 0;
    blocks.clear();
}

// ----------------- ALLOCATION / DEALLOCATION ----------------- //

void *MemoryManager::allocate(size_t sizeInBytes) {
    if (memblock == nullptr)
        return nullptr;
    uint16_t sizeOfAlloc = sizeInBytes / wordSize + (sizeInBytes % wordSize != 0 ? 1 : 0);
    auto holeArr = static_cast<uint16_t *>(getList());
    int offset = allocator(static_cast<int>(sizeOfAlloc), holeArr);
    delete[] holeArr;
    if (offset == -1)
        return nullptr;

    if (blocks.at(offset).size >=  sizeOfAlloc && blocks.at(offset).is_hole) {
        if (blocks[offset].size == sizeOfAlloc) {
            blocks[offset].is_hole = false;
        }
        else {
            uint16_t oldsize = blocks[offset].size;
            blocks[offset] = block(sizeOfAlloc, false);
            blocks[offset + sizeOfAlloc] = block(oldsize - sizeOfAlloc, true);
        }
    }
    return memblock + offset * wordSize;
}
void MemoryManager::free(void *address) {
    if (memblock == nullptr)
        return;
    auto memblockoffset = static_cast<uint8_t *>(address);
    int offset = static_cast<int>((memblockoffset - memblock) / wordSize);
    if (offset < 0)
        return;
    blocks[offset].is_hole = true;
    combineHole(blocks.find(offset));
}
void MemoryManager::combineHole(std::map<uint16_t, block>::iterator iter) {
    if (!iter->second.is_hole)
        return;
    auto next_it = std::next(iter);
    auto prev_it = std::prev(iter);
    if (iter == blocks.end())
        return;
    if (next_it != blocks.end()) {
        if (iter->second.is_hole == next_it->second.is_hole) {
            iter->second.size += next_it->second.size;
            blocks.erase(next_it);
        }
    }
    if (prev_it != blocks.end()) {
        if (iter->second.is_hole == prev_it->second.is_hole) {
            prev_it->second.size += iter->second.size;
            blocks.erase(iter);
        }
    }
}

// -------------------- SETTERS / GETTERS --------------------- //

void MemoryManager::setAllocator(std::function<int(int, void *)> allocator) {
    this->allocator = allocator;
}
void *MemoryManager::getList() {
    std::queue<std::pair<uint16_t, block>> holes;
    for (const auto &blk : blocks)
        if (blk.second.is_hole)
            holes.emplace(blk);

    if (holes.empty())
        return nullptr;
    auto* holeArr = new uint16_t[holes.size() * 2 + 1];
    holeArr[0] = holes.size();
    uint16_t i = 1;

    while (!holes.empty()) {
        holeArr[i++] = holes.front().first;
        holeArr[i++] = holes.front().second.size;
        holes.pop();
    }
    return static_cast<void *>(holeArr);
}
unsigned MemoryManager::getWordSize() {
    return wordSize;
}
void *MemoryManager::getMemoryStart() {
    return memblock;
}
unsigned MemoryManager::getMemoryLimit() {
    return sizeInBytes;
}

// -------------------- PRINTING / BITMAP --------------------- //

int MemoryManager::dumpMemoryMap(char *filename) {
    if (memblock == nullptr)
        return -1;
    int file_descriptor = open(filename, O_RDWR | O_CREAT | O_TRUNC, 0777);
    if (file_descriptor == -1)
        return -1;
    std::string dump;

    for (const auto &blk: blocks)
        if (blk.second.is_hole)
            dump.append("[" + std::to_string(blk.first) + ", " + std::to_string(blk.second.size) + "] - ");

    dump.erase(dump.size() - 3);
    write(file_descriptor, dump.data(), dump.size());
    close(file_descriptor);
    return 0;
}
void *MemoryManager::getBitmap() {
    const uint16_t SIZE_BYTE = 8;
    uint16_t sizeOfBitmap = (sizeInWords / SIZE_BYTE) + (sizeInWords % SIZE_BYTE != 0) + 2;
    auto bitArr = new uint8_t[sizeOfBitmap];
    std::memset(bitArr + 2, 0, sizeOfBitmap - 2);

    for(auto &blk: blocks)
        for (uint16_t i = blk.first; i < blk.first + blk.second.size; i++)
            if (!blk.second.is_hole)
                bitArr[i / SIZE_BYTE + 2] += 1 << i % SIZE_BYTE;

    bitArr[0] = (sizeOfBitmap - 2) % UINT8_MAX;
    bitArr[1] = (sizeOfBitmap - 2) / UINT8_MAX;
    return bitArr;
}

// ------------------- ALLOCATOR FUNCTIONS -------------------- //

int bestFit(int sizeInWords, void *list) {
    auto *holeArr = static_cast<uint16_t *>(list);
    if (holeArr == nullptr)
        return -1;
    int offset = -1;
    uint16_t hole_size;
    uint16_t best = UINT16_MAX;

    for (uint16_t i = 0; i < holeArr[0]; i++) {
         hole_size = holeArr[2 * i + 2];
        if (hole_size >= static_cast<uint16_t>(sizeInWords) && hole_size < best) {
            offset = holeArr[2 * i + 1];;
            best = hole_size;
        }
    }
    return static_cast<int>(offset);
}
int worstFit(int sizeInWords, void *list) {
    auto *holeArr = static_cast<uint16_t *>(list);
    if (holeArr == nullptr)
        return -1;
    int offset = -1;
    uint16_t hole_size;
    uint16_t worst = 0;

    for (uint16_t i = 0; i < holeArr[0]; i++) {
        hole_size = holeArr[2 * i + 2];
        if (hole_size >= static_cast<uint16_t>(sizeInWords) && hole_size > worst) {
            offset = holeArr[2 * i + 1];;
            worst = hole_size;
        }
    }
    return static_cast<int>(offset);
}
