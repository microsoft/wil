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
//! Helpers for using BSD sockets and Winsock functions and structures.
//! Does not require the use of the STL or C++ exceptions (see _nothrow functions)
#ifndef __WIL_NETWORK_INCLUDED
#define __WIL_NETWORK_INCLUDED

#ifdef _KERNEL_MODE
#error This header is not supported in kernel-mode.
#endif

//! define WIN32_LEAN_AND_MEAN at the project level to avoid windows.h including older winsock 1.1 headers from winsock.h
//! as including both the Winsock 1.1 header 'winsock.h' and the Winsock 2 header 'winsock2.h' will create compiler errors
//! alternatively, including winsock2.h before windows.h to prevent inclusion of the Winsock 1.1 winsock.h header from within windows.h
//! note: winsock2.h will include windows.h if not already included
//!
//! define _SECURE_SOCKET_TYPES_DEFINED_ at the project level to have access to SocketSecurity* functions in ws2tcpip.h
//!
//! define INCL_WINSOCK_API_TYPEDEFS at the project level to make function typedefs available across various networking headers
//! note, winsock2.h defaults is to not include function typedefs - but these can be necessary when supporting multiple OS versions
//!
//! Link libs for functions referenced in this file: ws2_32.lib, ntdll.lib, and Fwpuclnt.lib (for secure socket functions)

#if !defined(_WINSOCK2API_) && defined(_WINSOCKAPI_)
#error The Winsock 1.1 winsock.h header was included before the Winsock 2 winsock2.h header - this will cause compilation errors - define WIN32_LEAN_AND_MEAN to avoid winsock.h included by windows.h, or include winsock2.h before windows.h
#endif

//! Including Winsock and networking headers in the below specific sequence
//! These headers have many intra-header dependencies, creating difficulties when needing access to various functions and types
//! This specific sequence should compile correctly to give access to all available functions and types
#include <winsock2.h>
#include <ws2def.h>
#include <ws2ipdef.h>
#include <mswsock.h>
#include <mstcpip.h>
#include <ws2tcpip.h>
#include <windns.h>
#include <iphlpapi.h>

// the wil header for RAII types
#include "resource.h"

namespace wil
{
//! Functions and classes that support networking operations and structures
namespace network
{
    //! A type that calls WSACleanup on destruction (or reset()).
    //! WSAStartup must be called for the lifetime of all Winsock APIs (synchronous and asynchronous)
    //! WSACleanup will unload the full Winsock catalog - all the libraries - with the final reference
    //! which can lead to crashes if socket APIs are still being used after the final WSACleanup is called
    using unique_wsacleanup_call = ::wil::unique_call<decltype(&::WSACleanup), ::WSACleanup>;

    //! Calls WSAStartup; returns an RAII object that reverts, the RAII object will resolve to bool 'false' if failed
    WI_NODISCARD inline ::wil::network::unique_wsacleanup_call WSAStartup_nothrow() WI_NOEXCEPT
    {
        WSADATA unused_data{};
        const auto error{::WSAStartup(WINSOCK_VERSION, &unused_data)};
        LOG_IF_WIN32_ERROR(error);

        ::wil::network::unique_wsacleanup_call return_cleanup{};
        if (error != 0)
        {
            // internally set m_call to false
            // so the caller can check the return object against its operator bool
            // to determine if WSAStartup succeeded
            return_cleanup.release();
        }
        return return_cleanup;
    }

    //! Calls WSAStartup and fail-fasts on error; returns an RAII object that reverts
    WI_NODISCARD inline ::wil::network::unique_wsacleanup_call WSAStartup_failfast() WI_NOEXCEPT
    {
        WSADATA unused_data{};
        FAIL_FAST_IF_WIN32_ERROR(::WSAStartup(WINSOCK_VERSION, &unused_data));
        return {};
    }

#if defined(WIL_ENABLE_EXCEPTIONS)
    //! Calls WSAStartup and throws on error; returns an RAII object that reverts
    WI_NODISCARD inline ::wil::network::unique_wsacleanup_call WSAStartup()
    {
        WSADATA unused_data{};
        THROW_IF_WIN32_ERROR(::WSAStartup(WINSOCK_VERSION, &unused_data));
        return {};
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

    struct winsock_extension_function_table
    {
        static winsock_extension_function_table load() WI_NOEXCEPT;

        ~winsock_extension_function_table() WI_NOEXCEPT = default;

        // can copy, but the new object needs its own WSA reference count
        // (getting a WSA reference count should be no-fail once the first reference it taken)
        //
        // IF the target could not get a WSA reference count
        //   OR
        // IF we couldn't get our own WSA reference count
        //   (this should never happen the caller has a reference, but we failed to get a WSA reference)
        // THEN
        //   this object cannot carry forward any function pointers - it must show successfully loaded == false
        winsock_extension_function_table(const winsock_extension_function_table& rhs) WI_NOEXCEPT :
            wsa_reference_count{WSAStartup_nothrow()}
        {
            if (!wsa_reference_count || !rhs.wsa_reference_count)
            {
                ::memset(&f, 0, sizeof(f));
            }
            else
            {
                ::memcpy_s(&f, sizeof(f), &rhs.f, sizeof(rhs.f));
            }
        }

        winsock_extension_function_table& operator=(const winsock_extension_function_table& rhs) WI_NOEXCEPT
        {
            if (!wsa_reference_count || !rhs.wsa_reference_count)
            {
                ::memset(&f, 0, sizeof(f));
            }
            else
            {
                ::memcpy_s(&f, sizeof(f), &rhs.f, sizeof(rhs.f));
            }

            return *this;
        }

        //! Returns true if all functions were loaded, holding a WSAStartup reference
        WI_NODISCARD explicit operator bool() const WI_NOEXCEPT
        {
            return f.AcceptEx != nullptr;
        }

        ::wil::network::WINSOCK_EXTENSION_FUNCTION_TABLE f{};

    private:
        // constructed via load()
        winsock_extension_function_table() WI_NOEXCEPT :
            wsa_reference_count{WSAStartup_nothrow()}
        {
        }

        // must guarantee Winsock does not unload while we have dynamically loaded function pointers
        const ::wil::network::unique_wsacleanup_call wsa_reference_count;
    };

    struct rio_extension_function_table
    {
        static rio_extension_function_table load() WI_NOEXCEPT;

        ~rio_extension_function_table() WI_NOEXCEPT = default;

        // can copy, but the new object needs its own WSA reference count
        // (getting a WSA reference count should be no-fail once the first reference it taken)
        //
        // IF the target could not get a WSA reference count
        //   OR
        // IF we couldn't get our own WSA reference count
        //   (this should never happen the caller has a reference, but we failed to get a WSA reference)
        // THEN
        //   this object cannot carry forward any function pointers - it must show successfully loaded == false
        rio_extension_function_table(const rio_extension_function_table& rhs) WI_NOEXCEPT :
            wsa_reference_count{WSAStartup_nothrow()}
        {
            if (!wsa_reference_count || !rhs.wsa_reference_count)
            {
                ::memset(&f, 0, sizeof(f));
            }
            else
            {
                ::memcpy_s(&f, sizeof(f), &rhs.f, sizeof(rhs.f));
            }
        }

        rio_extension_function_table& operator=(const rio_extension_function_table& rhs) WI_NOEXCEPT
        {
            if (!wsa_reference_count || !rhs.wsa_reference_count)
            {
                ::memset(&f, 0, sizeof(f));
            }
            else
            {
                ::memcpy_s(&f, sizeof(f), &rhs.f, sizeof(rhs.f));
            }

            return *this;
        }

        //! Returns true if all functions were loaded, holding a WSAStartup reference
        WI_NODISCARD explicit operator bool() const WI_NOEXCEPT
        {
            return f.RIOReceive != nullptr;
        }

        ::RIO_EXTENSION_FUNCTION_TABLE f{};

    private:
        // constructed via load()
        rio_extension_function_table() WI_NOEXCEPT :
            wsa_reference_count{WSAStartup_nothrow()}
        {
        }

        // must guarantee Winsock does not unload while we have dynamically loaded function pointers
        const ::wil::network::unique_wsacleanup_call wsa_reference_count;
    };

    //
    //! encapsulates working with the sockaddr datatype
    //!
    //! sockaddr is a generic type - similar to a base class, but designed for C with BSD sockets (1983-ish)
    //! 'derived' structures are cast back to sockaddr* (so the initial struct members must be aligned)
    //!
    //! this data type was built to be 'extensible' so new network types could create their own address structures
    //! - appending fields to the initial fields of the sockaddr
    //!
    //! note that the address and port fields of TCPIP sockaddr* types were designed to be encoded in network-byte order
    //! - hence the common use of "host-to-network" and "network-to-host" APIs, e.g. htons(), htonl(), ntohs(), ntohl()
    //!
    //! Socket APIs that accept a socket address will accept 2 fields:
    //! - the sockaddr* (the address of the derived sockaddr type, cast down to a sockaddr*)
    //! - the length of the 'derived' socket address structure referenced by the sockaddr*
    //!
    //! Commonly used sockaddr* types that are using with TCPIP networking:
    //!
    //! sockaddr_storage / SOCKADDR_STORAGE
    //!   - a sockaddr* derived type that is guaranteed to be large enough to hold any possible socket address
    //! sockaddr_in / SOCKADDR_IN
    //!   - a sockaddr* derived type designed to contain an IPv4 address and port number
    //! sockaddr_in6 / SOCKADDR_IN6
    //!   - a sockaddr* derived type designed to contain an IPv6 address, port, scope id, and flow info
    //! SOCKADDR_INET
    //!   - a union of sockaddr_in and sockaddr_in6 -- i.e., large enough to contain any TCPIP address
    //! in_addr / IN_ADDR
    //!   - the raw address portion of a sockaddr_in
    //! in6_addr / IN6_ADDR
    //!   - the raw address portion of a sockaddr_in6
    //!
    //! SOCKET_ADDRESS
    //!   - not a derived sockaddr* type
    //!   - a structure containing both a sockaddr* and its length fields, returned from some networking functions
    //!

    [[nodiscard]] inline bool equals(const in_addr& lhs, const in_addr& rhs) WI_NOEXCEPT
    {
        return lhs.s_addr == rhs.s_addr;
    }

    [[nodiscard]] inline bool not_equals(const in_addr& lhs, const in_addr& rhs) WI_NOEXCEPT
    {
        return lhs.s_addr != rhs.s_addr;
    }

    [[nodiscard]] inline bool equals(const in6_addr& lhs, const in6_addr& rhs) WI_NOEXCEPT
    {
        return 0 == ::memcmp(&lhs, &rhs, sizeof(in6_addr));
    }

    [[nodiscard]] inline bool not_equals(const in6_addr& lhs, const in6_addr& rhs) WI_NOEXCEPT
    {
        return 0 != ::memcmp(&lhs, &rhs, sizeof(in6_addr));
    }

    // declaring char-arrays large enough for any IPv4 or IPv6 address + optional fields
    static_assert(INET6_ADDRSTRLEN > INET_ADDRSTRLEN);
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
#if defined(WIL_ENABLE_EXCEPTIONS)
        explicit socket_address(PCWSTR, unsigned short port = 0);
#endif

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
#if defined(WIL_ENABLE_EXCEPTIONS)
        void set_sockaddr(SOCKET);
        void set_sockaddr(PCWSTR);
        void set_sockaddr(PCSTR);
#endif
        [[nodiscard]] HRESULT set_sockaddr_nothrow(SOCKET) WI_NOEXCEPT;
        [[nodiscard]] HRESULT set_sockaddr_nothrow(PCWSTR) WI_NOEXCEPT;
        [[nodiscard]] HRESULT set_sockaddr_nothrow(PCSTR) WI_NOEXCEPT;

        void set_port(USHORT) WI_NOEXCEPT;
        void set_scope_id(ULONG) WI_NOEXCEPT;
        void set_flow_info(ULONG) WI_NOEXCEPT;

        // set_address* preserves the existing port set on the address
        // - so that one can call set_port() and set_address() in any order with expected results
        // - the address family is preserved unless it is specified (or inferred) as an argument
        void set_address_any() WI_NOEXCEPT;
        void set_address_any(ADDRESS_FAMILY family) WI_NOEXCEPT;
        void set_address_loopback() WI_NOEXCEPT;
        void set_address_loopback(ADDRESS_FAMILY family) WI_NOEXCEPT;
        void set_address(const IN_ADDR*) WI_NOEXCEPT;
        void set_address(const IN6_ADDR*) WI_NOEXCEPT;

        // write_address prints the IP address portion, not the scope id or port
#if defined(WIL_ENABLE_EXCEPTIONS) && (defined(_STRING_) || defined(WIL_DOXYGEN))
        [[nodiscard]] ::std::wstring write_address() const;
        [[nodiscard]] ::std::wstring write_complete_address() const;
#endif
        HRESULT write_address_nothrow(socket_address_wstring& address) const WI_NOEXCEPT;
        HRESULT write_address_nothrow(socket_address_string& address) const WI_NOEXCEPT;

        // write_complete_address_nothrow() prints the IP address as well as the scope id and port values
        HRESULT write_complete_address_nothrow(socket_address_wstring& address) const WI_NOEXCEPT;
        HRESULT write_complete_address_nothrow(socket_address_string& address) const WI_NOEXCEPT;

        // type: NlatUnspecified ('any'), NlatUnicast, NlatAnycast, NlatMulticast, NlatBroadcast, NlatInvalid
        [[nodiscard]] NL_ADDRESS_TYPE address_type() const WI_NOEXCEPT;
        [[nodiscard]] bool is_address_linklocal() const WI_NOEXCEPT;
        [[nodiscard]] bool is_address_loopback() const WI_NOEXCEPT;

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

        static constexpr int length{sizeof(::SOCKADDR_INET)};

    private:
        SOCKADDR_INET m_sockaddr{};
    };

    // When using dual-mode sockets, one might need to connect to a target IPv4 address.
    // Since dual-mode socket types are IPv6, one must map that IPv4 address to its 'mapped' IPv6 address
    inline wil::network::socket_address map_dual_mode_4to6(const wil::network::socket_address& inV4) WI_NOEXCEPT
    {
        constexpr IN6_ADDR v4MappedPrefix{{IN6ADDR_V4MAPPEDPREFIX_INIT}};

        wil::network::socket_address outV6{&v4MappedPrefix, inV4.port()};

        auto* const pIn6Addr{outV6.in6_addr()};
        const auto* const pIn4Addr{inV4.in_addr()};
        pIn6Addr->u.Byte[12] = pIn4Addr->S_un.S_un_b.s_b1;
        pIn6Addr->u.Byte[13] = pIn4Addr->S_un.S_un_b.s_b2;
        pIn6Addr->u.Byte[14] = pIn4Addr->S_un.S_un_b.s_b3;
        pIn6Addr->u.Byte[15] = pIn4Addr->S_un.S_un_b.s_b4;

        return outV6;
    }

    //
    // non-member swap
    //
    inline void swap(wil::network::socket_address& lhs, wil::network::socket_address& rhs) WI_NOEXCEPT
    {
        lhs.swap(rhs);
    }

    //! class addr_info encapsulates the ADDRINFO structure
    //! this structure contains a linked list of addresses returned from resolving a name via GetAddrInfo
    //! iterator semantics are supported to safely access these addresses
    //! supports all addrinfo types: ADDRINFOA, ADDRINFOW, ADDRINFOEXA, ADDRINFOEXW
template <typename T>
    class addr_info_t
    {
    public:
        addr_info_t(_In_ T* addrResult) WI_NOEXCEPT : m_addrResult{addrResult}
        {
        }

        // the d'tor calls the free function matching the addrinfo* type T
        ~addr_info_t() WI_NOEXCEPT;
        addr_info_t(const addr_info_t&) = delete;
        addr_info_t& operator=(const addr_info_t&) = delete;

        addr_info_t(addr_info_t&& rhs) WI_NOEXCEPT
        {
            m_addrResult = rhs.m_addrResult;
            rhs.m_addrResult = nullptr;
        }

        addr_info_t& operator=(addr_info_t&& rhs) WI_NOEXCEPT
        {
            if (this != &rhs)
            {
                if (m_addrResult)
                {
                    ::FreeAddrInfoW(m_addrResult);
                }
                m_addrResult = rhs.m_addrResult;
                rhs.m_addrResult = nullptr;
            }

            return *this;
        }

        class iterator
        {
        public:
            // defining iterator_traits allows STL <algorithm> functions to be used with this iterator class.
            // Notice this is a forward_iterator
            // - does not support random-access (e.g. vector::iterator)
            // - does not support bidirectional access (e.g. list::iterator)
#if defined(_ITERATOR_) || defined(WIL_DOXYGEN)
            using iterator_category = ::std::forward_iterator_tag;
#endif
            using value_type = wil::network::socket_address;
            using difference_type = size_t;
            using distance_type = size_t;
            using pointer = wil::network::socket_address*;
            using reference = wil::network::socket_address&;

            iterator(const ADDRINFOW* addrinfo) WI_NOEXCEPT :
                m_addr_info(const_cast<ADDRINFOW*>(addrinfo))
            {
                // must const cast so we can re-use this pointer as we walk the list
                if (m_addr_info)
                {
                    m_socket_address.set_sockaddr(m_addr_info->ai_addr, m_addr_info->ai_addrlen);
                }
            }

            ~iterator() WI_NOEXCEPT = default;
            iterator(const iterator&) WI_NOEXCEPT = default;
            iterator& operator=(const iterator&) WI_NOEXCEPT = default;
            iterator(iterator&&) WI_NOEXCEPT = default;
            iterator& operator=(iterator&&) WI_NOEXCEPT = default;

            const wil::network::socket_address& operator*() const WI_NOEXCEPT
            {
                return m_socket_address;
            }

            const wil::network::socket_address& operator*() WI_NOEXCEPT
            {
                return m_socket_address;
            }

            const wil::network::socket_address* operator->() const WI_NOEXCEPT
            {
                return &m_socket_address;
            }

            const wil::network::socket_address* operator->() WI_NOEXCEPT
            {
                return &m_socket_address;
            }

            [[nodiscard]] bool operator==(const iterator& rhs) const WI_NOEXCEPT
            {
                return m_addr_info == rhs.m_addr_info;
            }

            [[nodiscard]] bool operator!=(const iterator& rhs) const WI_NOEXCEPT
            {
                return !(*this == rhs);
            }

            iterator& operator++() WI_NOEXCEPT
            {
                this->operator+=(1);
                return *this;
            }

            iterator operator++(int) WI_NOEXCEPT
            {
                auto return_value = *this;
                this->operator+=(1);
                return return_value;
            }

            iterator& operator+=(size_t offset) WI_NOEXCEPT
            {
                for (size_t count = 0; count < offset; ++count)
                {
                    WI_ASSERT(m_addr_info);
                    if (m_addr_info)
                    {
                        m_addr_info = m_addr_info->ai_next;
                        if (m_addr_info)
                        {
                            m_socket_address.set_sockaddr(m_addr_info->ai_addr, m_addr_info->ai_addrlen);
                        }
                        else
                        {
                            m_socket_address.reset();
                        }
                    }
                }

                return *this;
            }

        private:
            // non-ownership of this pointer - the parent class must outlive the iterator
            T* m_addr_info{nullptr};
            wil::network::socket_address m_socket_address{};
        };

        iterator begin() const WI_NOEXCEPT
        {
            return {m_addrResult};
        }

        iterator end() const WI_NOEXCEPT
        {
            return {nullptr};
        }

    private:
        T* m_addrResult{};
    };

    typedef network::addr_info_t<ADDRINFOA> addr_info_a;
    template<>
    inline addr_info_t<ADDRINFOA>::~addr_info_t() WI_NOEXCEPT
    {
        if (m_addrResult)
        {
            ::freeaddrinfo(m_addrResult);
        }
    }
    typedef network::addr_info_t<ADDRINFOW> addr_info;
    template<>
    inline addr_info_t<ADDRINFOW>::~addr_info_t() WI_NOEXCEPT
    {
        if (m_addrResult)
        {
            ::FreeAddrInfoW(m_addrResult);
        }
    }
    // the Winsock headers require having set this #define to access ANSI-string versions of the Winsock API
#if defined(_WINSOCK_DEPRECATED_NO_WARNINGS)
    typedef network::addr_info_t<ADDRINFOEXA> addr_info_exa;
    template<>
    inline addr_info_t<ADDRINFOEXA>::~addr_info_t() WI_NOEXCEPT
    {
        if (m_addrResult)
        {
            ::FreeAddrInfoEx(m_addrResult);
        }
    }
#endif
    typedef network::addr_info_t<ADDRINFOEXW> addr_infoex;
    template<>
    inline addr_info_t<ADDRINFOEXW>::~addr_info_t() WI_NOEXCEPT
    {
        if (m_addrResult)
        {
            ::FreeAddrInfoExW(m_addrResult);
        }
    }


#if defined(WIL_ENABLE_EXCEPTIONS)
    //! wil function to capture resolving a name to a set of IP addresses, throwing on error
    //! returning an RAII object containing the results
    //! the returned RAII object exposes iterator semantics to walk the results
    inline ::wil::network::addr_info resolve_name(_In_ PCWSTR name)
    {
        ADDRINFOW* addrResult{};
        if (0 != ::GetAddrInfoW(name, nullptr, nullptr, &addrResult))
        {
            THROW_WIN32(::WSAGetLastError());
        }

        return {addrResult};
    }

    //! wil function to capture resolving the local machine to its set of IP addresses, throwing on error
    //! returning an RAII object containing the results
    //! the returned RAII object exposes iterator semantics to walk the results
    inline ::wil::network::addr_info resolve_local_addresses() WI_NOEXCEPT
    {
        return ::wil::network::resolve_name(L"");
    }

    //! wil function to capture resolving the local-host addresses, throwing on error
    //! returning an RAII object containing the results
    //! the returned RAII object exposes iterator semantics to walk the results
    inline ::wil::network::addr_info resolve_localhost_addresses() WI_NOEXCEPT
    {
        return ::wil::network::resolve_name(L"localhost");
    }
#endif

    //
    // socket_address definitions
    //
    inline wil::network::socket_address::socket_address(ADDRESS_FAMILY family) WI_NOEXCEPT
    {
        reset(family);
    }

    template <typename T>
    wil::network::socket_address::socket_address(_In_reads_bytes_(inLength) const SOCKADDR* addr, T inLength) WI_NOEXCEPT
    {
        set_sockaddr(addr, inLength);
    }

    inline wil::network::socket_address::socket_address(const SOCKADDR_IN* addr) WI_NOEXCEPT
    {
        set_sockaddr(addr);
    }

    inline wil::network::socket_address::socket_address(const SOCKADDR_IN6* addr) WI_NOEXCEPT
    {
        set_sockaddr(addr);
    }

    inline wil::network::socket_address::socket_address(const SOCKADDR_INET* addr) WI_NOEXCEPT
    {
        set_sockaddr(addr);
    }

    inline wil::network::socket_address::socket_address(const SOCKET_ADDRESS* addr) WI_NOEXCEPT
    {
        set_sockaddr(addr);
    }

    inline wil::network::socket_address::socket_address(const IN_ADDR* addr, unsigned short port) WI_NOEXCEPT
    {
        reset(AF_INET);
        set_address(addr);
        set_port(port);
    }

    inline wil::network::socket_address::socket_address(const IN6_ADDR* addr, unsigned short port) WI_NOEXCEPT
    {
        reset(AF_INET6);
        set_address(addr);
        set_port(port);
    }

#if defined(WIL_ENABLE_EXCEPTIONS)
    inline wil::network::socket_address::socket_address(PCWSTR addr, unsigned short port)
    {
        set_sockaddr(addr);
        set_port(port);
    }
#endif
    inline bool wil::network::socket_address::operator==(const wil::network::socket_address& rhs) const WI_NOEXCEPT
    {
        const auto& lhs{*this};

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

    inline bool wil::network::socket_address::operator!=(const wil::network::socket_address& rhs) const WI_NOEXCEPT
    {
        return !(*this == rhs);
    }

    inline bool wil::network::socket_address::operator<(const wil::network::socket_address& rhs) const WI_NOEXCEPT
    {
        const auto& lhs{*this};

        if (lhs.family() != rhs.family())
        {
            return lhs.family() < rhs.family();
        }

        // for operator<, we cannot just memcmp the raw sockaddr values - as they are in network-byte order
        // we have to first convert back to host-byte order to do comparisons
        // else the user will see odd behavior, like 1.1.1.1 < 0.0.0.0 (which won't make senses)
        switch (lhs.family())
        {
        case AF_INET:
        {
            // compare the address first
            auto comparison{::memcmp(lhs.in_addr(), rhs.in_addr(), sizeof(IN_ADDR))};
            if (comparison != 0)
            {
                return comparison < 0;
            }

            // then compare the port (host-byte-order)
            // only resolve the ntohs() once
            const auto lhs_port{lhs.port()};
            const auto rhs_port{rhs.port()};
            if (lhs_port != rhs_port)
            {
                return lhs_port < rhs_port;
            }

            return true;
        }

        case AF_INET6:
        {
            // compare the address first
            auto comparison{::memcmp(lhs.in6_addr(), rhs.in6_addr(), sizeof(IN6_ADDR))};
            if (comparison != 0)
            {
                return comparison < 0;
            }

            // then compare the port (host-byte-order)
            // only resolve the ntohs() once
            const auto lhs_port{lhs.port()};
            const auto rhs_port{rhs.port()};
            if (lhs_port != rhs_port)
            {
                return lhs_port < rhs_port;
            }

            // then compare the scope_id of the address
            const auto lhs_scope_id{lhs.scope_id()};
            const auto rhs_scope_id{rhs.scope_id()};
            if (lhs_scope_id != rhs_scope_id)
            {
                return lhs_scope_id < rhs_scope_id;
            }

            // then compare flow_info
            const auto lhs_flow_info{lhs.flow_info()};
            const auto rhs_flow_info{rhs.flow_info()};
            if (lhs_flow_info != rhs_flow_info)
            {
                return lhs_flow_info < rhs_flow_info;
            }

            return true;
        }

        default:
            // if not AF_INET or AF_INET6, and families don't match
            // then just raw memcmp the largest field of the union (v6)
            return ::memcmp(&lhs.m_sockaddr.Ipv6, &rhs.m_sockaddr.Ipv6, sizeof(SOCKADDR_IN6)) < 0;
        }
    }

    inline bool wil::network::socket_address::operator>(const wil::network::socket_address& rhs) const WI_NOEXCEPT
    {
        return !(*this < rhs);
    }

    inline void wil::network::socket_address::swap(socket_address& addr) WI_NOEXCEPT
    {
        SOCKADDR_INET tempAddr{};
        ::memcpy_s(&tempAddr, sizeof(tempAddr), &addr.m_sockaddr, sizeof(addr.m_sockaddr));
        ::memcpy_s(&addr.m_sockaddr, sizeof(addr.m_sockaddr), &m_sockaddr, sizeof(m_sockaddr));
        ::memcpy_s(&m_sockaddr, sizeof(m_sockaddr), &tempAddr, sizeof(tempAddr));
    }

    inline void wil::network::socket_address::reset(ADDRESS_FAMILY family) WI_NOEXCEPT
    {
#ifndef WI_NETWORK_TEST
        WI_ASSERT(family == AF_UNSPEC || family == AF_INET || family == AF_INET6);
#endif
        ::memset(&m_sockaddr, 0, wil::network::socket_address::length);
        m_sockaddr.si_family = family;
    }

    template <typename T>
    void wil::network::socket_address::set_sockaddr(_In_reads_bytes_(inLength) const SOCKADDR* addr, T inLength) WI_NOEXCEPT
    {
        WI_ASSERT(static_cast<size_t>(inLength) <= wil::network::socket_address::length);

        ::memset(&m_sockaddr, 0, wil::network::socket_address::length);
        if (addr)
        {
            ::memcpy_s(&m_sockaddr, sizeof(m_sockaddr), addr, inLength);
        }
    }

    inline void wil::network::socket_address::set_sockaddr(const SOCKADDR_IN* addr) WI_NOEXCEPT
    {
        ::memset(&m_sockaddr, 0, wil::network::socket_address::length);
        ::memcpy_s(&m_sockaddr.Ipv4, sizeof(m_sockaddr.Ipv4), addr, sizeof(*addr));
    }

    inline void wil::network::socket_address::set_sockaddr(const SOCKADDR_IN6* addr) WI_NOEXCEPT
    {
        ::memset(&m_sockaddr, 0, wil::network::socket_address::length);
        ::memcpy_s(&m_sockaddr.Ipv6, sizeof(m_sockaddr.Ipv6), addr, sizeof(*addr));
    }

    inline void wil::network::socket_address::set_sockaddr(const SOCKADDR_INET* addr) WI_NOEXCEPT
    {
        ::memset(&m_sockaddr, 0, wil::network::socket_address::length);
        ::memcpy_s(&m_sockaddr, sizeof(m_sockaddr), addr, sizeof(*addr));
    }

    inline void wil::network::socket_address::set_sockaddr(const SOCKET_ADDRESS* addr) WI_NOEXCEPT
    {
        FAIL_FAST_IF_MSG(
            addr->lpSockaddr && addr->iSockaddrLength > wil::network::socket_address::length,
            "SOCKET_ADDRESS contains an unsupported sockaddr type - larger than an IPv4 or IPv6 address (%d)",
            addr->iSockaddrLength);

        ::memset(&m_sockaddr, 0, wil::network::socket_address::length);
        if (addr->lpSockaddr)
        {
            ::memcpy_s(&m_sockaddr, sizeof(m_sockaddr), addr->lpSockaddr, addr->iSockaddrLength);
        }
    }

    inline bool wil::network::socket_address::is_address_linklocal() const WI_NOEXCEPT
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

    inline bool wil::network::socket_address::is_address_loopback() const WI_NOEXCEPT
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

    inline NL_ADDRESS_TYPE wil::network::socket_address::address_type() const WI_NOEXCEPT
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

    inline void wil::network::socket_address::set_port(USHORT port) WI_NOEXCEPT
    {
        WI_ASSERT(family() == AF_INET || family() == AF_INET6);

        // the port value is at the exact same offset with both the IPv4 and IPv6 unions
        static_assert(FIELD_OFFSET(SOCKADDR_INET, Ipv4.sin_port) == FIELD_OFFSET(SOCKADDR_INET, Ipv6.sin6_port));

        // port values in a sockaddr are always in network-byte order
        m_sockaddr.Ipv4.sin_port = ::htons(port);
    }

    inline void wil::network::socket_address::set_scope_id(ULONG scopeId) WI_NOEXCEPT
    {
        WI_ASSERT(family() == AF_INET6);

        if (family() == AF_INET6)
        {
            m_sockaddr.Ipv6.sin6_scope_id = ::htonl(scopeId);
        }
    }

    inline void wil::network::socket_address::set_flow_info(ULONG flowInfo) WI_NOEXCEPT
    {
        WI_ASSERT(family() == AF_INET6);

        if (family() == AF_INET6)
        {
            m_sockaddr.Ipv6.sin6_flowinfo = ::htonl(flowInfo);
        }
    }

    inline void wil::network::socket_address::set_address_any() WI_NOEXCEPT
    {
        set_address_any(family());
    }

    inline void wil::network::socket_address::set_address_any(ADDRESS_FAMILY family) WI_NOEXCEPT
    {
        WI_ASSERT(family == AF_INET || family == AF_INET6);

        // the port value is at the exact same offset with both the IPv4 and IPv6 unions
        static_assert(FIELD_OFFSET(SOCKADDR_INET, Ipv4.sin_port) == FIELD_OFFSET(SOCKADDR_INET, Ipv6.sin6_port));

        const auto original_port{m_sockaddr.Ipv4.sin_port};
        reset(family);
        m_sockaddr.Ipv4.sin_port = original_port;
    }

    inline void wil::network::socket_address::set_address_loopback() WI_NOEXCEPT
    {
        set_address_loopback(family());
    }

    inline void wil::network::socket_address::set_address_loopback(ADDRESS_FAMILY family) WI_NOEXCEPT
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

    inline void wil::network::socket_address::set_address(const IN_ADDR* addr) WI_NOEXCEPT
    {
        WI_ASSERT(family() == AF_INET);

        const auto original_port{m_sockaddr.Ipv4.sin_port};
        reset(AF_INET);
        m_sockaddr.Ipv4.sin_addr.s_addr = addr->s_addr;
        m_sockaddr.Ipv4.sin_port = original_port;
    }

    inline void wil::network::socket_address::set_address(const IN6_ADDR* addr) WI_NOEXCEPT
    {
        WI_ASSERT(family() == AF_INET6);

        const auto original_port{m_sockaddr.Ipv6.sin6_port};
        reset(AF_INET6);
        m_sockaddr.Ipv6.sin6_addr = *addr;
        m_sockaddr.Ipv6.sin6_port = original_port;
    }

#if defined(WIL_ENABLE_EXCEPTIONS)
    inline void wil::network::socket_address::set_sockaddr(SOCKET s)
    {
        THROW_IF_FAILED(set_sockaddr_nothrow(s));
    }

    inline void wil::network::socket_address::set_sockaddr(PCWSTR address)
    {
        THROW_IF_FAILED(set_sockaddr_nothrow(address));
    }

    inline void wil::network::socket_address::set_sockaddr(PCSTR address)
    {
        THROW_IF_FAILED(set_sockaddr_nothrow(address));
    }
#endif

    inline HRESULT wil::network::socket_address::set_sockaddr_nothrow(SOCKET s) WI_NOEXCEPT
    {
        reset(AF_UNSPEC);

        auto nameLength{wil::network::socket_address::length};
        auto error{::getsockname(s, sockaddr(), &nameLength)};
        if (error != 0)
        {
            error = ::WSAGetLastError();
            RETURN_WIN32(error);
        }
        return S_OK;
    }

    inline HRESULT wil::network::socket_address::set_sockaddr_nothrow(PCWSTR address) WI_NOEXCEPT
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

        reset(AF_UNSPEC);
        return E_INVALIDARG;
    }

    inline HRESULT wil::network::socket_address::set_sockaddr_nothrow(PCSTR address) WI_NOEXCEPT
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

        reset(AF_UNSPEC);
        return E_INVALIDARG;
    }

#if defined(WIL_ENABLE_EXCEPTIONS) && (defined(_STRING_) || defined(WIL_DOXYGEN))
    inline ::std::wstring wil::network::socket_address::write_address() const
    {
        wil::network::socket_address_wstring returnString{};
        THROW_IF_FAILED(write_address_nothrow(returnString));
        returnString[INET6_ADDRSTRLEN - 1] = L'\0';
        return returnString;
    }
#endif

    inline HRESULT wil::network::socket_address::write_address_nothrow(socket_address_wstring& address) const WI_NOEXCEPT
    {
        ::memset(address, 0, sizeof(socket_address_wstring));

        // socket_address will return an empty string for AF_UNSPEC (the default family type when constructed)
        // as to not fail if just default constructed
        if (family() == AF_UNSPEC)
        {
            return S_OK;
        }

        const void* const pAddr{family() == AF_INET
                                    ? static_cast<const void*>(&m_sockaddr.Ipv4.sin_addr)
                                    : static_cast<const void*>(&m_sockaddr.Ipv6.sin6_addr)};

        // the last param to InetNtopW is # of characters, not bytes
        const auto* error_value{::InetNtopW(family(), pAddr, address, INET6_ADDRSTRLEN)};
        if (error_value == nullptr)
        {
            RETURN_WIN32(::WSAGetLastError());
        }
        return S_OK;
    }

    inline HRESULT wil::network::socket_address::write_address_nothrow(socket_address_string& address) const WI_NOEXCEPT
    {
        ::memset(address, 0, sizeof(socket_address_string));

        // socket_address will return an empty string for AF_UNSPEC (the default family type when constructed)
        // as to not fail if just default constructed
        if (family() == AF_UNSPEC)
        {
            return S_OK;
        }

        const void* const pAddr{family() == AF_INET
                                    ? static_cast<const void*>(&m_sockaddr.Ipv4.sin_addr)
                                    : static_cast<const void*>(&m_sockaddr.Ipv6.sin6_addr)};

        // the last param to InetNtopA is # of characters, not bytes
        const auto* error_value{::InetNtopA(family(), pAddr, address, INET6_ADDRSTRLEN)};
        if (error_value == nullptr)
        {
            RETURN_WIN32(::WSAGetLastError());
        }
        return S_OK;
    }

#if defined(WIL_ENABLE_EXCEPTIONS) && (defined(_STRING_) || defined(WIL_DOXYGEN))
    inline ::std::wstring wil::network::socket_address::write_complete_address() const
    {
        wil::network::socket_address_wstring returnString{};
        THROW_IF_FAILED(write_complete_address_nothrow(returnString));
        returnString[INET6_ADDRSTRLEN - 1] = L'\0';
        return returnString;
    }
#endif

    inline HRESULT wil::network::socket_address::write_complete_address_nothrow(
        socket_address_wstring& address) const WI_NOEXCEPT
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
        if (::WSAAddressToStringW(
                const_cast<SOCKADDR*>(sockaddr()),
                wil::network::socket_address::length,
                nullptr,
                address,
                &addressLength) != 0)
        {
            RETURN_WIN32(::WSAGetLastError());
        }
        return S_OK;
    }

    // the Winsock headers require having set this #define to access ANSI-string versions of the Winsock API
#if defined(_WINSOCK_DEPRECATED_NO_WARNINGS)
    inline HRESULT wil::network::socket_address::write_complete_address_nothrow(socket_address_string& address) const WI_NOEXCEPT
    {
        ::memset(address, 0, sizeof(socket_address_string));

        // socket_address will return an empty string for AF_UNSPEC (the default family type when constructed)
        // as to not fail if just default constructed
        if (family() == AF_UNSPEC)
        {
            return S_OK;
        }

        DWORD addressLength{INET6_ADDRSTRLEN};
        if (::WSAAddressToStringA(
            const_cast<SOCKADDR*>(sockaddr()),
            wil::network::socket_address::length,
            nullptr,
            address,
            &addressLength) != 0)
        {
            RETURN_WIN32(::WSAGetLastError());
        }
        return S_OK;
    }
#endif

    inline ADDRESS_FAMILY wil::network::socket_address::family() const WI_NOEXCEPT
    {
        return m_sockaddr.si_family;
    }

    inline USHORT wil::network::socket_address::port() const WI_NOEXCEPT
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

    inline ULONG wil::network::socket_address::flow_info() const WI_NOEXCEPT
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

    inline ULONG wil::network::socket_address::scope_id() const WI_NOEXCEPT
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

    inline SOCKADDR* wil::network::socket_address::sockaddr() WI_NOEXCEPT
    {
        return reinterpret_cast<SOCKADDR*>(&m_sockaddr);
    }

    inline SOCKADDR_IN* wil::network::socket_address::sockaddr_in() WI_NOEXCEPT
    {
        WI_ASSERT(family() == AF_INET);
        return &m_sockaddr.Ipv4;
    }

    inline SOCKADDR_IN6* wil::network::socket_address::sockaddr_in6() WI_NOEXCEPT
    {
        WI_ASSERT(family() == AF_INET6);
        return &m_sockaddr.Ipv6;
    }

    inline SOCKADDR_INET* wil::network::socket_address::sockaddr_inet() WI_NOEXCEPT
    {
        return &m_sockaddr;
    }

    inline IN_ADDR* wil::network::socket_address::in_addr() WI_NOEXCEPT
    {
        WI_ASSERT(family() == AF_INET);
        return &m_sockaddr.Ipv4.sin_addr;
    }

    inline IN6_ADDR* wil::network::socket_address::in6_addr() WI_NOEXCEPT
    {
        WI_ASSERT(family() == AF_INET6);
        return &m_sockaddr.Ipv6.sin6_addr;
    }

    inline const SOCKADDR* wil::network::socket_address::sockaddr() const WI_NOEXCEPT
    {
        return reinterpret_cast<const SOCKADDR*>(&m_sockaddr);
    }

    inline const SOCKADDR_IN* wil::network::socket_address::sockaddr_in() const WI_NOEXCEPT
    {
        WI_ASSERT(family() == AF_INET);
        return &m_sockaddr.Ipv4;
    }

    inline const SOCKADDR_IN6* wil::network::socket_address::sockaddr_in6() const WI_NOEXCEPT
    {
        WI_ASSERT(family() == AF_INET6);
        return &m_sockaddr.Ipv6;
    }

    inline const SOCKADDR_INET* wil::network::socket_address::sockaddr_inet() const WI_NOEXCEPT
    {
        return &m_sockaddr;
    }

    inline const IN_ADDR* wil::network::socket_address::in_addr() const WI_NOEXCEPT
    {
        WI_ASSERT(family() == AF_INET);
        return &m_sockaddr.Ipv4.sin_addr;
    }

    inline const IN6_ADDR* wil::network::socket_address::in6_addr() const WI_NOEXCEPT
    {
        WI_ASSERT(family() == AF_INET6);
        return &m_sockaddr.Ipv6.sin6_addr;
    }

    inline winsock_extension_function_table winsock_extension_function_table::load() WI_NOEXCEPT
    {
        winsock_extension_function_table table;
        // if WSAStartup failed, immediately exit
        if (!table.wsa_reference_count)
        {
            return table;
        }

        // we need a temporary socket for the IOCTL to load the functions
        const wil::unique_socket localSocket{::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)};
        if (INVALID_SOCKET == localSocket.get())
        {
            return table;
        }

        const auto load_function_pointer = [](SOCKET lambdaSocket, GUID extensionGuid, void* functionPtr) WI_NOEXCEPT {
            constexpr DWORD controlCode{SIO_GET_EXTENSION_FUNCTION_POINTER};
            constexpr DWORD bytes{sizeof(functionPtr)};
            DWORD unused_bytes{};
            const auto error{::WSAIoctl(
                lambdaSocket,
                controlCode,
                &extensionGuid,
                DWORD{sizeof(extensionGuid)},
                functionPtr,
                bytes,
                &unused_bytes,
                nullptr,
                nullptr)};
            return error == 0 ? S_OK : HRESULT_FROM_WIN32(::WSAGetLastError());
        };

        // Load the functions into the table
        if (FAILED_LOG(load_function_pointer(localSocket.get(), WSAID_ACCEPTEX, &table.f.AcceptEx)) ||
            FAILED_LOG(load_function_pointer(localSocket.get(), WSAID_CONNECTEX, &table.f.ConnectEx)) ||
            FAILED_LOG(load_function_pointer(localSocket.get(), WSAID_DISCONNECTEX, &table.f.DisconnectEx)) ||
            FAILED_LOG(load_function_pointer(localSocket.get(), WSAID_GETACCEPTEXSOCKADDRS, &table.f.GetAcceptExSockaddrs)) ||
            FAILED_LOG(load_function_pointer(localSocket.get(), WSAID_TRANSMITFILE, &table.f.TransmitFile)) ||
            FAILED_LOG(load_function_pointer(localSocket.get(), WSAID_TRANSMITPACKETS, &table.f.TransmitPackets)) ||
            FAILED_LOG(load_function_pointer(localSocket.get(), WSAID_WSARECVMSG, &table.f.WSARecvMsg)) ||
            FAILED_LOG(load_function_pointer(localSocket.get(), WSAID_WSASENDMSG, &table.f.WSASendMsg)))
        {
            // if any failed to be found, something is very broken
            // all should load, or all should fail
            ::memset(&table.f, 0, sizeof(table.f));
        }

        return table;
    }

    //
    // definitions for rio_extension_function_table
    //
    inline rio_extension_function_table rio_extension_function_table::load() WI_NOEXCEPT
    {
        rio_extension_function_table table{};
        // if WSAStartup failed, immediately exit
        if (!table.wsa_reference_count)
        {
            return table;
        }

        // we need a temporary socket for the IOCTL to load the functions
        const ::wil::unique_socket localSocket{::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)};
        if (INVALID_SOCKET == localSocket.get())
        {
            return table;
        }

        constexpr DWORD controlCode{SIO_GET_MULTIPLE_EXTENSION_FUNCTION_POINTER};
        constexpr DWORD bytes{sizeof(table.f)};
        GUID rioGuid = WSAID_MULTIPLE_RIO;

        ::memset(&table.f, 0, bytes);
        table.f.cbSize = bytes;

        DWORD unused_bytes{};
        if (::WSAIoctl(
                localSocket.get(),
                controlCode,
                &rioGuid,
                DWORD{sizeof(rioGuid)},
                &table.f,
                bytes,
                &unused_bytes,
                nullptr,
                nullptr) != 0)
        {
            LOG_IF_WIN32_ERROR(::WSAGetLastError());

            ::memset(&table.f, 0, bytes);
        }
        return table;
    }

} // namespace network
} // namespace wil

#endif // __WIL_NETWORK_INCLUDED