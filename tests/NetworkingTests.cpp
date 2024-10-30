#include "pch.h"

#include <wil/networking.h>

#include "common.h"

constexpr auto* Test_in_addr_string = L"1.1.1.1";
in_addr Test_in_addr{};
constexpr auto* Test_in_addr_string2 = L"1.1.1.2";
in_addr Test_in_addr2{};
constexpr auto* Test_in6_addr_string = L"2001::1:1:1:1";
in6_addr Test_in6_addr{};
constexpr auto* Test_in6_addr_string2 = L"2001::1:1:1:2";
in6_addr Test_in6_addr2{};

constexpr auto* Test_linklocal_in_addr_string = L"169.254.111.222";
in_addr Test_linklocal_in_addr{};
constexpr auto* Test_linklocal_in6_addr_string = L"fe80::1:1:1:1";
in6_addr Test_linklocal_in6_addr{};

constexpr auto* Test_any_in_addr_string = L"0.0.0.0";
in_addr Test_any_in_addr{};
constexpr auto* Test_any_in6_addr_string = L"::";
in6_addr Test_any_in6_addr{};

constexpr uint16_t TestPort = 12345;

INIT_ONCE SocketTestInit = INIT_ONCE_STATIC_INIT;
void InitTestAddresses()
{
    InitOnceExecuteOnce(
        &SocketTestInit,
        [](PINIT_ONCE, PVOID, PVOID*) -> BOOL {
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

            return TRUE;
        },
        nullptr,
        nullptr);
}

TEST_CASE("SocketTests::Verifying_wsastartup_cleanup", "[sockets]")
{
    // verify socket APIs fail without having called WSAStartup
    const auto socket_test = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    const auto gle = ::WSAGetLastError();
    REQUIRE(socket_test == INVALID_SOCKET);
    REQUIRE(gle == WSANOTINITIALISED);

    SECTION("Verifying _nothrow")
    {
        const auto cleanup = wil::networking::WSAStartup_nothrow();
        const bool succeeded = !!cleanup;
        REQUIRE(succeeded);
        const auto socket_test = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        REQUIRE(socket_test != INVALID_SOCKET);
        ::closesocket(socket_test);
    }

    SECTION("Verifying _failfast")
    {
        const auto cleanup = wil::networking::WSAStartup_failfast();
        const auto socket_test = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        REQUIRE(socket_test != INVALID_SOCKET);
        ::closesocket(socket_test);
    }

#ifdef WIL_ENABLE_EXCEPTIONS
    SECTION("Verifying throwing")
    {
        const auto cleanup = wil::networking::WSAStartup();
        const auto socket_test = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        REQUIRE(socket_test != INVALID_SOCKET);
        ::closesocket(socket_test);
    }
#endif
}

TEST_CASE("SocketTests::Verifying_in_addr_interactions", "[sockets]")
{
    InitTestAddresses();
    REQUIRE(wil::networking::socket_address::length() == sizeof(SOCKADDR_INET));

    wil::networking::socket_address default_addr{};

    wil::networking::socket_address test_v4_addr{&Test_in_addr};
    wil::networking::socket_address test_v4_addr2{&Test_in_addr2};
    wil::networking::socket_address test_v4_addr_with_port{&Test_in_addr, TestPort};

    wil::networking::socket_address test_v6_addr{&Test_in6_addr};
    wil::networking::socket_address test_v6_addr2{&Test_in6_addr2};
    wil::networking::socket_address test_v6_addr_with_port{&Test_in6_addr, TestPort};

    wil::networking::socket_address test_v4_linklocal_addr{&Test_linklocal_in_addr};
    wil::networking::socket_address test_v4_linklocal_addr_with_port{&Test_linklocal_in_addr, TestPort};

    wil::networking::socket_address test_v6_linklocal_addr{&Test_linklocal_in6_addr};
    wil::networking::socket_address test_v6_linklocal_addr_with_port{&Test_linklocal_in6_addr, TestPort};

    wil::networking::socket_address test_v4_any_addr{&Test_any_in_addr};
    wil::networking::socket_address test_v4_any_addr_with_port{&Test_any_in_addr, TestPort};

    wil::networking::socket_address test_v6_any_addr{&Test_any_in6_addr};
    wil::networking::socket_address test_v6_any_addr_with_port{&Test_any_in6_addr, TestPort};

    SECTION("Default socket address properties")
    {
        REQUIRE(default_addr.family() == AF_UNSPEC);
        REQUIRE(!default_addr.is_address_any());
        REQUIRE(!default_addr.is_address_linklocal());
        REQUIRE(!default_addr.is_address_loopback());
        REQUIRE(NlatUnspecified == default_addr.get_address_type());

        REQUIRE(default_addr == default_addr);
    }

    SECTION("IPv4 in_addr properties")
    {
        REQUIRE(test_v4_addr.family() == AF_INET);
        REQUIRE(!test_v4_addr.is_address_any());
        REQUIRE(!test_v4_addr.is_address_linklocal());
        REQUIRE(!test_v4_addr.is_address_loopback());
        REQUIRE(NlatUnicast == test_v4_addr.get_address_type());
        REQUIRE(NlatUnicast == test_v4_addr2.get_address_type());

        REQUIRE(test_v4_addr.in_addr()->s_addr == Test_in_addr.s_addr);
        REQUIRE(test_v4_addr2.in_addr()->s_addr == Test_in_addr2.s_addr);
        REQUIRE(test_v4_addr.port() == 0);
        REQUIRE(test_v4_addr.scope_id() == 0);
        REQUIRE(test_v4_addr.flow_info() == 0);

        REQUIRE(test_v4_addr == test_v4_addr);
        REQUIRE(test_v4_addr != test_v4_addr2);
        REQUIRE(test_v4_addr < test_v4_addr2);
        REQUIRE(test_v4_addr2 > test_v4_addr);
        REQUIRE(test_v4_addr != default_addr);
        REQUIRE(test_v4_addr > default_addr);
    }

    SECTION("IPv4 in_addr with port properties")
    {
        REQUIRE(test_v4_addr_with_port.family() == AF_INET);
        REQUIRE(!test_v4_addr_with_port.is_address_any());
        REQUIRE(!test_v4_addr_with_port.is_address_linklocal());
        REQUIRE(!test_v4_addr_with_port.is_address_loopback());
        REQUIRE(NlatUnicast == test_v4_addr_with_port.get_address_type());

        REQUIRE(test_v4_addr_with_port.in_addr()->s_addr == Test_in_addr.s_addr);
        REQUIRE(test_v4_addr_with_port.in_addr()->s_addr == test_v4_addr.in_addr()->s_addr);
        REQUIRE(test_v4_addr_with_port.port() == TestPort);
        REQUIRE(test_v4_addr_with_port.scope_id() == 0);
        REQUIRE(test_v4_addr_with_port.flow_info() == 0);

        REQUIRE(test_v4_addr_with_port == test_v4_addr_with_port);
        REQUIRE(test_v4_addr_with_port != default_addr);
        REQUIRE(test_v4_addr_with_port != test_v4_addr);
        REQUIRE(test_v4_addr_with_port > test_v4_addr);
        REQUIRE(test_v4_addr_with_port > test_v4_addr2);
        REQUIRE(test_v4_addr_with_port > default_addr);
    }

    SECTION("IPv6 in6_addr properties")
    {
        REQUIRE(test_v6_addr.family() == AF_INET6);
        REQUIRE(!test_v6_addr.is_address_any());
        REQUIRE(!test_v6_addr.is_address_linklocal());
        REQUIRE(!test_v6_addr.is_address_loopback());
        REQUIRE(NlatUnicast == test_v6_addr.get_address_type());
        REQUIRE(NlatUnicast == test_v6_addr2.get_address_type());

        REQUIRE(0 == memcmp(test_v6_addr.in6_addr(), &Test_in6_addr, sizeof(in6_addr)));
        REQUIRE(0 == memcmp(test_v6_addr2.in6_addr(), &Test_in6_addr2, sizeof(in6_addr)));
        REQUIRE(test_v6_addr.port() == 0);
        REQUIRE(test_v6_addr.scope_id() == 0);
        REQUIRE(test_v6_addr.flow_info() == 0);

        REQUIRE(test_v6_addr == test_v6_addr);
        REQUIRE(test_v6_addr != test_v6_addr2);
        REQUIRE(test_v6_addr < test_v6_addr2);
        REQUIRE(test_v6_addr2 > test_v6_addr);
        REQUIRE(test_v6_addr != test_v4_addr);
        REQUIRE(test_v6_addr > test_v4_addr);
        REQUIRE(test_v6_addr != default_addr);
        REQUIRE(test_v6_addr > default_addr);
    }

    SECTION("IPv6 in6_addr with port properties")
    {
        REQUIRE(test_v6_addr_with_port.family() == AF_INET6);
        REQUIRE(!test_v6_addr_with_port.is_address_any());
        REQUIRE(!test_v6_addr_with_port.is_address_linklocal());
        REQUIRE(!test_v6_addr_with_port.is_address_loopback());
        REQUIRE(NlatUnicast == test_v6_addr_with_port.get_address_type());

        REQUIRE(0 == memcmp(test_v6_addr_with_port.in6_addr(), &Test_in6_addr, sizeof(in6_addr)));
        REQUIRE(0 == memcmp(test_v6_addr_with_port.in6_addr(), test_v6_addr.in6_addr(), sizeof(in6_addr)));
        REQUIRE(test_v6_addr_with_port.port() == TestPort);
        REQUIRE(test_v6_addr_with_port.scope_id() == 0);
        REQUIRE(test_v6_addr_with_port.flow_info() == 0);

        REQUIRE(test_v6_addr_with_port == test_v6_addr_with_port);
        REQUIRE(test_v6_addr_with_port != test_v4_addr);
        REQUIRE(test_v6_addr_with_port != test_v4_addr_with_port);
        REQUIRE(test_v6_addr_with_port != test_v6_addr);
        REQUIRE(test_v6_addr_with_port != test_v6_addr2);
        REQUIRE(test_v6_addr_with_port > test_v6_addr);
        REQUIRE(test_v6_addr_with_port > test_v6_addr2);
        REQUIRE(test_v6_addr_with_port != default_addr);
        REQUIRE(test_v6_addr_with_port > default_addr);
    }

    SECTION("IPv4 link-local in_addr properties")
    {
        REQUIRE(test_v4_linklocal_addr.family() == AF_INET);
        REQUIRE(!test_v4_linklocal_addr.is_address_any());
        REQUIRE(test_v4_linklocal_addr.is_address_linklocal());
        REQUIRE(!test_v4_linklocal_addr.is_address_loopback());
        REQUIRE(NlatUnicast == test_v4_linklocal_addr.get_address_type());

        REQUIRE(test_v4_linklocal_addr.in_addr()->s_addr == Test_linklocal_in_addr.s_addr);
        REQUIRE(test_v4_linklocal_addr.port() == 0);
        REQUIRE(test_v4_linklocal_addr.scope_id() == 0);
        REQUIRE(test_v4_linklocal_addr.flow_info() == 0);

        REQUIRE(test_v4_linklocal_addr == test_v4_linklocal_addr);
        REQUIRE(test_v4_linklocal_addr != default_addr);
        REQUIRE(test_v4_linklocal_addr != test_v4_addr);
        REQUIRE(test_v4_linklocal_addr != test_v4_addr_with_port);
        REQUIRE(test_v4_linklocal_addr != test_v6_addr);
        REQUIRE(test_v4_linklocal_addr != test_v6_addr_with_port);
    }

    SECTION("IPv4 link-local in_addr with port properties")
    {
        REQUIRE(test_v4_linklocal_addr_with_port.family() == AF_INET);
        REQUIRE(!test_v4_linklocal_addr_with_port.is_address_any());
        REQUIRE(test_v4_linklocal_addr_with_port.is_address_linklocal());
        REQUIRE(!test_v4_linklocal_addr_with_port.is_address_loopback());
        REQUIRE(NlatUnicast == test_v4_linklocal_addr_with_port.get_address_type());

        REQUIRE(test_v4_linklocal_addr_with_port.in_addr()->s_addr == Test_linklocal_in_addr.s_addr);
        REQUIRE(test_v4_linklocal_addr_with_port.in_addr()->s_addr == test_v4_linklocal_addr.in_addr()->s_addr);
        REQUIRE(test_v4_linklocal_addr_with_port.port() == TestPort);
        REQUIRE(test_v4_linklocal_addr_with_port.scope_id() == 0);
        REQUIRE(test_v4_linklocal_addr_with_port.flow_info() == 0);

        REQUIRE(test_v4_linklocal_addr_with_port == test_v4_linklocal_addr_with_port);
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
        REQUIRE(!test_v6_linklocal_addr.is_address_any());
        REQUIRE(test_v6_linklocal_addr.is_address_linklocal());
        REQUIRE(!test_v6_linklocal_addr.is_address_loopback());
        REQUIRE(NlatUnicast == test_v6_linklocal_addr.get_address_type());

        REQUIRE(0 == memcmp(test_v6_linklocal_addr.in6_addr(), &Test_linklocal_in6_addr, sizeof(in6_addr)));
        REQUIRE(test_v6_linklocal_addr.port() == 0);
        REQUIRE(test_v6_linklocal_addr.scope_id() == 0);
        REQUIRE(test_v6_linklocal_addr.flow_info() == 0);

        REQUIRE(test_v6_linklocal_addr == test_v6_linklocal_addr);
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
        REQUIRE(!test_v6_linklocal_addr_with_port.is_address_any());
        REQUIRE(test_v6_linklocal_addr_with_port.is_address_linklocal());
        REQUIRE(!test_v6_linklocal_addr_with_port.is_address_loopback());
        REQUIRE(NlatUnicast == test_v6_linklocal_addr_with_port.get_address_type());

        REQUIRE(0 == memcmp(test_v6_linklocal_addr_with_port.in6_addr(), &Test_linklocal_in6_addr, sizeof(in6_addr)));
        REQUIRE(0 == memcmp(test_v6_linklocal_addr_with_port.in6_addr(), test_v6_linklocal_addr.in6_addr(), sizeof(in6_addr)));
        REQUIRE(test_v6_linklocal_addr_with_port.port() == TestPort);
        REQUIRE(test_v6_linklocal_addr_with_port.scope_id() == 0);
        REQUIRE(test_v6_linklocal_addr_with_port.flow_info() == 0);

        REQUIRE(test_v6_linklocal_addr_with_port == test_v6_linklocal_addr_with_port);
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
        REQUIRE(test_v4_any_addr.is_address_any());
        REQUIRE(!test_v4_any_addr.is_address_linklocal());
        REQUIRE(!test_v4_any_addr.is_address_loopback());
        REQUIRE(NlatUnspecified == test_v4_any_addr.get_address_type());

        REQUIRE(test_v4_any_addr.in_addr()->s_addr == Test_any_in_addr.s_addr);
        REQUIRE(test_v4_any_addr.port() == 0);
        REQUIRE(test_v4_any_addr.scope_id() == 0);
        REQUIRE(test_v4_any_addr.flow_info() == 0);

        REQUIRE(test_v4_any_addr == test_v4_any_addr);
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
        REQUIRE(test_v4_any_addr_with_port.is_address_any());
        REQUIRE(!test_v4_any_addr_with_port.is_address_linklocal());
        REQUIRE(!test_v4_any_addr_with_port.is_address_loopback());
        REQUIRE(NlatUnspecified == test_v4_any_addr_with_port.get_address_type());

        REQUIRE(test_v4_any_addr_with_port.in_addr()->s_addr == Test_any_in_addr.s_addr);
        REQUIRE(test_v4_any_addr_with_port.in_addr()->s_addr == test_v4_any_addr.in_addr()->s_addr);
        REQUIRE(test_v4_any_addr_with_port.port() == TestPort);
        REQUIRE(test_v4_any_addr_with_port.scope_id() == 0);
        REQUIRE(test_v4_any_addr_with_port.flow_info() == 0);

        REQUIRE(test_v4_any_addr_with_port == test_v4_any_addr_with_port);
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
        REQUIRE(test_v6_any_addr.is_address_any());
        REQUIRE(!test_v6_any_addr.is_address_linklocal());
        REQUIRE(!test_v6_any_addr.is_address_loopback());
        REQUIRE(NlatUnspecified == test_v6_any_addr.get_address_type());

        REQUIRE(0 == memcmp(test_v6_any_addr.in6_addr(), &Test_any_in6_addr, sizeof(in6_addr)));
        REQUIRE(test_v6_any_addr.port() == 0);
        REQUIRE(test_v6_any_addr.scope_id() == 0);
        REQUIRE(test_v6_any_addr.flow_info() == 0);

        REQUIRE(test_v6_any_addr == test_v6_any_addr);
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
        REQUIRE(test_v6_any_addr_with_port.is_address_any());
        REQUIRE(!test_v6_any_addr_with_port.is_address_linklocal());
        REQUIRE(!test_v6_any_addr_with_port.is_address_loopback());
        REQUIRE(NlatUnspecified == test_v6_any_addr_with_port.get_address_type());

        REQUIRE(0 == memcmp(test_v6_any_addr_with_port.in6_addr(), &Test_any_in6_addr, sizeof(in6_addr)));
        REQUIRE(0 == memcmp(test_v6_any_addr_with_port.in6_addr(), test_v6_any_addr.in6_addr(), sizeof(in6_addr)));
        REQUIRE(test_v6_any_addr_with_port.port() == TestPort);
        REQUIRE(test_v6_any_addr_with_port.scope_id() == 0);
        REQUIRE(test_v6_any_addr_with_port.flow_info() == 0);

        REQUIRE(test_v6_any_addr_with_port == test_v6_any_addr_with_port);
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
