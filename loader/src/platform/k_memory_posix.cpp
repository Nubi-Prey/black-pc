// POSIX Memory Management and Contiguous Physical Allocator Implementation
#include "k_memory_posix.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <sys/mman.h>

namespace xposix {

namespace {

constexpr uintptr_t XBOX_RAM_BASE = 0x80000000;
constexpr size_t    XBOX_RAM_SIZE = 64 * 1024 * 1024; // 64 MB
void *g_ram_ptr = nullptr;
size_t g_ram_bump = 0x10000; // First 64KB reserved
std::mutex g_mem_mutex;

} // namespace

void memory_init()
{
    std::lock_guard<std::mutex> lock(g_mem_mutex);
    if (!g_ram_ptr) {
        g_ram_ptr = mmap((void *)XBOX_RAM_BASE, XBOX_RAM_SIZE,
                         PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
        assert(g_ram_ptr == (void *)XBOX_RAM_BASE && "k_memory_posix: Failed to map 64MB RAM at 0x80000000");
    }
}

void *ExAllocatePoolWithTag(uint32_t pool_type, uint32_t number_of_bytes, uint32_t tag)
{
    (void)pool_type; (void)tag;
    if (number_of_bytes == 0) return nullptr;

    // Allocate 16-byte aligned memory with a header storing allocation size
    size_t total = (number_of_bytes + 15 + 16) & ~15u;
    uint8_t *p = (uint8_t *)malloc(total);
    if (!p) return nullptr;

    memset(p, 0, total);
    *(uint32_t *)p = (uint32_t)number_of_bytes;
    return (void *)(p + 16);
}

void ExFreePool(void *ptr)
{
    if (!ptr) return;
    uint8_t *p = (uint8_t *)ptr - 16;
    free(p);
}

void *MmAllocateContiguousMemory(uint32_t number_of_bytes)
{
    return MmAllocateContiguousMemoryEx(number_of_bytes, 0, 0x03FFFFFF, 0x1000, 0x04);
}

void *MmAllocateContiguousMemoryEx(uint32_t number_of_bytes, uint32_t lowest_acceptable_address,
                                  uint32_t highest_acceptable_address, uint32_t alignment, uint32_t protect)
{
    (void)lowest_acceptable_address; (void)highest_acceptable_address; (void)protect;
    memory_init();

    std::lock_guard<std::mutex> lock(g_mem_mutex);
    size_t align = (alignment < 0x1000) ? 0x1000 : alignment;
    size_t aligned_bump = (g_ram_bump + align - 1) & ~(align - 1);
    size_t page_aligned_size = (number_of_bytes + 0xFFF) & ~0xFFFu;

    if (aligned_bump + page_aligned_size > XBOX_RAM_SIZE) {
        fprintf(stderr, "k_memory_posix: Out of contiguous Xbox RAM! (requested %u bytes)\n", number_of_bytes);
        return nullptr;
    }

    void *ptr = (void *)(XBOX_RAM_BASE + aligned_bump);
    g_ram_bump = aligned_bump + page_aligned_size;
    memset(ptr, 0, page_aligned_size);

    return ptr;
}

void MmFreeContiguousMemory(void *base_address)
{
    (void)base_address; // In simple bump allocator, memory reclaimed at reset
}

uint32_t MmGetPhysicalAddress(void *base_address)
{
    uintptr_t addr = (uintptr_t)base_address;
    if (addr >= XBOX_RAM_BASE && addr < XBOX_RAM_BASE + XBOX_RAM_SIZE) {
        return (uint32_t)(addr - XBOX_RAM_BASE);
    }
    return (uint32_t)addr;
}

NTSTATUS NtAllocateVirtualMemory(void **base_address, uint32_t zero_bits, uint32_t *region_size,
                                uint32_t allocation_type, uint32_t protect)
{
    (void)zero_bits; (void)allocation_type; (void)protect;
    if (!base_address || !region_size) return 0xC000000DL;

    size_t sz = (*region_size + 0xFFF) & ~0xFFFu;
    void *req = *base_address;

    int flags = MAP_PRIVATE | MAP_ANONYMOUS;
    if (req) flags |= MAP_FIXED;

    void *p = mmap(req, sz, PROT_READ | PROT_WRITE, flags, -1, 0);
    if (p == MAP_FAILED) return 0xC0000017L;

    *base_address = p;
    *region_size = (uint32_t)sz;
    return 0; // STATUS_SUCCESS
}

NTSTATUS NtFreeVirtualMemory(void **base_address, uint32_t *region_size, uint32_t free_type)
{
    (void)free_type;
    if (!base_address || !region_size || !*base_address) return 0xC000000DL;
    munmap(*base_address, *region_size);
    *base_address = nullptr;
    return 0;
}

} // namespace xposix
