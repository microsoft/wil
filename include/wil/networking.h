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
//! Helpers for using BSD sockets and Windows Winsock APIs and structures.
//! Does not require the use of the STL or C++ exceptions (see _nothrow functions)
#ifndef __WIL_NETWORKING_INCLUDED
#define __WIL_NETWORKING_INCLUDED

#ifdef _KERNEL_MODE
#error This header is not supported in kernel-mode.
#endif

// including winsock2.h before windows.h, to prevent inclusion of the older Winsock 1.1. header winsock.h in windows.h
// alternatively, one can define WIN32_LEAN_AND_MEAN to avoid windows.h including any sockets headers
// define _SECURE_SOCKET_TYPES_DEFINED_ at the project level to have access to SocketSecurity* APIs

#if !defined(_WINSOCK2API_) && defined(_WINSOCKAPI_)
#error The Winsock 1.1 winsock.h header was included before the Winsock 2 winsock2.h header - define WIN32_LEAN_AND_MEAN to avoid winsock.h included with Windows.h
#endif

#include <WinSock2.h>
#include <ws2def.h>
#include <ws2ipdef.h>
#include <mstcpip.h>
#include <ws2tcpip.h>

// wil headers
#include "resource.h"

namespace wil
{
//! Functions and classes that support networking operations and structures
namespace networking
{
    //! A type that calls WSACleanup on destruction (or reset()).
    using unique_wsacleanup_call = unique_call<decltype(&::WSACleanup), ::WSACleanup>;

    //! Calls WSAStartup; returns an RAII object that reverts, the RAII object will resolve to bool 'false' if failed
    WI_NODISCARD inline unique_wsacleanup_call WSAStartup_nothrow()
    {
        WSADATA unused_data{};
        const auto error = ::WSAStartup(WINSOCK_VERSION, &unused_data);
        LOG_IF_WIN32_ERROR(error);

        unique_wsacleanup_call return_cleanup{};
        if (error != 0)
        {
            // internally set m_call to false
            // so the caller can check the return object against its operator bool
            // to determine if it succeeded
            return_cleanup.release();
        }
        return return_cleanup;
    }

    //! Calls WSAStartup and fail-fasts if it fails; returns an RAII object that reverts
    WI_NODISCARD inline unique_wsacleanup_call WSAStartup_failfast()
    {
        WSADATA unused_data{};
        FAIL_FAST_IF_FAILED(::WSAStartup(WINSOCK_VERSION, &unused_data));
        return {};
    }

#ifdef WIL_ENABLE_EXCEPTIONS
    WI_NODISCARD inline unique_wsacleanup_call WSAStartup()
    {
        WSADATA unused_data{};
        THROW_IF_WIN32_ERROR(::WSAStartup(WINSOCK_VERSION, &unused_data));
        return {};
    }
#endif

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
    //   - a sockaddr* derived type that is guaranteed to be large enough to hold any possible socket address
    //
    // sockaddr_in / SOCKADDR_IN
    //   - a sockaddr* derived type designed to contain an IPv4 address and port number
    // sockaddr_in6 / SOCKADDR_IN6
    //   - a sockaddr* derived type designed to contain an IPv6 address, port, scope id, and flow info
    // SOCKADDR_INET
    //   - a union of sockaddr_in and sockaddr_in6 -- i.e., large enough to contain any TCPIP address
    //
    // in_addr (IN_ADDR)
    //   - the raw address portion of a sockaddr_in
    // in6_addr (IN6_ADDR)
    //   - the raw address portion of a sockaddr_in6
    //
    // SOCKET_ADDRESS
    //   - not a derived sockaddr* type
    //   - a structure containing both a sockaddr* and its length fields
    //
    // SOCKADDR_DL... data-link

    class addr_info;
    addr_info resolve_name_nothrow(_In_ PCWSTR name) WI_NOEXCEPT;

    // class addr_info encapsulates the ADDRINFO structure
    // this structure contains a linked list of addresses returned from resolving a name via GetAddrInfo
    // exposes iterator semantics to safely access these addresses
    class addr_info
    {
    public:
        struct iterator
        {
            // TODO
        };

        iterator begin();
        iterator end();

        [[nodiscard]] int get_last_error() const WI_NOEXCEPT
        {
            return m_lastError;
        }

        addr_info(const addr_info&) = delete;
        addr_info& operator=(const addr_info&) = delete;

        addr_info(addr_info&& rhs) WI_NOEXCEPT
        {
            m_addrResult = rhs.m_addrResult;
            rhs.m_addrResult = nullptr;
        }

        addr_info& operator=(addr_info&& rhs) WI_NOEXCEPT
        {
            if (m_addrResult)
            {
                ::FreeAddrInfoW(m_addrResult);
            }
            m_addrResult = rhs.m_addrResult;
            rhs.m_addrResult = nullptr;

            return *this;
        }

        ~addr_info() WI_NOEXCEPT
        {
            if (m_addrResult)
            {
                ::FreeAddrInfoW(m_addrResult);
            }
        }

    private:
        friend addr_info resolve_name_nothrow(_In_ PCWSTR name) WI_NOEXCEPT;
#ifdef WIL_ENABLE_EXCEPTIONS
        friend addr_info resolve_name(_In_ PCWSTR name);
#endif

        addr_info(_In_ ADDRINFOW* addrResult, int error) : m_addrResult{addrResult}, m_lastError{error}
        {
        }

        ADDRINFOW* m_addrResult{};
        int m_lastError{};
    };

    inline ::wil::networking::addr_info resolve_name_nothrow(_In_ PCWSTR name) WI_NOEXCEPT
    {
        int lastError = 0;
        ADDRINFOW* addrResult{};
        if (0 != ::GetAddrInfoW(name, nullptr, nullptr, &addrResult))
        {
            lastError = ::WSAGetLastError();
        }

        return {addrResult, lastError};
    }

    inline ::wil::networking::addr_info resolve_local_addresses_nothrow() WI_NOEXCEPT
    {
        return ::wil::networking::resolve_name_nothrow(L"");
    }

#ifdef WIL_ENABLE_EXCEPTIONS
    inline ::wil::networking::addr_info resolve_name(_In_ PCWSTR name)
    {
        ADDRINFOW* addrResult{};
        THROW_IF_WIN32_ERROR(::GetAddrInfoW(name, nullptr, nullptr, &addrResult));
        return {addrResult, 0};
    }

    inline ::wil::networking::addr_info resolve_local_addresses() WI_NOEXCEPT
    {
        return ::wil::networking::resolve_name(L"");
    }

#endif

    // INET6_ADDRSTRLEN is guaranteed to be larger than INET_ADDRSTRLEN for IPv4 addresses
    typedef WCHAR socket_address_wstring[INET6_ADDRSTRLEN];
    typedef CHAR socket_address_string[INET6_ADDRSTRLEN];

    class socket_address final
    {
    public:
        explicit socket_address(ADDRESS_FAMILY family = AF_UNSPEC) WI_NOEXCEPT;
        template <typename T>
        explicit socket_address(_In_reads_bytes_(inLength) const SOCKADDR* addr, T inLength) WI_NOEXCEPT;
        explicit socket_address(const SOCKADDR_IN*) WI_NOEXCEPT;
        explicit socket_address(const SOCKADDR_IN6*) WI_NOEXCEPT;
        explicit socket_address(const SOCKADDR_INET*) WI_NOEXCEPT;
        explicit socket_address(const SOCKET_ADDRESS*) WI_NOEXCEPT;
        explicit socket_address(const IN_ADDR*, unsigned short port = 0) WI_NOEXCEPT;
        explicit socket_address(const IN6_ADDR*, unsigned short port = 0) WI_NOEXCEPT;

        ~socket_address() = default;
        socket_address(const socket_address&) WI_NOEXCEPT = default;
        socket_address& operator=(const socket_address&) WI_NOEXCEPT = default;
        socket_address(socket_address&&) WI_NOEXCEPT = default;
        socket_address& operator=(socket_address&&) WI_NOEXCEPT = default;

        bool operator==(const socket_address&) const WI_NOEXCEPT;
        bool operator!=(const socket_address&) const WI_NOEXCEPT;
        bool operator<(const socket_address&) const WI_NOEXCEPT;
        bool operator>(const socket_address&) const WI_NOEXCEPT;

        void swap(socket_address&) WI_NOEXCEPT;
        void reset(ADDRESS_FAMILY family = AF_UNSPEC) WI_NOEXCEPT;

        // set_sockaddr overwrites the entire sockaddr in the object

        template <typename T>
        void set_sockaddr(_In_reads_bytes_(inLength) const SOCKADDR* addr, T inLength) WI_NOEXCEPT;
        void set_sockaddr(const SOCKADDR_IN*) WI_NOEXCEPT;
        void set_sockaddr(const SOCKADDR_IN6*) WI_NOEXCEPT;
        void set_sockaddr(const SOCKADDR_INET*) WI_NOEXCEPT;
        void set_sockaddr(const SOCKET_ADDRESS*) WI_NOEXCEPT;

        [[nodiscard]] bool is_address_any() const WI_NOEXCEPT;
        [[nodiscard]] bool is_address_linklocal() const WI_NOEXCEPT;
        [[nodiscard]] bool is_address_loopback() const WI_NOEXCEPT;

        // returns NlatUnspecified ('any'), NlatUnicast, NlatAnycast, NlatMulticast, NlatBroadcast,
        [[nodiscard]] NL_ADDRESS_TYPE get_address_type() const WI_NOEXCEPT;

        void set_port(USHORT) WI_NOEXCEPT;
        void set_scope_id(ULONG) WI_NOEXCEPT;
        void set_flow_info(ULONG) WI_NOEXCEPT;

        // set_address* preserves the existing address family and port in the object

        void set_address_any() WI_NOEXCEPT;
        void set_address_loopback() WI_NOEXCEPT;
        void set_address(const IN_ADDR*) WI_NOEXCEPT;
        void set_address(const IN6_ADDR*) WI_NOEXCEPT;
        [[nodiscard]] HRESULT set_address_nothrow(SOCKET) WI_NOEXCEPT;
        [[nodiscard]] HRESULT set_address_nothrow(_In_ PCWSTR) WI_NOEXCEPT;
#ifdef _WINSOCK_DEPRECATED_NO_WARNINGS // ANSI functions are deprecated
        [[nodiscard]] HRESULT set_address_nothrow(_In_ PCSTR) WI_NOEXCEPT;
#endif

        // write_address prints the IP address portion, not the scope id or port
#if defined(_STRING_) || defined(WIL_DOXYGEN)
        [[nodiscard]] std::wstring write_address() const;
#endif
        HRESULT write_address_nothrow(socket_address_wstring& address) const WI_NOEXCEPT;
        HRESULT write_address_nothrow(socket_address_string& address) const WI_NOEXCEPT;

        // write_complete_address prints the IP address, scope id, and port
#if defined(_STRING_) || defined(WIL_DOXYGEN)
        [[nodiscard]] std::wstring write_complete_address() const;
#endif
        HRESULT write_complete_address_nothrow(socket_address_wstring& address) const WI_NOEXCEPT;
#ifdef _WINSOCK_DEPRECATED_NO_WARNINGS // ANSI functions are deprecated
        HRESULT write_complete_address_nothrow(socket_address_string& address) const WI_NOEXCEPT;
#endif

        // Accessors
        [[nodiscard]] ADDRESS_FAMILY family() const WI_NOEXCEPT;
        [[nodiscard]] USHORT port() const WI_NOEXCEPT;
        [[nodiscard]] ULONG flow_info() const WI_NOEXCEPT;
        [[nodiscard]] ULONG scope_id() const WI_NOEXCEPT;

        [[nodiscard]] SOCKADDR* sockaddr() WI_NOEXCEPT;
        [[nodiscard]] SOCKADDR_IN* sockaddr_in() WI_NOEXCEPT;
        [[nodiscard]] SOCKADDR_IN6* sockaddr_in6() WI_NOEXCEPT;
        [[nodiscard]] SOCKADDR_INET* sockaddr_inet() WI_NOEXCEPT;
        [[nodiscard]] IN_ADDR* in_addr() WI_NOEXCEPT;
        [[nodiscard]] IN6_ADDR* in6_addr() WI_NOEXCEPT;

        [[nodiscard]] const SOCKADDR* sockaddr() const WI_NOEXCEPT;
        [[nodiscard]] const SOCKADDR_IN* sockaddr_in() const WI_NOEXCEPT;
        [[nodiscard]] const SOCKADDR_IN6* sockaddr_in6() const WI_NOEXCEPT;
        [[nodiscard]] const SOCKADDR_INET* sockaddr_inet() const WI_NOEXCEPT;
        [[nodiscard]] const IN_ADDR* in_addr() const WI_NOEXCEPT;
        [[nodiscard]] const IN6_ADDR* in6_addr() const WI_NOEXCEPT;

        [[nodiscard]] static int length() WI_NOEXCEPT
        {
            return c_sockaddr_size;
        }

    private:
        static constexpr int c_sockaddr_size = sizeof(SOCKADDR_INET);
        SOCKADDR_INET m_sockaddr{};
    };

    // for dual-mode sockets, when needing to explicitly connect to the target IPv4 address,
    // - one must map the IPv4 address to its mapped IPv6 address
    inline socket_address map_dual_mode_4to6(const socket_address& inV4) WI_NOEXCEPT
    {
        constexpr IN6_ADDR v4MappedPrefix{{IN6ADDR_V4MAPPEDPREFIX_INIT}};

        socket_address outV6{&v4MappedPrefix, inV4.port()};

        auto* const pIn6Addr = outV6.in6_addr();
        const auto* const pIn4Addr = inV4.in_addr();
        pIn6Addr->u.Byte[12] = pIn4Addr->S_un.S_un_b.s_b1;
        pIn6Addr->u.Byte[13] = pIn4Addr->S_un.S_un_b.s_b2;
        pIn6Addr->u.Byte[14] = pIn4Addr->S_un.S_un_b.s_b3;
        pIn6Addr->u.Byte[15] = pIn4Addr->S_un.S_un_b.s_b4;

        return outV6;
    }

    // non-member swap
    inline void swap(socket_address& lhs, socket_address& rhs) WI_NOEXCEPT
    {
        lhs.swap(rhs);
    }

    inline socket_address::socket_address(ADDRESS_FAMILY family) WI_NOEXCEPT
    {
        m_sockaddr.si_family = family;
    }

    template <typename T>
    socket_address::socket_address(_In_reads_bytes_(inLength) const SOCKADDR* addr, T inLength) WI_NOEXCEPT
    {
        set_sockaddr(addr, inLength);
    }

    inline socket_address::socket_address(const SOCKADDR_IN* addr) WI_NOEXCEPT
    {
        set_sockaddr(addr);
    }

    inline socket_address::socket_address(const SOCKADDR_IN6* addr) WI_NOEXCEPT
    {
        set_sockaddr(addr);
    }

    inline socket_address::socket_address(const SOCKADDR_INET* addr) WI_NOEXCEPT
    {
        set_sockaddr(addr);
    }

    inline socket_address::socket_address(const SOCKET_ADDRESS* addr) WI_NOEXCEPT
    {
        set_sockaddr(addr->lpSockaddr, addr->iSockaddrLength);
    }

    inline socket_address::socket_address(const IN_ADDR* addr, unsigned short port) WI_NOEXCEPT
    {
        m_sockaddr.si_family = AF_INET;
        set_address(addr);
        set_port(port);
    }

    inline socket_address::socket_address(const IN6_ADDR* addr, unsigned short port) WI_NOEXCEPT
    {
        m_sockaddr.si_family = AF_INET6;
        set_address(addr);
        set_port(port);
    }

    inline bool socket_address::operator==(const socket_address& rhs) const WI_NOEXCEPT
    {
        const auto& lhs = *this;

        // Follows the same documented comparison logic as GetTcpTable2 and GetTcp6Table2
        if (lhs.family() != rhs.family())
        {
            return false;
        }

        if (lhs.family() == AF_INET)
        {
            // don't compare the padding at the end of the SOCKADDR_IN
            return ::memcmp(&lhs.m_sockaddr.Ipv4, &rhs.m_sockaddr.Ipv4, sizeof(SOCKADDR_IN) - sizeof(SOCKADDR_IN::sin_zero)) == 0;
        }
        return ::memcmp(&lhs.m_sockaddr.Ipv6, &rhs.m_sockaddr.Ipv6, sizeof(SOCKADDR_IN6)) == 0;
    }

    inline bool socket_address::operator!=(const socket_address& rhs) const WI_NOEXCEPT
    {
        return !(*this == rhs);
    }

    inline bool socket_address::operator<(const socket_address& rhs) const WI_NOEXCEPT
    {
        const auto& lhs = *this;
        // Follows the same documented comparison logic as GetTcpTable2 and GetTcp6Table2
        if (lhs.family() != rhs.family())
        {
            return lhs.family() < rhs.family();
        }

        if (lhs.family() == AF_INET)
        {
            // don't compare the padding at the end of the SOCKADDR_IN
            return ::memcmp(&lhs.m_sockaddr.Ipv4, &rhs.m_sockaddr.Ipv4, sizeof(SOCKADDR_IN) - sizeof(SOCKADDR_IN::sin_zero)) < 0;
        }
        return ::memcmp(&lhs.m_sockaddr.Ipv6, &rhs.m_sockaddr.Ipv6, sizeof(SOCKADDR_IN6)) < 0;
    }

    inline bool socket_address::operator>(const socket_address& rhs) const WI_NOEXCEPT
    {
        return !(*this < rhs);
    }

    inline void socket_address::swap(socket_address& addr) WI_NOEXCEPT
    {
        SOCKADDR_INET tempAddr{};
        ::CopyMemory(&tempAddr, &addr.m_sockaddr, c_sockaddr_size);
        ::CopyMemory(&addr.m_sockaddr, &m_sockaddr, c_sockaddr_size);
        ::CopyMemory(&m_sockaddr, &tempAddr, c_sockaddr_size);
    }

    inline void socket_address::reset(ADDRESS_FAMILY family) WI_NOEXCEPT
    {
        ::ZeroMemory(&m_sockaddr, c_sockaddr_size);
        m_sockaddr.si_family = family;
    }

    template <typename T>
    void socket_address::set_sockaddr(_In_reads_bytes_(inLength) const SOCKADDR* addr, T inLength) WI_NOEXCEPT
    {
        const size_t length = static_cast<size_t>(inLength) < c_sockaddr_size ? inLength : c_sockaddr_size;
        ::ZeroMemory(&m_sockaddr, c_sockaddr_size);
        ::CopyMemory(&m_sockaddr, addr, length);
    }

    inline void socket_address::set_sockaddr(const SOCKADDR_IN* addr) WI_NOEXCEPT
    {
        ::ZeroMemory(&m_sockaddr, c_sockaddr_size);
        ::CopyMemory(&m_sockaddr.Ipv4, addr, sizeof(SOCKADDR_IN));
    }

    inline void socket_address::set_sockaddr(const SOCKADDR_IN6* addr) WI_NOEXCEPT
    {
        ::ZeroMemory(&m_sockaddr, c_sockaddr_size);
        ::CopyMemory(&m_sockaddr.Ipv6, addr, sizeof(SOCKADDR_IN6));
    }

    inline void socket_address::set_sockaddr(const SOCKADDR_INET* addr) WI_NOEXCEPT
    {
        ::ZeroMemory(&m_sockaddr, c_sockaddr_size);
        ::CopyMemory(&m_sockaddr, addr, sizeof(SOCKADDR_INET));
    }

    inline void socket_address::set_sockaddr(const SOCKET_ADDRESS* addr) WI_NOEXCEPT
    {
        FAIL_FAST_IF_MSG(
            addr->iSockaddrLength > c_sockaddr_size,
            "SOCKET_ADDRESS contains an unsupported sockaddr type - larger than an IPv4 or IPv6 address (%d)",
            addr->iSockaddrLength);

        ::ZeroMemory(&m_sockaddr, c_sockaddr_size);
        if (addr->lpSockaddr)
        {
            ::CopyMemory(&m_sockaddr, addr->lpSockaddr, addr->iSockaddrLength);
        }
    }

    inline bool socket_address::is_address_any() const WI_NOEXCEPT
    {
        if (scope_id() != 0)
        {
            return false;
        }

        switch (family())
        {
        case AF_UNSPEC:
            return false;

        case AF_INET:
            return ::IN4_IS_ADDR_UNSPECIFIED(in_addr());

        case AF_INET6:
            return ::IN6_IS_ADDR_UNSPECIFIED(in6_addr());
        }

        WI_ASSERT_MSG(false, "Unknown address family");
        return false;
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
        }

        WI_ASSERT_MSG(false, "Unknown address family");
        return false;
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
        }

        WI_ASSERT_MSG(false, "Unknown address family");
        return false;
    }

    inline NL_ADDRESS_TYPE socket_address::get_address_type() const WI_NOEXCEPT
    {
        switch (family())
        {
        case AF_UNSPEC:
            return NlatUnspecified;

        case AF_INET:
            return ::Ipv4AddressType(reinterpret_cast<const UCHAR*>(in_addr()));

        case AF_INET6:
            return ::Ipv6AddressType(reinterpret_cast<const UCHAR*>(in6_addr()));
        }

        WI_ASSERT_MSG(false, "Unknown address family");
        return NlatInvalid;
    }

    inline void socket_address::set_port(USHORT port) WI_NOEXCEPT
    {
        // port values in a sockaddr are always in network-byte order
        USHORT port_network_byte_order = htons(port);
        switch (family())
        {
        case AF_INET:
            m_sockaddr.Ipv4.sin_port = port_network_byte_order;
            break;

        case AF_INET6:
            m_sockaddr.Ipv6.sin6_port = port_network_byte_order;
            break;

        default:
            WI_ASSERT_MSG(false, "Unknown address family");
        }
    }

    inline void socket_address::set_scope_id(ULONG scopeId) WI_NOEXCEPT
    {
        WI_ASSERT(family() == AF_INET6);
        if (family() == AF_INET6)
        {
            m_sockaddr.Ipv6.sin6_scope_id = scopeId;
        }
    }

    inline void socket_address::set_flow_info(ULONG flowInfo) WI_NOEXCEPT
    {
        WI_ASSERT(family() == AF_INET6);
        if (family() == AF_INET6)
        {
            m_sockaddr.Ipv6.sin6_flowinfo = flowInfo;
        }
    }

    inline void socket_address::set_address_any() WI_NOEXCEPT
    {
        const auto original_family = family();
        switch (original_family)
        {
        case AF_UNSPEC:
            ::ZeroMemory(&m_sockaddr, c_sockaddr_size);
            break;

        case AF_INET:
        {
            auto original_port = m_sockaddr.Ipv4.sin_port;
            ::ZeroMemory(&m_sockaddr, c_sockaddr_size);
            m_sockaddr.Ipv4.sin_port = original_port;
            break;
        }

        case AF_INET6:
        {
            auto original_port = m_sockaddr.Ipv6.sin6_port;
            ::ZeroMemory(&m_sockaddr, c_sockaddr_size);
            m_sockaddr.Ipv6.sin6_port = original_port;
            break;
        }

        default:
            FAIL_FAST_MSG("Unknown family (%d)", original_family);
        }

        m_sockaddr.si_family = original_family;
    }

    inline void socket_address::set_address_loopback() WI_NOEXCEPT
    {
        const auto original_family = family();
        switch (original_family)
        {
        case AF_INET:
        {
            auto original_port = m_sockaddr.Ipv4.sin_port;
            ::ZeroMemory(&m_sockaddr, c_sockaddr_size);
            m_sockaddr.Ipv4.sin_port = original_port;
            m_sockaddr.Ipv4.sin_addr.s_addr = INADDR_LOOPBACK;
            break;
        }

        case AF_INET6:
        {
            auto original_port = m_sockaddr.Ipv6.sin6_port;
            ::ZeroMemory(&m_sockaddr, c_sockaddr_size);
            m_sockaddr.Ipv6.sin6_port = original_port;
            m_sockaddr.Ipv6.sin6_addr = IN6ADDR_LOOPBACK_INIT;
            break;
        }

        default:
            FAIL_FAST_MSG("Unknown family to create a loopback socket address (%d)", original_family);
        }

        m_sockaddr.si_family = original_family;
    }

    inline void socket_address::set_address(const IN_ADDR* addr) WI_NOEXCEPT
    {
        WI_ASSERT(family() == AF_INET);
        const auto original_port = m_sockaddr.Ipv4.sin_port;
        reset(AF_INET);
        m_sockaddr.Ipv4.sin_port = original_port;
        m_sockaddr.Ipv4.sin_addr.s_addr = addr->s_addr;
    }

    inline void socket_address::set_address(const IN6_ADDR* addr) WI_NOEXCEPT
    {
        WI_ASSERT(family() == AF_INET6);
        const auto original_port = m_sockaddr.Ipv4.sin_port;
        reset(AF_INET6);
        m_sockaddr.Ipv6.sin6_port = original_port;
        m_sockaddr.Ipv6.sin6_addr = *addr;
    }

    inline HRESULT socket_address::set_address_nothrow(SOCKET s) WI_NOEXCEPT
    {
        auto nameLength = length();
        auto error = ::getsockname(s, sockaddr(), &nameLength);
        if (error != 0)
        {
            error = ::WSAGetLastError();
            RETURN_WIN32(error);
        }
        return S_OK;
    }

    inline HRESULT socket_address::set_address_nothrow(_In_ PCWSTR wszAddr) WI_NOEXCEPT
    {
        ADDRINFOW hints{};
        hints.ai_flags = AI_NUMERICHOST;

        ADDRINFOW* pResult{};
        const auto error = ::GetAddrInfoW(wszAddr, nullptr, &hints, &pResult);
        if (0 == error)
        {
            set_sockaddr(pResult->ai_addr, pResult->ai_addrlen);
            ::FreeAddrInfoW(pResult);
            return S_OK;
        }
        RETURN_WIN32(error);
    }

// the Winsock headers require having set this #define to access ANSI-string versions of the Winsock API
#ifdef _WINSOCK_DEPRECATED_NO_WARNINGS
    inline HRESULT socket_address::set_address_nothrow(_In_ PCSTR szAddr) WI_NOEXCEPT
    {
        ADDRINFOA hints{};
        hints.ai_flags = AI_NUMERICHOST;

        ADDRINFOA* pResult{};
        const auto error = ::GetAddrInfoA(szAddr, nullptr, &hints, &pResult) if (0 == error)
        {
            set_sockaddr(pResult->ai_addr, pResult->ai_addrlen);
            FreeAddrInfoA(pResult);
            return S_OK;
        }
        RETURN_WIN32(error);
    }
#endif

#if defined(_STRING_) || defined(WIL_DOXYGEN)
    inline std::wstring socket_address::write_address() const
    {
        socket_address_wstring returnString{};
        write_address_nothrow(returnString);
        returnString[INET6_ADDRSTRLEN - 1] = L'\0';
        return returnString;
    }
#endif

    inline HRESULT socket_address::write_address_nothrow(socket_address_wstring& address) const WI_NOEXCEPT
    {
        ::ZeroMemory(address, sizeof(socket_address_wstring));

        const void* const pAddr = family() == AF_INET ? static_cast<const void*>(&m_sockaddr.Ipv4.sin_addr)
                                                      : static_cast<const void*>(&m_sockaddr.Ipv6.sin6_addr);
        // the last param to InetNtopW is # of characters, not bytes
        return nullptr != InetNtopW(family(), pAddr, address, INET6_ADDRSTRLEN);
    }

    inline HRESULT socket_address::write_address_nothrow(socket_address_string& address) const WI_NOEXCEPT
    {
        ::ZeroMemory(address, sizeof(socket_address_string));

        const void* const pAddr = family() == AF_INET ? static_cast<const void*>(&m_sockaddr.Ipv4.sin_addr)
                                                      : static_cast<const void*>(&m_sockaddr.Ipv6.sin6_addr);
        // the last param to InetNtopA is # of characters, not bytes
        const auto* error_value = InetNtopA(family(), pAddr, address, INET6_ADDRSTRLEN);
        if (error_value == nullptr)
        {
            const auto gle = ::WSAGetLastError();
            RETURN_WIN32(gle);
        }
        return S_OK;
    }

#if defined(_STRING_) || defined(WIL_DOXYGEN)
    inline std::wstring socket_address::write_complete_address() const
    {
        socket_address_wstring returnString{};
        write_complete_address_nothrow(returnString);
        returnString[INET6_ADDRSTRLEN - 1] = L'\0';
        return returnString;
    }
#endif

    inline HRESULT socket_address::write_complete_address_nothrow(socket_address_wstring& address) const WI_NOEXCEPT
    {
        ::ZeroMemory(address, sizeof(socket_address_wstring));
        // addressLength == # of chars, not bytes
        DWORD addressLength = INET6_ADDRSTRLEN;
        if (::WSAAddressToStringW(const_cast<SOCKADDR*>(sockaddr()), c_sockaddr_size, nullptr, address, &addressLength) != 0)
        {
            const auto gle = ::WSAGetLastError();
            RETURN_WIN32(gle);
        }
        return S_OK;
    }

// the Winsock headers require having set this #define to access ANSI-string versions of the Winsock API
#ifdef _WINSOCK_DEPRECATED_NO_WARNINGS
    inline HRESULT socket_address::write_complete_address_nothrow(socket_address_string& address) const WI_NOEXCEPT
    {
        ::ZeroMemory(address, sizeof(socket_address_string));
        DWORD addressLength = INET6_ADDRSTRLEN;
        if (::WSAAddressToStringA(const_cast<SOCKADDR*>(sockaddr()), c_sockaddrSize, nullptr, address, &addressLength) != 0)
        {
            const auto gle = ::WSAGetLastError();
            RETURN_WIN32(gle);
        }
        return S_OK;
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
            return ntohs(m_sockaddr.Ipv4.sin_port);
        case AF_INET6:
            return ntohs(m_sockaddr.Ipv6.sin6_port);
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
            return m_sockaddr.Ipv6.sin6_flowinfo;
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
            return m_sockaddr.Ipv6.sin6_scope_id;
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
        return &m_sockaddr.Ipv4;
    }

    inline SOCKADDR_IN6* socket_address::sockaddr_in6() WI_NOEXCEPT
    {
        return &m_sockaddr.Ipv6;
    }

    inline SOCKADDR_INET* socket_address::sockaddr_inet() WI_NOEXCEPT
    {
        return &m_sockaddr;
    }

    inline IN_ADDR* socket_address::in_addr() WI_NOEXCEPT
    {
        return &m_sockaddr.Ipv4.sin_addr;
    }

    inline IN6_ADDR* socket_address::in6_addr() WI_NOEXCEPT
    {
        return &m_sockaddr.Ipv6.sin6_addr;
    }

    inline const SOCKADDR* socket_address::sockaddr() const WI_NOEXCEPT
    {
        return reinterpret_cast<const SOCKADDR*>(&m_sockaddr);
    }

    inline const SOCKADDR_IN* socket_address::sockaddr_in() const WI_NOEXCEPT
    {
        return &m_sockaddr.Ipv4;
    }

    inline const SOCKADDR_IN6* socket_address::sockaddr_in6() const WI_NOEXCEPT
    {
        return &m_sockaddr.Ipv6;
    }

    inline const SOCKADDR_INET* socket_address::sockaddr_inet() const WI_NOEXCEPT
    {
        return &m_sockaddr;
    }

    inline const IN_ADDR* socket_address::in_addr() const WI_NOEXCEPT
    {
        return &m_sockaddr.Ipv4.sin_addr;
    }

    inline const IN6_ADDR* socket_address::in6_addr() const WI_NOEXCEPT
    {
        return &m_sockaddr.Ipv6.sin6_addr;
    }
} // namespace networking
} // namespace wil

#endif
