#ifndef UMAPYOGIN_LOG_H
#define UMAPYOGIN_LOG_H

#include <cstdarg>
#include <fmt/format.h>

namespace UmaPyogin
{
	class Log final
	{
	public:
#define LOG_LEVELS(X)                                                                              \
	X(Debug)                                                                                       \
	X(Info)                                                                                        \
	X(Warn)                                                                                        \
	X(Error)

		enum class Level
		{
#define DEFINE_LEVEL_ENUM(name) name,
			LOG_LEVELS(DEFINE_LEVEL_ENUM)
#undef DEFINE_LEVEL_ENUM
		};

		using LogHandler = void (*)(Level level, const char* message);

		static void SetLogHandler(LogHandler handler);

		static void Message(Level level, const char* message);

		template <typename... Args>
		static void Message(Level level, fmt::format_string<Args...> message, Args&&... args)
		{
			Message(level, fmt::format(message, std::forward<Args>(args)...).c_str());
		}

#define DEFINE_LOG_FORWARDER(name)                                                                 \
	template <typename... Args>                                                                    \
	static void name(fmt::format_string<Args...> message, Args&&... args)                          \
	{                                                                                              \
		Message(Level::name, message, std::forward<Args>(args)...);                                \
	}

		LOG_LEVELS(DEFINE_LOG_FORWARDER)

#undef DEFINE_LOG_FORWARDER
	private:
		static LogHandler s_Handler;
	};
} // namespace UmaPyogin

#endif
