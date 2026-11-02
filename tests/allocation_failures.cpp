#include <cstddef>
#include <cstdlib>
#include <new>
#include <utility>

namespace
{
bool failNextAllocation = false;
}

void FailNextAllocation()
{
    failNextAllocation = true;
}
bool AllocationFailurePending()
{
    return failNextAllocation;
}

// This fixture is confined to the single-threaded contract-test executable.
void *operator new(std::size_t size)
{
    if (std::exchange(failNextAllocation, false))
    {
        throw std::bad_alloc();
    }
    if (void *memory = std::malloc(size == 0 ? 1 : size))
    {
        return memory;
    }
    throw std::bad_alloc();
}
void *operator new[](std::size_t size)
{
    return ::operator new(size);
}
void operator delete(void *memory) noexcept
{
    std::free(memory);
}
void operator delete[](void *memory) noexcept
{
    std::free(memory);
}
void operator delete(void *memory, std::size_t) noexcept
{
    std::free(memory);
}
void operator delete[](void *memory, std::size_t) noexcept
{
    std::free(memory);
}
