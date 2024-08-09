#include "pch.h"

#include <wil/allocators.h>

#ifdef WIL_ENABLE_EXCEPTIONS
// TODO
#endif

#include "common.h"

TEST_CASE("AllocatorTraits::AliasTypes", "[allocators]")
{
    struct allocator_with_all
    {
        // NOTE: Using random types to test functionality
        using value_type = int8_t;
        using pointer = int16_t;
        using const_pointer = int32_t;
        using void_pointer = int64_t;
        using const_void_pointer = uint8_t;
        using difference_type = uint16_t;
        using size_type = uint32_t;
        using propagate_on_container_copy_assignment = uint64_t;
        using propagate_on_container_move_assignment = float;
        using propagate_on_container_swap = double;
        using is_always_equal = void;
    };

    REQUIRE(wistd::is_same_v<wil::allocator_traits<allocator_with_all>::value_type, int8_t>);
    REQUIRE(wistd::is_same_v<wil::allocator_traits<allocator_with_all>::pointer, int16_t>);
    REQUIRE(wistd::is_same_v<wil::allocator_traits<allocator_with_all>::const_pointer, int32_t>);
    REQUIRE(wistd::is_same_v<wil::allocator_traits<allocator_with_all>::void_pointer, int64_t>);
    REQUIRE(wistd::is_same_v<wil::allocator_traits<allocator_with_all>::const_void_pointer, uint8_t>);
    REQUIRE(wistd::is_same_v<wil::allocator_traits<allocator_with_all>::difference_type, uint16_t>);
    REQUIRE(wistd::is_same_v<wil::allocator_traits<allocator_with_all>::size_type, uint32_t>);
    REQUIRE(wistd::is_same_v<wil::allocator_traits<allocator_with_all>::propagate_on_container_copy_assignment, uint64_t>);
    REQUIRE(wistd::is_same_v<wil::allocator_traits<allocator_with_all>::propagate_on_container_move_assignment, float>);
    REQUIRE(wistd::is_same_v<wil::allocator_traits<allocator_with_all>::propagate_on_container_swap, double>);
    REQUIRE(wistd::is_same_v<wil::allocator_traits<allocator_with_all>::is_always_equal, void>);

    struct allocator_with_none
    {
        using value_type = int;
    };

    REQUIRE(wistd::is_same_v<wil::allocator_traits<allocator_with_none>::value_type, int>);
    REQUIRE(wistd::is_same_v<wil::allocator_traits<allocator_with_none>::pointer, int*>);
    REQUIRE(wistd::is_same_v<wil::allocator_traits<allocator_with_none>::const_pointer, const int*>);
    REQUIRE(wistd::is_same_v<wil::allocator_traits<allocator_with_none>::void_pointer, void*>);
    REQUIRE(wistd::is_same_v<wil::allocator_traits<allocator_with_none>::const_void_pointer, const void*>);
    REQUIRE(wistd::is_same_v<wil::allocator_traits<allocator_with_none>::difference_type, ptrdiff_t>);
    REQUIRE(wistd::is_same_v<wil::allocator_traits<allocator_with_none>::size_type, size_t>);
    REQUIRE(!wil::allocator_traits<allocator_with_none>::propagate_on_container_copy_assignment::value);
    REQUIRE(!wil::allocator_traits<allocator_with_none>::propagate_on_container_move_assignment::value);
    REQUIRE(!wil::allocator_traits<allocator_with_none>::propagate_on_container_swap::value);
    REQUIRE(wil::allocator_traits<allocator_with_none>::is_always_equal::value); // Empty type

    struct non_empty_allocator
    {
        using value_type = int;

        int taking_up_space;
    };

    REQUIRE(!wil::allocator_traits<non_empty_allocator>::is_always_equal::value); // Non-empty type
}

template <typename T>
struct allocator_base
{
    using value_type = T;

    // Result for allocation
    wistd::aligned_storage_t<sizeof(T), alignof(T)> buffer[8] = {};

    int allocate_call_count = 0;
    T* allocate(size_t count)
    {
        ++allocate_call_count;
        if (count > ARRAYSIZE(buffer))
        {
            return nullptr;
        }

        return reinterpret_cast<T*>(&buffer[0]);
    }

    // NOTE: allocate with hint not required; left up to derived types to implement
    // NOTE: allocate_at_least not required; left up to derived types to implement

    int deallocate_call_count = 0;
    void deallocate(T*, size_t)
    {
        ++deallocate_call_count;
    }

    // NOTE: construct not required; left up to derived types to implement
    // NOTE: destroy not required; left up to derived types to implement
    // NOTE: max_size not required; left up to derived types to implement
    // NOTE: select_on_container_copy_construction not required; left up to derived types to implement
};

// Simple type to validate construction
struct allocated_type
{
    static constexpr const int magic = 0xc0ffee;
    int value = magic;

    constexpr allocated_type() noexcept = default;

    constexpr allocated_type(int value) noexcept : value(value)
    {
    }

    ~allocated_type()
    {
        value = ~magic;
    }
};

template <typename T>
struct allocator_uses_hint : allocator_base<T>
{
    using allocator_base<T>::allocate;

    int allocate_hint_call_count = 0;
    T* allocate(size_t count, const void* hint)
    {
        ++allocate_hint_call_count;
        auto startIndex = hint ? reinterpret_cast<const T*>(hint) - reinterpret_cast<T*>(&this->buffer[0]) : 0;
        if ((startIndex + count) > ARRAYSIZE(this->buffer))
        {
            return nullptr;
        }

        return reinterpret_cast<T*>(&this->buffer[startIndex]);
    }
};

TEST_CASE("AllocatorTraits::AllocateDeallocate", "[allocators]")
{
    using AllocBase = allocator_base<allocated_type>;
    AllocBase alloc;

    auto ptr = wil::allocator_traits<AllocBase>::allocate(alloc, 1);
    REQUIRE(ptr != nullptr);
    REQUIRE(alloc.allocate_call_count == 1);
    REQUIRE(ptr->value == 0); // No constructor has run yet
    wil::allocator_traits<AllocBase>::deallocate(alloc, ptr, 1);
    REQUIRE(alloc.deallocate_call_count == 1);
    REQUIRE(ptr->value == 0); // Destructor not run

    // Call with hint should call the same allocate function
    auto ptr2 = wil::allocator_traits<AllocBase>::allocate(alloc, 1, ptr);
    REQUIRE(ptr == ptr2);
    REQUIRE(alloc.allocate_call_count == 2);
    REQUIRE(ptr2->value == 0); // No constructor has run yet
    wil::allocator_traits<AllocBase>::deallocate(alloc, ptr, 1);
    REQUIRE(alloc.deallocate_call_count == 2);
    REQUIRE(ptr2->value == 0); // Destructor not run

    using AllocHint = allocator_uses_hint<allocated_type>;
    AllocHint hint;
    ptr = wil::allocator_traits<AllocHint>::allocate(hint, 1);
    REQUIRE(ptr != nullptr);
    REQUIRE(hint.allocate_call_count == 1);
    REQUIRE(hint.allocate_hint_call_count == 0);
    REQUIRE(ptr->value == 0); // No constructor has run yet
    wil::allocator_traits<AllocHint>::deallocate(hint, ptr, 1);
    REQUIRE(hint.deallocate_call_count == 1);
    REQUIRE(ptr->value == 0); // Destructor not run

    // All with hint should use the new method
    ptr2 = wil::allocator_traits<AllocHint>::allocate(hint, 1, ptr);
    REQUIRE(ptr2 == ptr);
    REQUIRE(hint.allocate_call_count == 1); // Should still be 1
    REQUIRE(hint.allocate_hint_call_count == 1);
    REQUIRE(ptr2->value == 0);
    wil::allocator_traits<AllocHint>::deallocate(hint, ptr2, 1);
    REQUIRE(hint.deallocate_call_count == 2);
    REQUIRE(ptr->value == 0); // Destructor not run

    // Failed allocation
    ptr = wil::allocator_traits<AllocBase>::allocate(alloc, 100);
    REQUIRE(ptr == nullptr);
    ptr = wil::allocator_traits<AllocHint>::allocate(hint, 100, ptr2);
    REQUIRE(ptr == nullptr);
}

template <typename T>
struct allocator_with_construct_destroy : allocator_base<T>
{
    int construct_call_count = 0;
    template <typename... Args>
    void construct(T* ptr, Args&&... args)
    {
        ++construct_call_count;
        ::new(static_cast<void*>(ptr)) T(wistd::forward<Args>(args)...);
    }

    int destroy_call_count = 0;
    void destroy(T* ptr) noexcept
    {
        ++destroy_call_count;
        ptr->~T();
    }
};

TEST_CASE("AllocatorTraits::ConstructDestroy", "[allocators]")
{
    // 'allocator_base' has no construct/destroy methods, so placement new and manual invocation of the destructor should be used
    using AllocBase = allocator_base<allocated_type>;
    AllocBase alloc;

    // No argument construction
    auto ptr = wil::allocator_traits<AllocBase>::allocate(alloc, 1);
    REQUIRE(ptr != nullptr);
    wil::allocator_traits<AllocBase>::construct(alloc, ptr);
    REQUIRE(ptr->value == allocated_type::magic);
    wil::allocator_traits<AllocBase>::destroy(alloc, ptr);
    REQUIRE(ptr->value == ~allocated_type::magic);
    wil::allocator_traits<AllocBase>::deallocate(alloc, ptr, 1);

    // Single argument construction
    ptr = wil::allocator_traits<AllocBase>::allocate(alloc, 1);
    REQUIRE(ptr != nullptr);
    wil::allocator_traits<AllocBase>::construct(alloc, ptr, 42);
    REQUIRE(ptr->value == 42);
    wil::allocator_traits<AllocBase>::destroy(alloc, ptr);
    REQUIRE(ptr->value == ~allocated_type::magic);
    wil::allocator_traits<AllocBase>::deallocate(alloc, ptr, 1);

    using AllocConstructDestroy = allocator_with_construct_destroy<allocated_type>;
    AllocConstructDestroy allocConstructDestroy;

    // No argument construction
    ptr = wil::allocator_traits<AllocConstructDestroy>::allocate(allocConstructDestroy, 1);
    REQUIRE(ptr != nullptr);
    wil::allocator_traits<AllocConstructDestroy>::construct(allocConstructDestroy, ptr);
    REQUIRE(allocConstructDestroy.allocate_call_count == 1);
    REQUIRE(ptr->value == allocated_type::magic);
    wil::allocator_traits<AllocConstructDestroy>::destroy(allocConstructDestroy, ptr);
    REQUIRE(allocConstructDestroy.destroy_call_count == 1);
    REQUIRE(ptr->value == ~allocated_type::magic);
    wil::allocator_traits<AllocConstructDestroy>::deallocate(allocConstructDestroy, ptr, 1);

    // Single argument construction
    ptr = wil::allocator_traits<AllocConstructDestroy>::allocate(allocConstructDestroy, 1);
    REQUIRE(ptr != nullptr);
    wil::allocator_traits<AllocConstructDestroy>::construct(allocConstructDestroy, ptr, 42);
    REQUIRE(allocConstructDestroy.allocate_call_count == 2);
    REQUIRE(ptr->value == 42);
    wil::allocator_traits<AllocConstructDestroy>::destroy(allocConstructDestroy, ptr);
    REQUIRE(allocConstructDestroy.destroy_call_count == 2);
    REQUIRE(ptr->value == ~allocated_type::magic);
    wil::allocator_traits<AllocConstructDestroy>::deallocate(allocConstructDestroy, ptr, 1);
}

template <typename T>
struct allocator_with_max_size : allocator_base<T>
{
    size_t max_size() const noexcept
    {
        return 42;
    }
};

TEST_CASE("AllocatorTraits::MaxSize", "[allocators]")
{
    using AllocBase = allocator_base<allocated_type>;
    AllocBase alloc;
    REQUIRE(wil::allocator_traits<AllocBase>::max_size(alloc) == (SIZE_MAX / sizeof(allocated_type)));

    using AllocMaxSize = allocator_with_max_size<allocated_type>;
    AllocMaxSize allocMaxSize;
    REQUIRE(wil::allocator_traits<AllocMaxSize>::max_size(allocMaxSize) == 42);
}

template <typename T>
struct allocator_with_select_on_container_copy_construction : allocator_base<T>
{
    mutable int copy_call_count = 0;
    allocator_with_select_on_container_copy_construction select_on_container_copy_construction() const noexcept
    {
        ++copy_call_count;
        return *this;
    }
};

TEST_CASE("AllocatorTraits::SelectOnContainerCopyConstruction", "[allocators]")
{
    using AllocBase = allocator_base<allocated_type>;
    AllocBase alloc;
    (void)wil::allocator_traits<AllocBase>::select_on_container_copy_construction(alloc); // Verify it can be called
}
