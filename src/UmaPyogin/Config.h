#ifndef UMAPYOGIN_CONFIG_H
#define UMAPYOGIN_CONFIG_H

#include <cstdint>
#include <string>

namespace UmaPyogin
{
	enum class ConfigEntryType
	{
		String,
		Int,
		Float,
		Bool,
	};

	template <ConfigEntryType EntryType>
	struct ConfigEntryTypeMapping;

	template <>
	struct ConfigEntryTypeMapping<ConfigEntryType::String>
	{
		using Type = std::string;
	};

	template <>
	struct ConfigEntryTypeMapping<ConfigEntryType::Int>
	{
		using Type = std::int64_t;
	};

	template <>
	struct ConfigEntryTypeMapping<ConfigEntryType::Float>
	{
		using Type = float;
	};

	template <>
	struct ConfigEntryTypeMapping<ConfigEntryType::Bool>
	{
		using Type = bool;
	};

#define CONFIG_ENTRIES(X)                                                                          \
	X(String, StaticLocalizationFilePath)                                                          \
	X(String, StoryLocalizationDirPath)                                                            \
	X(String, TextDataDictPath)                                                                    \
	X(String, CharacterSystemTextDataDictPath)                                                     \
	X(String, RaceJikkyoCommentDataDictPath)                                                       \
	X(String, RaceJikkyoMessageDataDictPath)                                                       \
	X(String, ExtraAssetBundlePath)                                                                \
	X(String, ReplaceFontPath)                                                                     \
	X(Int, OverrideFPS)

	struct Config
	{
#define DECLARE_CONFIG_FIELDS(EntryType, FieldName)                                                \
	using FieldName##Type = typename ConfigEntryTypeMapping<ConfigEntryType::EntryType>::Type;     \
	FieldName##Type FieldName;

		CONFIG_ENTRIES(DECLARE_CONFIG_FIELDS)

#undef DECLARE_CONFIG_FIELDS
	};
} // namespace UmaPyogin

#endif
