#ifndef UMAPYOGIN_HOOK_H
#define UMAPYOGIN_HOOK_H

#include "Il2Cpp.h"
#include "Misc.h"

namespace UmaPyogin::Hook
{
	void Install();

	Il2CppString* LocalizeJP_Get(std::int32_t id);
} // namespace UmaPyogin::Hook

#endif
