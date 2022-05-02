#include "Plugin.h"
#include "Hook.h"
#include "Localization.h"

namespace UmaPyogin
{
	HookInstaller::~HookInstaller()
	{
	}

	Plugin& Plugin::GetInstance()
	{
		static Plugin instance;
		return instance;
	}

	void Plugin::SetLogHandler(Log::LogHandler handler)
	{
		Log::SetLogHandler(handler);
	}

	void Plugin::LoadConfig(Config&& config)
	{
		m_Config = std::move(config);
	}

	void Plugin::InstallHook(std::unique_ptr<HookInstaller>&& hookInstaller)
	{
		m_HookInstaller = std::move(hookInstaller);
		Hook::Install();
	}

	Config const& Plugin::GetConfig() const
	{
		return m_Config;
	}

	HookInstaller* Plugin::GetHookInstaller() const
	{
		return m_HookInstaller.get();
	}
} // namespace UmaPyogin
