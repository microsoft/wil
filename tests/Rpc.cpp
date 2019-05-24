#include "common.h"

#include <wil/rpc.h>

void RpcMethodReturnsVoid(DWORD toRaise = 0)
{
    if (toRaise)
    {
        RaiseException(toRaise, 0, 0, nullptr);
    }
}

HRESULT RpcMethodReturnsHResult(HRESULT toReturn = S_OK, DWORD toRaise = 0)
{
    RpcMethodReturnsVoid(toRaise);
    return toReturn;
}

GUID RpcMethodReturnsGuid(DWORD toRaise = 0)
{
    RpcMethodReturnsVoid(toRaise);
    return __uuidof(IUnknown);
}

TEST_CASE("Rpc::NonThrowing", "[rpc]")
{
    SECTION("Success paths")
    {
        REQUIRE(wil::call_rpc_nothrow([] { RpcMethodReturnsVoid(); }) == S_OK);
        REQUIRE(wil::call_rpc_nothrow([] { return RpcMethodReturnsHResult(); }) == S_OK);

        GUID tmp{};
        REQUIRE(wil::call_rpc_nothrow(tmp, [] { return RpcMethodReturnsGuid(); }) == S_OK);
        REQUIRE(tmp == __uuidof(IUnknown));
    }

    SECTION("Failures in the method")
    {
        REQUIRE(wil::call_rpc_nothrow([] { return RpcMethodReturnsHResult(E_CHANGED_STATE); }) == E_CHANGED_STATE);
    }

    SECTION("Failures in the fabric")
    {
        REQUIRE(wil::call_rpc_nothrow([] { RpcMethodReturnsVoid(RPC_S_CALL_FAILED); }) == RPC_S_CALL_FAILED);
        REQUIRE(wil::call_rpc_nothrow([] { RpcMethodReturnsHResult(E_CHANGED_STATE, RPC_S_CALL_FAILED); }) == RPC_S_CALL_FAILED);

        GUID tmp{};
        REQUIRE(wil::call_rpc_nothrow(tmp, [] { return RpcMethodReturnsGuid(RPC_S_CALL_FAILED); }) == RPC_S_CALL_FAILED);
    }
}

TEST_CASE("Rpc::Throwing", "[rpc]")
{
    SECTION("Success paths")
    {
        REQUIRE_NOTHROW(wil::call_rpc([] { RpcMethodReturnsVoid(); }));
        REQUIRE_NOTHROW(wil::call_rpc([] { return RpcMethodReturnsHResult(); }));

        GUID tmp{};
        REQUIRE_NOTHROW(tmp = wil::call_rpc<GUID>([] { return RpcMethodReturnsGuid(); }));
        REQUIRE(tmp == __uuidof(IUnknown));
    }

    SECTION("Failures in the method")
    {
        REQUIRE_THROWS_RESULT(E_CHANGED_STATE, [] { wil::call_rpc([] { return RpcMethodReturnsHResult(E_CHANGED_STATE); }); });
    }

    SECTION("Failures in the fabric")
    {
        REQUIRE_THROWS_RESULT(RPC_S_CALL_FAILED, [] { wil::call_rpc([] { RpcMethodReturnsVoid(RPC_S_CALL_FAILED); }); });
        REQUIRE_THROWS_RESULT(RPC_S_CALL_FAILED, [] { wil::call_rpc([] { RpcMethodReturnsHResult(E_CHANGED_STATE, RPC_S_CALL_FAILED); }); });

        REQUIRE_THROWS_RESULT(RPC_S_CALL_FAILED, [] { auto x = wil::call_rpc<GUID>([] { return RpcMethodReturnsGuid(RPC_S_CALL_FAILED); }); });
    }
}
#endif
