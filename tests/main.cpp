#include "pch.h"

#pragma comment(lib, "Pathcch.lib")
#pragma comment(lib, "RuntimeObject.lib")
#pragma comment(lib, "Synchronization.lib")
#pragma comment(lib, "RpcRt4.lib")

#include <catch2/catch_session.hpp>
#include <cinttypes>

#ifdef __MINGW32__
#include <windows.h>
#else
#include <Windows.h>
#endif

#include <DbgHelp.h> // NOTE: Must be included after Windows.h
#include <TlHelp32.h>
#include <wil/resource.h>
#include <wil/win32_helpers.h>

#define POINTER_STRING_SIZE (int)(sizeof(void*) * 2)

static LONG __stdcall on_crash(EXCEPTION_POINTERS* exPtrs);

static void __stdcall timer_callback(PTP_CALLBACK_INSTANCE, void*, PTP_TIMER);
static void print_all_stacks();

int main(int argc, char* argv[])
{
    // We want to print out information such as the callstack on unhandled exceptions
    ::SetUnhandledExceptionFilter(on_crash);

    // Even on CI machines, test execution rarely lasts longer than one minute for all tests in a particular configuration.
    // If any test hangs, it should do so long before a minute has elapsed. We set a timer here for ten minutes, which
    // should be more than enough time for all tests to complete, and if the timer triggers, we enumerate all threads in
    // this process, printing out their callstacks to help diagnose the issue.
    wil::unique_threadpool_timer timer(::CreateThreadpoolTimer(timer_callback, nullptr, nullptr));
    if (!timer)
    {
        std::printf("Failed to create timer\n");
        return -1;
    }

    auto duration = wil::filetime::from_int64(static_cast<unsigned long long>(wil::filetime_duration::one_minute * -10));
    ::SetThreadpoolTimer(timer.get(), &duration, 0, 0);

    return Catch::Session().run(argc, argv);
}

static void __stdcall timer_callback(PTP_CALLBACK_INSTANCE, void*, PTP_TIMER)
{
    std::printf("Possible test hang detected; printing callstacks for all threads\n\n");
    print_all_stacks();
    std::fflush(stdout); // Seems necessary for CI machines to capture the output

    ::ExitProcess(42); // Easy to identify code
}

// NOLINTBEGIN(performance-no-int-to-ptr): Debugging APIs represent pointers as integers

static void print_exception_record(EXCEPTION_RECORD* exr)
{
    std::printf("ExceptionAddress: %p\n", exr->ExceptionAddress);
    std::printf("   ExceptionCode: %lX\n", exr->ExceptionCode);
    std::printf("  ExceptionFlags: %lX\n", exr->ExceptionFlags);
    std::printf("NumberParameters: %lu\n", exr->NumberParameters);
    for (DWORD i = 0; i < exr->NumberParameters; ++i)
    {
        std::printf("%*sParameter[%lu]: %p\n", (i < 10) ? 4 : 3, "", i, (void*)exr->ExceptionInformation[i]);
    }
    std::printf("\n");
}

static void print_callstack(HANDLE thread, const CONTEXT* ctx)
{
    std::size_t modulePathLen = MAX_PATH;
    auto modulePath = std::make_unique<wchar_t[]>(modulePathLen);

    // As of now, we can't rely on having C++23's basic_stacktrace so we get to do this the old fashioned way...
    auto proc = ::GetCurrentProcess();

    if (!::SymInitializeW(proc, nullptr, TRUE))
    {
        return;
    }

    auto makeAddr = [](DWORD64 addr) {
        return ADDRESS64{addr, 0, AddrModeFlat};
    };

    auto record = *ctx;
    STACKFRAME64 frame = {};
#ifdef _M_IX86
    DWORD imgType = IMAGE_FILE_MACHINE_I386;
    frame.AddrPC = makeAddr(record.Eip);
    frame.AddrFrame = makeAddr(record.Ebp);
    frame.AddrStack = makeAddr(record.Esp);
#elif defined(_M_X64)
    DWORD imgType = IMAGE_FILE_MACHINE_AMD64;
    frame.AddrPC = makeAddr(record.Rip);
    frame.AddrFrame = makeAddr(record.Rbp);
    frame.AddrStack = makeAddr(record.Rsp);
#elif defined(_M_ARM64)
    DWORD imgType = IMAGE_FILE_MACHINE_ARM64;
    frame.AddrPC = makeAddr(record.Pc);
    frame.AddrFrame = makeAddr(record.Fp);
    frame.AddrStack = makeAddr(record.Sp);
#else
#error Unsupported architecture
#endif

    auto symBuffer = std::make_unique<std::byte[]>(sizeof(SYMBOL_INFOW) + MAX_SYM_NAME * sizeof(wchar_t));
    auto symInfo = reinterpret_cast<SYMBOL_INFOW*>(symBuffer.get());
    symInfo->SizeOfStruct = sizeof(*symInfo);
    symInfo->MaxNameLen = MAX_SYM_NAME;

    IMAGEHLP_LINEW64 line = {sizeof(line)};

    std::printf("Callstack:\n");
    std::printf(" # %-*s %-*s Call Site\n", POINTER_STRING_SIZE, "Child-SP", POINTER_STRING_SIZE, "RetAddr");
    std::size_t frameNum = 0;
    while (::StackWalk64(imgType, proc, thread, &frame, &record, nullptr, nullptr, nullptr, nullptr))
    {
        std::wstring_view moduleName;
        HMODULE mod = nullptr;
        if (::GetModuleHandleExW(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)frame.AddrPC.Offset, &mod))
        {
            // TODO: Allow expanding the size of 'modulePath'
            if (::GetModuleFileNameW(mod, modulePath.get(), static_cast<DWORD>(modulePathLen)))
            {
                // Just want the filename part
                moduleName = modulePath.get();
                if (auto pos = moduleName.find_last_of(L'\\'); pos != std::wstring_view::npos)
                {
                    moduleName = moduleName.substr(pos + 1);
                }

                // Remove the extension
                if (auto pos = moduleName.find_last_of(L'.'); pos != std::wstring_view::npos)
                {
                    moduleName = moduleName.substr(0, pos);
                }
            }
        }
        if (moduleName.empty())
        {
            moduleName = L"<unknown>";
        }

        std::printf(
            "%02zX %p %p %.*ls!",
            frameNum++,
            (void*)frame.AddrStack.Offset,
            (void*)frame.AddrReturn.Offset,
            (int)moduleName.size(),
            moduleName.data());

        // Try and get source information
        DWORD64 displacement64;
        if (::SymFromAddrW(proc, frame.AddrPC.Offset, &displacement64, symInfo))
        {
            std::printf("%.*ls+0x%llX", (int)symInfo->NameLen, symInfo->Name, displacement64);

            // See if we can get the source+line information
            DWORD displacement;
            if (::SymGetLineFromAddrW64(proc, frame.AddrPC.Offset, &displacement, &line))
            {
                std::printf(" [%ls @ %lu]", line.FileName, line.LineNumber);
            }
            else if (moduleName != L"<unknown>")
            {
                // When no source is available, show module+offset as the RVA is more useful with internal symbols and
                // this syntax can be more easily used with WinDBG to find the source+line
                std::printf(" [%.*ls+%p]", (int)moduleName.size(), moduleName.data(), (void*)((BYTE*)frame.AddrPC.Offset - (BYTE*)mod));
            }
            std::printf("\n");
        }
        else
        {
            std::printf("%p\n", (void*)frame.AddrPC.Offset);
        }
    }
    std::printf("\n");

    ::SymCleanup(proc);
}

static LONG __stdcall on_crash(EXCEPTION_POINTERS* exPtrs)
{
    std::printf("Unhandled exception thrown during test execution\n\n");

    print_exception_record(exPtrs->ExceptionRecord);

    // TODO: Print registers? That's unlikely to be super useful in contexts without binaries/symbols, however in such situations
    // a debugger should be available

    print_callstack(::GetCurrentThread(), exPtrs->ContextRecord);

    if (exPtrs->ExceptionRecord->ExceptionRecord)
    {
        std::printf("Nested exceptions:\n\n");
        for (auto rec = exPtrs->ExceptionRecord->ExceptionRecord; rec; rec = rec->ExceptionRecord)
        {
            print_exception_record(exPtrs->ExceptionRecord->ExceptionRecord);
        }
    }

    std::fflush(stdout); // Seems necessary for CI machines to capture the output
    return EXCEPTION_CONTINUE_SEARCH;
}

static void print_all_stacks()
{
    auto pid = ::GetCurrentProcessId();
    auto thisThreadId = ::GetCurrentThreadId();

    wil::unique_handle snapshot(::CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0));
    if (!snapshot)
    {
        return;
    }

    THREADENTRY32 entry = {sizeof(entry)};
    if (::Thread32First(snapshot.get(), &entry))
    {
        do
        {
            if (entry.th32OwnerProcessID != pid)
            {
                continue;
            }

            // Skip current thread (since suspending it would deadlock)
            if (entry.th32ThreadID == thisThreadId)
            {
                continue;
            }

            std::printf("Callstack for thread %lu:\n\n", entry.th32ThreadID);

            wil::unique_handle thread(::OpenThread(THREAD_ALL_ACCESS, FALSE, entry.th32ThreadID));
            if (thread)
            {
                // Suspend, get context, print stack, resume
                if (::SuspendThread(thread.get()) == static_cast<DWORD>(-1))
                {
                    std::printf("ERROR: Failed to suspend the thread (%lu)\n\n", ::GetLastError());
                    continue;
                }

                auto resume = wil::scope_exit([&]() {
                    ::ResumeThread(thread.get());
                });

                CONTEXT ctx = {};
                ctx.ContextFlags = CONTEXT_FULL;

                if (::GetThreadContext(thread.get(), &ctx))
                {
                    print_callstack(thread.get(), &ctx);
                }
                else
                {
                    std::printf("ERROR: Failed to get thread context (%lu)\n\n", ::GetLastError());
                }
            }
        } while (::Thread32Next(snapshot.get(), &entry));
    }
}

// NOLINTEND(performance-no-int-to-ptr)

extern "C" __declspec(dllexport) const char* __asan_default_options()
{
    return
        // Tests validate OOM, so this is expected
        "allocator_may_return_null=1"
        // Some structs in Windows have dynamic size where we over-allocate for extra data past the end
        ":new_delete_type_mismatch=0";
}
