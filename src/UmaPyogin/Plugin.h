#ifndef UMAPYOGIN_PLUGIN_H
#define UMAPYOGIN_PLUGIN_H

#include <memory>

#include "Config.h"
#include "Log.h"
#include "Misc.h"

namespace UmaPyogin
{
	struct HookInstaller
	{
		virtual ~HookInstaller();
		virtual void InstallHook(OpaqueFunctionPointer addr, OpaqueFunctionPointer hook,
		                         OpaqueFunctionPointer* orig) = 0;
		virtual OpaqueFunctionPointer LookupSymbol(const char* name) = 0;
	};

	class Plugin
	{
	public:
		static Plugin& GetInstance();

		void SetLogHandler(Log::LogHandler handler);
		void LoadConfig(Config&& config);
		void InstallHook(std::unique_ptr<HookInstaller>&& hookInstaller);

		Config const& GetConfig() const;
		HookInstaller* GetHookInstaller() const;

		Plugin(Plugin const&) = delete;
		Plugin& operator=(Plugin const&) = delete;

	private:
		Plugin() = default;

		Config m_Config;
		std::unique_ptr<HookInstaller> m_HookInstaller;
	};
} // namespace UmaPyogin

#endif
