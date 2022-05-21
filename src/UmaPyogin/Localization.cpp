#include "Localization.h"
#include "Hook.h"
#include "Log.h"
#include "Misc.h"

#include <charconv>
#include <fstream>

#include <simdjson.h>

#ifdef _WIN32
#define PATH_STR(path) (path).string()
#else
#define PATH_STR(path) (path).native()
#endif

namespace UmaPyogin::Localization
{
	namespace
	{
		std::optional<std::vector<char>> ReadFileWithPadding(std::filesystem::path const& path)
		{
			std::ifstream file(path, std::ios_base::binary | std::ios_base::ate);
			if (!file.is_open())
			{
				Log::Error("UmaPyogin: Failed to open localization file {}", PATH_STR(path));
				return {};
			}

			const auto size = static_cast<std::size_t>(file.tellg());
			file.seekg(0, std::ios_base::beg);

			std::vector<char> buffer(size + simdjson::SIMDJSON_PADDING);
			file.read(buffer.data(), size);

			return std::move(buffer);
		}
	} // namespace

	StaticLocalization& StaticLocalization::GetInstance()
	{
		static StaticLocalization s_Instance;
		return s_Instance;
	}

	void StaticLocalization::LoadFrom(std::filesystem::path const& path)
	{
		auto buffer = ReadFileWithPadding(path);
		if (!buffer)
		{
			return;
		}

		simdjson::dom::parser parser;
		auto document =
		    parser.parse(buffer->data(), buffer->size() - simdjson::SIMDJSON_PADDING, false);
		if (document.error() != simdjson::SUCCESS)
		{
			Log::Error("UmaPyogin: Failed to parse localization file {}(error: {})", PATH_STR(path),
			           document.error());
			return;
		}

		std::int32_t i = 0;
		auto lastIsNull = false;
		while (true)
		{
			const auto source = Hook::LocalizeJP_Get(i++);
			if (!source || !source->chars[0])
			{
				if (lastIsNull)
				{
					break;
				}
				lastIsNull = true;
				m_LocalizedStrings.emplace_back(std::nullopt);
				continue;
			}

			lastIsNull = false;
			const auto sourceString = Misc::ToUTF8(source->chars);
			const auto localizedString = document[sourceString].get_string();
			if (localizedString.error())
			{
				m_LocalizedStrings.emplace_back(std::nullopt);
				continue;
			}

			m_LocalizedStrings.emplace_back(Misc::ToUTF16(localizedString.value_unsafe()));
		}
	}

	const std::u16string* StaticLocalization::Localize(std::int32_t id) const
	{
		if (id < m_LocalizedStrings.size())
		{
			const auto& maybeStr = m_LocalizedStrings[id];
			return maybeStr ? &*maybeStr : nullptr;
		}
		return nullptr;
	}

#define CHECK_ERROR(expr)                                                                          \
	if (const auto err = expr.error(); err != simdjson::SUCCESS)                                   \
	{                                                                                              \
		Log::Error("UmaPyogin: Malformed localization file {}, error {} while get " #expr,         \
		           PATH_STR(path), err);                                                           \
		return;                                                                                    \
	}

	StoryLocalization& StoryLocalization::GetInstance()
	{
		static StoryLocalization s_Instance;
		return s_Instance;
	}

	void StoryLocalization::LoadFrom(std::filesystem::path const& path)
	{
#ifdef _WIN32
#define PATH_LITERAL(x) L##x
#else
#define PATH_LITERAL(x) x
#endif
		constexpr const std::filesystem::path::value_type StoryTimelinePrefix[] =
		    PATH_LITERAL("storytimeline_");
		constexpr const std::filesystem::path::value_type StoryRacePrefix[] =
		    PATH_LITERAL("storyrace_");
#undef PATH_LITERAL

		assert(std::filesystem::is_directory(path));

		try
		{
			std::mutex timeLineMutex;
			std::mutex raceMutex;
			Misc::Parallel::OnePassForEach(
			    std::filesystem::recursive_directory_iterator(
			        path, std::filesystem::directory_options::follow_directory_symlink),
			    std::filesystem::recursive_directory_iterator(),
			    [&](std::filesystem::directory_entry const& entry) {
				    if (!entry.is_regular_file() || entry.path().extension() != ".json")
				    {
					    return;
				    }

				    const auto filePath = entry.path();
				    const auto fileStem = filePath.stem();
				    if (fileStem.native().starts_with(StoryTimelinePrefix))
				    {
					    const auto timelineId =
					        std::stoi(fileStem.native().substr(sizeof(StoryTimelinePrefix) - 1));
					    LoadTimeline(timelineId, filePath, timeLineMutex);
				    }
				    else if (fileStem.native().starts_with(StoryRacePrefix))
				    {
					    const auto raceId =
					        std::stoi(fileStem.native().substr(sizeof(StoryRacePrefix) - 1));
					    LoadRace(raceId, filePath, raceMutex);
				    }
			    });
		}
		catch (const std::exception& e)
		{
			Log::Error("UmaPyogin: Failed to load story localization from {}: {}", PATH_STR(path),
			           e.what());
		}
	}

	const StoryLocalization::StoryTextData*
	StoryLocalization::GetStoryTextData(std::size_t id) const
	{
		if (const auto iter = m_StoryTextDataMap.find(id); iter != m_StoryTextDataMap.end())
		{
			return &iter->second;
		}
		return nullptr;
	}

	const StoryLocalization::RaceTextData* StoryLocalization::GetRaceTextData(std::size_t id) const
	{
		if (const auto iter = m_RaceTextDataMap.find(id); iter != m_RaceTextDataMap.end())
		{
			return &iter->second;
		}
		return nullptr;
	}

	void StoryLocalization::LoadTimeline(std::size_t timelineId, std::filesystem::path const& path,
	                                     std::mutex& mutex)
	{
		auto buffer = ReadFileWithPadding(path);
		if (!buffer)
		{
			return;
		}

		simdjson::dom::parser parser;
		auto document =
		    parser.parse(buffer->data(), buffer->size() - simdjson::SIMDJSON_PADDING, false);
		if (document.error() != simdjson::SUCCESS)
		{
			Log::Error("UmaPyogin: Failed to parse localization file {}(error: {})", PATH_STR(path),
			           document.error());
			return;
		}

		StoryTextData data;

		const auto title = document["Title"].get_string();
		CHECK_ERROR(title);
		data.Title = Misc::ToUTF16(title.value_unsafe());
		const auto textBlockList = document["TextBlockList"].get_array();
		CHECK_ERROR(textBlockList);
		for (const auto block : textBlockList)
		{
			if (block.is_null())
			{
				data.TextBlockList.emplace_back();
			}
			else
			{
				StoryTextBlock textBlock;
				const auto name = block["Name"].get_string();
				CHECK_ERROR(name);
				textBlock.Name = Misc::ToUTF16(name.value_unsafe());
				const auto text = block["Text"].get_string();
				CHECK_ERROR(text);
				textBlock.Text = Misc::ToUTF16(text.value_unsafe());
				const auto choiceDataList = block["ChoiceDataList"].get_array();
				CHECK_ERROR(choiceDataList);
				for (const auto choiceData : choiceDataList)
				{
					const auto choiceDataText = choiceData.get_string();
					CHECK_ERROR(choiceDataText);
					textBlock.ChoiceDataList.emplace_back(
					    Misc::ToUTF16(choiceDataText.value_unsafe()));
				}
				const auto colorTextInfoList = block["ColorTextInfoList"].get_array();
				CHECK_ERROR(colorTextInfoList);
				for (const auto colorTextInfo : colorTextInfoList)
				{
					const auto colorTextInfoText = colorTextInfo.get_string();
					CHECK_ERROR(colorTextInfoText);
					textBlock.ColorTextInfoList.emplace_back(
					    Misc::ToUTF16(colorTextInfoText.value_unsafe()));
				}

				data.TextBlockList.emplace_back(std::move(textBlock));
			}
		}

		std::unique_lock lock(mutex);
		m_StoryTextDataMap.emplace(timelineId, std::move(data));
	}

	void StoryLocalization::LoadRace(std::size_t raceId, std::filesystem::path const& path,
	                                 std::mutex& mutex)
	{
		auto buffer = ReadFileWithPadding(path);
		if (!buffer)
		{
			return;
		}

		simdjson::dom::parser parser;
		auto document =
		    parser.parse(buffer->data(), buffer->size() - simdjson::SIMDJSON_PADDING, false);
		if (document.error() != simdjson::SUCCESS)
		{
			Log::Error("UmaPyogin: Failed to parse localization file {}(error: {})", PATH_STR(path),
			           document.error());
			return;
		}

		RaceTextData data;

		const auto array = document.get_array();
		CHECK_ERROR(array);

		for (const auto item : array)
		{
			const auto text = item.get_string();
			CHECK_ERROR(text);
			data.textData.emplace_back(Misc::ToUTF16(text.value_unsafe()));
		}

		std::unique_lock lock(mutex);
		m_RaceTextDataMap.emplace(raceId, std::move(data));
	}

	DatabaseLocalization& DatabaseLocalization::GetInstance()
	{
		static DatabaseLocalization s_Instance;
		return s_Instance;
	}

	void
	DatabaseLocalization::LoadFrom(std::filesystem::path const& textDataDictPath,
	                               std::filesystem::path const& characterSystemTextDataDictPath,
	                               std::filesystem::path const& raceJikkyoCommentDataDictPath,
	                               std::filesystem::path const& raceJikkyoMessageDataDictPath)
	{
		// TextData
		{
			auto buffer = ReadFileWithPadding(textDataDictPath);
			if (buffer)
			{
				simdjson::dom::parser parser;
				auto document = parser.parse(buffer->data(),
				                             buffer->size() - simdjson::SIMDJSON_PADDING, false);
				if (document.error() != simdjson::SUCCESS)
				{
					Log::Error("UmaPyogin: Failed to parse localization file {}(error: {})",
					           PATH_STR(textDataDictPath), document.error());
					return;
				}

				if (document.is_object())
				{
					for (const auto& [category, indexTextMap] : document.get_object())
					{
						std::size_t categoryValue;
						if (const auto [ptr, ec] = std::from_chars(
						        category.data(), category.data() + category.size(), categoryValue);
						    ec != std::errc{})
						{
							Log::Error("UmaPyogin: Failed to parse category {}(in file {})",
							           category, PATH_STR(textDataDictPath));
							continue;
						}

						if (indexTextMap.is_object())
						{
							auto& map = m_TextDataMap[categoryValue];

							for (const auto& [index, text] : indexTextMap.get_object())
							{
								if (text.is_string())
								{
									std::size_t indexValue;
									if (const auto [ptr, ec] = std::from_chars(
									        index.data(), index.data() + index.size(), indexValue);
									    ec != std::errc{})
									{
										Log::Error(
										    "UmaPyogin: Failed to parse index {}(in file {})",
										    index, PATH_STR(textDataDictPath));
										continue;
									}

									map.emplace(indexValue, Misc::ToUTF16(text.get_string()));
								}
							}
						}
					}
				}
			}
		}

		// CharacterSystemTextData
		{
			auto buffer = ReadFileWithPadding(characterSystemTextDataDictPath);
			if (buffer)
			{
				simdjson::dom::parser parser;
				auto document = parser.parse(buffer->data(),
				                             buffer->size() - simdjson::SIMDJSON_PADDING, false);
				if (document.error() != simdjson::SUCCESS)
				{
					Log::Error("UmaPyogin: Failed to parse localization file {}(error: {})",
					           PATH_STR(textDataDictPath), document.error());
					return;
				}

				if (document.is_object())
				{
					for (const auto& [characterId, voiceIdTextMap] : document.get_object())
					{
						std::size_t characterIdValue;
						if (const auto [ptr, ec] = std::from_chars(
						        characterId.data(), characterId.data() + characterId.size(),
						        characterIdValue);
						    ec != std::errc{})
						{
							Log::Error("UmaPyogin: Failed to parse characterId {}(in file {})",
							           characterId, PATH_STR(characterSystemTextDataDictPath));
							continue;
						}

						if (voiceIdTextMap.is_object())
						{
							auto& map = m_CharacterSystemTextDataMap[characterIdValue];

							for (const auto& [voiceId, text] : voiceIdTextMap.get_object())
							{
								if (text.is_string())
								{
									std::size_t voiceIdValue;

									if (const auto [ptr, ec] = std::from_chars(
									        voiceId.data(), voiceId.data() + voiceId.size(),
									        voiceIdValue);
									    ec != std::errc{})
									{
										Log::Error(
										    "UmaPyogin: Failed to parse voiceId {}(in file {})",
										    voiceId, PATH_STR(characterSystemTextDataDictPath));
										continue;
									}

									map.emplace(voiceIdValue, Misc::ToUTF16(text.get_string()));
								}
							}
						}
					}
				}
			}
		}

		// RaceJikkyoCommentData
		{
			auto buffer = ReadFileWithPadding(raceJikkyoCommentDataDictPath);
			if (buffer)
			{
				simdjson::dom::parser parser;
				auto document = parser.parse(buffer->data(),
				                             buffer->size() - simdjson::SIMDJSON_PADDING, false);
				if (document.error() != simdjson::SUCCESS)
				{
					Log::Error("UmaPyogin: Failed to parse localization file {}(error: {})",
					           PATH_STR(raceJikkyoCommentDataDictPath), document.error());
					return;
				}

				if (document.is_object())
				{
					for (const auto& [id, text] : document.get_object())
					{
						std::size_t idValue;
						if (const auto [ptr, ec] =
						        std::from_chars(id.data(), id.data() + id.size(), idValue);
						    ec != std::errc{})
						{
							Log::Error("UmaPyogin: Failed to parse id {}(in file {})", id,
							           PATH_STR(raceJikkyoCommentDataDictPath));
							continue;
						}

						if (text.is_string())
						{
							m_RaceJikkyoCommentDataMap.emplace(idValue,
							                                   Misc::ToUTF16(text.get_string()));
						}
					}
				}
			}
		}

		// RaceJikkyoMessageData
		{
			auto buffer = ReadFileWithPadding(raceJikkyoMessageDataDictPath);
			if (buffer)
			{
				simdjson::dom::parser parser;
				auto document = parser.parse(buffer->data(),
				                             buffer->size() - simdjson::SIMDJSON_PADDING, false);
				if (document.error() != simdjson::SUCCESS)
				{
					Log::Error("UmaPyogin: Failed to parse localization file {}(error: {})",
					           PATH_STR(raceJikkyoMessageDataDictPath), document.error());
					return;
				}

				if (document.is_object())
				{
					for (const auto& [id, text] : document.get_object())
					{
						std::size_t idValue;
						if (const auto [ptr, ec] =
						        std::from_chars(id.data(), id.data() + id.size(), idValue);
						    ec != std::errc{})
						{
							Log::Error("UmaPyogin: Failed to parse id {}(in file {})", id,
							           PATH_STR(raceJikkyoMessageDataDictPath));
							continue;
						}

						if (text.is_string())
						{
							m_RaceJikkyoMessageDataMap.emplace(idValue,
							                                   Misc::ToUTF16(text.get_string()));
						}
					}
				}
			}
		}
	}

	const std::u16string* DatabaseLocalization::GetTextData(std::size_t category, std::size_t index)
	{
		if (const auto iter = m_TextDataMap.find(category); iter != m_TextDataMap.end())
		{
			if (const auto iter2 = iter->second.find(index); iter2 != iter->second.end())
			{
				return &iter2->second;
			}
		}

		return nullptr;
	}

	const std::u16string* DatabaseLocalization::GetCharacterSystemTextData(std::size_t characterId,
	                                                                       std::size_t voiceId)
	{
		if (const auto iter = m_CharacterSystemTextDataMap.find(characterId);
		    iter != m_CharacterSystemTextDataMap.end())
		{
			if (const auto iter2 = iter->second.find(voiceId); iter2 != iter->second.end())
			{
				return &iter2->second;
			}
		}

		return nullptr;
	}

	const std::u16string* DatabaseLocalization::GetRaceJikkyoCommentData(std::size_t id)
	{
		if (const auto iter = m_RaceJikkyoCommentDataMap.find(id);
		    iter != m_RaceJikkyoCommentDataMap.end())
		{
			return &iter->second;
		}

		return nullptr;
	}

	const std::u16string* DatabaseLocalization::GetRaceJikkyoMessageData(std::size_t id)
	{
		if (const auto iter = m_RaceJikkyoMessageDataMap.find(id);
		    iter != m_RaceJikkyoMessageDataMap.end())
		{
			return &iter->second;
		}

		return nullptr;
	}
} // namespace UmaPyogin::Localization
