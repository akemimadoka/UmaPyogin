#include "Log.h"

namespace UmaPyogin
{

	Log::LogHandler Log::s_Handler = nullptr;

	void Log::SetLogHandler(LogHandler handler)
	{
		s_Handler = handler;
	}

	void Log::Message(Level level, const char* message)
	{
		s_Handler(level, message);
	}
} // namespace UmaPyogin
