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
// @file
// Helpers for using BSD sockets and Winsock functions and structures.
// Does not require the use of the STL or C++ exceptions (see _nothrow functions)
// ReSharper disable CppRedundantQualifier
#ifndef __WIL_NETWORK_INCLUDED
#define __WIL_NETWORK_INCLUDED

#ifdef _KERNEL_MODE
#error This header is not supported in kernel-mode.
#endif

// define WIN32_LEAN_AND_MEAN at the project level to avoid windows.h including older winsock 1.1 headers from winsock.h
// as including both the Winsock 1.1 header 'winsock.h' and the Winsock 2 header 'winsock2.h' will create compiler errors
// alternatively, including winsock2.h before windows.h to prevent inclusion of the Winsock 1.1 winsock.h header from within
// windows.h note: winsock2.h will include windows.h if not already included
//
// define _SECURE_SOCKET_TYPES_DEFINED_ at the project level to have access to SocketSecurity* functions in ws2tcpip.h
//
// define INCL_WINSOCK_API_TYPEDEFS at the project level to make function typedefs available across various networking headers
// note, winsock2.h defaults is to not include function typedefs - but these can be necessary when supporting multiple OS versions
//
// Link libs for functions referenced in this file: ws2_32.lib, ntdll.lib, and Fwpuclnt.lib (for secure socket functions)

#if !defined(_WINSOCK2API_) && defined(_WINSOCKAPI_)
#error The Winsock 1.1 winsock.h header was included before the Winsock 2 winsock2.h header - this will cause compilation errors - define WIN32_LEAN_AND_MEAN to avoid winsock.h included by windows.h, or include winsock2.h before windows.h
#endif

// Including Winsock and networking headers in the below specific sequence
// These headers have many intra-header dependencies, creating difficulties when needing access to various functions and types
// This specific sequence should compile correctly to give access to all available functions and types
#include <utility>
#include <winsock2.h>
#include <ws2def.h>
#include <ws2ipdef.h>
#include <mswsock.h>
#include <mstcpip.h>
#include <ws2tcpip.h>
#include <windns.h>
#include <iphlpapi.h>

// wil headers
#include "wistd_type_traits.h"
#include "resource.h"

// Callers who are using the STL by pulling in other STL headers
// might not have included <string>, which is required in network.h when WIL_USE_STL is defined
// ensuring it's included so callers don't have to include <string> just for network.h
#if (WIL_USE_STL && defined(WIL_ENABLE_EXCEPTIONS)) || defined(WIL_DOXYGEN)
#include <string>
#endif

namespace wil
{
// Functions and classes that support networking operations and structures
namespace network
{
    // A type that calls WSACleanup on destruction (or reset()).
    // WSAStartup must be called for the lifetime of all Winsock APIs (synchronous and asynchronous)
    // WSACleanup will unload the full Winsock catalog - all the libraries - with the final reference
    // which can lead to crashes if socket APIs are still being used after the final WSACleanup is called
    using unique_wsacleanup_call = ::wil::unique_call<decltype(&::WSACleanup), ::WSACleanup>;

    // Calls WSAStartup; returns an RAII object that reverts, the RAII object will resolve to bool 'false' if failed
    WI_NODISCARD inline ::wil::network::unique_wsacleanup_call WSAStartup_nothrow() WI_NOEXCEPT
    {
        WSADATA unused_data;
        const auto error{::WSAStartup(WINSOCK_VERSION, &unused_data)};
        LOG_IF_WIN32_ERROR(error);
        return unique_wsacleanup_call{error == 0};
    }

    // Calls WSAStartup and fail-fasts on error; returns an RAII object that reverts
    WI_NODISCARD inline ::wil::network::unique_wsacleanup_call WSAStartup_failfast() WI_NOEXCEPT
    {
        WSADATA unused_data;
        FAIL_FAST_IF_WIN32_ERROR(::WSAStartup(WINSOCK_VERSION, &unused_data));
        return {};
    }

#if defined(WIL_ENABLE_EXCEPTIONS)
    // Calls WSAStartup and throws on error; returns an RAII object that reverts
    WI_NODISCARD inline ::wil::network::unique_wsacleanup_call WSAStartup()
    {
        WSADATA unused_data;
        THROW_IF_WIN32_ERROR(::WSAStartup(WINSOCK_VERSION, &unused_data));
        return {};
    }
#endif

    //
    // utility functions to compare inaddr types
    //
    WI_NODISCARD inline bool equals(const ::in_addr& lhs, const ::in_addr& rhs) WI_NOEXCEPT
    {
        return lhs.s_addr == rhs.s_addr;
    }

    WI_NODISCARD inline bool not_equals(const ::in_addr& lhs, const ::in_addr& rhs) WI_NOEXCEPT
    {
        return !::wil::network::equals(lhs, rhs);
    }

    WI_NODISCARD inline bool equals(const ::in6_addr& lhs, const ::in6_addr& rhs) WI_NOEXCEPT
    {
        return 0 == ::memcmp(&lhs, &rhs, sizeof(::in6_addr));
    }

    WI_NODISCARD inline bool not_equals(const ::in6_addr& lhs, const ::in6_addr& rhs) WI_NOEXCEPT
    {
        return !::wil::network::equals(lhs, rhs);
    }

    //
    // encapsulates working with the sockaddr datatype
    //
    // sockaddr is a generic type - similar to a base class, but designed for C with BSD sockets (1983-ish)
    // 'derived' structures are cast back to sockaddr* (so the initial struct members must be aligned)
    //
    // this data type was built to be 'extensible' so new network types could create their own address structures
    // - appending fields to the initial fields of the sockaddr
    //
    // note that the address and port fields of TCPIP sockaddr* types were designed to be encoded in network-byte order
    // - hence the common use of "host-to-network" and "network-to-host" APIs, e.g. htons(), htonl(), ntohs(), ntohl()
    //
    // Socket APIs that accept a socket address will accept 2 fields:
    // - the sockaddr* (the address of the derived sockaddr type, cast down to a sockaddr*)
    // - the length of the 'derived' socket address structure referenced by the sockaddr*
    //
    // Commonly used sockaddr* types that are using with TCPIP networking:
    //
    // sockaddr_storage / SOCKADDR_STORAGE
    //   - a sockaddr* derived type that is guaranteed to be large enough to hold any possible socket address (not just TCPIP related)
    // sockaddr_in / SOCKADDR_IN
    //   - a sockaddr* derived type designed to contain an IPv4 address and port number
    // sockaddr_in6 / SOCKADDR_IN6
    //   - a sockaddr* derived type designed to contain an IPv6 address, port, scope id, and flow info
    // SOCKADDR_INET
    //   - a union of sockaddr_in and sockaddr_in6 -- i.e., large enough to contain any TCPIP IPv4 or IPV6 address
    // in_addr / IN_ADDR
    //   - the raw address portion of a sockaddr_in
    // in6_addr / IN6_ADDR
    //   - the raw address portion of a sockaddr_in6
    //
    // SOCKET_ADDRESS
    //   - not a derived sockaddr* type
    //   - a structure containing both a sockaddr* and its length fields, returned from some networking functions
    //

    // declaring char-arrays large enough for any IPv4 or IPv6 address + optional fields
    static_assert(INET6_ADDRSTRLEN > INET_ADDRSTRLEN);
    using socket_address_wstring = WCHAR[INET6_ADDRSTRLEN];
    using socket_address_string = CHAR[INET6_ADDRSTRLEN];

    class socket_address final
    {
    public:
        constexpr socket_address() WI_NOEXCEPT = default;
        explicit socket_address(ADDRESS_FAMILY family) WI_NOEXCEPT;
        explicit socket_address(_In_reads_bytes_(addr_size) const SOCKADDR* addr, size_t addr_size) WI_NOEXCEPT;
        explicit socket_address(const SOCKADDR_IN* addr) WI_NOEXCEPT;
        explicit socket_address(const SOCKADDR_IN6* addr) WI_NOEXCEPT;
        explicit socket_address(const SOCKADDR_INET* addr) WI_NOEXCEPT;
        explicit socket_address(const SOCKET_ADDRESS* addr) WI_NOEXCEPT;
        explicit socket_address(const IN_ADDR* addr, unsigned short port = 0) WI_NOEXCEPT;
        explicit socket_address(const IN6_ADDR* addr, unsigned short port = 0) WI_NOEXCEPT;
#if defined(WIL_ENABLE_EXCEPTIONS)
        explicit socket_address(PCWSTR addr, unsigned short port = 0);
#endif

        bool operator==(const socket_address& rhs) const WI_NOEXCEPT;
        bool operator!=(const socket_address& rhs) const WI_NOEXCEPT;
        bool operator<(const socket_address& rhs) const WI_NOEXCEPT;
        bool operator>(const socket_address& rhs) const WI_NOEXCEPT;

        // returns -1, 0, 1, following memcmp
        int compare(const socket_address& rhs) const WI_NOEXCEPT;
        void swap(socket_address&) WI_NOEXCEPT;

        // reset overwrites the entire sockaddr in the object (including address family)
        void reset() WI_NOEXCEPT;
        void reset(ADDRESS_FAMILY family) WI_NOEXCEPT;
        void reset(_In_reads_bytes_(addr_size) const SOCKADDR* addr, size_t addr_size) WI_NOEXCEPT;
        void reset(const SOCKADDR_IN* addr) WI_NOEXCEPT;
        void reset(const SOCKADDR_IN6* addr) WI_NOEXCEPT;
        void reset(const SOCKADDR_INET* addr) WI_NOEXCEPT;
        void reset(const SOCKET_ADDRESS* addr) WI_NOEXCEPT;

        void set_address_any() WI_NOEXCEPT;
        void set_address_any(ADDRESS_FAMILY family) WI_NOEXCEPT;
        void set_address_loopback() WI_NOEXCEPT;
        void set_address_loopback(ADDRESS_FAMILY family) WI_NOEXCEPT;
        void set_address(const IN_ADDR* addr) WI_NOEXCEPT;
        void set_address(const IN6_ADDR* addr) WI_NOEXCEPT;

#if defined(WIL_ENABLE_EXCEPTIONS)
        void reset_address(SOCKET s);
        void reset_address(PCWSTR address);
        void reset_address(PCSTR address);
#endif

        WI_NODISCARD HRESULT reset_address_nothrow(SOCKET s) WI_NOEXCEPT;
        WI_NODISCARD HRESULT reset_address_nothrow(PCWSTR address) WI_NOEXCEPT;
        WI_NODISCARD HRESULT reset_address_nothrow(PCSTR address) WI_NOEXCEPT;

        void set_port(USHORT port) WI_NOEXCEPT;
        void set_scope_id(ULONG scopeId) WI_NOEXCEPT;
        void set_flow_info(ULONG flowInfo) WI_NOEXCEPT;

        // format_address prints the IP address, not the scope id or port
        HRESULT format_address_nothrow(socket_address_wstring& address) const WI_NOEXCEPT;
        HRESULT format_address_nothrow(socket_address_string& address) const WI_NOEXCEPT;

        // format_complete_address_nothrow() prints the IP address as well as the scope id and port values
        HRESULT format_complete_address_nothrow(socket_address_wstring& address) const WI_NOEXCEPT;
        HRESULT format_complete_address_nothrow(socket_address_string& address) const WI_NOEXCEPT;

#if (WIL_USE_STL && defined(WIL_ENABLE_EXCEPTIONS)) || defined(WIL_DOXYGEN)
        WI_NODISCARD ::std::wstring format_address() const;
        WI_NODISCARD ::std::wstring format_complete_address() const;
#endif

        // type: NlatUnspecified ('any'), NlatUnicast, NlatAnycast, NlatMulticast, NlatBroadcast, NlatInvalid
        WI_NODISCARD NL_ADDRESS_TYPE address_type() const WI_NOEXCEPT;
        WI_NODISCARD bool is_address_linklocal() const WI_NOEXCEPT;
        WI_NODISCARD bool is_address_loopback() const WI_NOEXCEPT;

        WI_NODISCARD ADDRESS_FAMILY family() const WI_NOEXCEPT;
        WI_NODISCARD USHORT port() const WI_NOEXCEPT;
        WI_NODISCARD ULONG flow_info() const WI_NOEXCEPT;
        WI_NODISCARD ULONG scope_id() const WI_NOEXCEPT;

        WI_NODISCARD SOCKADDR* sockaddr() WI_NOEXCEPT;
        WI_NODISCARD SOCKADDR_IN* sockaddr_in() WI_NOEXCEPT;
        WI_NODISCARD SOCKADDR_IN6* sockaddr_in6() WI_NOEXCEPT;
        WI_NODISCARD SOCKADDR_INET* sockaddr_inet() WI_NOEXCEPT;
        WI_NODISCARD IN_ADDR* in_addr() WI_NOEXCEPT;
        WI_NODISCARD IN6_ADDR* in6_addr() WI_NOEXCEPT;

        WI_NODISCARD const SOCKADDR* sockaddr() const WI_NOEXCEPT;
        WI_NODISCARD const SOCKADDR_IN* sockaddr_in() const WI_NOEXCEPT;
        WI_NODISCARD const SOCKADDR_IN6* sockaddr_in6() const WI_NOEXCEPT;
        WI_NODISCARD const SOCKADDR_INET* sockaddr_inet() const WI_NOEXCEPT;
        WI_NODISCARD const IN_ADDR* in_addr() const WI_NOEXCEPT;
        WI_NODISCARD const IN6_ADDR* in6_addr() const WI_NOEXCEPT;

        WI_NODISCARD SOCKADDR_STORAGE sockaddr_storage() const WI_NOEXCEPT;

        WI_NODISCARD constexpr int size() const WI_NOEXCEPT
        {
            return sizeof(m_sockaddr);
        }

    private:
        SOCKADDR_INET m_sockaddr{};
    };

    // When using dual-mode sockets, one might need to connect to a target IPv4 address.
    // Since dual-mode socket types are IPv6, one must map that IPv4 address to its 'mapped' IPv6 address
    inline ::wil::network::socket_address map_dual_mode_4to6(const ::wil::network::socket_address& ipv4_address) WI_NOEXCEPT
    {
        constexpr IN6_ADDR ipv4MappedPrefix{{IN6ADDR_V4MAPPEDPREFIX_INIT}};

        ::wil::network::socket_address return_ipv6_address{&ipv4MappedPrefix, ipv4_address.port()};

        auto* const return_in6_addr{return_ipv6_address.in6_addr()};
        const auto* const ipv4_inaddr{ipv4_address.in_addr()};
        return_in6_addr->u.Byte[12] = ipv4_inaddr->S_un.S_un_b.s_b1;
        return_in6_addr->u.Byte[13] = ipv4_inaddr->S_un.S_un_b.s_b2;
        return_in6_addr->u.Byte[14] = ipv4_inaddr->S_un.S_un_b.s_b3;
        return_in6_addr->u.Byte[15] = ipv4_inaddr->S_un.S_un_b.s_b4;

        return return_ipv6_address;
    }

    // non-member swap
    inline void swap(::wil::network::socket_address& lhs, ::wil::network::socket_address& rhs) WI_NOEXCEPT
    {
        lhs.swap(rhs);
    }

    // class addr_info encapsulates the ADDRINFO-related structures returned from the socket functions
    // getaddrinfo, GetAddrInfoW, GetAddrInfoWEx
    // iterator semantics are supported to safely access these addresses
    // template T supports pointers to the 3 address structures: ADDRINFOA*, ADDRINFOW*, ADDRINFOEXW*
    template <typename T>
    class addr_info_iterator_t
    {
    public:
        // defining iterator_traits allows STL <algorithm> functions to be used with this iterator class.
        // Notice this is a forward_iterator
        // - does not support random-access (e.g. vector::iterator)
        // - does not support bidirectional access (e.g. list::iterator)
#if WIL_USE_STL || defined(WIL_DOXYGEN)
        using iterator_category = ::std::forward_iterator_tag;
#endif
        using value_type = ::wil::network::socket_address;
        using difference_type = size_t;
        using distance_type = ptrdiff_t;
        using pointer = const ::wil::network::socket_address*;
        using reference = const ::wil::network::socket_address&;

        constexpr addr_info_iterator_t() WI_NOEXCEPT = default;

        explicit addr_info_iterator_t(const T* addrinfo) WI_NOEXCEPT :
            // must const cast so we can re-use this pointer as we walk the list
            m_current_ptr(const_cast<T*>(addrinfo))
        {
            if (m_current_ptr)
            {
                m_socket_address.reset(m_current_ptr->ai_addr, m_current_ptr->ai_addrlen);
            }
        }

        const ::wil::network::socket_address& operator*() const WI_NOEXCEPT
        {
            return m_socket_address;
        }

        const ::wil::network::socket_address* operator->() const WI_NOEXCEPT
        {
            return &m_socket_address;
        }

        WI_NODISCARD bool operator==(const addr_info_iterator_t& rhs) const WI_NOEXCEPT
        {
            return m_current_ptr == rhs.m_current_ptr;
        }

        WI_NODISCARD bool operator!=(const addr_info_iterator_t& rhs) const WI_NOEXCEPT
        {
            return !(*this == rhs);
        }

        addr_info_iterator_t& operator++() WI_NOEXCEPT
        {
            WI_ASSERT(m_current_ptr);
            if (m_current_ptr)
            {
                m_current_ptr = m_current_ptr->ai_next;
            }

            if (m_current_ptr)
            {
                m_socket_address.reset(m_current_ptr->ai_addr, m_current_ptr->ai_addrlen);
            }
            else
            {
                m_socket_address.reset();
            }
            return *this;
        }

        addr_info_iterator_t operator++(int) WI_NOEXCEPT
        {
            auto return_value = *this;
            this->operator++();
            return return_value;
        }

        // begin and end are added here to enable support for range-based for loops
        addr_info_iterator_t begin() const WI_NOEXCEPT
        {
            return *this;
        }

        constexpr addr_info_iterator_t end() const WI_NOEXCEPT
        {
            return {};
        }

    private:
        // non-ownership of this pointer - the parent class must outlive the iterator
        T* m_current_ptr{nullptr};
        ::wil::network::socket_address m_socket_address{};
    }; // class addr_info_iterator_t

    //
    // For example:
    //     wil::unique_addrinfo resolved_addr;
    //     if (0 == GetAddrInfoW(L"name_to_resolve.xyz", nullptr, nullptr, resolved_addr.addressof()))
    //     {
    //         for (const auto& address : wil::network::addr_info_iterator{resolved_addr.get()})
    //         {
    //             // address == wil::network::socket_address containing a resolved IP address
    //         }
    //     }
    //
    // If one can use exceptions:
    //     const auto addresses = wil::network::resolve_name(name_to_resolve.xyz);
    //     for (const auto& address : wil::network::addr_info_iterator{addresses.get()})
    //     {
    //         // address == wil::network::socket_address containing a resolved IP address
    //     }

    using addr_info_ansi_iterator = addr_info_iterator_t<ADDRINFOA>;
    using addr_info_iterator = addr_info_iterator_t<ADDRINFOW>;
    // not defining a type for ADDRINFOEXA as that type is formally __declspec(deprecated)
    using addr_infoex_iterator = addr_info_iterator_t<ADDRINFOEXW>;

#if defined(WIL_ENABLE_EXCEPTIONS)
    // function to capture resolving IP addresses assigned to the local machine, throwing on error
    // returning an RAII object containing the results
    inline ::wil::unique_addrinfo resolve_name(PCWSTR name)
    {
        ADDRINFOW* addrResult{};
        if (::GetAddrInfoW(name, nullptr, nullptr, &addrResult) != 0)
        {
            THROW_WIN32(::WSAGetLastError());
        }

        return ::wil::unique_addrinfo{addrResult};
    }

    // function to capture resolving IP addresses assigned to the local machine, throwing on error
    // returning an RAII object containing the results
    inline ::wil::unique_addrinfo resolve_local_addresses()
    {
        constexpr auto* local_address_name_string = L"";
        return ::wil::network::resolve_name(local_address_name_string);
    }

    // wil function to capture resolving the local-host (loopback) addresses, throwing on error
    // returning an RAII object containing the results
    inline ::wil::unique_addrinfo resolve_localhost_addresses()
    {
        constexpr auto* localhost_address_name_string = L"localhost";
        return ::wil::network::resolve_name(localhost_address_name_string);
    }
#endif

    struct WINSOCK_EXTENSION_FUNCTION_TABLE
    {
        LPFN_ACCEPTEX AcceptEx{nullptr};
        LPFN_CONNECTEX ConnectEx{nullptr};
        LPFN_DISCONNECTEX DisconnectEx{nullptr};
        LPFN_GETACCEPTEXSOCKADDRS GetAcceptExSockaddrs{nullptr};
        LPFN_TRANSMITFILE TransmitFile{nullptr};
        LPFN_TRANSMITPACKETS TransmitPackets{nullptr};
        LPFN_WSARECVMSG WSARecvMsg{nullptr};
        LPFN_WSASENDMSG WSASendMsg{nullptr};
    };

    struct WINSOCK_SOCKET_NOTIFICATION_TABLE
    {
        using LPFN_PROCESS_SOCKET_NOTIFICATION = DWORD(WSAAPI*)(
            HANDLE completionPort,
            UINT32 registrationCount,
            _Inout_updates_opt_(registrationCount) SOCK_NOTIFY_REGISTRATION* registrationInfos,
            UINT32 timeoutMs,
            ULONG completionCount,
            _Out_writes_to_opt_(completionCount, *receivedEntryCount) OVERLAPPED_ENTRY* completionPortEntries,
            _Out_opt_ UINT32* receivedEntryCount);

        LPFN_PROCESS_SOCKET_NOTIFICATION ProcessSocketNotifications{nullptr};
    };

    template <typename F>
    struct socket_extension_function_table_t;
    using winsock_extension_function_table = socket_extension_function_table_t<WINSOCK_EXTENSION_FUNCTION_TABLE>;
    using rio_extension_function_table = socket_extension_function_table_t<RIO_EXTENSION_FUNCTION_TABLE>;
    using process_socket_notification_table = socket_extension_function_table_t<WINSOCK_SOCKET_NOTIFICATION_TABLE>;

    template <typename F>
    struct socket_extension_function_table_t
    {
        socket_extension_function_table_t() WI_NOEXCEPT : wsa_reference_count{WSAStartup_nothrow()}
        {
            load();
        }

        ~socket_extension_function_table_t() WI_NOEXCEPT = default;

        // assignments are not useful with this class as it is the same contents in each object
        socket_extension_function_table_t& operator=(const socket_extension_function_table_t& rhs) = delete;
        socket_extension_function_table_t& operator=(socket_extension_function_table_t&& rhs) = delete;

        // can copy, but the new object needs its own WSA reference count
        // (getting a WSA reference count should be no-fail once the first reference it taken)
        //
        // IF the target could not get a WSA reference count
        //   OR
        // IF we couldn't get our own WSA reference count
        //   (this should never happen the caller has a reference, but we failed to get a WSA reference)
        // THEN
        //   this object cannot carry forward any function pointers - it must show successfully loaded == false
        socket_extension_function_table_t(const socket_extension_function_table_t& rhs) WI_NOEXCEPT
            : wsa_reference_count{WSAStartup_nothrow()}
        {
            if (!wsa_reference_count || !rhs.wsa_reference_count)
            {
                ::memset(&function_table, 0, sizeof(function_table));
            }
            else
            {
                ::memcpy_s(&function_table, sizeof(function_table), &rhs.function_table, sizeof(rhs.function_table));
            }
        }

        // the move constructor will take ownership of the WSA ref count
        socket_extension_function_table_t(socket_extension_function_table_t&& rhs) WI_NOEXCEPT
            : wsa_reference_count{::wistd::move(rhs.wsa_reference_count)}
        {
            if (wsa_reference_count)
            {
                ::memcpy_s(&function_table, sizeof(function_table), &rhs.function_table, sizeof(rhs.function_table));
            }

            // always have the moved-from function table zeroed
            ::memset(&rhs.function_table, 0, sizeof(rhs.function_table));
        }

        // Returns true if all functions were loaded, holding a WSAStartup reference
        WI_NODISCARD explicit operator bool() const WI_NOEXCEPT;

        const F* operator->() const WI_NOEXCEPT
        {
            return &function_table;
        }

    private:
        // function table
        F function_table{};
        void load() WI_NOEXCEPT;

        // must guarantee Winsock does not unload while we have dynamically loaded function pointers
        ::wil::network::unique_wsacleanup_call wsa_reference_count;
    };

    //
    // explicit specializations for socket_extension_function_table_t
    //
    template <>
    inline socket_extension_function_table_t<WINSOCK_EXTENSION_FUNCTION_TABLE>::operator bool() const WI_NOEXCEPT
    {
        return function_table.AcceptEx != nullptr;
    }

    template <>
    inline socket_extension_function_table_t<RIO_EXTENSION_FUNCTION_TABLE>::operator bool() const WI_NOEXCEPT
    {
        return function_table.RIOReceive != nullptr;
    }

    template <>
    inline socket_extension_function_table_t<WINSOCK_SOCKET_NOTIFICATION_TABLE>::operator bool() const WI_NOEXCEPT
    {
        return function_table.ProcessSocketNotifications != nullptr;
    }

    template <>
    inline void socket_extension_function_table_t<WINSOCK_EXTENSION_FUNCTION_TABLE>::load() WI_NOEXCEPT
    {
        // if WSAStartup failed, immediately exit
        if (!wsa_reference_count)
        {
            return;
        }

        // we need a temporary socket for the IOCTL to load the functions
        const ::wil::unique_socket localSocket{::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)};
        if (!localSocket)
        {
            return;
        }

        const auto load_function_pointer = [&](GUID extensionGuid, void* functionPtr) WI_NOEXCEPT {
            DWORD unused_bytes;
            const auto error{::WSAIoctl(
                localSocket.get(),
                SIO_GET_EXTENSION_FUNCTION_POINTER,
                &extensionGuid,
                DWORD{sizeof(extensionGuid)},
                functionPtr,
                DWORD{sizeof(functionPtr)},
                &unused_bytes,
                nullptr,
                nullptr)};
            return error == 0 ? S_OK : HRESULT_FROM_WIN32(::WSAGetLastError());
        };

        if (FAILED_LOG(load_function_pointer(WSAID_ACCEPTEX, &function_table.AcceptEx)) ||
            FAILED_LOG(load_function_pointer(WSAID_CONNECTEX, &function_table.ConnectEx)) ||
            FAILED_LOG(load_function_pointer(WSAID_DISCONNECTEX, &function_table.DisconnectEx)) ||
            FAILED_LOG(load_function_pointer(WSAID_GETACCEPTEXSOCKADDRS, &function_table.GetAcceptExSockaddrs)) ||
            FAILED_LOG(load_function_pointer(WSAID_TRANSMITFILE, &function_table.TransmitFile)) ||
            FAILED_LOG(load_function_pointer(WSAID_TRANSMITPACKETS, &function_table.TransmitPackets)) ||
            FAILED_LOG(load_function_pointer(WSAID_WSARECVMSG, &function_table.WSARecvMsg)) ||
            FAILED_LOG(load_function_pointer(WSAID_WSASENDMSG, &function_table.WSASendMsg)))
        {
            // all should load successfully (they have been supported since Windows Vista)
            // if any function failed to load, it's likely a 3rd party Winsock provider was installed
            // which doesn't support these extension functions.
            ::memset(&function_table, 0, sizeof(function_table));
        }
    }

    template <>
    inline void socket_extension_function_table_t<RIO_EXTENSION_FUNCTION_TABLE>::load() WI_NOEXCEPT
    {
        // if WSAStartup failed, immediately exit
        if (!wsa_reference_count)
        {
            return;
        }

        // we need a temporary socket for the IOCTL to load the functions
        const ::wil::unique_socket localSocket{::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)};
        if (!localSocket)
        {
            return;
        }

        function_table.cbSize = DWORD{sizeof(function_table)};

        GUID rioGuid = WSAID_MULTIPLE_RIO;
        DWORD unused_bytes;
        if (::WSAIoctl(
                localSocket.get(),
                SIO_GET_MULTIPLE_EXTENSION_FUNCTION_POINTER,
                &rioGuid,
                DWORD{sizeof(rioGuid)},
                &function_table,
                DWORD{sizeof(function_table)},
                &unused_bytes,
                nullptr,
                nullptr) != 0)
        {
            LOG_IF_WIN32_ERROR(::WSAGetLastError());
            ::memset(&function_table, 0, sizeof(function_table));
        }
    }

    template <>
    inline void socket_extension_function_table_t<WINSOCK_SOCKET_NOTIFICATION_TABLE>::load() WI_NOEXCEPT
    {
        // if WSAStartup failed, immediately exit
        if (!wsa_reference_count)
        {
            return;
        }

        // directly LoadLibrary/GetProcAddress the function pointer - we don't need to hold the module handle
        // since WSAStartup will keep the Winsock dll loaded
        if (const ::wil::unique_hmodule ws2_32{LoadLibraryExW(L"ws2_32.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32)})
        {
            if (auto* fn_ptr{GetProcAddress(ws2_32.get(), "ProcessSocketNotifications")})
            {
                // casting directly from GetProcAddress fails with -Wcast-function-type-mismatch
                // clang allows this if we cast the return type to void first, then cast to the function pointer type
                function_table.ProcessSocketNotifications =
                    reinterpret_cast<WINSOCK_SOCKET_NOTIFICATION_TABLE::LPFN_PROCESS_SOCKET_NOTIFICATION>(
                        reinterpret_cast<void(__stdcall*)()>(fn_ptr));
            }
        }
    }

    //
    // socket_address definitions
    //
    inline socket_address::socket_address(ADDRESS_FAMILY family) WI_NOEXCEPT
    {
        reset(family);
    }

    inline socket_address::socket_address(_In_reads_bytes_(addr_size) const SOCKADDR* addr, size_t addr_size) WI_NOEXCEPT
    {
        reset(addr, addr_size);
    }

    inline socket_address::socket_address(const SOCKADDR_IN* addr) WI_NOEXCEPT
    {
        reset(addr);
    }

    inline socket_address::socket_address(const SOCKADDR_IN6* addr) WI_NOEXCEPT
    {
        reset(addr);
    }

    inline socket_address::socket_address(const SOCKADDR_INET* addr) WI_NOEXCEPT
    {
        reset(addr);
    }

    inline socket_address::socket_address(const SOCKET_ADDRESS* addr) WI_NOEXCEPT
    {
        reset(addr);
    }

    inline socket_address::socket_address(const IN_ADDR* addr, unsigned short port) WI_NOEXCEPT
    {
        reset(AF_INET);
        set_address(addr);
        set_port(port);
    }

    inline socket_address::socket_address(const IN6_ADDR* addr, unsigned short port) WI_NOEXCEPT
    {
        reset(AF_INET6);
        set_address(addr);
        set_port(port);
    }

#if defined(WIL_ENABLE_EXCEPTIONS)
    inline socket_address::socket_address(PCWSTR addr, unsigned short port)
    {
        reset_address(addr);
        set_port(port);
    }
#endif

    inline int socket_address::compare(const ::wil::network::socket_address& rhs) const WI_NOEXCEPT
    {
        const auto& lhs{*this};

        if (lhs.family() != rhs.family())
        {
            return lhs.family() < rhs.family() ? -1 : 1;
        }

        switch (lhs.family())
        {
        case AF_INET:
        {
            // compare the address first
            const auto comparison{::memcmp(lhs.in_addr(), rhs.in_addr(), sizeof(IN_ADDR))};
            if (comparison != 0)
            {
                return comparison;
            }

            // then compare the port (host-byte-order)
            const auto lhs_port{lhs.port()};
            const auto rhs_port{rhs.port()};
            if (lhs_port != rhs_port)
            {
                return lhs_port < rhs_port ? -1 : 1;
            }

            // must be exactly equal
            return 0;
        }

        case AF_INET6:
        {
            // compare the address first
            const auto comparison{::memcmp(lhs.in6_addr(), rhs.in6_addr(), sizeof(IN6_ADDR))};
            if (comparison != 0)
            {
                return comparison;
            }

            // then compare the port (host-byte-order)
            // only resolve the ntohs() once
            const auto lhs_port{lhs.port()};
            const auto rhs_port{rhs.port()};
            if (lhs_port != rhs_port)
            {
                return lhs_port < rhs_port ? -1 : 1;
            }

            // then compare the scope_id of the address
            const auto lhs_scope_id{lhs.scope_id()};
            const auto rhs_scope_id{rhs.scope_id()};
            if (lhs_scope_id != rhs_scope_id)
            {
                return lhs_scope_id < rhs_scope_id ? -1 : 1;
            }

            // then compare flow_info
            const auto lhs_flow_info{lhs.flow_info()};
            const auto rhs_flow_info{rhs.flow_info()};
            if (lhs_flow_info != rhs_flow_info)
            {
                return lhs_flow_info < rhs_flow_info ? -1 : 1;
            }

            // must be exactly equal
            return 0;
        }

        default:
            // if not AF_INET or AF_INET6, and families don't match
            // then just raw memcmp the largest field of the union (v6)
            return ::memcmp(&lhs.m_sockaddr.Ipv6, &rhs.m_sockaddr.Ipv6, sizeof(SOCKADDR_IN6)) < 0;
        }
    }

    inline bool socket_address::operator==(const ::wil::network::socket_address& rhs) const WI_NOEXCEPT
    {
        return compare(rhs) == 0;
    }

    inline bool socket_address::operator!=(const ::wil::network::socket_address& rhs) const WI_NOEXCEPT
    {
        return compare(rhs) != 0;
    }

    inline bool socket_address::operator<(const ::wil::network::socket_address& rhs) const WI_NOEXCEPT
    {
        return compare(rhs) < 0;
    }

    inline bool socket_address::operator>(const ::wil::network::socket_address& rhs) const WI_NOEXCEPT
    {
        return compare(rhs) > 0;
    }

    inline void socket_address::swap(socket_address& addr) WI_NOEXCEPT
    {
        wistd::swap_wil(*this, addr);
    }

    inline void socket_address::reset() WI_NOEXCEPT
    {
        ::memset(&m_sockaddr, 0, size());
    }

    inline void socket_address::reset(ADDRESS_FAMILY family) WI_NOEXCEPT
    {
#if (!defined(WI_NETWORK_TEST))
        WI_ASSERT(family == AF_UNSPEC || family == AF_INET || family == AF_INET6);
#endif
        ::memset(&m_sockaddr, 0, size());
        m_sockaddr.si_family = family;
    }

    inline void socket_address::reset(_In_reads_bytes_(addr_size) const SOCKADDR* addr, size_t addr_size) WI_NOEXCEPT
    {
        WI_ASSERT(addr_size <= static_cast<size_t>(size()));

        ::memset(&m_sockaddr, 0, size());
        if (addr)
        {
            ::memcpy_s(&m_sockaddr, sizeof(m_sockaddr), addr, addr_size);
        }
    }

    inline void socket_address::reset(const SOCKADDR_IN* addr) WI_NOEXCEPT
    {
        ::memset(&m_sockaddr, 0, size());
        ::memcpy_s(&m_sockaddr.Ipv4, sizeof(m_sockaddr.Ipv4), addr, sizeof(*addr));
    }

    inline void socket_address::reset(const SOCKADDR_IN6* addr) WI_NOEXCEPT
    {
        ::memset(&m_sockaddr, 0, size());
        ::memcpy_s(&m_sockaddr.Ipv6, sizeof(m_sockaddr.Ipv6), addr, sizeof(*addr));
    }

    inline void socket_address::reset(const SOCKADDR_INET* addr) WI_NOEXCEPT
    {
        ::memset(&m_sockaddr, 0, size());
        ::memcpy_s(&m_sockaddr, sizeof(m_sockaddr), addr, sizeof(*addr));
    }

    inline void socket_address::reset(const SOCKET_ADDRESS* addr) WI_NOEXCEPT
    {
        FAIL_FAST_IF_MSG(
            addr->lpSockaddr && addr->iSockaddrLength > size(),
            "SOCKET_ADDRESS contains an unsupported sockaddr type - larger than an IPv4 or IPv6 address (%d)",
            addr->iSockaddrLength);

        ::memset(&m_sockaddr, 0, size());
        if (addr->lpSockaddr)
        {
            ::memcpy_s(&m_sockaddr, sizeof(m_sockaddr), addr->lpSockaddr, addr->iSockaddrLength);
        }
    }

    inline bool socket_address::is_address_linklocal() const WI_NOEXCEPT
    {
        switch (family())
        {
        case AF_UNSPEC:
            return false;
        case AF_INET:
            return ::IN4_IS_ADDR_LINKLOCAL(in_addr());
        case AF_INET6:
            return ::IN6_IS_ADDR_LINKLOCAL(in6_addr());
        default:
            WI_ASSERT_MSG(false, "Unknown address family");
            return false;
        }
    }

    inline bool socket_address::is_address_loopback() const WI_NOEXCEPT
    {
        switch (family())
        {
        case AF_UNSPEC:
            return false;
        case AF_INET:
            return ::IN4_IS_ADDR_LOOPBACK(in_addr());
        case AF_INET6:
            return ::IN6_IS_ADDR_LOOPBACK(in6_addr());
        default:
            WI_ASSERT_MSG(false, "Unknown address family");
            return false;
        }
    }

    inline NL_ADDRESS_TYPE socket_address::address_type() const WI_NOEXCEPT
    {
        switch (family())
        {
        case AF_UNSPEC:
            return ::NlatUnspecified;
        case AF_INET:
            return ::Ipv4AddressType(reinterpret_cast<const UCHAR*>(in_addr()));
        case AF_INET6:
            return ::Ipv6AddressType(reinterpret_cast<const UCHAR*>(in6_addr()));
        default:
            WI_ASSERT_MSG(false, "Unknown address family");
            return ::NlatInvalid;
        }
    }

    inline void socket_address::set_port(USHORT port) WI_NOEXCEPT
    {
        WI_ASSERT(family() == AF_INET || family() == AF_INET6);

        // the port value is at the exact same offset with both the IPv4 and IPv6 unions
        static_assert(FIELD_OFFSET(SOCKADDR_INET, Ipv4.sin_port) == FIELD_OFFSET(SOCKADDR_INET, Ipv6.sin6_port));

        // port values in a sockaddr are always in network-byte order
        m_sockaddr.Ipv4.sin_port = ::htons(port);
    }

    inline void socket_address::set_scope_id(ULONG scopeId) WI_NOEXCEPT
    {
        WI_ASSERT(family() == AF_INET6);

        if (family() == AF_INET6)
        {
            m_sockaddr.Ipv6.sin6_scope_id = ::htonl(scopeId);
        }
    }

    inline void socket_address::set_flow_info(ULONG flowInfo) WI_NOEXCEPT
    {
        WI_ASSERT(family() == AF_INET6);

        if (family() == AF_INET6)
        {
            m_sockaddr.Ipv6.sin6_flowinfo = ::htonl(flowInfo);
        }
    }

    inline void socket_address::set_address_any() WI_NOEXCEPT
    {
        set_address_any(family());
    }

    inline void socket_address::set_address_any(ADDRESS_FAMILY family) WI_NOEXCEPT
    {
        WI_ASSERT(family == AF_INET || family == AF_INET6);

        // the port value is at the exact same offset with both the IPv4 and IPv6 unions
        static_assert(FIELD_OFFSET(SOCKADDR_INET, Ipv4.sin_port) == FIELD_OFFSET(SOCKADDR_INET, Ipv6.sin6_port));

        const auto original_port{m_sockaddr.Ipv4.sin_port};
        reset(family);
        m_sockaddr.Ipv4.sin_port = original_port;
    }

    inline void socket_address::set_address_loopback() WI_NOEXCEPT
    {
        set_address_loopback(family());
    }

    inline void socket_address::set_address_loopback(ADDRESS_FAMILY family) WI_NOEXCEPT
    {
        // the port value is at the exact same offset with both the IPv4 and IPv6 unions
        static_assert(FIELD_OFFSET(SOCKADDR_INET, Ipv4.sin_port) == FIELD_OFFSET(SOCKADDR_INET, Ipv6.sin6_port));

        const auto original_port{m_sockaddr.Ipv4.sin_port};
        reset(family);
        switch (family)
        {
        case AF_INET:
            m_sockaddr.Ipv4.sin_addr.s_addr = ::htonl(INADDR_LOOPBACK);
            break;
        case AF_INET6:
            m_sockaddr.Ipv6.sin6_addr = {{IN6ADDR_LOOPBACK_INIT}};
            break;
        default:
            WI_ASSERT_MSG(false, "Unknown address family");
        }
        m_sockaddr.Ipv4.sin_port = original_port;
    }

    inline void socket_address::set_address(const IN_ADDR* addr) WI_NOEXCEPT
    {
        WI_ASSERT(family() == AF_INET);

        const auto original_port{m_sockaddr.Ipv4.sin_port};
        reset(AF_INET);
        m_sockaddr.Ipv4.sin_addr.s_addr = addr->s_addr;
        m_sockaddr.Ipv4.sin_port = original_port;
    }

    inline void socket_address::set_address(const IN6_ADDR* addr) WI_NOEXCEPT
    {
        WI_ASSERT(family() == AF_INET6);

        const auto original_port{m_sockaddr.Ipv6.sin6_port};
        reset(AF_INET6);
        m_sockaddr.Ipv6.sin6_addr = *addr;
        m_sockaddr.Ipv6.sin6_port = original_port;
    }

#if defined(WIL_ENABLE_EXCEPTIONS)
    inline void socket_address::reset_address(SOCKET s)
    {
        THROW_IF_FAILED(reset_address_nothrow(s));
    }

    inline void socket_address::reset_address(PCWSTR address)
    {
        THROW_IF_FAILED(reset_address_nothrow(address));
    }

    inline void socket_address::reset_address(PCSTR address)
    {
        THROW_IF_FAILED(reset_address_nothrow(address));
    }
#endif

    inline HRESULT socket_address::reset_address_nothrow(SOCKET s) WI_NOEXCEPT
    {
        reset();

        auto nameLength{size()};
        auto error{::getsockname(s, sockaddr(), &nameLength)};
        if (error != 0)
        {
            error = ::WSAGetLastError();
            RETURN_WIN32(error);
        }
        return S_OK;
    }

    inline HRESULT socket_address::reset_address_nothrow(PCWSTR address) WI_NOEXCEPT
    {
        PCWSTR terminator_unused;

        reset(AF_INET);
        constexpr BOOLEAN strict_string{TRUE};
        if (RtlIpv4StringToAddressW(address, strict_string, &terminator_unused, in_addr()) == 0)
        {
            m_sockaddr.si_family = AF_INET;
            return S_OK;
        }

        reset(AF_INET6);
        if (RtlIpv6StringToAddressW(address, &terminator_unused, in6_addr()) == 0)
        {
            m_sockaddr.si_family = AF_INET6;
            return S_OK;
        }

        reset();
        return E_INVALIDARG;
    }

    inline HRESULT socket_address::reset_address_nothrow(PCSTR address) WI_NOEXCEPT
    {
        PCSTR terminator_unused;

        reset(AF_INET);
        constexpr BOOLEAN strict_string{TRUE};
        if (RtlIpv4StringToAddressA(address, strict_string, &terminator_unused, in_addr()) == 0)
        {
            m_sockaddr.si_family = AF_INET;
            return S_OK;
        }

        reset(AF_INET6);
        if (RtlIpv6StringToAddressA(address, &terminator_unused, in6_addr()) == 0)
        {
            m_sockaddr.si_family = AF_INET6;
            return S_OK;
        }

        reset();
        return E_INVALIDARG;
    }

    inline HRESULT socket_address::format_address_nothrow(socket_address_wstring& address) const WI_NOEXCEPT
    {
        ::memset(address, 0, sizeof(socket_address_wstring));

        // socket_address will return an empty string for AF_UNSPEC (the default family type when constructed)
        // as to not fail if just default constructed
        if (family() == AF_UNSPEC)
        {
            return S_OK;
        }

        const void* const pAddr{
            family() == AF_INET ? static_cast<const void*>(&m_sockaddr.Ipv4.sin_addr)
                                : static_cast<const void*>(&m_sockaddr.Ipv6.sin6_addr)};

        // the last param to InetNtopW is # of characters, not bytes
        const auto* error_value{::InetNtopW(family(), pAddr, address, INET6_ADDRSTRLEN)};
        if (error_value == nullptr)
        {
            RETURN_WIN32(::WSAGetLastError());
        }
        return S_OK;
    }

    inline HRESULT socket_address::format_address_nothrow(socket_address_string& address) const WI_NOEXCEPT
    {
        ::memset(address, 0, sizeof(socket_address_string));

        // socket_address will return an empty string for AF_UNSPEC (the default family type when constructed)
        // as to not fail if just default constructed
        if (family() == AF_UNSPEC)
        {
            return S_OK;
        }

        const void* const pAddr{
            family() == AF_INET ? static_cast<const void*>(&m_sockaddr.Ipv4.sin_addr)
                                : static_cast<const void*>(&m_sockaddr.Ipv6.sin6_addr)};

        // the last param to InetNtopA is # of characters, not bytes
        const auto* error_value{::InetNtopA(family(), pAddr, address, INET6_ADDRSTRLEN)};
        if (error_value == nullptr)
        {
            RETURN_WIN32(::WSAGetLastError());
        }
        return S_OK;
    }

#if (WIL_USE_STL && defined(WIL_ENABLE_EXCEPTIONS)) || defined(WIL_DOXYGEN)
    inline ::std::wstring socket_address::format_address() const
    {
        ::wil::network::socket_address_wstring returnString{};
        THROW_IF_FAILED(format_address_nothrow(returnString));
        returnString[INET6_ADDRSTRLEN - 1] = L'\0';
        return returnString;
    }
#endif

    inline HRESULT socket_address::format_complete_address_nothrow(socket_address_wstring& address) const WI_NOEXCEPT
    {
        ::memset(address, 0, sizeof(socket_address_wstring));

        // socket_address will return an empty string for AF_UNSPEC (the default family type when constructed)
        // as to not fail if just default constructed
        if (family() == AF_UNSPEC)
        {
            return S_OK;
        }

        // addressLength == # of chars, not bytes
        DWORD addressLength{INET6_ADDRSTRLEN};
        if (::WSAAddressToStringW(const_cast<SOCKADDR*>(sockaddr()), size(), nullptr, address, &addressLength) != 0)
        {
            RETURN_WIN32(::WSAGetLastError());
        }
        return S_OK;
    }

    // the Winsock headers require having set this #define to access ANSI-string versions of the Winsock API
#if defined(_WINSOCK_DEPRECATED_NO_WARNINGS)
    inline HRESULT socket_address::format_complete_address_nothrow(socket_address_string& address) const WI_NOEXCEPT
    {
        ::memset(address, 0, sizeof(socket_address_string));

        // socket_address will return an empty string for AF_UNSPEC (the default family type when constructed)
        // as to not fail if just default constructed
        if (family() == AF_UNSPEC)
        {
            return S_OK;
        }

        DWORD addressLength{INET6_ADDRSTRLEN};
        if (::WSAAddressToStringA(const_cast<SOCKADDR*>(sockaddr()), size(), nullptr, address, &addressLength) != 0)
        {
            RETURN_WIN32(::WSAGetLastError());
        }
        return S_OK;
    }
#endif

#if (WIL_USE_STL && defined(WIL_ENABLE_EXCEPTIONS)) || defined(WIL_DOXYGEN)
    inline std::wstring socket_address::format_complete_address() const
    {
        ::wil::network::socket_address_wstring returnString{};
        THROW_IF_FAILED(format_complete_address_nothrow(returnString));
        returnString[INET6_ADDRSTRLEN - 1] = L'\0';
        return returnString;
    }
#endif

    inline ADDRESS_FAMILY socket_address::family() const WI_NOEXCEPT
    {
        return m_sockaddr.si_family;
    }

    inline USHORT socket_address::port() const WI_NOEXCEPT
    {
        switch (family())
        {
        case AF_UNSPEC:
            return 0;
        case AF_INET:
            return ::ntohs(m_sockaddr.Ipv4.sin_port);
        case AF_INET6:
            return ::ntohs(m_sockaddr.Ipv6.sin6_port);
        default:
            WI_ASSERT_MSG(false, "Unknown address family");
            return 0;
        }
    }

    inline ULONG socket_address::flow_info() const WI_NOEXCEPT
    {
        switch (family())
        {
        case AF_UNSPEC:
        // fallthrough
        case AF_INET:
            return 0;
        case AF_INET6:
            return ::ntohl(m_sockaddr.Ipv6.sin6_flowinfo);
        default:
            WI_ASSERT_MSG(false, "Unknown address family");
            return 0;
        }
    }

    inline ULONG socket_address::scope_id() const WI_NOEXCEPT
    {
        switch (family())
        {
        case AF_UNSPEC:
        // fallthrough
        case AF_INET:
            return 0;
        case AF_INET6:
            return ::ntohl(m_sockaddr.Ipv6.sin6_scope_id);
        default:
            WI_ASSERT_MSG(false, "Unknown address family");
            return 0;
        }
    }

    inline SOCKADDR* socket_address::sockaddr() WI_NOEXCEPT
    {
        return reinterpret_cast<SOCKADDR*>(&m_sockaddr);
    }

    inline SOCKADDR_IN* socket_address::sockaddr_in() WI_NOEXCEPT
    {
        WI_ASSERT(family() == AF_INET);
        return &m_sockaddr.Ipv4;
    }

    inline SOCKADDR_IN6* socket_address::sockaddr_in6() WI_NOEXCEPT
    {
        WI_ASSERT(family() == AF_INET6);
        return &m_sockaddr.Ipv6;
    }

    inline SOCKADDR_INET* socket_address::sockaddr_inet() WI_NOEXCEPT
    {
        return &m_sockaddr;
    }

    inline IN_ADDR* socket_address::in_addr() WI_NOEXCEPT
    {
        WI_ASSERT(family() == AF_INET);
        return &m_sockaddr.Ipv4.sin_addr;
    }

    inline IN6_ADDR* socket_address::in6_addr() WI_NOEXCEPT
    {
        WI_ASSERT(family() == AF_INET6);
        return &m_sockaddr.Ipv6.sin6_addr;
    }

    inline const SOCKADDR* socket_address::sockaddr() const WI_NOEXCEPT
    {
        return reinterpret_cast<const SOCKADDR*>(&m_sockaddr);
    }

    inline const SOCKADDR_IN* socket_address::sockaddr_in() const WI_NOEXCEPT
    {
        WI_ASSERT(family() == AF_INET);
        return &m_sockaddr.Ipv4;
    }

    inline const SOCKADDR_IN6* socket_address::sockaddr_in6() const WI_NOEXCEPT
    {
        WI_ASSERT(family() == AF_INET6);
        return &m_sockaddr.Ipv6;
    }

    inline const SOCKADDR_INET* socket_address::sockaddr_inet() const WI_NOEXCEPT
    {
        return &m_sockaddr;
    }

    inline const IN_ADDR* socket_address::in_addr() const WI_NOEXCEPT
    {
        WI_ASSERT(family() == AF_INET);
        return &m_sockaddr.Ipv4.sin_addr;
    }

    inline const IN6_ADDR* socket_address::in6_addr() const WI_NOEXCEPT
    {
        WI_ASSERT(family() == AF_INET6);
        return &m_sockaddr.Ipv6.sin6_addr;
    }

    inline SOCKADDR_STORAGE socket_address::sockaddr_storage() const noexcept
    {
        SOCKADDR_STORAGE return_sockaddr{};
        memcpy_s(&return_sockaddr, sizeof(return_sockaddr), &m_sockaddr, sizeof(m_sockaddr));
        return return_sockaddr;
    }
} // namespace network
} // namespace wil

#endif // __WIL_NETWORK_INCLUDED