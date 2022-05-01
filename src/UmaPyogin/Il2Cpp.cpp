#include "Il2Cpp.h"
#include "Log.h"
#include "Plugin.h"

namespace UmaPyogin::Il2CppSymbols
{
#define DEFINE_FUNCTION_POINTERS(returnType, name, params) returnType(*name) params;

	LOAD_FUNCTIONS(DEFINE_FUNCTION_POINTERS)

#undef DEFINE_FUNCTION_POINTERS

	void LoadIl2CppSymbols()
	{
		const auto hookInstaller = Plugin::GetInstance().GetHookInstaller();

#define LOAD_SYM(returnType, name, params)                                                         \
	if (name = reinterpret_cast<decltype(name)>(hookInstaller->LookupSymbol(#name)); !name)        \
	{                                                                                              \
		Log::Info("UmaPyogin: Failed to load symbol: " #name);                                     \
		return;                                                                                    \
	}

		LOAD_FUNCTIONS(LOAD_SYM)

#undef LOAD_SYM
	}

} // namespace UmaPyogin::Il2CppSymbols
