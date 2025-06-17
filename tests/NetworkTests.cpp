#include "pch.h"

#include <cstdint>
#include <iterator>
#include <algorithm>
#include <numeric>
#include <string>

// define this macro to printf sockaddr strings to the console
// as tests are running to verify correctness by hand
// #define PRINTF_SOCKET_ADDRESSES

// set this to give access to all functions
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define WI_NETWORK_TEST
#include <wil/network.h>

#include "common.h"

constexpr char Test_in_addr_char_string[] = "1.1.1.1";
constexpr wchar_t Test_in_addr_string[] = L"1.1.1.1";
static in_addr Test_in_addr{};
constexpr char Test_in_addr_char_string2[] = "1.1.1.2";
constexpr wchar_t Test_in_addr_string2[] = L"1.1.1.2";
static in_addr Test_in_addr2{};
constexpr char Test_in6_addr_char_string[] = "2001::1:1:1:1";
constexpr wchar_t Test_in6_addr_string[] = L"2001::1:1:1:1";
static in6_addr Test_in6_addr{};
constexpr char Test_in6_addr_char_string2[] = "2001::1:1:1:2";
constexpr wchar_t Test_in6_addr_string2[] = L"2001::1:1:1:2";
static in6_addr Test_in6_addr2{};

constexpr wchar_t Test_linklocal_in_addr_string[] = L"169.254.111.222";
static in_addr Test_linklocal_in_addr{};
constexpr wchar_t Test_linklocal_in6_addr_string[] = L"fe80::1:1:1111:2222";
static in6_addr Test_linklocal_in6_addr{};

constexpr char Test_any_in_addr_char_string[] = "0.0.0.0";
constexpr wchar_t Test_any_in_addr_string[] = L"0.0.0.0";
constexpr char Test_any_in_addr_char_string_with_port[] = "0.0.0.0:12345";
constexpr wchar_t Test_any_in_addr_string_with_port[] = L"0.0.0.0:12345";
static in_addr Test_any_in_addr{};
constexpr char Test_any_in6_addr_char_string[] = "::";
constexpr wchar_t Test_any_in6_addr_string[] = L"::";
constexpr char Test_any_in6_addr_char_string_with_port[] = "[::]:12345";
constexpr wchar_t Test_any_in6_addr_string_with_port[] = L"[::]:12345";
static in6_addr Test_any_in6_addr{};

constexpr wchar_t Test_loopback_in_addr_string[] = L"127.0.0.1";
constexpr wchar_t Test_loopback_in_addr_string_with_port[] = L"127.0.0.1:12345";
static in_addr Test_loopback_in_addr{};
constexpr wchar_t Test_loopback_in6_addr_string[] = L"::1";
constexpr wchar_t Test_loopback_in6_addr_string_with_port[] = L"[::1]:12345";
static in6_addr Test_loopback_in6_addr{};

constexpr uint16_t TestPort = 12345;

static INIT_ONCE SocketTestInit = INIT_ONCE_STATIC_INIT;

static BOOL WINAPI InitializeAllStrings(PINIT_ONCE, PVOID, PVOID*)
{
    LPCWSTR terminator{};
    auto error = RtlIpv4StringToAddressW(Test_in_addr_string, TRUE, &terminator, &Test_in_addr);
    if (error != ERROR_SUCCESS)
    {
        return FALSE;
    }
    error = RtlIpv4StringToAddressW(Test_in_addr_string2, TRUE, &terminator, &Test_in_addr2);
    if (error != ERROR_SUCCESS)
    {
        return FALSE;
    }

    error = RtlIpv6StringToAddressW(Test_in6_addr_string, &terminator, &Test_in6_addr);
    if (error != ERROR_SUCCESS)
    {
        return FALSE;
    }
    error = RtlIpv6StringToAddressW(Test_in6_addr_string2, &terminator, &Test_in6_addr2);
    if (error != ERROR_SUCCESS)
    {
        return FALSE;
    }

    error = RtlIpv4StringToAddressW(Test_linklocal_in_addr_string, TRUE, &terminator, &Test_linklocal_in_addr);
    if (error != ERROR_SUCCESS)
    {
        return FALSE;
    }

    error = RtlIpv6StringToAddressW(Test_linklocal_in6_addr_string, &terminator, &Test_linklocal_in6_addr);
    if (error != ERROR_SUCCESS)
    {
        return FALSE;
    }

    error = RtlIpv4StringToAddressW(Test_any_in_addr_string, TRUE, &terminator, &Test_any_in_addr);
    if (error != ERROR_SUCCESS)
    {
        return FALSE;
    }

    error = RtlIpv6StringToAddressW(Test_any_in6_addr_string, &terminator, &Test_any_in6_addr);
    if (error != ERROR_SUCCESS)
    {
        return FALSE;
    }

    error = RtlIpv4StringToAddressW(Test_loopback_in_addr_string, TRUE, &terminator, &Test_loopback_in_addr);
    if (error != ERROR_SUCCESS)
    {
        return FALSE;
    }

    error = RtlIpv6StringToAddressW(Test_loopback_in6_addr_string, &terminator, &Test_loopback_in6_addr);
    if (error != ERROR_SUCCESS)
    {
        return FALSE;
    }

    return TRUE;
}
static void InitTestAddresses()
{
    InitOnceExecuteOnce(&SocketTestInit, InitializeAllStrings, nullptr, nullptr);
}

TEST_CASE("NetworkingTests::Verifying_wsastartup_cleanup", "[networking]")
{
    // verify socket APIs fail without having called WSAStartup
    // i.e., WSAStartup was not called elsewhere in the test app
    //       since that would break the preconditions of this test case
    const auto verify_socket_test = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    const auto verify_gle = ::WSAGetLastError();
    REQUIRE(verify_socket_test == INVALID_SOCKET);
    REQUIRE(verify_gle == WSANOTINITIALISED);

    SECTION("Verifying _nothrow")
    {
        const auto cleanup = wil::network::WSAStartup_nothrow();
        const bool succeeded = !!cleanup;
        REQUIRE(succeeded);
        const auto socket_test = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        REQUIRE(socket_test != INVALID_SOCKET);
        ::closesocket(socket_test);
    }

    SECTION("Verifying _failfast")
    {
        const auto cleanup = wil::network::WSAStartup_failfast();
        const auto socket_test = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        REQUIRE(socket_test != INVALID_SOCKET);
        ::closesocket(socket_test);
    }

#ifdef WIL_ENABLE_EXCEPTIONS
    SECTION("Verifying throwing")
    {
        const auto cleanup = wil::network::WSAStartup();
        const auto socket_test = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        REQUIRE(socket_test != INVALID_SOCKET);
        ::closesocket(socket_test);
    }
#endif
}

TEST_CASE("NetworkingTests::Verifying_constructors", "[networking]")
{
    using wil::network::socket_address;
    InitTestAddresses();

    SECTION("socket_address(ADDRESS_FAMILY)")
    {
        socket_address default_addr;
        REQUIRE(default_addr.family() == AF_UNSPEC);
        REQUIRE(default_addr.address_type() == NlatUnspecified);
        REQUIRE(!default_addr.is_address_linklocal());
        REQUIRE(!default_addr.is_address_loopback());

        socket_address v4_addr{AF_INET};
        REQUIRE(v4_addr.family() == AF_INET);
        REQUIRE(v4_addr.address_type() == NlatUnspecified);
        REQUIRE(!v4_addr.is_address_linklocal());
        REQUIRE(!v4_addr.is_address_loopback());
        REQUIRE(v4_addr.port() == 0);
#ifdef WIL_ENABLE_EXCEPTIONS
        REQUIRE(Test_any_in_addr_string == v4_addr.format_address());
#endif

        SOCKADDR_STORAGE v4_addr_storage = v4_addr.sockaddr_storage();
        REQUIRE(v4_addr_storage.ss_family == AF_INET);
        REQUIRE(0 == memcmp(&v4_addr_storage, v4_addr.sockaddr_in(), sizeof(SOCKADDR_IN)));

        socket_address v6_addr{AF_INET6};
        REQUIRE(v6_addr.family() == AF_INET6);
        REQUIRE(v6_addr.address_type() == NlatUnspecified);
        REQUIRE(!v6_addr.is_address_linklocal());
        REQUIRE(!v6_addr.is_address_loopback());
        REQUIRE(v6_addr.port() == 0);
#ifdef WIL_ENABLE_EXCEPTIONS
        REQUIRE(Test_any_in6_addr_string == v6_addr.format_address());
#endif

        SOCKADDR_STORAGE v6_addr_storage = v6_addr.sockaddr_storage();
        REQUIRE(v6_addr_storage.ss_family == AF_INET6);
        REQUIRE(0 == memcmp(&v6_addr_storage, v6_addr.sockaddr_in6(), sizeof(SOCKADDR_IN6)));
    }

    SECTION("socket_address(const SOCKADDR*, T)")
    {
        SOCKADDR_IN v4_test_sockaddr{};
        v4_test_sockaddr.sin_family = AF_INET;
        v4_test_sockaddr.sin_port = htons(TestPort); // the raw value is in network byte order
        memcpy(&v4_test_sockaddr.sin_addr, &Test_linklocal_in_addr, sizeof(in_addr));

        socket_address v4_addr{reinterpret_cast<sockaddr*>(&v4_test_sockaddr), sizeof(v4_test_sockaddr)};
        REQUIRE(v4_addr.family() == AF_INET);
        REQUIRE(v4_addr.address_type() == NlatUnicast);
        REQUIRE(v4_addr.is_address_linklocal());
        REQUIRE(!v4_addr.is_address_loopback());
        REQUIRE(v4_addr.port() == TestPort);
#ifdef WIL_ENABLE_EXCEPTIONS
        REQUIRE(Test_linklocal_in_addr_string == v4_addr.format_address());
#endif

        SOCKADDR_STORAGE v4_addr_storage = v4_addr.sockaddr_storage();
        REQUIRE(v4_addr_storage.ss_family == AF_INET);
        REQUIRE(0 == memcmp(&v4_addr_storage, v4_addr.sockaddr_in(), sizeof(SOCKADDR_IN)));

        SOCKADDR_IN6 v6_test_sockaddr{};
        v6_test_sockaddr.sin6_family = AF_INET6;
        // these raw value are in network byte order
        v6_test_sockaddr.sin6_port = htons(TestPort);
        v6_test_sockaddr.sin6_flowinfo = htonl(ULONG_MAX - 1);
        v6_test_sockaddr.sin6_scope_id = htonl(ULONG_MAX - 1);
        memcpy(&v6_test_sockaddr.sin6_addr, &Test_linklocal_in6_addr, sizeof(in6_addr));

        socket_address v6_addr{reinterpret_cast<sockaddr*>(&v6_test_sockaddr), sizeof(v6_test_sockaddr)};
        REQUIRE(v6_addr.family() == AF_INET6);
        REQUIRE(v6_addr.address_type() == NlatUnicast);
        REQUIRE(v6_addr.is_address_linklocal());
        REQUIRE(!v6_addr.is_address_loopback());
        REQUIRE(v6_addr.port() == TestPort);
        REQUIRE(v6_addr.flow_info() == ULONG_MAX - 1);
        REQUIRE(v6_addr.scope_id() == ULONG_MAX - 1);
#ifdef WIL_ENABLE_EXCEPTIONS
        REQUIRE(Test_linklocal_in6_addr_string == v6_addr.format_address());
#endif

        SOCKADDR_STORAGE v6_addr_storage = v6_addr.sockaddr_storage();
        REQUIRE(v6_addr_storage.ss_family == AF_INET6);
        REQUIRE(0 == memcmp(&v6_addr_storage, v6_addr.sockaddr_in6(), sizeof(SOCKADDR_IN6)));
    }

    SECTION("socket_address(const SOCKADDR_IN*)")
    {
        SOCKADDR_IN v4_test_sockaddr{};
        v4_test_sockaddr.sin_family = AF_INET;
        v4_test_sockaddr.sin_port = htons(TestPort); // the raw value is in network byte order
        memcpy(&v4_test_sockaddr.sin_addr, &Test_linklocal_in_addr, sizeof(in_addr));

        socket_address v4_addr{&v4_test_sockaddr};
        REQUIRE(v4_addr.family() == AF_INET);
        REQUIRE(v4_addr.address_type() == NlatUnicast);
        REQUIRE(v4_addr.is_address_linklocal());
        REQUIRE(!v4_addr.is_address_loopback());
        REQUIRE(v4_addr.port() == TestPort);
#ifdef WIL_ENABLE_EXCEPTIONS
        REQUIRE(Test_linklocal_in_addr_string == v4_addr.format_address());
#endif

        SOCKADDR_STORAGE v4_addr_storage = v4_addr.sockaddr_storage();
        REQUIRE(v4_addr_storage.ss_family == AF_INET);
        REQUIRE(0 == memcmp(&v4_addr_storage, v4_addr.sockaddr_in(), sizeof(SOCKADDR_IN)));
    }

    SECTION("socket_address(const SOCKADDR_IN6*)")
    {
        SOCKADDR_IN6 v6_test_sockaddr{};
        v6_test_sockaddr.sin6_family = AF_INET6;
        // these raw value are in network byte order
        v6_test_sockaddr.sin6_port = htons(TestPort);
        v6_test_sockaddr.sin6_flowinfo = htonl(ULONG_MAX - 1);
        v6_test_sockaddr.sin6_scope_id = htonl(ULONG_MAX - 1);
        memcpy(&v6_test_sockaddr.sin6_addr, &Test_linklocal_in6_addr, sizeof(in6_addr));

        socket_address v6_addr{&v6_test_sockaddr};
        REQUIRE(v6_addr.family() == AF_INET6);
        REQUIRE(v6_addr.address_type() == NlatUnicast);
        REQUIRE(v6_addr.is_address_linklocal());
        REQUIRE(!v6_addr.is_address_loopback());
        REQUIRE(v6_addr.port() == TestPort);
        REQUIRE(v6_addr.flow_info() == ULONG_MAX - 1);
        REQUIRE(v6_addr.scope_id() == ULONG_MAX - 1);
#ifdef WIL_ENABLE_EXCEPTIONS
        REQUIRE(Test_linklocal_in6_addr_string == v6_addr.format_address());
#endif

        SOCKADDR_STORAGE v6_addr_storage = v6_addr.sockaddr_storage();
        REQUIRE(v6_addr_storage.ss_family == AF_INET6);
        REQUIRE(0 == memcmp(&v6_addr_storage, v6_addr.sockaddr_in6(), sizeof(SOCKADDR_IN6)));
    }

    SECTION("socket_address(const SOCKADDR_INET*)")
    {
        SOCKADDR_IN v4_test_sockaddr{};
        v4_test_sockaddr.sin_family = AF_INET;
        v4_test_sockaddr.sin_port = htons(TestPort); // the raw value is in network byte order
        memcpy(&v4_test_sockaddr.sin_addr, &Test_linklocal_in_addr, sizeof(in_addr));

        SOCKADDR_INET v4_inet_addr{};
        memcpy(&v4_inet_addr.Ipv4, &v4_test_sockaddr, sizeof(SOCKADDR_IN));

        socket_address v4_addr{&v4_inet_addr};
        REQUIRE(v4_addr.family() == AF_INET);
        REQUIRE(v4_addr.address_type() == NlatUnicast);
        REQUIRE(v4_addr.is_address_linklocal());
        REQUIRE(!v4_addr.is_address_loopback());
        REQUIRE(v4_addr.port() == TestPort);
#ifdef WIL_ENABLE_EXCEPTIONS
        REQUIRE(Test_linklocal_in_addr_string == v4_addr.format_address());
#endif
        REQUIRE(0 == memcmp(&v4_inet_addr, v4_addr.sockaddr_inet(), sizeof(SOCKADDR_INET)));

        SOCKADDR_STORAGE v4_addr_storage = v4_addr.sockaddr_storage();
        REQUIRE(v4_addr_storage.ss_family == AF_INET);
        REQUIRE(0 == memcmp(&v4_addr_storage, v4_addr.sockaddr_in(), sizeof(SOCKADDR_IN)));

        SOCKADDR_IN6 v6_test_sockaddr{};
        v6_test_sockaddr.sin6_family = AF_INET6;
        // these raw value are in network byte order
        v6_test_sockaddr.sin6_port = htons(TestPort);
        v6_test_sockaddr.sin6_flowinfo = htonl(ULONG_MAX - 1);
        v6_test_sockaddr.sin6_scope_id = htonl(ULONG_MAX - 1);
        memcpy(&v6_test_sockaddr.sin6_addr, &Test_linklocal_in6_addr, sizeof(in6_addr));

        SOCKADDR_INET v6_inet_addr{};
        memcpy(&v6_inet_addr.Ipv6, &v6_test_sockaddr, sizeof(SOCKADDR_IN6));

        socket_address v6_addr{&v6_inet_addr};
        REQUIRE(v6_addr.family() == AF_INET6);
        REQUIRE(v6_addr.address_type() == NlatUnicast);
        REQUIRE(v6_addr.is_address_linklocal());
        REQUIRE(!v6_addr.is_address_loopback());
        REQUIRE(v6_addr.port() == TestPort);
        REQUIRE(v6_addr.flow_info() == ULONG_MAX - 1);
        REQUIRE(v6_addr.scope_id() == ULONG_MAX - 1);
#ifdef WIL_ENABLE_EXCEPTIONS
        REQUIRE(Test_linklocal_in6_addr_string == v6_addr.format_address());
#endif
        REQUIRE(0 == memcmp(&v6_inet_addr, v6_addr.sockaddr_inet(), sizeof(SOCKADDR_INET)));

        SOCKADDR_STORAGE v6_addr_storage = v6_addr.sockaddr_storage();
        REQUIRE(v6_addr_storage.ss_family == AF_INET6);
        REQUIRE(0 == memcmp(&v6_addr_storage, v6_addr.sockaddr_in6(), sizeof(SOCKADDR_IN6)));
    }

    SECTION("socket_address(const SOCKET_ADDRESS*)")
    {
        SOCKET_ADDRESS default_socketaddress{};
        socket_address default_addr{&default_socketaddress};
        REQUIRE(default_addr.family() == AF_UNSPEC);
        REQUIRE(default_addr.address_type() == NlatUnspecified);
        REQUIRE(!default_addr.is_address_linklocal());
        REQUIRE(!default_addr.is_address_loopback());

        SOCKADDR_IN v4_test_sockaddr{};
        v4_test_sockaddr.sin_family = AF_INET;
        v4_test_sockaddr.sin_port = htons(TestPort); // the raw value is in network byte order
        memcpy(&v4_test_sockaddr.sin_addr, &Test_linklocal_in_addr, sizeof(in_addr));

        SOCKET_ADDRESS v4_socketaddress{};
        v4_socketaddress.lpSockaddr = reinterpret_cast<sockaddr*>(&v4_test_sockaddr);
        v4_socketaddress.iSockaddrLength = sizeof(v4_test_sockaddr);

        socket_address v4_addr{&v4_socketaddress};
        REQUIRE(v4_addr.family() == AF_INET);
        REQUIRE(v4_addr.address_type() == NlatUnicast);
        REQUIRE(v4_addr.is_address_linklocal());
        REQUIRE(!v4_addr.is_address_loopback());
        REQUIRE(v4_addr.port() == TestPort);
#ifdef WIL_ENABLE_EXCEPTIONS
        REQUIRE(Test_linklocal_in_addr_string == v4_addr.format_address());
#endif

        SOCKADDR_IN6 v6_test_sockaddr{};
        v6_test_sockaddr.sin6_family = AF_INET6;
        // these raw value are in network byte order
        v6_test_sockaddr.sin6_port = htons(TestPort);
        v6_test_sockaddr.sin6_flowinfo = htonl(ULONG_MAX - 1);
        v6_test_sockaddr.sin6_scope_id = htonl(ULONG_MAX - 1);
        memcpy(&v6_test_sockaddr.sin6_addr, &Test_linklocal_in6_addr, sizeof(in6_addr));

        SOCKET_ADDRESS v6_socketaddress{};
        v6_socketaddress.lpSockaddr = reinterpret_cast<sockaddr*>(&v6_test_sockaddr);
        v6_socketaddress.iSockaddrLength = sizeof(v6_test_sockaddr);

        socket_address v6_addr{&v6_socketaddress};
        REQUIRE(v6_addr.family() == AF_INET6);
        REQUIRE(v6_addr.address_type() == NlatUnicast);
        REQUIRE(v6_addr.is_address_linklocal());
        REQUIRE(!v6_addr.is_address_loopback());
        REQUIRE(v6_addr.port() == TestPort);
        REQUIRE(v6_addr.flow_info() == ULONG_MAX - 1);
        REQUIRE(v6_addr.scope_id() == ULONG_MAX - 1);
#ifdef WIL_ENABLE_EXCEPTIONS
        REQUIRE(Test_linklocal_in6_addr_string == v6_addr.format_address());
#endif
    }
}

TEST_CASE("NetworkingTests::Verifying_in_addr_interactions", "[networking]")
{
    InitTestAddresses();

    wil::network::socket_address default_addr{};
    REQUIRE(default_addr.size() == sizeof(SOCKADDR_INET));

    wil::network::socket_address test_v4_addr{&Test_in_addr};
    wil::network::socket_address test_v4_addr2{&Test_in_addr2};
    wil::network::socket_address test_v4_addr_with_port{&Test_in_addr, TestPort};

    wil::network::socket_address test_v6_addr{&Test_in6_addr};
    wil::network::socket_address test_v6_addr2{&Test_in6_addr2};
    wil::network::socket_address test_v6_addr_with_port{&Test_in6_addr, TestPort};

    wil::network::socket_address test_v4_linklocal_addr{&Test_linklocal_in_addr};
    wil::network::socket_address test_v4_linklocal_addr_with_port{&Test_linklocal_in_addr, TestPort};

    wil::network::socket_address test_v6_linklocal_addr{&Test_linklocal_in6_addr};
    wil::network::socket_address test_v6_linklocal_addr_with_port{&Test_linklocal_in6_addr, TestPort};

    wil::network::socket_address test_v4_any_addr{&Test_any_in_addr};
    wil::network::socket_address test_v4_any_addr_with_port{&Test_any_in_addr, TestPort};

    wil::network::socket_address test_v6_any_addr{&Test_any_in6_addr};
    wil::network::socket_address test_v6_any_addr_with_port{&Test_any_in6_addr, TestPort};

    using wil::network::equals;

    SECTION("IPv4 in_addr properties")
    {
        REQUIRE(test_v4_addr.family() == AF_INET);
        REQUIRE(test_v4_addr.address_type() == NlatUnicast);
        REQUIRE(!test_v4_addr.is_address_linklocal());
        REQUIRE(!test_v4_addr.is_address_loopback());
        REQUIRE(NlatUnicast == test_v4_addr.address_type());
        REQUIRE(NlatUnicast == test_v4_addr2.address_type());

        REQUIRE(equals(*test_v4_addr.in_addr(), Test_in_addr));
        REQUIRE(equals(*test_v4_addr2.in_addr(), Test_in_addr2));
        REQUIRE(test_v4_addr.port() == 0);
        REQUIRE(test_v4_addr.scope_id() == 0);
        REQUIRE(test_v4_addr.flow_info() == 0);

        REQUIRE(test_v4_addr == test_v4_addr);
        REQUIRE(!(test_v4_addr != test_v4_addr));
        REQUIRE(!(test_v4_addr < test_v4_addr));
        REQUIRE(!(test_v4_addr > test_v4_addr));
        REQUIRE(test_v4_addr != test_v4_addr2);
        REQUIRE(test_v4_addr < test_v4_addr2);
        REQUIRE(test_v4_addr2 > test_v4_addr);
        REQUIRE(test_v4_addr != default_addr);
        REQUIRE(test_v4_addr > default_addr);
        REQUIRE(default_addr < test_v4_addr);
    }

    SECTION("IPv4 in_addr with port properties")
    {
        REQUIRE(test_v4_addr_with_port.family() == AF_INET);
        REQUIRE(test_v4_addr_with_port.address_type() == NlatUnicast);
        REQUIRE(!test_v4_addr_with_port.is_address_linklocal());
        REQUIRE(!test_v4_addr_with_port.is_address_loopback());
        REQUIRE(NlatUnicast == test_v4_addr_with_port.address_type());

        REQUIRE(equals(*test_v4_addr_with_port.in_addr(), Test_in_addr));
        REQUIRE(equals(*test_v4_addr_with_port.in_addr(), *test_v4_addr.in_addr()));
        REQUIRE(test_v4_addr_with_port.port() == TestPort);
        REQUIRE(test_v4_addr_with_port.scope_id() == 0);
        REQUIRE(test_v4_addr_with_port.flow_info() == 0);

        REQUIRE(test_v4_addr_with_port == test_v4_addr_with_port);
        REQUIRE(!(test_v4_addr_with_port != test_v4_addr_with_port));
        REQUIRE(!(test_v4_addr_with_port < test_v4_addr_with_port));
        REQUIRE(!(test_v4_addr_with_port > test_v4_addr_with_port));
        REQUIRE(test_v4_addr_with_port != default_addr);
        REQUIRE(test_v4_addr_with_port != test_v4_addr);
        REQUIRE(test_v4_addr_with_port > test_v4_addr);
        REQUIRE(test_v4_addr_with_port < test_v4_addr2);
        REQUIRE(test_v4_addr_with_port > default_addr);
        REQUIRE(default_addr < test_v4_addr_with_port);
    }

    SECTION("IPv6 in6_addr properties")
    {
        REQUIRE(test_v6_addr.family() == AF_INET6);
        REQUIRE(test_v6_addr.address_type() == NlatUnicast);
        REQUIRE(!test_v6_addr.is_address_linklocal());
        REQUIRE(!test_v6_addr.is_address_loopback());
        REQUIRE(NlatUnicast == test_v6_addr2.address_type());

        REQUIRE(equals(*test_v6_addr.in6_addr(), Test_in6_addr));
        REQUIRE(equals(*test_v6_addr2.in6_addr(), Test_in6_addr2));
        REQUIRE(test_v6_addr.port() == 0);
        REQUIRE(test_v6_addr.scope_id() == 0);
        REQUIRE(test_v6_addr.flow_info() == 0);

        REQUIRE(test_v6_addr == test_v6_addr);
        REQUIRE(!(test_v6_addr != test_v6_addr));
        REQUIRE(!(test_v6_addr < test_v6_addr));
        REQUIRE(!(test_v6_addr > test_v6_addr));
        REQUIRE(test_v6_addr != test_v6_addr2);
        REQUIRE(test_v6_addr < test_v6_addr2);
        REQUIRE(test_v6_addr2 > test_v6_addr);
        REQUIRE(test_v6_addr != test_v4_addr);
        REQUIRE(test_v6_addr > test_v4_addr);
        REQUIRE(test_v4_addr < test_v6_addr);
        REQUIRE(test_v6_addr != default_addr);
        REQUIRE(test_v6_addr > default_addr);
        REQUIRE(default_addr < test_v6_addr);
    }

    SECTION("IPv6 in6_addr with port properties")
    {
        REQUIRE(test_v6_addr_with_port.family() == AF_INET6);
        REQUIRE(test_v6_addr_with_port.address_type() == NlatUnicast);
        REQUIRE(!test_v6_addr_with_port.is_address_linklocal());
        REQUIRE(!test_v6_addr_with_port.is_address_loopback());
        REQUIRE(NlatUnicast == test_v6_addr_with_port.address_type());

        REQUIRE(equals(*test_v6_addr_with_port.in6_addr(), Test_in6_addr));
        REQUIRE(equals(*test_v6_addr_with_port.in6_addr(), *test_v6_addr.in6_addr()));
        REQUIRE(test_v6_addr_with_port.port() == TestPort);
        REQUIRE(test_v6_addr_with_port.scope_id() == 0);
        REQUIRE(test_v6_addr_with_port.flow_info() == 0);

        REQUIRE(test_v6_addr_with_port == test_v6_addr_with_port);
        REQUIRE(!(test_v6_addr_with_port != test_v6_addr_with_port));
        REQUIRE(!(test_v6_addr_with_port < test_v6_addr_with_port));
        REQUIRE(!(test_v6_addr_with_port > test_v6_addr_with_port));
        REQUIRE(test_v6_addr_with_port != test_v4_addr);
        REQUIRE(test_v6_addr_with_port > test_v4_addr);
        REQUIRE(test_v4_addr < test_v6_addr_with_port);
        REQUIRE(test_v6_addr_with_port != test_v4_addr_with_port);
        REQUIRE(test_v6_addr_with_port != test_v6_addr);
        REQUIRE(test_v6_addr_with_port != test_v6_addr2);
        REQUIRE(test_v6_addr_with_port > test_v6_addr);
        REQUIRE(test_v6_addr_with_port < test_v6_addr2);
        REQUIRE(test_v6_addr_with_port != default_addr);
        REQUIRE(test_v6_addr_with_port > default_addr);
        REQUIRE(default_addr < test_v6_addr_with_port);
    }

    SECTION("IPv4 link-local in_addr properties")
    {
        REQUIRE(test_v4_linklocal_addr.family() == AF_INET);
        REQUIRE(test_v4_linklocal_addr.address_type() == NlatUnicast);
        REQUIRE(test_v4_linklocal_addr.is_address_linklocal());
        REQUIRE(!test_v4_linklocal_addr.is_address_loopback());

        REQUIRE(equals(*test_v4_linklocal_addr.in_addr(), Test_linklocal_in_addr));
        REQUIRE(test_v4_linklocal_addr.port() == 0);
        REQUIRE(test_v4_linklocal_addr.scope_id() == 0);
        REQUIRE(test_v4_linklocal_addr.flow_info() == 0);

        REQUIRE(test_v4_linklocal_addr == test_v4_linklocal_addr);
        REQUIRE(!(test_v4_linklocal_addr != test_v4_linklocal_addr));
        REQUIRE(!(test_v4_linklocal_addr < test_v4_linklocal_addr));
        REQUIRE(!(test_v4_linklocal_addr > test_v4_linklocal_addr));
        REQUIRE(test_v4_linklocal_addr != default_addr);
        REQUIRE(test_v4_linklocal_addr != test_v4_addr);
        REQUIRE(test_v4_linklocal_addr != test_v4_addr_with_port);
        REQUIRE(test_v4_linklocal_addr != test_v6_addr);
        REQUIRE(test_v4_linklocal_addr != test_v6_addr_with_port);
    }

    SECTION("IPv4 link-local in_addr with port properties")
    {
        REQUIRE(test_v4_linklocal_addr_with_port.family() == AF_INET);
        REQUIRE(test_v4_linklocal_addr_with_port.address_type() == NlatUnicast);
        REQUIRE(test_v4_linklocal_addr_with_port.is_address_linklocal());
        REQUIRE(!test_v4_linklocal_addr_with_port.is_address_loopback());

        REQUIRE(equals(*test_v4_linklocal_addr_with_port.in_addr(), Test_linklocal_in_addr));
        REQUIRE(equals(*test_v4_linklocal_addr_with_port.in_addr(), *test_v4_linklocal_addr.in_addr()));
        REQUIRE(test_v4_linklocal_addr_with_port.port() == TestPort);
        REQUIRE(test_v4_linklocal_addr_with_port.scope_id() == 0);
        REQUIRE(test_v4_linklocal_addr_with_port.flow_info() == 0);

        REQUIRE(test_v4_linklocal_addr_with_port == test_v4_linklocal_addr_with_port);
        REQUIRE(!(test_v4_linklocal_addr_with_port != test_v4_linklocal_addr_with_port));
        REQUIRE(!(test_v4_linklocal_addr_with_port < test_v4_linklocal_addr_with_port));
        REQUIRE(!(test_v4_linklocal_addr_with_port > test_v4_linklocal_addr_with_port));
        REQUIRE(test_v4_linklocal_addr_with_port != default_addr);
        REQUIRE(test_v4_linklocal_addr_with_port != test_v4_addr);
        REQUIRE(test_v4_linklocal_addr_with_port != test_v4_addr_with_port);
        REQUIRE(test_v4_linklocal_addr_with_port != test_v6_addr);
        REQUIRE(test_v4_linklocal_addr_with_port != test_v6_addr_with_port);
        REQUIRE(test_v4_linklocal_addr_with_port != test_v4_linklocal_addr);
    }

    SECTION("IPv6 link-local in6_addr properties")
    {
        REQUIRE(test_v6_linklocal_addr.family() == AF_INET6);
        REQUIRE(test_v6_linklocal_addr.address_type() == NlatUnicast);
        REQUIRE(test_v6_linklocal_addr.is_address_linklocal());
        REQUIRE(!test_v6_linklocal_addr.is_address_loopback());

        REQUIRE(equals(*test_v6_linklocal_addr.in6_addr(), Test_linklocal_in6_addr));
        REQUIRE(test_v6_linklocal_addr.port() == 0);
        REQUIRE(test_v6_linklocal_addr.scope_id() == 0);
        REQUIRE(test_v6_linklocal_addr.flow_info() == 0);

        REQUIRE(test_v6_linklocal_addr == test_v6_linklocal_addr);
        REQUIRE(!(test_v6_linklocal_addr != test_v6_linklocal_addr));
        REQUIRE(!(test_v6_linklocal_addr < test_v6_linklocal_addr));
        REQUIRE(!(test_v6_linklocal_addr > test_v6_linklocal_addr));
        REQUIRE(test_v6_linklocal_addr != default_addr);
        REQUIRE(test_v6_linklocal_addr != test_v4_addr);
        REQUIRE(test_v6_linklocal_addr != test_v4_addr_with_port);
        REQUIRE(test_v6_linklocal_addr != test_v6_addr);
        REQUIRE(test_v6_linklocal_addr != test_v6_addr_with_port);
        REQUIRE(test_v6_linklocal_addr != test_v4_linklocal_addr);
        REQUIRE(test_v6_linklocal_addr != test_v4_linklocal_addr_with_port);
    }

    SECTION("IPv6 link-local in6_addr with port properties")
    {
        REQUIRE(test_v6_linklocal_addr_with_port.family() == AF_INET6);
        REQUIRE(test_v6_linklocal_addr_with_port.address_type() == NlatUnicast);
        REQUIRE(test_v6_linklocal_addr_with_port.is_address_linklocal());
        REQUIRE(!test_v6_linklocal_addr_with_port.is_address_loopback());

        REQUIRE(equals(*test_v6_linklocal_addr_with_port.in6_addr(), Test_linklocal_in6_addr));
        REQUIRE(equals(*test_v6_linklocal_addr_with_port.in6_addr(), *test_v6_linklocal_addr.in6_addr()));
        REQUIRE(test_v6_linklocal_addr_with_port.port() == TestPort);
        REQUIRE(test_v6_linklocal_addr_with_port.scope_id() == 0);
        REQUIRE(test_v6_linklocal_addr_with_port.flow_info() == 0);

        REQUIRE(test_v6_linklocal_addr_with_port == test_v6_linklocal_addr_with_port);
        REQUIRE(!(test_v6_linklocal_addr_with_port != test_v6_linklocal_addr_with_port));
        REQUIRE(!(test_v6_linklocal_addr_with_port < test_v6_linklocal_addr_with_port));
        REQUIRE(!(test_v6_linklocal_addr_with_port > test_v6_linklocal_addr_with_port));
        REQUIRE(test_v6_linklocal_addr_with_port != default_addr);
        REQUIRE(test_v6_linklocal_addr_with_port != test_v4_addr);
        REQUIRE(test_v6_linklocal_addr_with_port != test_v4_addr_with_port);
        REQUIRE(test_v6_linklocal_addr_with_port != test_v6_addr);
        REQUIRE(test_v6_linklocal_addr_with_port != test_v6_addr_with_port);
        REQUIRE(test_v6_linklocal_addr_with_port != test_v4_linklocal_addr);
        REQUIRE(test_v6_linklocal_addr_with_port != test_v4_linklocal_addr_with_port);
        REQUIRE(test_v6_linklocal_addr_with_port != test_v6_linklocal_addr);
    }

    SECTION("IPv4 any-addr in_addr properties")
    {
        REQUIRE(test_v4_any_addr.family() == AF_INET);
        REQUIRE(test_v4_any_addr.address_type() == NlatUnspecified);
        REQUIRE(!test_v4_any_addr.is_address_linklocal());
        REQUIRE(!test_v4_any_addr.is_address_loopback());

        REQUIRE(equals(*test_v4_any_addr.in_addr(), Test_any_in_addr));
        REQUIRE(test_v4_any_addr.port() == 0);
        REQUIRE(test_v4_any_addr.scope_id() == 0);
        REQUIRE(test_v4_any_addr.flow_info() == 0);

        REQUIRE(test_v4_any_addr == test_v4_any_addr);
        REQUIRE(!(test_v4_any_addr != test_v4_any_addr));
        REQUIRE(!(test_v4_any_addr < test_v4_any_addr));
        REQUIRE(!(test_v4_any_addr > test_v4_any_addr));
        REQUIRE(test_v4_any_addr != default_addr);
        REQUIRE(test_v4_any_addr != test_v4_addr);
        REQUIRE(test_v4_any_addr != test_v4_addr_with_port);
        REQUIRE(test_v4_any_addr != test_v6_addr);
        REQUIRE(test_v4_any_addr != test_v6_addr_with_port);
        REQUIRE(test_v4_any_addr != test_v4_linklocal_addr);
        REQUIRE(test_v4_any_addr != test_v4_linklocal_addr_with_port);
        REQUIRE(test_v4_any_addr != test_v6_linklocal_addr);
        REQUIRE(test_v4_any_addr != test_v6_linklocal_addr_with_port);
    }

    SECTION("IPv4 any-addr in_addr with port properties")
    {
        REQUIRE(test_v4_any_addr_with_port.family() == AF_INET);
        REQUIRE(test_v4_any_addr_with_port.address_type() == NlatUnspecified);
        REQUIRE(!test_v4_any_addr_with_port.is_address_linklocal());
        REQUIRE(!test_v4_any_addr_with_port.is_address_loopback());

        REQUIRE(equals(*test_v4_any_addr_with_port.in_addr(), Test_any_in_addr));
        REQUIRE(equals(*test_v4_any_addr_with_port.in_addr(), *test_v4_any_addr.in_addr()));
        REQUIRE(test_v4_any_addr_with_port.port() == TestPort);
        REQUIRE(test_v4_any_addr_with_port.scope_id() == 0);
        REQUIRE(test_v4_any_addr_with_port.flow_info() == 0);

        REQUIRE(test_v4_any_addr_with_port == test_v4_any_addr_with_port);
        REQUIRE(!(test_v4_any_addr_with_port != test_v4_any_addr_with_port));
        REQUIRE(!(test_v4_any_addr_with_port < test_v4_any_addr_with_port));
        REQUIRE(!(test_v4_any_addr_with_port > test_v4_any_addr_with_port));
        REQUIRE(test_v4_any_addr_with_port != default_addr);
        REQUIRE(test_v4_any_addr_with_port != test_v4_addr);
        REQUIRE(test_v4_any_addr_with_port != test_v4_addr_with_port);
        REQUIRE(test_v4_any_addr_with_port != test_v6_addr);
        REQUIRE(test_v4_any_addr_with_port != test_v6_addr_with_port);
        REQUIRE(test_v4_any_addr_with_port != test_v4_linklocal_addr);
        REQUIRE(test_v4_any_addr_with_port != test_v4_linklocal_addr_with_port);
        REQUIRE(test_v4_any_addr_with_port != test_v6_linklocal_addr);
        REQUIRE(test_v4_any_addr_with_port != test_v6_linklocal_addr_with_port);
        REQUIRE(test_v4_any_addr_with_port != test_v4_any_addr);
    }

    SECTION("IPv6 any-addr in6_addr properties")
    {
        REQUIRE(test_v6_any_addr.family() == AF_INET6);
        REQUIRE(test_v6_any_addr.address_type() == NlatUnspecified);
        REQUIRE(!test_v6_any_addr.is_address_linklocal());
        REQUIRE(!test_v6_any_addr.is_address_loopback());

        REQUIRE(equals(*test_v6_any_addr.in6_addr(), Test_any_in6_addr));
        REQUIRE(test_v6_any_addr.port() == 0);
        REQUIRE(test_v6_any_addr.scope_id() == 0);
        REQUIRE(test_v6_any_addr.flow_info() == 0);

        REQUIRE(test_v6_any_addr == test_v6_any_addr);
        REQUIRE(!(test_v6_any_addr != test_v6_any_addr));
        REQUIRE(!(test_v6_any_addr < test_v6_any_addr));
        REQUIRE(!(test_v6_any_addr > test_v6_any_addr));
        REQUIRE(test_v6_any_addr != default_addr);
        REQUIRE(test_v6_any_addr != test_v4_addr);
        REQUIRE(test_v6_any_addr != test_v4_addr_with_port);
        REQUIRE(test_v6_any_addr != test_v6_addr);
        REQUIRE(test_v6_any_addr != test_v6_addr_with_port);
        REQUIRE(test_v6_any_addr != test_v4_linklocal_addr);
        REQUIRE(test_v6_any_addr != test_v4_linklocal_addr_with_port);
        REQUIRE(test_v6_any_addr != test_v6_linklocal_addr);
        REQUIRE(test_v6_any_addr != test_v6_linklocal_addr_with_port);
        REQUIRE(test_v6_any_addr != test_v4_any_addr);
        REQUIRE(test_v6_any_addr != test_v4_any_addr_with_port);
    }

    SECTION("IPv6 any-addr in6_addr with port properties")
    {
        REQUIRE(test_v6_any_addr_with_port.family() == AF_INET6);
        REQUIRE(test_v6_any_addr_with_port.address_type() == NlatUnspecified);
        REQUIRE(!test_v6_any_addr_with_port.is_address_linklocal());
        REQUIRE(!test_v6_any_addr_with_port.is_address_loopback());

        REQUIRE(equals(*test_v6_any_addr_with_port.in6_addr(), Test_any_in6_addr));
        REQUIRE(equals(*test_v6_any_addr_with_port.in6_addr(), *test_v6_any_addr.in6_addr()));
        REQUIRE(test_v6_any_addr_with_port.port() == TestPort);
        REQUIRE(test_v6_any_addr_with_port.scope_id() == 0);
        REQUIRE(test_v6_any_addr_with_port.flow_info() == 0);

        REQUIRE(test_v6_any_addr_with_port == test_v6_any_addr_with_port);
        REQUIRE(!(test_v6_any_addr_with_port != test_v6_any_addr_with_port));
        REQUIRE(!(test_v6_any_addr_with_port < test_v6_any_addr_with_port));
        REQUIRE(!(test_v6_any_addr_with_port > test_v6_any_addr_with_port));
        REQUIRE(test_v6_any_addr_with_port != default_addr);
        REQUIRE(test_v6_any_addr_with_port != test_v4_addr);
        REQUIRE(test_v6_any_addr_with_port != test_v4_addr_with_port);
        REQUIRE(test_v6_any_addr_with_port != test_v6_addr);
        REQUIRE(test_v6_any_addr_with_port != test_v6_addr_with_port);
        REQUIRE(test_v6_any_addr_with_port != test_v4_linklocal_addr);
        REQUIRE(test_v6_any_addr_with_port != test_v4_linklocal_addr_with_port);
        REQUIRE(test_v6_any_addr_with_port != test_v6_linklocal_addr);
        REQUIRE(test_v6_any_addr_with_port != test_v6_linklocal_addr_with_port);
        REQUIRE(test_v6_any_addr_with_port != test_v4_any_addr);
        REQUIRE(test_v6_any_addr_with_port != test_v4_any_addr_with_port);
        REQUIRE(test_v6_any_addr_with_port != test_v6_any_addr);
    }
}

TEST_CASE("NetworkingTests::Verifying_operators", "[networking]")
{
    using wil::network::equals;

#ifdef WIL_ENABLE_EXCEPTIONS
    using wil::network::socket_address;
    SECTION("verify v4 address comparisons")
    {
        REQUIRE(!(socket_address{L"1.1.1.1"} < socket_address{L"1.1.1.1"}));
        REQUIRE(!(socket_address{L"1.1.1.1"} > socket_address{L"1.1.1.1"}));
        REQUIRE(socket_address{L"1.1.1.1"} == socket_address{L"1.1.1.1"});
        REQUIRE(!(socket_address{L"1.1.1.1"} != socket_address{L"1.1.1.1"}));

        REQUIRE(socket_address{L"1.1.1.1"} < socket_address{L"1.1.1.2"});
        REQUIRE(!(socket_address{L"1.1.1.1"} > socket_address{L"1.1.1.2"}));
        REQUIRE(socket_address{L"1.1.1.1"} != socket_address{L"1.1.1.2"});
        REQUIRE(!(socket_address{L"1.1.1.1"} == socket_address{L"1.1.1.2"}));

        REQUIRE(socket_address{L"1.1.1.1"} < socket_address{L"2.1.1.1"});
        REQUIRE(!(socket_address{L"1.1.1.1"} > socket_address{L"2.1.1.1"}));
        REQUIRE(socket_address{L"1.1.1.1"} != socket_address{L"2.1.1.1"});
        REQUIRE(!(socket_address{L"1.1.1.1"} == socket_address{L"2.1.1.1"}));

        REQUIRE(socket_address{L"1.0.0.0"} > socket_address{L"0.0.0.1"});
        REQUIRE(!(socket_address{L"1.0.0.0"} < socket_address{L"0.0.0.1"}));
        REQUIRE(socket_address{L"1.0.0.0"} != socket_address{L"0.0.0.1"});
        REQUIRE(!(socket_address{L"1.0.0.0"} == socket_address{L"0.0.0.1"}));

        REQUIRE(socket_address{L"1.1.1.1", 1} < socket_address{L"1.1.1.1", 2});
        REQUIRE(!(socket_address{L"1.1.1.1", 1} > socket_address{L"1.1.1.1", 2}));
        REQUIRE(socket_address{L"1.1.1.1", 1} != socket_address{L"1.1.1.1", 2});
        REQUIRE(!(socket_address{L"1.1.1.1", 1} == socket_address{L"1.1.1.1", 2}));

        REQUIRE(socket_address{L"1.1.1.1", 1} > socket_address{L"0.0.0.0", 65535});
        REQUIRE(!(socket_address{L"1.1.1.1", 1} < socket_address{L"0.0.0.0", 65535}));
        REQUIRE(socket_address{L"1.1.1.1", 1} != socket_address{L"0.0.0.0", 65535});
        REQUIRE(!(socket_address{L"1.1.1.1", 1} == socket_address{L"0.0.0.0", 65535}));

        REQUIRE(socket_address{L"254.254.254.254"} > socket_address{L"127.127.127.127"});
        REQUIRE(!(socket_address{L"254.254.254.254"} < socket_address{L"127.127.127.127"}));
        REQUIRE(socket_address{L"254.254.254.254"} != socket_address{L"127.127.127.127"});
        REQUIRE(!(socket_address{L"254.254.254.254"} == socket_address{L"127.127.127.127"}));
    }

    SECTION("verify v6 address comparisons")
    {
        REQUIRE(!(socket_address{L"2001::1002"} < socket_address{L"2001::1002"}));
        REQUIRE(!(socket_address{L"2001::1002"} > socket_address{L"2001::1002"}));
        REQUIRE(socket_address{L"2001::1002"} == socket_address{L"2001::1002"});
        REQUIRE(!(socket_address{L"2001::1002"} != socket_address{L"2001::1002"}));

        REQUIRE(socket_address{L"2001::1002"} < socket_address{L"2001::1003"});
        REQUIRE(!(socket_address{L"2001::1002"} > socket_address{L"2001::1003"}));
        REQUIRE(socket_address{L"2001::1002"} != socket_address{L"2001::1003"});
        REQUIRE(!(socket_address{L"2001::1002"} == socket_address{L"2001::1003"}));

        REQUIRE(socket_address{L"2001::1002"} > socket_address{L"1002::2001"});
        REQUIRE(!(socket_address{L"2001::1002"} < socket_address{L"1002::2001"}));
        REQUIRE(socket_address{L"2001::1002"} != socket_address{L"1002::2001"});
        REQUIRE(!(socket_address{L"2001::1002"} == socket_address{L"1002::2001"}));

        REQUIRE(socket_address{L"2001::1002"} > socket_address{L"::1"});
        REQUIRE(!(socket_address{L"2001::1002"} < socket_address{L"::1"}));
        REQUIRE(socket_address{L"2001::1002"} != socket_address{L"::1"});
        REQUIRE(!(socket_address{L"2001::1002"} == socket_address{L"::1"}));

        REQUIRE(socket_address{L"2001::1002", 1} < socket_address{L"2001::1002", 2});
        REQUIRE(!(socket_address{L"2001::1002", 1} > socket_address{L"2001::1002", 2}));
        REQUIRE(socket_address{L"2001::1002", 1} != socket_address{L"2001::1002", 2});
        REQUIRE(!(socket_address{L"2001::1002", 1} == socket_address{L"2001::1002", 2}));

        socket_address lhs_scope_id_test{L"2001::1002", 1};
        lhs_scope_id_test.set_scope_id(10000);
        socket_address rhs_scope_id_test{L"2001::1002", 1};
        rhs_scope_id_test.set_scope_id(100000);
        REQUIRE(lhs_scope_id_test != rhs_scope_id_test);
        REQUIRE(lhs_scope_id_test < rhs_scope_id_test);
        REQUIRE(!(lhs_scope_id_test > rhs_scope_id_test));
        REQUIRE(lhs_scope_id_test != rhs_scope_id_test);
        REQUIRE(!(lhs_scope_id_test == rhs_scope_id_test));

        socket_address lhs_flow_info_test{L"2001::1002", 1};
        lhs_flow_info_test.set_flow_info(10000);
        socket_address rhs_flow_info_test{L"2001::1002", 1};
        rhs_flow_info_test.set_flow_info(100000);
        REQUIRE(lhs_flow_info_test != rhs_flow_info_test);
        REQUIRE(lhs_flow_info_test < rhs_flow_info_test);
        REQUIRE(!(lhs_flow_info_test > rhs_flow_info_test));
        REQUIRE(lhs_flow_info_test != rhs_flow_info_test);
        REQUIRE(!(lhs_flow_info_test == rhs_flow_info_test));
    }
#endif
}

TEST_CASE("NetworkingTests::Verifying_set_functions", "[networking]")
{
    InitTestAddresses();

    wil::network::socket_address default_addr{};
    REQUIRE(default_addr.size() == sizeof(SOCKADDR_INET));

    wil::network::socket_address test_v4_addr{&Test_in_addr};
    wil::network::socket_address test_v4_addr2{&Test_in_addr2};
    wil::network::socket_address test_v4_addr_with_port{&Test_in_addr, TestPort};

    wil::network::socket_address test_v6_addr{&Test_in6_addr};
    wil::network::socket_address test_v6_addr2{&Test_in6_addr2};
    wil::network::socket_address test_v6_addr_with_port{&Test_in6_addr, TestPort};

    wil::network::socket_address test_v4_linklocal_addr{&Test_linklocal_in_addr};
    wil::network::socket_address test_v4_linklocal_addr_with_port{&Test_linklocal_in_addr, TestPort};

    wil::network::socket_address test_v6_linklocal_addr{&Test_linklocal_in6_addr};
    wil::network::socket_address test_v6_linklocal_addr_with_port{&Test_linklocal_in6_addr, TestPort};

    wil::network::socket_address test_v4_any_addr{&Test_any_in_addr};
    wil::network::socket_address test_v4_any_addr_with_port{&Test_any_in_addr, TestPort};

    wil::network::socket_address test_v6_any_addr{&Test_any_in6_addr};
    wil::network::socket_address test_v6_any_addr_with_port{&Test_any_in6_addr, TestPort};

    wil::network::socket_address test_v4_loopback_addr{&Test_loopback_in_addr};
    wil::network::socket_address test_v4_loopback_addr_with_port{&Test_loopback_in_addr, TestPort};

    wil::network::socket_address test_v6_loopback_addr{&Test_loopback_in6_addr};
    wil::network::socket_address test_v6_loopback_addr_with_port{&Test_loopback_in6_addr, TestPort};

    // need WSAStartup called for some functions below
    auto wsa_startup_tracking = wil::network::WSAStartup_nothrow();
    REQUIRE(static_cast<bool>(wsa_startup_tracking));

    using wil::network::equals;

    SECTION("verify set_address_any")
    {
        const auto VerifyV4AnyAddress = [&](const wil::network::socket_address& v4_address, bool with_port) {
            wil::network::socket_address_wstring any_address_test_string{};
            REQUIRE(SUCCEEDED(v4_address.format_address_nothrow(any_address_test_string)));
            REQUIRE(0 == memcmp(Test_any_in_addr_string, any_address_test_string, sizeof(Test_any_in_addr_string)));
#ifdef WIL_ENABLE_EXCEPTIONS
            REQUIRE(Test_any_in_addr_string == v4_address.format_address());
#endif

            ZeroMemory(any_address_test_string, sizeof(any_address_test_string));

            REQUIRE(SUCCEEDED(v4_address.format_complete_address_nothrow(any_address_test_string)));
            if (with_port)
            {
                REQUIRE(0 == memcmp(Test_any_in_addr_string_with_port, any_address_test_string, sizeof(Test_any_in_addr_string_with_port)));
#ifdef WIL_ENABLE_EXCEPTIONS
                REQUIRE(Test_any_in_addr_string_with_port == v4_address.format_complete_address());
#endif
            }
            else
            {
                REQUIRE(0 == memcmp(Test_any_in_addr_string, any_address_test_string, sizeof(Test_any_in_addr_string)));
#ifdef WIL_ENABLE_EXCEPTIONS
                REQUIRE(Test_any_in_addr_string == v4_address.format_complete_address());
#endif
            }

            // also char* versions
            wil::network::socket_address_string any_address_test_char_string{};
            REQUIRE(SUCCEEDED(v4_address.format_address_nothrow(any_address_test_char_string)));
            REQUIRE(0 == memcmp(Test_any_in_addr_char_string, any_address_test_char_string, sizeof(Test_any_in_addr_char_string)));

            ZeroMemory(any_address_test_char_string, sizeof(any_address_test_char_string));

            REQUIRE(SUCCEEDED(v4_address.format_complete_address_nothrow(any_address_test_char_string)));
            if (with_port)
            {
                REQUIRE(0 == memcmp(Test_any_in_addr_char_string_with_port, any_address_test_char_string, sizeof(Test_any_in_addr_char_string_with_port)));
            }
            else
            {
                REQUIRE(0 == memcmp(Test_any_in_addr_char_string, any_address_test_char_string, sizeof(Test_any_in_addr_char_string)));
            }
        };

        const auto VerifyV6AnyAddress = [&](const wil::network::socket_address& v6_address, bool with_port) {
            wil::network::socket_address_wstring any_address_test_string{};
            REQUIRE(SUCCEEDED(v6_address.format_address_nothrow(any_address_test_string)));
            REQUIRE(0 == memcmp(Test_any_in6_addr_string, any_address_test_string, sizeof(Test_any_in6_addr_string)));
#ifdef WIL_ENABLE_EXCEPTIONS
            REQUIRE(Test_any_in6_addr_string == v6_address.format_address());
#endif

            ZeroMemory(any_address_test_string, sizeof(any_address_test_string));

            REQUIRE(SUCCEEDED(v6_address.format_complete_address_nothrow(any_address_test_string)));
            if (with_port)
            {
                REQUIRE(0 == memcmp(Test_any_in6_addr_string_with_port, any_address_test_string, sizeof(Test_any_in6_addr_string_with_port)));
#ifdef WIL_ENABLE_EXCEPTIONS
                REQUIRE(Test_any_in6_addr_string_with_port == v6_address.format_complete_address());
#endif
            }
            else
            {
                REQUIRE(0 == memcmp(Test_any_in6_addr_string, any_address_test_string, sizeof(Test_any_in6_addr_string)));
#ifdef WIL_ENABLE_EXCEPTIONS
                REQUIRE(Test_any_in6_addr_string == v6_address.format_complete_address());
#endif
            }

            // also char* versions
            wil::network::socket_address_string any_address_test_char_string{};
            REQUIRE(SUCCEEDED(v6_address.format_address_nothrow(any_address_test_char_string)));
            REQUIRE(0 == memcmp(Test_any_in6_addr_char_string, any_address_test_char_string, sizeof(Test_any_in6_addr_char_string)));

            ZeroMemory(any_address_test_char_string, sizeof(any_address_test_char_string));

            REQUIRE(SUCCEEDED(v6_address.format_complete_address_nothrow(any_address_test_char_string)));
            if (with_port)
            {
                REQUIRE(0 == memcmp(Test_any_in6_addr_char_string_with_port, any_address_test_char_string, sizeof(Test_any_in6_addr_char_string_with_port)));
            }
            else
            {
                REQUIRE(0 == memcmp(Test_any_in6_addr_char_string, any_address_test_char_string, sizeof(Test_any_in6_addr_char_string)));
            }
        };

        wil::network::socket_address v4_address;
        v4_address.set_address_any(AF_INET);
        REQUIRE(v4_address.family() == AF_INET);
        REQUIRE(v4_address.port() == 0);
        REQUIRE(v4_address.address_type() == NlatUnspecified);
        REQUIRE(!v4_address.is_address_linklocal());
        REQUIRE(!v4_address.is_address_loopback());
        REQUIRE(v4_address == test_v4_any_addr);
        VerifyV4AnyAddress(v4_address, false);

        v4_address.set_port(TestPort);
        REQUIRE(v4_address.family() == AF_INET);
        REQUIRE(v4_address.port() == TestPort);
        REQUIRE(v4_address.address_type() == NlatUnspecified);
        REQUIRE(!v4_address.is_address_linklocal());
        REQUIRE(!v4_address.is_address_loopback());
        REQUIRE(v4_address == test_v4_any_addr_with_port);
        VerifyV4AnyAddress(v4_address, true);

        // verify changing families
        v4_address.set_address_any(AF_INET6);
        REQUIRE(v4_address.family() == AF_INET6);
        REQUIRE(v4_address.port() == TestPort);
        REQUIRE(v4_address.address_type() == NlatUnspecified);
        REQUIRE(!v4_address.is_address_linklocal());
        REQUIRE(!v4_address.is_address_loopback());
        REQUIRE(v4_address == test_v6_any_addr_with_port);
        VerifyV6AnyAddress(v4_address, true);

        wil::network::socket_address v6_address;
        v6_address.set_address_any(AF_INET6);
        REQUIRE(v6_address.family() == AF_INET6);
        REQUIRE(v6_address.port() == 0);
        REQUIRE(v6_address.address_type() == NlatUnspecified);
        REQUIRE(!v6_address.is_address_linklocal());
        REQUIRE(!v6_address.is_address_loopback());
        REQUIRE(v6_address == test_v6_any_addr);
        VerifyV6AnyAddress(v6_address, false);

        v6_address.set_port(TestPort);
        REQUIRE(v6_address.family() == AF_INET6);
        REQUIRE(v6_address.port() == TestPort);
        REQUIRE(v6_address.address_type() == NlatUnspecified);
        REQUIRE(!v6_address.is_address_linklocal());
        REQUIRE(!v6_address.is_address_loopback());
        REQUIRE(v6_address == test_v6_any_addr_with_port);
        VerifyV6AnyAddress(v6_address, true);

        // verify changing families
        v6_address.set_address_any(AF_INET);
        REQUIRE(v6_address.family() == AF_INET);
        REQUIRE(v6_address.port() == TestPort);
        REQUIRE(v6_address.address_type() == NlatUnspecified);
        REQUIRE(!v6_address.is_address_linklocal());
        REQUIRE(!v6_address.is_address_loopback());
        REQUIRE(v6_address == test_v4_any_addr_with_port);
        VerifyV4AnyAddress(v6_address, true);

        wil::network::socket_address defaulted_v4_address{AF_INET};
        defaulted_v4_address.set_address_any();
        REQUIRE(defaulted_v4_address.family() == AF_INET);
        REQUIRE(defaulted_v4_address.port() == 0);
        REQUIRE(defaulted_v4_address.address_type() == NlatUnspecified);
        REQUIRE(!defaulted_v4_address.is_address_linklocal());
        REQUIRE(!defaulted_v4_address.is_address_loopback());
        REQUIRE(defaulted_v4_address == test_v4_any_addr);
        VerifyV4AnyAddress(defaulted_v4_address, false);

        defaulted_v4_address.set_port(TestPort);
        REQUIRE(defaulted_v4_address.family() == AF_INET);
        REQUIRE(defaulted_v4_address.port() == TestPort);
        REQUIRE(defaulted_v4_address.address_type() == NlatUnspecified);
        REQUIRE(!defaulted_v4_address.is_address_linklocal());
        REQUIRE(!defaulted_v4_address.is_address_loopback());
        REQUIRE(defaulted_v4_address == test_v4_any_addr_with_port);
        VerifyV4AnyAddress(defaulted_v4_address, true);

        wil::network::socket_address defaulted_v6_address{AF_INET6};
        defaulted_v6_address.set_address_any();
        REQUIRE(defaulted_v6_address.family() == AF_INET6);
        REQUIRE(defaulted_v6_address.port() == 0);
        REQUIRE(defaulted_v6_address.address_type() == NlatUnspecified);
        REQUIRE(!defaulted_v6_address.is_address_linklocal());
        REQUIRE(!defaulted_v6_address.is_address_loopback());
        REQUIRE(defaulted_v6_address == test_v6_any_addr);
        VerifyV6AnyAddress(defaulted_v6_address, false);

        defaulted_v6_address.set_port(TestPort);
        REQUIRE(defaulted_v6_address.family() == AF_INET6);
        REQUIRE(defaulted_v6_address.port() == TestPort);
        REQUIRE(defaulted_v6_address.address_type() == NlatUnspecified);
        REQUIRE(!defaulted_v6_address.is_address_linklocal());
        REQUIRE(!defaulted_v6_address.is_address_loopback());
        REQUIRE(defaulted_v6_address == test_v6_any_addr_with_port);
        VerifyV6AnyAddress(defaulted_v6_address, true);
    }

    SECTION("verify set_address_loopback")
    {
        const auto VerifyV4LoopbackAddress = [&](const wil::network::socket_address& v4_address, bool with_port) {
            wil::network::socket_address_wstring loopback_address_test_string{};
            REQUIRE(SUCCEEDED(v4_address.format_address_nothrow(loopback_address_test_string)));
            REQUIRE(0 == memcmp(Test_loopback_in_addr_string, loopback_address_test_string, sizeof(Test_loopback_in_addr_string)));
#ifdef WIL_ENABLE_EXCEPTIONS
            REQUIRE(Test_loopback_in_addr_string == v4_address.format_address());
#endif

            ZeroMemory(loopback_address_test_string, sizeof(loopback_address_test_string));

            REQUIRE(SUCCEEDED(v4_address.format_complete_address_nothrow(loopback_address_test_string)));
            if (with_port)
            {
                REQUIRE(0 == memcmp(Test_loopback_in_addr_string_with_port, loopback_address_test_string, sizeof(Test_loopback_in_addr_string_with_port)));
#ifdef WIL_ENABLE_EXCEPTIONS
                REQUIRE(Test_loopback_in_addr_string_with_port == v4_address.format_complete_address());
#endif
            }
            else
            {
                REQUIRE(0 == memcmp(Test_loopback_in_addr_string, loopback_address_test_string, sizeof(Test_loopback_in_addr_string)));
#ifdef WIL_ENABLE_EXCEPTIONS
                REQUIRE(Test_loopback_in_addr_string == v4_address.format_complete_address());
#endif
            }
        };

        const auto VerifyV6LoopbackAddress = [&](const wil::network::socket_address& v6_address, bool with_port) {
            wil::network::socket_address_wstring loopback_address_test_string{};
            REQUIRE(SUCCEEDED(v6_address.format_address_nothrow(loopback_address_test_string)));
            REQUIRE(0 == memcmp(Test_loopback_in6_addr_string, loopback_address_test_string, sizeof(Test_loopback_in6_addr_string)));
#ifdef WIL_ENABLE_EXCEPTIONS
            REQUIRE(Test_loopback_in6_addr_string == v6_address.format_address());
#endif

            ZeroMemory(loopback_address_test_string, sizeof(loopback_address_test_string));

            REQUIRE(SUCCEEDED(v6_address.format_complete_address_nothrow(loopback_address_test_string)));
            if (with_port)
            {
                REQUIRE(0 == memcmp(Test_loopback_in6_addr_string_with_port, loopback_address_test_string, sizeof(Test_loopback_in6_addr_string_with_port)));
#ifdef WIL_ENABLE_EXCEPTIONS
                REQUIRE(Test_loopback_in6_addr_string_with_port == v6_address.format_complete_address());
#endif
            }
            else
            {
                REQUIRE(0 == memcmp(Test_loopback_in6_addr_string, loopback_address_test_string, sizeof(Test_loopback_in6_addr_string)));
#ifdef WIL_ENABLE_EXCEPTIONS
                REQUIRE(Test_loopback_in6_addr_string == v6_address.format_complete_address());
#endif
            }
        };

        wil::network::socket_address v4_address;
        v4_address.set_address_loopback(AF_INET);
        REQUIRE(v4_address.family() == AF_INET);
        REQUIRE(v4_address.port() == 0);
        REQUIRE(v4_address.address_type() == NlatUnicast);
        REQUIRE(!v4_address.is_address_linklocal());
        REQUIRE(v4_address.is_address_loopback());
        REQUIRE(v4_address == test_v4_loopback_addr);
        VerifyV4LoopbackAddress(v4_address, false);

        v4_address.set_port(TestPort);
        REQUIRE(v4_address.family() == AF_INET);
        REQUIRE(v4_address.port() == TestPort);
        REQUIRE(v4_address.address_type() == NlatUnicast);
        REQUIRE(!v4_address.is_address_linklocal());
        REQUIRE(v4_address.is_address_loopback());
        REQUIRE(v4_address == test_v4_loopback_addr_with_port);
        VerifyV4LoopbackAddress(v4_address, true);

        // verify changing families
        v4_address.set_address_loopback(AF_INET6);
        REQUIRE(v4_address.family() == AF_INET6);
        REQUIRE(v4_address.port() == TestPort);
        REQUIRE(v4_address.address_type() == NlatUnicast);
        REQUIRE(!v4_address.is_address_linklocal());
        REQUIRE(v4_address.is_address_loopback());
        REQUIRE(v4_address == test_v6_loopback_addr_with_port);
        VerifyV6LoopbackAddress(v4_address, true);

        wil::network::socket_address v6_address;
        v6_address.set_address_loopback(AF_INET6);
        REQUIRE(v6_address.family() == AF_INET6);
        REQUIRE(v6_address.port() == 0);
        REQUIRE(v6_address.address_type() == NlatUnicast);
        REQUIRE(!v6_address.is_address_linklocal());
        REQUIRE(v6_address.is_address_loopback());
        REQUIRE(v6_address == test_v6_loopback_addr);
        VerifyV6LoopbackAddress(v6_address, false);

        v6_address.set_port(TestPort);
        REQUIRE(v6_address.family() == AF_INET6);
        REQUIRE(v6_address.port() == TestPort);
        REQUIRE(v6_address.address_type() == NlatUnicast);
        REQUIRE(!v6_address.is_address_linklocal());
        REQUIRE(v6_address.is_address_loopback());
        REQUIRE(v6_address == test_v6_loopback_addr_with_port);
        VerifyV6LoopbackAddress(v6_address, true);

        // verify changing families
        v6_address.set_address_loopback(AF_INET);
        REQUIRE(v6_address.family() == AF_INET);
        REQUIRE(v6_address.port() == TestPort);
        REQUIRE(v6_address.address_type() == NlatUnicast);
        REQUIRE(!v6_address.is_address_linklocal());
        REQUIRE(v6_address.is_address_loopback());
        REQUIRE(v6_address == test_v4_loopback_addr_with_port);
        VerifyV4LoopbackAddress(v6_address, true);

        wil::network::socket_address defaulted_v4_address{AF_INET};
        defaulted_v4_address.set_address_loopback();
        REQUIRE(defaulted_v4_address.family() == AF_INET);
        REQUIRE(defaulted_v4_address.port() == 0);
        REQUIRE(defaulted_v4_address.address_type() == NlatUnicast);
        REQUIRE(!defaulted_v4_address.is_address_linklocal());
        REQUIRE(defaulted_v4_address.is_address_loopback());
        REQUIRE(defaulted_v4_address == test_v4_loopback_addr);
        VerifyV4LoopbackAddress(defaulted_v4_address, false);

        defaulted_v4_address.set_port(TestPort);
        REQUIRE(defaulted_v4_address.family() == AF_INET);
        REQUIRE(defaulted_v4_address.port() == TestPort);
        REQUIRE(defaulted_v4_address.address_type() == NlatUnicast);
        REQUIRE(!defaulted_v4_address.is_address_linklocal());
        REQUIRE(defaulted_v4_address.is_address_loopback());
        REQUIRE(defaulted_v4_address == test_v4_loopback_addr_with_port);
        VerifyV4LoopbackAddress(defaulted_v4_address, true);

        wil::network::socket_address defaulted_v6_address{AF_INET6};
        defaulted_v6_address.set_address_loopback();
        REQUIRE(defaulted_v6_address.family() == AF_INET6);
        REQUIRE(defaulted_v6_address.port() == 0);
        REQUIRE(defaulted_v6_address.address_type() == NlatUnicast);
        REQUIRE(!defaulted_v6_address.is_address_linklocal());
        REQUIRE(defaulted_v6_address.is_address_loopback());
        REQUIRE(defaulted_v6_address == test_v6_loopback_addr);
        VerifyV6LoopbackAddress(defaulted_v6_address, false);

        defaulted_v6_address.set_port(TestPort);
        REQUIRE(defaulted_v6_address.family() == AF_INET6);
        REQUIRE(defaulted_v6_address.port() == TestPort);
        REQUIRE(defaulted_v6_address.address_type() == NlatUnicast);
        REQUIRE(!defaulted_v6_address.is_address_linklocal());
        REQUIRE(defaulted_v6_address.is_address_loopback());
        REQUIRE(defaulted_v6_address == test_v6_loopback_addr_with_port);
        VerifyV6LoopbackAddress(defaulted_v6_address, true);
    }

    SECTION("verify v4 reset_address_nothrow")
    {
        wil::network::socket_address v4_address;
        v4_address.set_address_loopback(AF_INET);
        v4_address.set_port(TestPort);
        REQUIRE(v4_address.address_type() == NlatUnicast);
        REQUIRE(v4_address.is_address_loopback());
        REQUIRE(TestPort == v4_address.port());

        REQUIRE(SUCCEEDED(v4_address.reset_address_nothrow(Test_in_addr_string)));
        REQUIRE(equals(*v4_address.in_addr(), Test_in_addr));
        REQUIRE(v4_address.port() == 0);
        REQUIRE(v4_address.address_type() == NlatUnicast);
        REQUIRE(!v4_address.is_address_loopback());

        wil::network::socket_address_wstring v4_address_string{};
        REQUIRE(SUCCEEDED(v4_address.format_address_nothrow(v4_address_string)));
        REQUIRE(0 == memcmp(Test_in_addr_string, v4_address_string, sizeof(Test_in_addr_string)));

        REQUIRE(SUCCEEDED(v4_address.reset_address_nothrow(Test_in_addr_string2)));
        REQUIRE(equals(*v4_address.in_addr(), Test_in_addr2));
        REQUIRE(v4_address.port() == 0);
        REQUIRE(v4_address.address_type() == NlatUnicast);
        REQUIRE(!v4_address.is_address_loopback());

        REQUIRE(SUCCEEDED(v4_address.format_address_nothrow(v4_address_string)));
        REQUIRE(0 == memcmp(Test_in_addr_string2, v4_address_string, sizeof(Test_in_addr_string2)));

        REQUIRE(SUCCEEDED(v4_address.reset_address_nothrow(Test_in_addr_char_string)));
        REQUIRE(equals(*v4_address.in_addr(), Test_in_addr));
        REQUIRE(v4_address.port() == 0);
        REQUIRE(v4_address.address_type() == NlatUnicast);
        REQUIRE(!v4_address.is_address_loopback());

        wil::network::socket_address_string v4_address_char_string{};
        REQUIRE(SUCCEEDED(v4_address.format_address_nothrow(v4_address_char_string)));
        REQUIRE(0 == memcmp(Test_in_addr_char_string, v4_address_char_string, sizeof(Test_in_addr_char_string)));

        REQUIRE(SUCCEEDED(v4_address.reset_address_nothrow(Test_in_addr_char_string2)));
        REQUIRE(equals(*v4_address.in_addr(), Test_in_addr2));
        REQUIRE(v4_address.port() == 0);
        REQUIRE(v4_address.address_type() == NlatUnicast);
        REQUIRE(!v4_address.is_address_loopback());

        REQUIRE(SUCCEEDED(v4_address.format_address_nothrow(v4_address_char_string)));
        REQUIRE(0 == memcmp(Test_in_addr_char_string2, v4_address_char_string, sizeof(Test_in_addr_char_string2)));

        // reset_address via a SOCKET bound to an address
        wil::unique_socket test_socket{::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)};
        REQUIRE(test_socket.get() != INVALID_SOCKET);
        wil::network::socket_address test_address;
        test_address.set_address_loopback(AF_INET);
        test_address.set_port(TestPort);

        int gle = 0;
        auto bind_error = ::bind(test_socket.get(), test_address.sockaddr(), test_address.size());
        if (bind_error != 0)
        {
            gle = ::WSAGetLastError();
        }
        REQUIRE(gle == 0);
        REQUIRE(bind_error == 0);

        v4_address.reset();
        REQUIRE(v4_address.address_type() == NlatUnspecified);
        REQUIRE(!v4_address.is_address_loopback());
        REQUIRE(0 == v4_address.port());

        REQUIRE(SUCCEEDED(v4_address.reset_address_nothrow(test_socket.get())));
        REQUIRE(AF_INET == v4_address.family());
        REQUIRE(v4_address.address_type() == NlatUnicast);
        REQUIRE(v4_address.is_address_loopback());
        REQUIRE(TestPort == v4_address.port());

        REQUIRE(SUCCEEDED(v4_address.format_address_nothrow(v4_address_string)));
        REQUIRE(0 == memcmp(Test_loopback_in_addr_string, v4_address_string, sizeof(Test_loopback_in_addr_string)));
    }

    SECTION("verify v4 reset_address throwing version")
    {
#ifdef WIL_ENABLE_EXCEPTIONS
        wil::network::socket_address v4_address;
        v4_address.set_address_loopback(AF_INET);
        v4_address.set_port(TestPort);
        REQUIRE(v4_address.address_type() == NlatUnicast);
        REQUIRE(v4_address.is_address_loopback());
        REQUIRE(TestPort == v4_address.port());

        v4_address.reset_address(Test_in_addr_string);
        REQUIRE(equals(*v4_address.in_addr(), Test_in_addr));
        REQUIRE(v4_address.port() == 0);
        REQUIRE(v4_address.address_type() == NlatUnicast);
        REQUIRE(!v4_address.is_address_loopback());

        std::wstring v4_address_string;
        v4_address_string = v4_address.format_address();
        REQUIRE(v4_address_string == Test_in_addr_string);

        v4_address.reset_address(Test_in_addr_string2);
        REQUIRE(equals(*v4_address.in_addr(), Test_in_addr2));
        REQUIRE(v4_address.port() == 0);
        REQUIRE(v4_address.address_type() == NlatUnicast);
        REQUIRE(!v4_address.is_address_loopback());

        v4_address_string = v4_address.format_address();
        REQUIRE(v4_address_string == Test_in_addr_string2);

        v4_address.reset_address(Test_in_addr_char_string);
        REQUIRE(equals(*v4_address.in_addr(), Test_in_addr));
        REQUIRE(v4_address.port() == 0);
        REQUIRE(v4_address.address_type() == NlatUnicast);
        REQUIRE(!v4_address.is_address_loopback());

        v4_address_string = v4_address.format_address();
        REQUIRE(v4_address_string == Test_in_addr_string);

        v4_address.reset_address(Test_in_addr_char_string2);
        REQUIRE(equals(*v4_address.in_addr(), Test_in_addr2));
        REQUIRE(v4_address.port() == 0);
        REQUIRE(v4_address.address_type() == NlatUnicast);
        REQUIRE(!v4_address.is_address_loopback());

        v4_address_string = v4_address.format_address();
        REQUIRE(v4_address_string == Test_in_addr_string2);

        // reset_address via a SOCKET bound to an address
        wil::unique_socket test_socket{::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)};
        REQUIRE(test_socket.get() != INVALID_SOCKET);
        wil::network::socket_address test_address;
        test_address.set_address_loopback(AF_INET);
        test_address.set_port(TestPort);

        int gle = 0;
        auto bind_error = ::bind(test_socket.get(), test_address.sockaddr(), test_address.size());
        if (bind_error != 0)
        {
            gle = ::WSAGetLastError();
        }
        REQUIRE(gle == 0);
        REQUIRE(bind_error == 0);

        v4_address.reset();
        REQUIRE(v4_address.address_type() == NlatUnspecified);
        REQUIRE(!v4_address.is_address_loopback());
        REQUIRE(0 == v4_address.port());

        v4_address.reset_address(test_socket.get());
        REQUIRE(AF_INET == v4_address.family());
        REQUIRE(v4_address.address_type() == NlatUnicast);
        REQUIRE(v4_address.is_address_loopback());
        REQUIRE(TestPort == v4_address.port());

        v4_address_string = v4_address.format_address();
        REQUIRE(v4_address_string == Test_loopback_in_addr_string);
#endif
    }

    SECTION("verify v6 reset_address_nothrow")
    {
        wil::network::socket_address v6_address;
        v6_address.set_address_loopback(AF_INET6);
        v6_address.set_port(TestPort);
        REQUIRE(v6_address.address_type() == NlatUnicast);
        REQUIRE(v6_address.is_address_loopback());
        REQUIRE(TestPort == v6_address.port());

        REQUIRE(SUCCEEDED(v6_address.reset_address_nothrow(Test_in6_addr_string)));
        REQUIRE(equals(*v6_address.in6_addr(), Test_in6_addr));
        REQUIRE(v6_address.port() == 0);
        REQUIRE(v6_address.address_type() == NlatUnicast);
        REQUIRE(!v6_address.is_address_loopback());

        wil::network::socket_address_wstring v6_address_string{};
        REQUIRE(SUCCEEDED(v6_address.format_address_nothrow(v6_address_string)));
        REQUIRE(0 == memcmp(Test_in6_addr_string, v6_address_string, sizeof(Test_in6_addr_string)));

        REQUIRE(SUCCEEDED(v6_address.reset_address_nothrow(Test_in6_addr_string2)));
        REQUIRE(equals(*v6_address.in6_addr(), Test_in6_addr2));
        REQUIRE(v6_address.port() == 0);
        REQUIRE(v6_address.address_type() == NlatUnicast);
        REQUIRE(!v6_address.is_address_loopback());

        REQUIRE(SUCCEEDED(v6_address.format_address_nothrow(v6_address_string)));
        REQUIRE(0 == memcmp(Test_in6_addr_string2, v6_address_string, sizeof(Test_in6_addr_string2)));

        REQUIRE(SUCCEEDED(v6_address.reset_address_nothrow(Test_in6_addr_char_string)));
        REQUIRE(equals(*v6_address.in6_addr(), Test_in6_addr));
        REQUIRE(v6_address.port() == 0);
        REQUIRE(v6_address.address_type() == NlatUnicast);
        REQUIRE(!v6_address.is_address_loopback());

        wil::network::socket_address_string v6_address_char_string{};
        REQUIRE(SUCCEEDED(v6_address.format_address_nothrow(v6_address_char_string)));
        REQUIRE(0 == memcmp(Test_in6_addr_char_string, v6_address_char_string, sizeof(Test_in6_addr_char_string)));

        REQUIRE(SUCCEEDED(v6_address.reset_address_nothrow(Test_in6_addr_char_string2)));
        REQUIRE(equals(*v6_address.in6_addr(), Test_in6_addr2));
        REQUIRE(v6_address.port() == 0);
        REQUIRE(v6_address.address_type() == NlatUnicast);
        REQUIRE(!v6_address.is_address_loopback());

        REQUIRE(SUCCEEDED(v6_address.format_address_nothrow(v6_address_char_string)));
        REQUIRE(0 == memcmp(Test_in6_addr_char_string2, v6_address_char_string, sizeof(Test_in6_addr_char_string2)));

        // reset_address via a SOCKET bound to an address
        wil::unique_socket test_socket{::socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP)};
        REQUIRE(test_socket.get() != INVALID_SOCKET);
        wil::network::socket_address test_address;
        test_address.set_address_loopback(AF_INET6);
        test_address.set_port(TestPort);

        int gle = 0;
        auto bind_error = ::bind(test_socket.get(), test_address.sockaddr(), test_address.size());
        if (bind_error != 0)
        {
            gle = ::WSAGetLastError();
        }
        REQUIRE(gle == 0);
        REQUIRE(bind_error == 0);

        v6_address.reset();
        REQUIRE(v6_address.address_type() == NlatUnspecified);
        REQUIRE(!v6_address.is_address_loopback());
        REQUIRE(0 == v6_address.port());

        REQUIRE(SUCCEEDED(v6_address.reset_address_nothrow(test_socket.get())));
        REQUIRE(AF_INET6 == v6_address.family());
        REQUIRE(v6_address.address_type() == NlatUnicast);
        REQUIRE(v6_address.is_address_loopback());
        REQUIRE(TestPort == v6_address.port());

        REQUIRE(SUCCEEDED(v6_address.format_address_nothrow(v6_address_string)));
        REQUIRE(0 == memcmp(Test_loopback_in6_addr_string, v6_address_string, sizeof(Test_loopback_in6_addr_string)));
    }

    SECTION("verify v6 reset_address throwing version")
    {
#ifdef WIL_ENABLE_EXCEPTIONS
        wil::network::socket_address v6_address;
        v6_address.set_address_loopback(AF_INET6);
        v6_address.set_port(TestPort);
        REQUIRE(v6_address.address_type() == NlatUnicast);
        REQUIRE(v6_address.is_address_loopback());
        REQUIRE(TestPort == v6_address.port());

        v6_address.reset_address(Test_in6_addr_string);
        REQUIRE(equals(*v6_address.in6_addr(), Test_in6_addr));
        REQUIRE(v6_address.port() == 0);
        REQUIRE(v6_address.address_type() == NlatUnicast);
        REQUIRE(!v6_address.is_address_loopback());

        std::wstring v6_address_string;
        v6_address_string = v6_address.format_address();
        REQUIRE(v6_address_string == Test_in6_addr_string);

        v6_address.reset_address(Test_in6_addr_string2);
        REQUIRE(equals(*v6_address.in6_addr(), Test_in6_addr2));
        REQUIRE(v6_address.port() == 0);
        REQUIRE(v6_address.address_type() == NlatUnicast);
        REQUIRE(!v6_address.is_address_loopback());

        v6_address_string = v6_address.format_address();
        REQUIRE(v6_address_string == Test_in6_addr_string2);

        v6_address.reset_address(Test_in6_addr_char_string);
        REQUIRE(equals(*v6_address.in6_addr(), Test_in6_addr));
        REQUIRE(v6_address.port() == 0);
        REQUIRE(v6_address.address_type() == NlatUnicast);
        REQUIRE(!v6_address.is_address_loopback());

        v6_address_string = v6_address.format_address();
        REQUIRE(v6_address_string == Test_in6_addr_string);

        v6_address.reset_address(Test_in6_addr_char_string2);
        REQUIRE(equals(*v6_address.in6_addr(), Test_in6_addr2));
        REQUIRE(v6_address.port() == 0);
        REQUIRE(v6_address.address_type() == NlatUnicast);
        REQUIRE(!v6_address.is_address_loopback());

        v6_address_string = v6_address.format_address();
        REQUIRE(v6_address_string == Test_in6_addr_string2);

        // set_address via a SOCKET bound to an address
        wil::unique_socket test_socket{::socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP)};
        REQUIRE(test_socket.get() != INVALID_SOCKET);
        wil::network::socket_address test_address;
        test_address.set_address_loopback(AF_INET6);
        test_address.set_port(TestPort);

        int gle = 0;
        auto bind_error = ::bind(test_socket.get(), test_address.sockaddr(), test_address.size());
        if (bind_error != 0)
        {
            gle = ::WSAGetLastError();
        }
        REQUIRE(gle == 0);
        REQUIRE(bind_error == 0);

        v6_address.reset();
        REQUIRE(v6_address.address_type() == NlatUnspecified);
        REQUIRE(!v6_address.is_address_loopback());
        REQUIRE(0 == v6_address.port());

        v6_address.reset_address(test_socket.get());
        REQUIRE(AF_INET6 == v6_address.family());
        REQUIRE(v6_address.address_type() == NlatUnicast);
        REQUIRE(v6_address.is_address_loopback());
        REQUIRE(TestPort == v6_address.port());

        v6_address_string = v6_address.format_address();
        REQUIRE(v6_address_string == Test_loopback_in6_addr_string);
#endif
    }

    SECTION("verify additional set_* properties")
    {
        SOCKADDR_IN v4_test_sockaddr{};
        v4_test_sockaddr.sin_family = AF_INET;
        v4_test_sockaddr.sin_port = htons(TestPort); // the raw value is in network byte order
        memcpy(&v4_test_sockaddr.sin_addr, &Test_linklocal_in_addr, sizeof(in_addr));

        wil::network::socket_address v4_addr{&v4_test_sockaddr};
        REQUIRE(v4_addr.family() == AF_INET);
        REQUIRE(v4_addr.address_type() == NlatUnicast);
        REQUIRE(v4_addr.is_address_linklocal());
        REQUIRE(!v4_addr.is_address_loopback());
        REQUIRE(v4_addr.port() == TestPort);
#ifdef WIL_ENABLE_EXCEPTIONS
        REQUIRE(Test_linklocal_in_addr_string == v4_addr.format_address());
#endif

        v4_addr.set_port(TestPort + 1);
        // should be stored in network-byte-order
        REQUIRE(v4_addr.port() == TestPort + 1);
        REQUIRE(v4_addr.sockaddr_in()->sin_port == htons(TestPort + 1));

        SOCKADDR_IN6 v6_test_sockaddr{};
        v6_test_sockaddr.sin6_family = AF_INET6;
        // these raw values are in network byte order
        v6_test_sockaddr.sin6_port = htons(TestPort);
        v6_test_sockaddr.sin6_flowinfo = htonl(123456);
        v6_test_sockaddr.sin6_scope_id = htonl(234567);
        memcpy(&v6_test_sockaddr.sin6_addr, &Test_linklocal_in6_addr, sizeof(in6_addr));

        wil::network::socket_address v6_addr{&v6_test_sockaddr};
        REQUIRE(v6_addr.family() == AF_INET6);
        REQUIRE(v6_addr.address_type() == NlatUnicast);
        REQUIRE(v6_addr.is_address_linklocal());
        REQUIRE(!v6_addr.is_address_loopback());
        REQUIRE(v6_addr.port() == TestPort);
        REQUIRE(v6_addr.flow_info() == 123456);
        REQUIRE(v6_addr.scope_id() == 234567);
#ifdef WIL_ENABLE_EXCEPTIONS
        REQUIRE(Test_linklocal_in6_addr_string == v6_addr.format_address());
#endif

        v6_addr.set_flow_info(345678);
        // should be stored in network-byte-order
        REQUIRE(v6_addr.flow_info() == 345678);
        REQUIRE(v6_addr.sockaddr_in6()->sin6_flowinfo == htonl(345678));

        v6_addr.set_scope_id(456789);
        // should be stored in network-byte-order
        REQUIRE(v6_addr.scope_id() == 456789);
        REQUIRE(v6_addr.sockaddr_in6()->sin6_scope_id == htonl(456789));
    }

    SECTION("verify swap")
    {
        SOCKADDR_IN v4_test_sockaddr{};
        v4_test_sockaddr.sin_family = AF_INET;
        v4_test_sockaddr.sin_port = htons(TestPort); // the raw value is in network byte order
        memcpy(&v4_test_sockaddr.sin_addr, &Test_linklocal_in_addr, sizeof(in_addr));

        wil::network::socket_address v4_addr{&v4_test_sockaddr};
        REQUIRE(v4_addr.family() == AF_INET);
        REQUIRE(v4_addr.address_type() == NlatUnicast);
        REQUIRE(v4_addr.is_address_linklocal());
        REQUIRE(!v4_addr.is_address_loopback());
        REQUIRE(v4_addr.port() == TestPort);
#ifdef WIL_ENABLE_EXCEPTIONS
        REQUIRE(Test_linklocal_in_addr_string == v4_addr.format_address());
#endif

        SOCKADDR_IN6 v6_test_sockaddr{};
        v6_test_sockaddr.sin6_family = AF_INET6;
        // these raw values are in network byte order
        v6_test_sockaddr.sin6_port = htons(TestPort);
        v6_test_sockaddr.sin6_flowinfo = htonl(123456);
        v6_test_sockaddr.sin6_scope_id = htonl(234567);
        memcpy(&v6_test_sockaddr.sin6_addr, &Test_linklocal_in6_addr, sizeof(in6_addr));

        wil::network::socket_address v6_addr{&v6_test_sockaddr};
        REQUIRE(v6_addr.family() == AF_INET6);
        REQUIRE(v6_addr.address_type() == NlatUnicast);
        REQUIRE(v6_addr.is_address_linklocal());
        REQUIRE(!v6_addr.is_address_loopback());
        REQUIRE(v6_addr.port() == TestPort);
        REQUIRE(v6_addr.flow_info() == 123456);
        REQUIRE(v6_addr.scope_id() == 234567);
#ifdef WIL_ENABLE_EXCEPTIONS
        REQUIRE(Test_linklocal_in6_addr_string == v6_addr.format_address());
#endif

        // swap v4 and v6
        wil::network::swap(v4_addr, v6_addr);

        // verify each has the others' properties
        REQUIRE(v6_addr.family() == AF_INET);
        REQUIRE(v6_addr.address_type() == NlatUnicast);
        REQUIRE(v6_addr.is_address_linklocal());
        REQUIRE(!v6_addr.is_address_loopback());
        REQUIRE(v6_addr.port() == TestPort);
#ifdef WIL_ENABLE_EXCEPTIONS
        REQUIRE(Test_linklocal_in_addr_string == v6_addr.format_address());
#endif

        REQUIRE(v4_addr.family() == AF_INET6);
        REQUIRE(v4_addr.address_type() == NlatUnicast);
        REQUIRE(v4_addr.is_address_linklocal());
        REQUIRE(!v4_addr.is_address_loopback());
        REQUIRE(v4_addr.port() == TestPort);
        REQUIRE(v4_addr.flow_info() == 123456);
        REQUIRE(v4_addr.scope_id() == 234567);
#ifdef WIL_ENABLE_EXCEPTIONS
        REQUIRE(Test_linklocal_in6_addr_string == v4_addr.format_address());
#endif
    }

    SECTION("verify map_dual_mode_4to6")
    {
        constexpr wchar_t dual_mode_string[] = L"::ffff:1.1.1.1";

        SOCKADDR_IN v4_test_sockaddr{};
        v4_test_sockaddr.sin_family = AF_INET;
        v4_test_sockaddr.sin_port = htons(TestPort); // the raw value is in network byte order
        memcpy(&v4_test_sockaddr.sin_addr, &Test_in_addr, sizeof(in_addr));

        wil::network::socket_address v4_addr{&v4_test_sockaddr};

        wil::network::socket_address mapped_addr = wil::network::map_dual_mode_4to6(v4_addr);
        REQUIRE(mapped_addr.family() == AF_INET6);
        REQUIRE(IN6_IS_ADDR_V4MAPPED(mapped_addr.in6_addr()));
#ifdef WIL_ENABLE_EXCEPTIONS
        REQUIRE(dual_mode_string == mapped_addr.format_address());
#else
        (void)dual_mode_string;
#endif
    }
}

TEST_CASE("NetworkingTests::Verifying_failure_paths", "[networking]")
{
    InitTestAddresses();
    auto wsa_startup_tracking = wil::network::WSAStartup_nothrow();
    REQUIRE(static_cast<bool>(wsa_startup_tracking));

    SECTION("verify reset_address socket failure path")
    {

        // reset_address via a SOCKET bound to an address - but this time do not call bind
        wil::unique_socket test_socket{::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)};
        REQUIRE(test_socket.get() != INVALID_SOCKET);

        wil::network::socket_address test_address;
        REQUIRE(FAILED(test_address.reset_address_nothrow(test_socket.get())));

#ifdef WIL_ENABLE_EXCEPTIONS
        bool exception_thrown = false;
        try
        {
            test_address.reset_address(test_socket.get());
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(WSAEINVAL));
            exception_thrown = true;
        }
        catch (...)
        {
            REQUIRE(false);
        }
        REQUIRE(exception_thrown);
#endif
    }

    SECTION("verify reset_address_nothrow bad-address-string failure path")
    {
        wil::network::socket_address test_address;
        REQUIRE(FAILED(test_address.reset_address_nothrow(L"abcdefg")));
        REQUIRE(FAILED(test_address.reset_address_nothrow("abcdefg")));

#ifdef WIL_ENABLE_EXCEPTIONS
        bool exception_thrown = false;
        try
        {
            test_address.reset_address(L"abcdefg");
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER));
            exception_thrown = true;
        }
        catch (...)
        {
            REQUIRE(false);
        }
        REQUIRE(exception_thrown);

        exception_thrown = false;
        try
        {
            test_address.reset_address("abcdefg");
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER));
            exception_thrown = true;
        }
        catch (...)
        {
            REQUIRE(false);
        }
        REQUIRE(exception_thrown);
#endif
    }

    SECTION("verify format_address_nothrow AF_UNSPEC")
    {
        wil::network::socket_address af_unspec_address;
        af_unspec_address.reset();

        wil::network::socket_address_string string_address;
        REQUIRE(SUCCEEDED(af_unspec_address.format_address_nothrow(string_address)));
        REQUIRE('\0' == string_address[0]);
        REQUIRE(SUCCEEDED(af_unspec_address.format_complete_address_nothrow(string_address)));
        REQUIRE('\0' == string_address[0]);

        wil::network::socket_address_wstring wstring_address;
        REQUIRE(SUCCEEDED(af_unspec_address.format_address_nothrow(wstring_address)));
        REQUIRE(L'\0' == wstring_address[0]);
        REQUIRE(SUCCEEDED(af_unspec_address.format_complete_address_nothrow(wstring_address)));
        REQUIRE(L'\0' == wstring_address[0]);

#ifdef WIL_ENABLE_EXCEPTIONS
        std::wstring test_string = af_unspec_address.format_address();
        REQUIRE(test_string.empty());

        test_string = af_unspec_address.format_complete_address();
        REQUIRE(test_string.empty());
#endif
    }

    SECTION("verify format_address_nothrow failure path")
    {
        wil::network::socket_address test_address;
        // set an unsupported family for verifying failure paths
        test_address.reset(AF_APPLETALK);

        wil::network::socket_address_wstring wstring_address;
        REQUIRE(FAILED(test_address.format_address_nothrow(wstring_address)));
        REQUIRE(FAILED(test_address.format_complete_address_nothrow(wstring_address)));

#ifdef WIL_ENABLE_EXCEPTIONS
        bool exception_thrown = false;
        try
        {
            std::wstring test_string = test_address.format_address();
            // should never get here
            REQUIRE(test_string.empty());
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(WSAEAFNOSUPPORT));
            exception_thrown = true;
        }
        catch (...)
        {
            REQUIRE(false);
        }
        REQUIRE(exception_thrown);

        exception_thrown = false;
        try
        {
            std::wstring test_string = test_address.format_complete_address();
            // should never get here
            REQUIRE(test_string.empty());
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(WSAEINVAL));
            exception_thrown = true;
        }
        catch (...)
        {
            REQUIRE(false);
        }
        REQUIRE(exception_thrown);
#endif

        wil::network::socket_address_string string_address;
        REQUIRE(FAILED(test_address.format_address_nothrow(string_address)));
        REQUIRE(FAILED(test_address.format_complete_address_nothrow(string_address)));
    }

    SECTION("verify format_address_nothrow maximum string size")
    {
        {
            wil::network::socket_address test_mapped_address;
            REQUIRE(SUCCEEDED(test_mapped_address.reset_address_nothrow(L"0000:0000:0000:0000:0000:ffff:255.255.255.255")));
            test_mapped_address.set_port(std::numeric_limits<USHORT>::max());
            test_mapped_address.set_scope_id(std::numeric_limits<ULONG>::max());
            test_mapped_address.set_flow_info(std::numeric_limits<ULONG>::max());

            wil::network::socket_address_wstring test_mapped_address_string;
            REQUIRE(SUCCEEDED(test_mapped_address.format_address_nothrow(test_mapped_address_string)));
            REQUIRE(SUCCEEDED(test_mapped_address.format_complete_address_nothrow(test_mapped_address_string)));
#ifdef WIL_ENABLE_EXCEPTIONS
            std::wstring test_mapped_address_wstring = test_mapped_address.format_complete_address();
            constexpr auto* expected_test_mapped_address_string = L"[::ffff:255.255.255.255%4294967295]:65535";
            REQUIRE(expected_test_mapped_address_string == test_mapped_address_wstring);
#endif
        }

        {
            wil::network::socket_address test_max_v6_address;
            REQUIRE(SUCCEEDED(test_max_v6_address.reset_address_nothrow(L"ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff")));
            test_max_v6_address.set_port(std::numeric_limits<USHORT>::max());
            test_max_v6_address.set_scope_id(std::numeric_limits<ULONG>::max());
            test_max_v6_address.set_flow_info(std::numeric_limits<ULONG>::max());

            wil::network::socket_address_wstring test_max_v6_address_string;
            REQUIRE(SUCCEEDED(test_max_v6_address.format_address_nothrow(test_max_v6_address_string)));
            REQUIRE(SUCCEEDED(test_max_v6_address.format_complete_address_nothrow(test_max_v6_address_string)));
#ifdef WIL_ENABLE_EXCEPTIONS
            std::wstring test_max_v6_address_wstring = test_max_v6_address.format_complete_address();
            constexpr auto* expected_test_max_v6_address_string = L"[ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff%4294967295]:65535";
            REQUIRE(expected_test_max_v6_address_string == test_max_v6_address_wstring);
#endif
        }
    }
}

TEST_CASE("NetworkingTests::Verifying_function_tables", "[networking]")
{
    using wil::network::equals;

    SECTION("verify winsock_extension_function_table")
    {
        const auto verify_extension_table = [](const wil::network::winsock_extension_function_table& table) {
            // create a listening socket and post an AcceptEx on it
            wil::unique_socket listeningSocket{::socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP)};
            REQUIRE(listeningSocket.get() != INVALID_SOCKET);
            wil::network::socket_address listenAddress{AF_INET6};
            listenAddress.set_address_loopback();
            listenAddress.set_port(TestPort);

            int gle = 0;
            auto bind_error = ::bind(listeningSocket.get(), listenAddress.sockaddr(), listenAddress.size());
            if (bind_error != 0)
            {
                gle = ::WSAGetLastError();
            }
            REQUIRE(gle == 0);
            REQUIRE(bind_error == 0);

            gle = 0;
            auto listen_error = ::listen(listeningSocket.get(), 1);
            if (listen_error != 0)
            {
                gle = ::WSAGetLastError();
            }
            REQUIRE(gle == 0);
            REQUIRE(listen_error == 0);

            // the buffer to supply to AcceptEx to capture the address information
            static constexpr size_t singleAddressOutputBufferSize = listenAddress.size() + 16;
            char acceptex_output_buffer[singleAddressOutputBufferSize * 2]{};
            wil::unique_socket acceptSocket{::socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP)};
            REQUIRE(acceptSocket.get() != INVALID_SOCKET);

            DWORD acceptex_bytes_received{};
            wil::unique_event_nothrow acceptex_overlapped_event{};
            REQUIRE(SUCCEEDED(acceptex_overlapped_event.create()));
            REQUIRE(acceptex_overlapped_event.get() != nullptr);
            OVERLAPPED acceptex_overlapped{};
            acceptex_overlapped.hEvent = acceptex_overlapped_event.get();

            gle = 0;
            auto acceptex_return = table->AcceptEx(
                listeningSocket.get(),
                acceptSocket.get(),
                acceptex_output_buffer,
                0,
                singleAddressOutputBufferSize,
                singleAddressOutputBufferSize,
                &acceptex_bytes_received,
                &acceptex_overlapped);
            if (!acceptex_return)
            {
                gle = ::WSAGetLastError();
            }
            // should fail with ERROR_IO_PENDING
            REQUIRE(!acceptex_return);
            REQUIRE(gle == ERROR_IO_PENDING);
            // ensure that if this test function fails (returns) before AcceptEx completes asynchronously
            // that we wait for this async (overlapped) call to complete
            const auto ensure_acceptex_overlapped_completes = wil::scope_exit([&] {
                // close the sockets to cancel any pended IO
                acceptSocket.reset();
                listeningSocket.reset();
                // now wait for our async call
                acceptex_overlapped_event.wait();
            });

            // now create a socket to connect to it
            wil::unique_event_nothrow connectex_overlapped_event{};
            REQUIRE(SUCCEEDED(connectex_overlapped_event.create()));
            REQUIRE(connectex_overlapped_event.get() != nullptr);
            OVERLAPPED connectex_overlapped{};
            connectex_overlapped.hEvent = connectex_overlapped_event.get();

            wil::unique_socket connectingSocket{::socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP)};
            REQUIRE(connectingSocket.get() != INVALID_SOCKET);
            // ConnectEx requires a bound socket
            wil::network::socket_address connecting_from_address{AF_INET6};
            connecting_from_address.set_address_loopback();
            connecting_from_address.set_port(0); // just an ephemeral port, ConnectEx will find a port

            gle = 0;
            bind_error = ::bind(connectingSocket.get(), connecting_from_address.sockaddr(), connecting_from_address.size());
            if (bind_error != 0)
            {
                gle = ::WSAGetLastError();
            }
            REQUIRE(gle == 0);
            REQUIRE(bind_error == 0);

            gle = 0;
            auto connectex_return = table->ConnectEx(
                connectingSocket.get(), listenAddress.sockaddr(), listenAddress.size(), nullptr, 0, nullptr, &connectex_overlapped);
            if (!connectex_return)
            {
                gle = ::WSAGetLastError();
            }
            // should fail with ERROR_IO_PENDING
            REQUIRE(!connectex_return);
            REQUIRE(gle == ERROR_IO_PENDING);
            // ensure that if this test function fails (returns) before ConnectEx completes asynchronously
            // that we wait for this async (overlapped) call to complete
            const auto ensure_connectex_overlapped_completes = wil::scope_exit([&] {
                // close the socket to cancel any pended IO
                connectingSocket.reset();
                // now wait for our async call
                connectex_overlapped_event.wait();
            });

            // wait for both connect and accept to complete
            DWORD transfer_unused{};
            DWORD flags_unused{};
            gle = 0;
            auto connectex_overlapped_result = ::WSAGetOverlappedResult(
                connectingSocket.get(),
                &connectex_overlapped,
                &transfer_unused,
                TRUE, // should wait for connect to complete
                &flags_unused);
            if (!connectex_overlapped_result)
            {
                gle = ::WSAGetLastError();
            }
            REQUIRE(gle == 0);
            REQUIRE(connectex_overlapped_result == TRUE);

            gle = 0;
            auto acceptex_overlapped_result = ::WSAGetOverlappedResult(
                acceptSocket.get(),
                &acceptex_overlapped,
                &transfer_unused,
                TRUE, // should wait for connect to complete
                &flags_unused);
            if (!acceptex_overlapped_result)
            {
                gle = ::WSAGetLastError();
            }
            REQUIRE(gle == 0);
            REQUIRE(acceptex_overlapped_result == TRUE);

            // issue a DisconnectEx from the client
            wil::unique_event_nothrow disconnectex_overlapped_event{};
            REQUIRE(SUCCEEDED(disconnectex_overlapped_event.create()));
            REQUIRE(disconnectex_overlapped_event.get() != nullptr);
            OVERLAPPED disconnectex_overlapped{};
            disconnectex_overlapped.hEvent = disconnectex_overlapped_event.get();

            auto disconnectex_return = table->DisconnectEx(
                connectingSocket.get(),
                &disconnectex_overlapped,
                0, // not passing the reuse-socket flag
                0);
            if (!disconnectex_return)
            {
                gle = ::WSAGetLastError();
            }
            // should fail with ERROR_IO_PENDING
            REQUIRE(!disconnectex_return);
            REQUIRE(gle == ERROR_IO_PENDING);

            gle = 0;
            auto disconnectex_overlapped_result = ::WSAGetOverlappedResult(
                connectingSocket.get(),
                &disconnectex_overlapped,
                &transfer_unused,
                TRUE, // should wait for connect to complete
                &flags_unused);
            if (!disconnectex_overlapped_result)
            {
                gle = ::WSAGetLastError();
            }
            REQUIRE(gle == 0);
            REQUIRE(disconnectex_overlapped_result == TRUE);
        };

        // verify the first 3 function pointers are calling through correctly to confirm the function table is correct
        wil::network::winsock_extension_function_table test_table;
        REQUIRE(static_cast<bool>(test_table));
        REQUIRE(test_table->AcceptEx);
        REQUIRE(test_table->ConnectEx);
        REQUIRE(test_table->DisconnectEx);
        REQUIRE(test_table->GetAcceptExSockaddrs);
        REQUIRE(test_table->TransmitFile);
        REQUIRE(test_table->TransmitPackets);
        REQUIRE(test_table->WSARecvMsg);
        REQUIRE(test_table->WSASendMsg);
        verify_extension_table(test_table);

        // verify copy c'tor
        wil::network::winsock_extension_function_table copied_test_table{test_table};
        REQUIRE(static_cast<bool>(copied_test_table));
        REQUIRE(copied_test_table->AcceptEx);
        REQUIRE(copied_test_table->ConnectEx);
        REQUIRE(copied_test_table->DisconnectEx);
        REQUIRE(copied_test_table->GetAcceptExSockaddrs);
        REQUIRE(copied_test_table->TransmitFile);
        REQUIRE(copied_test_table->TransmitPackets);
        REQUIRE(copied_test_table->WSARecvMsg);
        REQUIRE(copied_test_table->WSASendMsg);
        verify_extension_table(copied_test_table);

        // verify move c'tor
        wil::network::winsock_extension_function_table move_ctor_test_table{std::move(test_table)};
        REQUIRE(!static_cast<bool>(test_table));
        REQUIRE(!test_table->AcceptEx);
        REQUIRE(!test_table->ConnectEx);
        REQUIRE(!test_table->DisconnectEx);
        REQUIRE(!test_table->GetAcceptExSockaddrs);
        REQUIRE(!test_table->TransmitFile);
        REQUIRE(!test_table->TransmitPackets);
        REQUIRE(!test_table->WSARecvMsg);
        REQUIRE(!test_table->WSASendMsg);
        REQUIRE(static_cast<bool>(move_ctor_test_table));
        REQUIRE(move_ctor_test_table->AcceptEx);
        REQUIRE(move_ctor_test_table->ConnectEx);
        REQUIRE(move_ctor_test_table->DisconnectEx);
        REQUIRE(move_ctor_test_table->GetAcceptExSockaddrs);
        REQUIRE(move_ctor_test_table->TransmitFile);
        REQUIRE(move_ctor_test_table->TransmitPackets);
        REQUIRE(move_ctor_test_table->WSARecvMsg);
        REQUIRE(move_ctor_test_table->WSASendMsg);
        verify_extension_table(move_ctor_test_table);
    }

    SECTION("verify rio_extension_function_table")
    {
        // verify 2 function pointers are calling through correctly to confirm the function table is correct
        wil::network::rio_extension_function_table test_table;
        REQUIRE(static_cast<bool>(test_table));
        REQUIRE(test_table->cbSize > 0);
        REQUIRE(test_table->RIOReceive);
        REQUIRE(test_table->RIOReceiveEx);
        REQUIRE(test_table->RIOSend);
        REQUIRE(test_table->RIOSendEx);
        REQUIRE(test_table->RIOCloseCompletionQueue);
        REQUIRE(test_table->RIOCreateCompletionQueue);
        REQUIRE(test_table->RIOCreateRequestQueue);
        REQUIRE(test_table->RIODequeueCompletion);
        REQUIRE(test_table->RIODeregisterBuffer);
        REQUIRE(test_table->RIONotify);
        REQUIRE(test_table->RIORegisterBuffer);
        REQUIRE(test_table->RIOResizeCompletionQueue);
        REQUIRE(test_table->RIOResizeRequestQueue);

        wil::unique_event_nothrow rio_completion_notification_event{};
        REQUIRE(SUCCEEDED(rio_completion_notification_event.create()));
        REQUIRE(rio_completion_notification_event.get() != nullptr);

        RIO_NOTIFICATION_COMPLETION rio_completion_notification{};
        rio_completion_notification.Type = RIO_EVENT_COMPLETION;
        rio_completion_notification.Event.EventHandle = rio_completion_notification_event.get();
        rio_completion_notification.Event.NotifyReset = FALSE;

        int gle = 0;
        const RIO_CQ rio_cq = test_table->RIOCreateCompletionQueue(
            10, // queue size
            &rio_completion_notification);
        if (rio_cq == RIO_INVALID_CQ)
        {
            gle = WSAGetLastError();
        }
        REQUIRE(gle == 0);
        REQUIRE(rio_cq != RIO_INVALID_CQ);

        test_table->RIOCloseCompletionQueue(rio_cq);
    }

    SECTION("verify socket_notification_function_table")
    {
        wil::network::process_socket_notification_table test_table;
        REQUIRE(static_cast<bool>(test_table));
        REQUIRE(test_table->ProcessSocketNotifications);

        wil::unique_socket listeningSocket{::socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP)};
        REQUIRE(listeningSocket.get() != INVALID_SOCKET);
        REQUIRE(!!listeningSocket);

        wil::unique_handle iocp{::CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0)};
        REQUIRE(iocp.get() != NULL);

        SOCK_NOTIFY_REGISTRATION notification;
        notification.socket = listeningSocket.get();
        notification.completionKey = nullptr;
        notification.eventFilter = SOCK_NOTIFY_REGISTER_EVENTS_ALL;
        notification.operation = SOCK_NOTIFY_OP_ENABLE;
        notification.triggerFlags = SOCK_NOTIFY_TRIGGER_LEVEL;

        OVERLAPPED_ENTRY completionEntry{};
        UINT32 entryCount{};
        DWORD notificationError =
            test_table->ProcessSocketNotifications(iocp.get(), 1, &notification, 0, 1, &completionEntry, &entryCount);
        REQUIRE(notificationError == WAIT_TIMEOUT);
    }

    SECTION("verify unique_socket_invalid_value")
    {
        // verify operator bool only returns false on INVALID_SOCKET
        wil::unique_socket test{NULL};
        REQUIRE(static_cast<bool>(test) == true);
        test.release(); // don't pass null to closesocket
        test.reset(INVALID_SOCKET);
        REQUIRE(static_cast<bool>(test) == false);
    }
}

TEST_CASE("NetworkingTests::Verifying_addr_info", "[networking]")
{
    using wil::network::addr_info_ansi_iterator;
    using wil::network::addr_info_iterator;
    using wil::network::addr_infoex_iterator;
    using wil::network::equals;

    const auto cleanup = wil::network::WSAStartup_nothrow();
    InitTestAddresses();

    // the end() iterator is just a default constructed iterator object
    const addr_info_iterator AddrInfoEndIterator{};

    SECTION("verify resolve_local_addresses")
    {
#if (defined(WIL_ENABLE_EXCEPTIONS))
        {
            const wil::unique_addrinfo test_addr = wil::network::resolve_local_addresses();
            const addr_info_iterator test_addr_iterator{test_addr.get()};

            // verify the begin() and end() interface contract
            REQUIRE(AddrInfoEndIterator == test_addr_iterator.end());
            REQUIRE(test_addr_iterator == test_addr_iterator.begin());
            REQUIRE(test_addr_iterator != AddrInfoEndIterator);

            // verify operator->
            REQUIRE(!test_addr_iterator->is_address_loopback());

            // verify range-for with a temp object created
            uint32_t count = 0;
            for (const auto& address : addr_info_iterator(test_addr.get()))
            {
                const auto family = address.family();
                REQUIRE((family == AF_INET || family == AF_INET6));
                REQUIRE(!address.is_address_loopback());

                wil::network::socket_address_wstring address_string;
                REQUIRE(SUCCEEDED(address.format_address_nothrow(address_string)));
#ifdef PRINTF_SOCKET_ADDRESSES
                wprintf(L"... resolve_local_addresses : %ws\n", address_string);
#endif
                ++count;
            }

            REQUIRE(count > 0);

            // verify make_range with a temp object created
            count = 0;
            for (const auto& address : wil::make_range(addr_info_iterator(test_addr.get()), addr_info_iterator{}))
            {
                const auto family = address.family();
                REQUIRE((family == AF_INET || family == AF_INET6));
                REQUIRE(!address.is_address_loopback());

                wil::network::socket_address_wstring address_string;
                REQUIRE(SUCCEEDED(address.format_address_nothrow(address_string)));
#ifdef PRINTF_SOCKET_ADDRESSES
                wprintf(L"... resolve_local_addresses : %ws\n", address_string);
#endif
                ++count;
            }

            REQUIRE(count > 0);

            // verify range-for with previously created iterator
            count = 0;
            for (const auto& address : test_addr_iterator)
            {
                const auto family = address.family();
                REQUIRE((family == AF_INET || family == AF_INET6));
                REQUIRE(!address.is_address_loopback());

                wil::network::socket_address_wstring address_string;
                REQUIRE(SUCCEEDED(address.format_address_nothrow(address_string)));
#ifdef PRINTF_SOCKET_ADDRESSES
                wprintf(L"... resolve_local_addresses : %ws\n", address_string);
#endif
                ++count;
            }

            REQUIRE(count > 0);

            // verify range-for with the same previously created iterator
            count = 0;
            for (const auto& address : test_addr_iterator)
            {
                const auto family = address.family();
                REQUIRE((family == AF_INET || family == AF_INET6));
                REQUIRE(!address.is_address_loopback());

                wil::network::socket_address_wstring address_string;
                REQUIRE(SUCCEEDED(address.format_address_nothrow(address_string)));
#ifdef PRINTF_SOCKET_ADDRESSES
                wprintf(L"... resolve_local_addresses (reusing iterator) : %ws\n", address_string);
#endif
                ++count;
            }

            REQUIRE(count > 0);
        }
#endif
    }

    SECTION("verify resolve_localhost_addresses")
    {
#if (defined(WIL_ENABLE_EXCEPTIONS))
        const wil::unique_addrinfo test_addr = wil::network::resolve_localhost_addresses();
        const addr_info_iterator test_addr_iterator{test_addr.get()};
        REQUIRE(test_addr_iterator != AddrInfoEndIterator);

        // verify operator->
        REQUIRE(test_addr_iterator->is_address_loopback());

        uint32_t count = 0;
        // this use of make_range is not what anyone would do in real code - this is just verifying interface behavior
        for (const auto& address : wil::make_range(test_addr_iterator, AddrInfoEndIterator))
        {
            const auto family = address.family();
            REQUIRE((family == AF_INET || family == AF_INET6));
            REQUIRE(address.is_address_loopback());

            switch (address.family())
            {
            case AF_INET:
                REQUIRE(equals(*address.in_addr(), Test_loopback_in_addr));
                break;

            case AF_INET6:
                REQUIRE(equals(*address.in6_addr(), Test_loopback_in6_addr));
                break;

            default:
                REQUIRE(false);
            }

            wil::network::socket_address_wstring address_string;
            REQUIRE(SUCCEEDED(address.format_address_nothrow(address_string)));
#ifdef PRINTF_SOCKET_ADDRESSES
            wprintf(L"... resolve_localhost_addresses : %ws\n", address_string);
#endif
            ++count;
        }

        REQUIRE(count > 0);

        count = 0;
        const auto localhost_addrinfo = wil::network::resolve_name(L"localhost");
        for (const auto& address : wil::make_range(addr_info_iterator{localhost_addrinfo.get()}, addr_info_iterator{}))
        {
            const auto family = address.family();
            REQUIRE((family == AF_INET || family == AF_INET6));
            REQUIRE(address.is_address_loopback());

            switch (address.family())
            {
            case AF_INET:
                REQUIRE(equals(*address.in_addr(), Test_loopback_in_addr));
                break;

            case AF_INET6:
                REQUIRE(equals(*address.in6_addr(), Test_loopback_in6_addr));
                break;

            default:
                REQUIRE(false);
            }

            wil::network::socket_address_wstring address_string;
            REQUIRE(SUCCEEDED(address.format_address_nothrow(address_string)));
#ifdef PRINTF_SOCKET_ADDRESSES
            wprintf(L"... wil::network::resolve_name : %ws\n", address_string);
#endif
            ++count;
        }
        REQUIRE(count > 0);
#endif
    }

    SECTION("verify const addr_info_iterator")
    {
        constexpr auto* local_address_name_string = L"localhost";
        ADDRINFOW* addrResult{};
        REQUIRE(::GetAddrInfoW(local_address_name_string, nullptr, nullptr, &addrResult) == 0);

        const wil::unique_addrinfo test_addr{addrResult};
        const addr_info_iterator test_addr_iterator{test_addr.get()};
        REQUIRE(test_addr_iterator != AddrInfoEndIterator);

        // verify operator->
        REQUIRE(test_addr_iterator->is_address_loopback());

        const auto& test_address_reference = *test_addr_iterator;
        REQUIRE(test_address_reference.is_address_loopback());
    }

    SECTION("verify addr_info_iterator increment")
    {
        constexpr auto* local_address_name_string = L"localhost";
        ADDRINFOW* addrResult{};
        REQUIRE(::GetAddrInfoW(local_address_name_string, nullptr, nullptr, &addrResult) == 0);

        const wil::unique_addrinfo initial_addr{addrResult};
        const addr_info_iterator initial_addr_iterator{initial_addr.get()};
        REQUIRE(initial_addr_iterator != AddrInfoEndIterator);

        auto total_count = 0;
        // this use of manually walking iterators is not what anyone would do in real code - this is just verifying interface behavior
        for (auto iter = initial_addr_iterator; iter != addr_info_iterator{}; ++iter)
        {
            ++total_count;
        }

        ADDRINFOW* testAddrResult{};
        REQUIRE(::GetAddrInfoW(local_address_name_string, nullptr, nullptr, &testAddrResult) == 0);
        const wil::unique_addrinfo test_addr{testAddrResult};
        addr_info_iterator test_iterator = addr_info_iterator{test_addr.get()};
        REQUIRE(test_iterator != AddrInfoEndIterator);

        for (auto count = 0; count < total_count; ++count)
        {
            ++test_iterator;
        }
        REQUIRE(test_iterator == AddrInfoEndIterator);

        test_iterator = addr_info_iterator{test_addr.get()};
        for (auto count = 0; count < total_count; ++count)
        {
            test_iterator++;
        }
        REQUIRE(test_iterator == test_iterator.end());
    }

    SECTION("verify addr_info_iterator move behavior")
    {
        constexpr auto* local_address_name_string = L"";
        ADDRINFOW* addrResult{};
        REQUIRE(::GetAddrInfoW(local_address_name_string, nullptr, nullptr, &addrResult) == 0);

        wil::unique_addrinfo moved_from_addr{addrResult};
        REQUIRE(addr_info_iterator{moved_from_addr.get()} != AddrInfoEndIterator);

        const wil::unique_addrinfo moved_to_addr = std::move(moved_from_addr);
        REQUIRE(moved_to_addr != moved_from_addr);

        // moved_from_addr should be end() now
        REQUIRE(addr_info_iterator{moved_from_addr.get()} == AddrInfoEndIterator);
        REQUIRE(addr_info_iterator{moved_to_addr.get()} != AddrInfoEndIterator);

        for (const auto& address : wil::make_range(addr_info_iterator{moved_to_addr.get()}, addr_info_iterator{}))
        {
            const auto family = address.family();
            REQUIRE((family == AF_INET || family == AF_INET6));
            REQUIRE(!address.is_address_loopback());

            wil::network::socket_address_wstring address_string;
            REQUIRE(SUCCEEDED(address.format_address_nothrow(address_string)));
#ifdef PRINTF_SOCKET_ADDRESSES
            wprintf(L"... moved resolve_local_addresses : %ws\n", address_string);
#endif
        }
    }

    SECTION("verify addr_info_iterator move assignment behavior")
    {
        constexpr auto* local_address_name_string = L"";
        ADDRINFOW* addrResult{};
        REQUIRE(::GetAddrInfoW(local_address_name_string, nullptr, nullptr, &addrResult) == 0);

        wil::unique_addrinfo moved_from_addr{addrResult};
        addr_info_iterator moved_from_addr_iterator{moved_from_addr.get()};
        REQUIRE(moved_from_addr_iterator != AddrInfoEndIterator);

        ADDRINFOW* moveToAddrResult{};
        REQUIRE(::GetAddrInfoW(local_address_name_string, nullptr, nullptr, &moveToAddrResult) == 0);

        wil::unique_addrinfo moved_to_addr{moveToAddrResult};
        moved_to_addr = std::move(moved_from_addr);
        REQUIRE(moved_to_addr != moved_from_addr);

        // moved_from_addr should be end() now
        moved_from_addr_iterator = addr_info_iterator{moved_from_addr.get()};
        REQUIRE(moved_from_addr_iterator == AddrInfoEndIterator);
        REQUIRE(addr_info_iterator{moved_to_addr.get()} != AddrInfoEndIterator);

        // move to self
        moved_to_addr = std::move(moved_to_addr);
        REQUIRE(addr_info_iterator{moved_to_addr.get()} != AddrInfoEndIterator);

        uint32_t count = 0;
        for (const auto& address : wil::make_range(addr_info_iterator{moved_to_addr.get()}, addr_info_iterator{}))
        {
            const auto family = address.family();
            REQUIRE((family == AF_INET || family == AF_INET6));
            REQUIRE(!address.is_address_loopback());
            ++count;

            wil::network::socket_address_wstring address_string;
            REQUIRE(SUCCEEDED(address.format_address_nothrow(address_string)));
#ifdef PRINTF_SOCKET_ADDRESSES
            wprintf(L"... move assignment resolve_local_addresses : %ws\n", address_string);
#endif
        }
        REQUIRE(count > 0);
    }

    // retest with unique_addrinfo_ansi
    SECTION("verify addr_info_ansi_iterator increment")
    {
        wil::unique_addrinfo_ansi initial_addr;
        REQUIRE(0 == getaddrinfo("localhost", nullptr, nullptr, initial_addr.addressof()));
        addr_info_ansi_iterator initial_addr_iterator{initial_addr.get()};
        REQUIRE(initial_addr_iterator != addr_info_ansi_iterator{});

        auto total_count = 0;
        for (auto iter = initial_addr_iterator; iter != addr_info_ansi_iterator{}; ++iter)
        {
            ++total_count;
        }

        wil::unique_addrinfo_ansi test_addr;
        REQUIRE(0 == getaddrinfo("localhost", nullptr, nullptr, test_addr.addressof()));
        addr_info_ansi_iterator test_iterator{test_addr.get()};
        REQUIRE(test_iterator != addr_info_ansi_iterator{});

        for (auto count = 0; count < total_count; ++count)
        {
            ++test_iterator;
        }

        test_iterator = addr_info_ansi_iterator{test_addr.get()};
        for (auto count = 0; count < total_count; ++count)
        {
            test_iterator++;
        }
        REQUIRE(test_iterator == addr_info_ansi_iterator{});
    }

    SECTION("verify addr_info_ansi_iterator move behavior")
    {
        wil::unique_addrinfo_ansi moved_from_addr;
        REQUIRE(0 == getaddrinfo("", nullptr, nullptr, moved_from_addr.addressof()));
        addr_info_ansi_iterator moved_from_addr_iterator{moved_from_addr.get()};
        REQUIRE(moved_from_addr_iterator != addr_info_ansi_iterator{});

        const wil::unique_addrinfo_ansi moved_to_addr = std::move(moved_from_addr);
        REQUIRE(moved_to_addr != moved_from_addr);

        // moved_from_addr should be end() now
        moved_from_addr_iterator = addr_info_ansi_iterator{moved_from_addr.get()};
        REQUIRE(moved_from_addr_iterator == addr_info_ansi_iterator{});
        REQUIRE(addr_info_ansi_iterator{moved_to_addr.get()} != addr_info_ansi_iterator{});

        for (const auto& address : wil::make_range(addr_info_ansi_iterator{moved_to_addr.get()}, addr_info_ansi_iterator{}))
        {
            const auto family = address.family();
            REQUIRE((family == AF_INET || family == AF_INET6));
            REQUIRE(!address.is_address_loopback());

            wil::network::socket_address_wstring address_string;
            REQUIRE(SUCCEEDED(address.format_address_nothrow(address_string)));
#ifdef PRINTF_SOCKET_ADDRESSES
            wprintf(L"... moved getaddrinfo(unique_addrinfo_ansi) : %ws\n", address_string);
#endif
        }
    }

    SECTION("verify addr_info_ansi_iterator move assignment behavior")
    {
        wil::unique_addrinfo_ansi moved_from_addr;
        REQUIRE(0 == getaddrinfo("", nullptr, nullptr, moved_from_addr.addressof()));
        const addr_info_ansi_iterator moved_from_addr_iterator{moved_from_addr.get()};
        REQUIRE(moved_from_addr_iterator != addr_info_ansi_iterator{});

        wil::unique_addrinfo_ansi moved_to_addr;
        REQUIRE(0 == getaddrinfo("", nullptr, nullptr, moved_to_addr.addressof()));
        moved_to_addr = std::move(moved_from_addr);
        REQUIRE(moved_to_addr != moved_from_addr);

        // moved_from_addr should be end() now
        REQUIRE(addr_info_ansi_iterator{moved_from_addr.get()} == addr_info_ansi_iterator{});
        REQUIRE(addr_info_ansi_iterator{moved_to_addr.get()} != addr_info_ansi_iterator{});

        // move to self
        moved_to_addr = std::move(moved_to_addr);
        REQUIRE(addr_info_ansi_iterator{moved_to_addr.get()} != addr_info_ansi_iterator{});

        for (const auto& address : wil::make_range(addr_info_ansi_iterator{moved_to_addr.get()}, addr_info_ansi_iterator{}))
        {
            const auto family = address.family();
            REQUIRE((family == AF_INET || family == AF_INET6));
            REQUIRE(!address.is_address_loopback());

            wil::network::socket_address_wstring address_string;
            REQUIRE(SUCCEEDED(address.format_address_nothrow(address_string)));
#ifdef PRINTF_SOCKET_ADDRESSES
            wprintf(L"... move assignment getaddrinfo(unique_addrinfo_ansi) : %ws\n", address_string);
#endif
        }
    }

    // retest with unique_addrinfoex
    SECTION("verify addr_info_ansi_iterator increment")
    {
        wil::unique_addrinfoex initial_addr;
        REQUIRE(0 == GetAddrInfoExW(L"localhost", nullptr, NS_ALL, nullptr, nullptr, initial_addr.addressof(), nullptr, nullptr, nullptr, nullptr));
        REQUIRE(addr_infoex_iterator{initial_addr.get()} != addr_infoex_iterator{});

        auto total_count = 0;
        for (auto iter = addr_infoex_iterator{initial_addr.get()}; iter != addr_infoex_iterator{}; ++iter)
        {
            ++total_count;
        }

        wil::unique_addrinfoex test_addr;
        REQUIRE(0 == GetAddrInfoExW(L"localhost", nullptr, NS_ALL, nullptr, nullptr, test_addr.addressof(), nullptr, nullptr, nullptr, nullptr));
        addr_infoex_iterator test_iterator = addr_infoex_iterator{test_addr.get()};
        REQUIRE(test_iterator != addr_infoex_iterator{});

        for (auto count = 0; count < total_count; ++count)
        {
            ++test_iterator;
        }

        test_iterator = addr_infoex_iterator{test_addr.get()};
        for (auto count = 0; count < total_count; ++count)
        {
            test_iterator++;
        }
        REQUIRE(test_iterator == test_iterator.end());
    }

    SECTION("verify addr_infoex_iterator move behavior")
    {
        wil::unique_addrinfoex moved_from_addr;
        REQUIRE(0 == GetAddrInfoExW(L"", nullptr, NS_ALL, nullptr, nullptr, moved_from_addr.addressof(), nullptr, nullptr, nullptr, nullptr));
        REQUIRE(addr_infoex_iterator{moved_from_addr.get()} != addr_infoex_iterator{});

        const wil::unique_addrinfoex moved_to_addr = std::move(moved_from_addr);
        REQUIRE(moved_to_addr != moved_from_addr);

        // moved_from_addr should be end() now
        REQUIRE(addr_infoex_iterator{moved_from_addr.get()} == addr_infoex_iterator{});
        REQUIRE(addr_infoex_iterator{moved_to_addr.get()} != addr_infoex_iterator{});

        for (const auto& address : wil::make_range(addr_infoex_iterator{moved_to_addr.get()}, addr_infoex_iterator{}))
        {
            const auto family = address.family();
            REQUIRE((family == AF_INET || family == AF_INET6));
            REQUIRE(!address.is_address_loopback());

            wil::network::socket_address_wstring address_string;
            REQUIRE(SUCCEEDED(address.format_address_nothrow(address_string)));
#ifdef PRINTF_SOCKET_ADDRESSES
            wprintf(L"... moved GetAddrInfoExW(unique_addrinfoex) : %ws\n", address_string);
#endif
        }
    }

    SECTION("verify addr_infoex_iterator move assignment behavior")
    {
        wil::unique_addrinfoex moved_from_addr;
        REQUIRE(0 == GetAddrInfoExW(L"", nullptr, NS_ALL, nullptr, nullptr, moved_from_addr.addressof(), nullptr, nullptr, nullptr, nullptr));
        REQUIRE(addr_infoex_iterator{moved_from_addr.get()} != addr_infoex_iterator{});

        wil::unique_addrinfoex moved_to_addr;
        REQUIRE(0 == GetAddrInfoExW(L"", nullptr, NS_ALL, nullptr, nullptr, moved_to_addr.addressof(), nullptr, nullptr, nullptr, nullptr));
        moved_to_addr = std::move(moved_from_addr);
        REQUIRE(moved_to_addr != moved_from_addr);

        // moved_from_addr should be end() now
        REQUIRE(addr_infoex_iterator{moved_from_addr.get()} == addr_infoex_iterator{});
        REQUIRE(addr_infoex_iterator{moved_to_addr.get()} != addr_infoex_iterator{});

        // move to self
        moved_to_addr = std::move(moved_to_addr);
        REQUIRE(addr_infoex_iterator{moved_to_addr.get()} != addr_infoex_iterator{});

        for (const auto& address : wil::make_range(addr_infoex_iterator{moved_to_addr.get()}, addr_infoex_iterator{}))
        {
            const auto family = address.family();
            REQUIRE((family == AF_INET || family == AF_INET6));
            REQUIRE(!address.is_address_loopback());

            wil::network::socket_address_wstring address_string;
            REQUIRE(SUCCEEDED(address.format_address_nothrow(address_string)));
#ifdef PRINTF_SOCKET_ADDRESSES
            wprintf(L"... move assignment GetAddrInfoExW(unique_addrinfoex) : %ws\n", address_string);
#endif
        }
    }
}