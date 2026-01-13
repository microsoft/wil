#include "common.h"
#include <wil/windowing.h>

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
TEST_CASE("EnumWindows", "[windowing]")
{
    // lambda can return a bool
    wil::for_each_window_nothrow([](HWND hwnd) {
        REQUIRE(IsWindow(hwnd));
        return true;
    });

    // or not return anything (we'll stop if we get an exception)
    wil::for_each_window_nothrow([](HWND hwnd) {
        REQUIRE(IsWindow(hwnd));
    });

    // or return an HRESULT and we'll stop if it's not S_OK
    wil::for_each_window_nothrow([](HWND hwnd) {
        REQUIRE(IsWindow(hwnd));
        return S_FALSE;
    });

    // mutable lambda
    std::vector<HWND> windows;
    wil::for_each_window_nothrow([&windows](HWND hwnd) {
        windows.push_back(hwnd);
    });
    wil::for_each_window_nothrow([windows = std::vector<HWND>{}](HWND hwnd) mutable {
        windows.push_back(hwnd);
    });

    // With captures
    const auto pid = GetCurrentProcessId();
    wil::for_each_window_nothrow([pid](HWND hwnd) {
        if (pid == GetWindowThreadProcessId(hwnd, nullptr))
        {
            REQUIRE(IsWindow(hwnd));
        };
        return true;
    });

#ifdef WIL_ENABLE_EXCEPTIONS
    // throwing version
    wil::for_each_window([](HWND hwnd) {
        REQUIRE(IsWindow(hwnd));
        return true;
    });
    wil::for_each_window([](HWND hwnd) {
        REQUIRE(IsWindow(hwnd));
    });
    wil::for_each_window([](HWND hwnd) {
        REQUIRE(IsWindow(hwnd));
        return S_FALSE;
    });
    windows.clear();
    wil::for_each_window([&windows](HWND hwnd) {
        windows.push_back(hwnd);
    });
    wil::for_each_window([windows = std::vector<HWND>{}](HWND hwnd) mutable {
        windows.push_back(hwnd);
    });
    REQUIRE_THROWS_AS(
        wil::for_each_window([](HWND) {
            throw std::exception();
        }),
        std::exception);
#endif
}

TEST_CASE("EnumThreadWindows", "[windowing]")
{
    // find any window
    DWORD thread_id{};
    wil::for_each_window_nothrow([&thread_id](HWND hwnd) {
        if (IsWindow(hwnd) && IsWindowVisible(hwnd))
        {
            DWORD pid;
            thread_id = ::GetWindowThreadProcessId(hwnd, &pid);

            // Ideally, the window handle will be from a long lived process like Explorer so that it doesn't get
            // destroyed - or perhaps more accurately so that the thread doesn't terminate - before we're done with this
            // test.
            wil::unique_handle proc{::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid)};
            if (proc)
            {
                wchar_t processName[MAX_PATH];
                DWORD size = MAX_PATH;
                if (::QueryFullProcessImageNameW(proc.get(), 0, processName, &size))
                {
                    auto len = ::wcslen(processName);
                    if ((len >= 13) && (::wcscmp(processName + len - 13, L"\\explorer.exe") == 0))
                    {
                        // Assume long lived process - stop searching
                        return false;
                    }
                }
            }

            // Not Explorer - we'll save the thread id, but continue iteration to try and find a better handle
            return true;
        }
        return true;
    });

    // nothrow version
    {
        wil::for_each_thread_window_nothrow(thread_id, [](HWND hwnd) {
            REQUIRE(IsWindow(hwnd));
            return true;
        });

        wil::for_each_thread_window_nothrow(thread_id, [](HWND hwnd) {
            REQUIRE(IsWindow(hwnd));
        });

        // lambda can also return an HRESULT and we'll stop if it's not S_OK
        wil::for_each_thread_window_nothrow(thread_id, [](HWND hwnd) {
            REQUIRE(IsWindow(hwnd));
            return S_FALSE;
        });

        // mutable lambda
        std::vector<HWND> windows;
        wil::for_each_thread_window_nothrow(thread_id, [&windows, thread_id](HWND hwnd) {
            REQUIRE(GetWindowThreadProcessId(hwnd, nullptr) == thread_id);
            windows.push_back(hwnd);
        });
        wil::for_each_thread_window_nothrow(thread_id, [windows = std::vector<HWND>{}](HWND hwnd) mutable {
            windows.push_back(hwnd);
        });
    }

#ifdef WIL_ENABLE_EXCEPTIONS
    // throwing version
    {
        wil::for_each_thread_window(thread_id, [](HWND hwnd) {
            REQUIRE(IsWindow(hwnd));
            return true;
        });

        wil::for_each_thread_window(thread_id, [](HWND hwnd) {
            REQUIRE(IsWindow(hwnd));
        });

        // lambda can also return an HRESULT and we'll stop if it's not S_OK
        wil::for_each_thread_window(thread_id, [](HWND hwnd) {
            REQUIRE(IsWindow(hwnd));
            return S_FALSE;
        });

        // mutable lambda
        std::vector<HWND> windows;
        wil::for_each_thread_window(thread_id, [&windows, thread_id](HWND hwnd) {
            REQUIRE(GetWindowThreadProcessId(hwnd, nullptr) == thread_id);
            windows.push_back(hwnd);
        });
        wil::for_each_thread_window(thread_id, [windows = std::vector<HWND>{}](HWND hwnd) mutable {
            windows.push_back(hwnd);
        });

        // with exceptions
        REQUIRE_THROWS_AS(
            wil::for_each_thread_window(
                thread_id,
                [](HWND) {
                    throw std::exception();
                }),
            std::exception);
    }
#endif
}

static bool has_child_window(HWND hwnd)
{
    bool hasChildWindow = false;
    wil::for_each_child_window_nothrow(hwnd, [&hasChildWindow](HWND) {
        hasChildWindow = true;
        return false;
    });
    return hasChildWindow;
}

TEST_CASE("EnumChildWindows", "[windowing]")
{
    // find any window
    HWND parent{};

    wil::for_each_window_nothrow([&parent](HWND hwnd) {
        if (IsWindow(hwnd) && IsWindowVisible(hwnd) && has_child_window(hwnd))
        {
            // Make sure we choose a window that has children
            bool hasChildren = false;
            wil::for_each_child_window_nothrow(hwnd, [&](HWND) {
                hasChildren = true;
                return false;
            });

            if (hasChildren)
            {
                parent = hwnd;
                return false;
            }
        }
        return true;
    });

    // Avoid confusing issues below
    REQUIRE(parent != nullptr);

    // nothrow version
    {
        wil::for_each_child_window_nothrow(parent, [](HWND hwnd) {
            REQUIRE(IsWindow(hwnd));
            return true;
        });

        wil::for_each_child_window_nothrow(parent, [](HWND hwnd) {
            REQUIRE(IsWindow(hwnd));
        });

        // lambda can also return an HRESULT and we'll stop if it's not S_OK
        wil::for_each_child_window_nothrow(parent, [](HWND hwnd) {
            REQUIRE(IsWindow(hwnd));
            return S_FALSE;
        });

        // mutable lambda
        std::vector<HWND> windows;
        wil::for_each_child_window_nothrow(parent, [&windows](HWND hwnd) {
            windows.push_back(hwnd);
        });
        wil::for_each_child_window_nothrow(parent, [windows = std::vector<HWND>{}](HWND hwnd) mutable {
            windows.push_back(hwnd);
        });
    }

#ifdef WIL_ENABLE_EXCEPTIONS
    // throwing version
    {
        wil::for_each_child_window(parent, [](HWND hwnd) {
            REQUIRE(IsWindow(hwnd));
            return true;
        });

        wil::for_each_child_window(parent, [](HWND hwnd) {
            REQUIRE(IsWindow(hwnd));
        });

        // lambda can also return an HRESULT and we'll stop if it's not S_OK
        wil::for_each_child_window(parent, [](HWND hwnd) {
            REQUIRE(IsWindow(hwnd));
            return S_FALSE;
        });

        // mutable lambda
        std::vector<HWND> windows;
        wil::for_each_child_window(parent, [&windows](HWND hwnd) {
            windows.push_back(hwnd);
        });
        REQUIRE(!windows.empty());
        wil::for_each_child_window(parent, [windows = std::vector<HWND>{}](HWND hwnd) mutable {
            windows.push_back(hwnd);
        });

        // with exceptions
        REQUIRE_THROWS_AS(
            wil::for_each_child_window(
                parent,
                [](HWND) {
                    throw std::exception();
                }),
            std::exception);
    }
#endif
}

#endif