//*********************************************************
//
//    Copyright (c) Microsoft. All rights reserved.
//    This code is licensed under the MIT License.
//    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF
//    ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
//    TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
//    PARTICULAR PURPOSE AND NONINFRINGEMENT.
//
//*********************************************************
//! @file
//! Various types and helpers for interfacing with various Win32 APIs
#ifndef __WIL_PROCESS_HELPERS_INCLUDED
#define __WIL_PROCESS_HELPERS_INCLUDED


#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)

#if (__WI_LIBCPP_STD_VER >= 17) && WI_HAS_INCLUDE(<string_view>, 1) // Assume present if C++17
#if defined(WIL_ENABLE_EXCEPTIONS)

#if __cpp_lib_string_view >= 201606L

#include <wil/stl.h>

#include <string>
#include <TlHelp32.h>

namespace wil
{
    // Process enumeration
    namespace details
    {
        // An iterator that can be used to enumerate processes in the system.
        // Usage:
        // for (auto process : process_iterator(L"ProcessName.exe"))
        // {
        //     // Do something with process, e.g.
        //     // wil::unique_handle hProcess(::OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, process.th32ProcessID));
        // }
        struct process_entry
        {
            DWORD ProcessID{};
            ULONG_PTR ThreadCount{};
            DWORD ParentProcessID{};
            LONG BasePriority{};
            std::wstring ProcessName;

            process_entry(wil::zwstring_view processName = L"") :
                m_processName(processName.c_str()),
                m_hSnapshot(::CreateToolhelp32Snapshot(TH32CS_SNAPALL, 0))
            {
                if (m_hSnapshot)
                {
                    PROCESSENTRY32W processEntry{};
                    processEntry.dwSize = sizeof(processEntry);
                    BOOL const success = ::Process32FirstW(m_hSnapshot.get(), &processEntry);

                    if (!success)
                    {
                        m_hSnapshot.reset();
                        return;
                    }
                    while (is_valid() && !matches_name(processEntry.szExeFile))
                    {
                        // Skip until we find the first process that matches the name.
                        move_next(&processEntry);
                    }

                    if (is_valid())
                    {
                        copy_data(&processEntry);
                    }
                    else
                    {
                        copy_data(nullptr);
                    }
                }
            }

            process_entry(process_entry const&) = default;
            process_entry(process_entry&&) = default;

            struct end
            {
            };

            auto operator*() const
            {
                return *this;
            }

            process_entry& operator++()
            {
                bool found = false;
                PROCESSENTRY32W processEntry{};
                processEntry.dwSize = sizeof(processEntry);
                while (!found)
                {
                    move_next(&processEntry);
                    if (!m_hSnapshot)
                    {
                        return *this;
                    }
                    if (!matches_name(processEntry.szExeFile))
                    {
                        continue;
                    }
                    else
                    {
                        found = true;
                        copy_data(&processEntry);
                    }
                }

                return *this;
            }

            void move_next(PROCESSENTRY32W* processEntry)
            {
                BOOL const success = ::Process32NextW(m_hSnapshot.get(), processEntry);
                if (!success)
                {
                    m_hSnapshot.reset();
                    copy_data(nullptr);
                    return;
                }
            }

            bool is_valid() const
            {
                return m_hSnapshot != nullptr;
            }

            bool operator==(process_entry const& other) const
            {
                return m_hSnapshot == other.m_hSnapshot && ProcessID == other.ProcessID;
            }

            bool operator==(end const&) const
            {
                return !is_valid();
            }

            bool operator!=(process_entry const& other) const
            {
                return !(*this == other);
            }

        private:
            std::wstring m_processName;
            ::wil::shared_handle m_hSnapshot;

            bool matches_name(wil::zwstring_view processName) const
            {
                return m_processName.empty() || (_wcsicmp(processName.data(), m_processName.data()) == 0);
            }

            void copy_data(PROCESSENTRY32W const* processEntry)
            {
                if (!processEntry)
                {
                    ProcessID = 0;
                    ThreadCount = 0;
                    ParentProcessID = 0;
                    BasePriority = 0;
                    ProcessName.clear();
                    return;
                }
                ProcessID = processEntry->th32ProcessID;
                ThreadCount = processEntry->cntThreads;
                ParentProcessID = processEntry->th32ParentProcessID;
                BasePriority = processEntry->pcPriClassBase;
                ProcessName = processEntry->szExeFile;
            }
        };
    } // namespace details

    struct process_iterator
    {
        explicit process_iterator(wil::zwstring_view processName = L"") : m_processEntry(processName)
        {
        }

        ::wil::details::process_entry& begin()
        {
            return m_processEntry;
        }

        ::wil::details::process_entry::end end() const
        {
            return ::wil::details::process_entry::end{};
        }

        process_iterator(process_iterator const&) = default;
        process_iterator(process_iterator&&) = default;

    private:
        ::wil::details::process_entry m_processEntry{};
    };
} // namespace wil

#endif // __cpp_lib_string_view >= 201606L
#endif // WIL_ENABLE_EXCEPTIONS
#endif // (__WI_LIBCPP_STD_VER >= 17) && WI_HAS_INCLUDE(<string_view>, 1)
#endif // WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)

#endif // __WIL_PROCESS_HELPERS_INCLUDED