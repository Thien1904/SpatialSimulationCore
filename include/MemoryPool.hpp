#ifndef MEMORY_POOL_HPP
#define MEMORY_POOL_HPP

#include <vector>
#include <memory>
#include <cstddef>
#include <utility>

template <typename T, size_t BlockSize = 1024>
class MemoryPool {
private:
    struct Node {
        T element;
        Node* next;
    };

    std::vector<std::unique_ptr<Node[]>> chunks;
    Node* freeList = nullptr;

    void expandPool() {
        auto newChunk = std::make_unique<Node[]>(BlockSize);
        for (size_t i = 0; i < BlockSize - 1; ++i) {
            newChunk[i].next = &newChunk[i + 1];
        }
        newChunk[BlockSize - 1].next = freeList;
        freeList = newChunk.get();
        chunks.push_back(std::move(newChunk));
    }

public:
    MemoryPool() { expandPool(); }

    template <typename... Args>
    T* allocate(Args&&... args) {
        if (!freeList) {
            expandPool();
        }
        Node* node = freeList;
        freeList = freeList->next;
        
        T* ptr = &(node->element);
        new (ptr) T(std::forward<Args>(args)...);
        return ptr;
    }

    void deallocate(T* ptr) {
        if (!ptr) return;
        ptr->~T();
        Node* node = reinterpret_cast<Node*>(ptr);
        node->next = freeList;
        freeList = node;
    }
};

#endif