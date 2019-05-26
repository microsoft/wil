#include "common.h"

#include <wil/rpc_helpers.h>

void RpcMethodReturnsVoid(ULONG toRaise)
{
    if (toRaise)
    {
        RaiseException(toRaise, 0, 0, nullptr);
    }
}

HRESULT RpcMethodReturnsHResult(HRESULT toReturn, ULONG toRaise)
{
    RpcMethodReturnsVoid(toRaise);
    return toReturn;
}

GUID RpcMethodReturnsGuid(ULONG toRaise)
{
    RpcMethodReturnsVoid(toRaise);
    return __uuidof(IUnknown);
}

TEST_CASE("Rpc::NonThrowing", "[rpc]")
{
    SECTION("Success paths")
    {
        REQUIRE(wil::invoke_rpc_nothrow(RpcMethodReturnsVoid, 0UL) == S_OK);
        REQUIRE(wil::invoke_rpc_nothrow(RpcMethodReturnsHResult, S_OK, 0UL) == S_OK);

        GUID tmp{};
        REQUIRE(wil::invoke_rpc_result_nothrow(tmp, RpcMethodReturnsGuid, 0UL) == S_OK);
        REQUIRE(tmp == __uuidof(IUnknown));
    }

    SECTION("Failures in the method")
    {
        REQUIRE(wil::invoke_rpc_nothrow(RpcMethodReturnsHResult, E_CHANGED_STATE, 0) == E_CHANGED_STATE);
    }

    SECTION("Failures in the fabric")
    {
        REQUIRE(wil::invoke_rpc_nothrow(RpcMethodReturnsVoid, RPC_S_CALL_FAILED) == HRESULT_FROM_WIN32(RPC_S_CALL_FAILED));
        REQUIRE(wil::invoke_rpc_nothrow(RpcMethodReturnsHResult, E_CHANGED_STATE, RPC_S_CALL_FAILED) == HRESULT_FROM_WIN32(RPC_S_CALL_FAILED));

        GUID tmp{};
        REQUIRE(wil::invoke_rpc_result_nothrow(tmp, RpcMethodReturnsGuid, RPC_S_CALL_FAILED) == HRESULT_FROM_WIN32(RPC_S_CALL_FAILED));
    }
}

#ifdef WIL_ENABLE_EXCEPTIONS

#include <sstream>

class WilExceptionMatcher : public Catch::MatcherBase<wil::ResultException>
{
    HRESULT m_expected;
public:
    WilExceptionMatcher(HRESULT ex) : m_expected(ex) { }

    bool match(wil::ResultException const& ex) const override {
        return ex.GetErrorCode() == m_expected;
    }

    std::string describe() const override {
        std::ostringstream ss;
        ss << "wil::ResultException expects code 0x%08lx" << std::hex << m_expected;
        return ss.str();
    }
};

#define REQUIRE_THROWS_WIL_HR(hr, expr) REQUIRE_THROWS_MATCHES(expr, wil::ResultException, WilExceptionMatcher(hr))

TEST_CASE("Rpc::Throwing", "[rpc]")
{
    SECTION("Success paths")
    {
        REQUIRE_NOTHROW(wil::invoke_rpc(RpcMethodReturnsVoid, 0UL));
    }

    SECTION("Failures in the method")
    {
        REQUIRE_THROWS_WIL_HR(E_CHANGED_STATE, wil::invoke_rpc(RpcMethodReturnsHResult, E_CHANGED_STATE, 0UL));
    }

    SECTION("Failures in the fabric")
    {
        REQUIRE_THROWS_WIL_HR(HRESULT_FROM_WIN32(RPC_S_CALL_FAILED), wil::invoke_rpc(RpcMethodReturnsVoid, RPC_S_CALL_FAILED));
        REQUIRE_THROWS_WIL_HR(HRESULT_FROM_WIN32(RPC_S_CALL_FAILED), wil::invoke_rpc(RpcMethodReturnsHResult, E_CHANGED_STATE, RPC_S_CALL_FAILED));
    }
}
#endif
