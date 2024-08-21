#include "pch.h"

#include <wil/allocators.h>

#ifdef WIL_ENABLE_EXCEPTIONS
#include <vector>
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

template <typename T, typename Other, typename Another>
struct allocator_with_many_args : allocator_base<T>
{
};

template <typename T>
struct allocator_with_rebind : allocator_base<T>
{
    template <typename U>
    struct rebind
    {
        using other = double; // Make it very obvious we're using this one
    };
};

TEST_CASE("AllocatorTraits::Rebind")
{
    REQUIRE(wistd::is_same_v<wil::allocator_traits<allocator_base<int>>::rebind_alloc<float>, allocator_base<float>>);
    REQUIRE(
        wistd::is_same_v<wil::allocator_traits<allocator_with_many_args<int, char, double>>::rebind_alloc<float>, allocator_with_many_args<float, char, double>>);
    REQUIRE(wistd::is_same_v<wil::allocator_traits<allocator_with_rebind<int>>::rebind_alloc<float>, double>);

    REQUIRE(wistd::is_same_v<wil::allocator_traits<allocator_base<int>>::rebind_traits<float>, wil::allocator_traits<allocator_base<float>>>);
}

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

    using AllocSelect = allocator_with_select_on_container_copy_construction<allocated_type>;
    AllocSelect allocSelect;
    (void)wil::allocator_traits<AllocSelect>::select_on_container_copy_construction(allocSelect);
    REQUIRE(allocSelect.copy_call_count == 1);
}

template <typename Allocator>
static void DoSTLContainerAllocatorTest(Allocator& alloc)
{
#ifdef WIL_ENABLE_EXCEPTIONS
    // NOTE: 'Allocator' may be non-throwing, in which case it is incompatible with STL containers which assume allocation
    // failures will throw, however we should not be under memory pressure during test execution
    std::vector<int, Allocator> v(alloc);
    REQUIRE(v.get_allocator() == alloc);

    for (int i = 0; i < 42; ++i)
    {
        v.push_back(i);
    }

    REQUIRE(v.size() == 42);

    for (int i = 0; i < 42; ++i)
    {
        REQUIRE(v[i] == i);
    }

    // Copy construction
    auto ptr = v.data();
    std::vector v2(v);
    REQUIRE(v2.get_allocator() == alloc);
    REQUIRE(v2 == v);
    REQUIRE(v2.data() != ptr); // Deep copy

    // Move construction
    ptr = v2.data();
    std::vector v3(std::move(v2));
    REQUIRE(v3.get_allocator() == alloc);
    REQUIRE(v3 == v);
    REQUIRE(v3.data() == ptr); // Same allocator; should be same pointer

    // Copy assignment
    ptr = v.data();
    v2 = v;
    REQUIRE(v2.get_allocator() == alloc);
    REQUIRE(v2 == v);
    REQUIRE(v2.data() != ptr); // Deep copy

    // Move assignment
    v2.clear();
    ptr = v3.data();
    v2 = std::move(v3);
    REQUIRE(v2.get_allocator() == alloc);
    REQUIRE(v2 == v);
    REQUIRE(v2.data() == ptr); // Same allocator; should be same pointer

    // Swap
    auto ptr2 = v.data();
    v2.swap(v);
    REQUIRE(v.get_allocator() == alloc);
    REQUIRE(v2.get_allocator() == alloc);
    REQUIRE(v.data() == ptr);
    REQUIRE(v2.data() == ptr2);
#endif
}

#ifdef WIL_ENABLE_EXCEPTIONS
template <typename Allocator>
static void DoThrowingAllocatorTest(Allocator&& alloc)
{
    // Equality tests
    REQUIRE(alloc == alloc);
    REQUIRE_FALSE(alloc != alloc);
    REQUIRE(alloc == Allocator(alloc));
    REQUIRE_FALSE(alloc != Allocator(alloc));

    auto ptr = alloc.allocate(42);
    REQUIRE(ptr != nullptr);
    alloc.deallocate(ptr, 42);

    // Through allocator traits
    ptr = wil::allocator_traits<wistd::remove_reference_t<Allocator>>::allocate(alloc, 10);
    REQUIRE(ptr != nullptr);
    wil::allocator_traits<wistd::remove_reference_t<Allocator>>::deallocate(alloc, ptr, 10);

    // Failure
    REQUIRE_THROWS_AS(alloc.allocate(SIZE_MAX), std::bad_alloc);

    // Failure should also occur for reasonably large requests
    REQUIRE_THROWS_AS(alloc.allocate(SIZE_MAX / (sizeof(int) * 2)), std::bad_alloc);

    DoSTLContainerAllocatorTest(alloc);
}
#endif

template <typename Allocator>
static void DoNothrowAllocatorTest(Allocator&& alloc)
{
    // Equality tests
    REQUIRE(alloc == alloc);
    REQUIRE_FALSE(alloc != alloc);
    REQUIRE(alloc == Allocator(alloc));
    REQUIRE_FALSE(alloc != Allocator(alloc));

    auto ptr = alloc.allocate(42);
    REQUIRE(ptr != nullptr);
    alloc.deallocate(ptr, 42);

    // Through allocator traits
    ptr = wil::allocator_traits<wistd::remove_reference_t<Allocator>>::allocate(alloc, 10);
    REQUIRE(ptr != nullptr);
    wil::allocator_traits<wistd::remove_reference_t<Allocator>>::deallocate(alloc, ptr, 10);

    // Failure
    ptr = alloc.allocate(SIZE_MAX);
    REQUIRE(ptr == nullptr);

    // Failure should also occur for reasonably large requests
    ptr = alloc.allocate(SIZE_MAX / (sizeof(int) * 2));
    REQUIRE(ptr == nullptr);

    DoSTLContainerAllocatorTest(alloc);
}

template <typename Allocator>
static void DoFailfastAllocatorTest(Allocator&& alloc)
{
    // Equality tests
    REQUIRE(alloc == alloc);
    REQUIRE_FALSE(alloc != alloc);
    REQUIRE(alloc == Allocator(alloc));
    REQUIRE_FALSE(alloc != Allocator(alloc));

    auto ptr = alloc.allocate(42);
    REQUIRE(ptr != nullptr);
    alloc.deallocate(ptr, 42);

    // Through allocator traits
    ptr = wil::allocator_traits<wistd::remove_reference_t<Allocator>>::allocate(alloc, 10);
    REQUIRE(ptr != nullptr);
    wil::allocator_traits<wistd::remove_reference_t<Allocator>>::deallocate(alloc, ptr, 10);

    // Failure
    REQUIRE_ERROR(alloc.allocate(SIZE_MAX));

    // Failure should also occur for reasonably large requests
    REQUIRE_ERROR(alloc.allocate(SIZE_MAX / (sizeof(int) * 2)));

    DoSTLContainerAllocatorTest(alloc);
}

template <template <typename, typename> typename Allocator>
static void DoAllocatorTests()
{
#ifdef WIL_ENABLE_EXCEPTIONS
    SECTION("Throwing")
    {
        DoThrowingAllocatorTest(Allocator<int, wil::err_exception_policy>{});
    }
#endif

    SECTION("Nothrow")
    {
        DoNothrowAllocatorTest(Allocator<int, wil::err_returncode_policy>{});
    }

    SECTION("Failfast")
    {
        DoFailfastAllocatorTest(Allocator<int, wil::err_failfast_policy>{});
    }
}

#ifdef WIL_ENABLE_EXCEPTIONS
TEST_CASE("AllocatorTests::NewDeleteAllocator")
{
    DoAllocatorTests<wil::new_delete_allocator_t>();
}
#endif

TEST_CASE("AllocatorTests::CoTaskMemAllocator")
{
    DoAllocatorTests<wil::cotaskmem_allocator_t>();
}

TEST_CASE("AllocatorTests::ProcessHeapAllocator")
{
    DoAllocatorTests<wil::process_heap_allocator_t>();
}

TEST_CASE("AllocatorTests::HeapAllocator")
{
    auto checkHeapEmpty = [](HANDLE heap) {
        HEAP_SUMMARY summary{sizeof(summary)};
        REQUIRE(::HeapSummary(heap, 0, &summary));
        REQUIRE(summary.cbAllocated == 0);
    };

    auto doTest = [](HANDLE heap) {
#ifdef WIL_ENABLE_EXCEPTIONS
        SECTION("Throwing")
        {
            wil::heap_allocator<int> alloc(heap);
            DoThrowingAllocatorTest(alloc);
        }
#endif

        SECTION("Nothrow")
        {
            wil::heap_allocator_nothrow<int> alloc(heap);
            DoNothrowAllocatorTest(alloc);
        }

        SECTION("Failfast")
        {
            wil::heap_allocator_failfast<int> alloc(heap);
            DoFailfastAllocatorTest(alloc);
        }
    };

    // We need to construct with a heap and therefore cannot use 'DoAllocatorTests' directly
    SECTION("Process heap")
    {
        doTest(::GetProcessHeap());
    }

    SECTION("Custom heap")
    {
        wil::unique_hheap heap(::HeapCreate(0, 0, 0x10000));
        doTest(heap.get());
        checkHeapEmpty(heap.get());
    }

#ifdef WIL_ENABLE_EXCEPTIONS
    SECTION("Different heap move assignment")
    {
        wil::unique_hheap heap1(::HeapCreate(0, 0, 0x10000));
        wil::heap_allocator<int> alloc1(heap1.get());
        wil::unique_hheap heap2(::HeapCreate(0, 0, 0x10000));
        wil::heap_allocator<int> alloc2(heap2.get());

        // Different heaps; should not compare equal
        REQUIRE_FALSE(alloc1 == alloc2);
        REQUIRE(alloc1 != alloc2);

        {
            std::vector<int, wil::heap_allocator<int>> v1(alloc1);
            std::vector<int, wil::heap_allocator<int>> v2(alloc2);

            for (int i = 0; i < 42; ++i)
            {
                v1.push_back(i);
            }

            // Move assignment should effectively be a copy since the underlying heap is not the same
            auto ptr = v1.data();
            v2 = std::move(v1);
            REQUIRE(v2.get_allocator() == alloc2); // Should not have changed
            REQUIRE(ptr != v2.data()); // Needs new allocation

            // NOTE: v1 is in a valid but unspecified state, so we can't use it for comparison
            REQUIRE(v2.size() == 42);
            for (int i = 0; i < 42; ++i)
            {
                REQUIRE(v2[i] == i);
            }

            // NOTE: Swapping containers with a non-equal allocator is UB when 'propagate_on_container_swap' is false
        }

        checkHeapEmpty(heap1.get());
        checkHeapEmpty(heap2.get());
    }
#endif
}

TEST_CASE("AllocatorTests::VirtualAllocator")
{
    DoAllocatorTests<wil::virtual_allocator_t>();
}

TEST_CASE("AllocatorTests::LocalAllocator")
{
    DoAllocatorTests<wil::local_allocator_t>();
}

TEST_CASE("AllocatorTests::GlobalAllocator")
{
    DoAllocatorTests<wil::global_allocator_t>();
}
