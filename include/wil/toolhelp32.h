#ifndef __WIL_TOOLHELP32_INCLUDED
#define __WIL_TOOLHELP32_INCLUDED
#include <TlHelp32.h>
namespace wil
{
	namespace details
	{

	}

	template<typename TCallback>
    void for_each_process(TCallback&& /*callback*/)
	{

	}

	template<typename TCallback>
    void for_each_thread(TCallback&& /*callback*/)
	{

	}

	template<typename TCallback>
    void for_each_module(TCallback&& /*callback*/)
	{

	}

	template<typename TCallback>
    void for_each_heap(TCallback&& /*callback*/)
	{

	}
}

#endif