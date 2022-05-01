#ifndef UMAPYOGIN_MISC_H
#define UMAPYOGIN_MISC_H

#include <string>
#include <string_view>

namespace UmaPyogin
{
	using OpaqueFunctionPointer = void (*)();
	using HookFunction = void (*)(OpaqueFunctionPointer addr, OpaqueFunctionPointer hook,
	                              OpaqueFunctionPointer* orig);
	using SymbolLookupFunction = OpaqueFunctionPointer (*)(void* library, const char* name);

	namespace Misc
	{
		std::u16string ToUTF16(const std::string_view& str);
		std::string ToUTF8(const std::u16string_view& str);
	} // namespace Misc
} // namespace UmaPyogin

#endif
