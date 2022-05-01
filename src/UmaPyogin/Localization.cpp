#include "Localization.h"
#include "Hook.h"
#include "Log.h"
#include "Misc.h"

#include <charconv>
#include <fstream>

#include <simdjson.h>

namespace UmaPyogin::Localization
{
	namespace
	{
		std::optional<std::vector<char>> ReadFileWithPadding(std::filesystem::path const& path)
		{
			std::ifstream file(path, std::ios_base::binary | std::ios_base::ate);
			if (!file.is_open())
			{
				Log::Error("UmaPyogin: Failed to open localization file {}", path.native());
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
			Log::Error("UmaPyogin: Failed to parse localization file {}(error: {})",
			           path.native(), document.error());
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

	HashLocalization& HashLocalization::GetInstance()
	{
		static HashLocalization s_Instance;
		return s_Instance;
	}

#define CHECK_ERROR(expr)                                                                          \
	if (const auto err = expr.error(); err != simdjson::SUCCESS)                                   \
	{                                                                                              \
		Log::Error("UmaPyogin: Malformed localization file {}, error {} while get " #expr,         \
		           path.native(), err);                                                    \
		return;                                                                                    \
	}

	void HashLocalization::LoadFrom(std::filesystem::path const& path)
	{
		assert(std::filesystem::is_directory(path));

		for (const auto& entry : std::filesystem::recursive_directory_iterator(
		         path, std::filesystem::directory_options::follow_directory_symlink))
		{
			if (!entry.is_regular_file() || entry.path().extension() != ".json")
			{
				continue;
			}

			auto buffer = ReadFileWithPadding(entry.path());
			if (!buffer)
			{
				continue;
			}

			simdjson::dom::parser parser;
			auto document =
			    parser.parse(buffer->data(), buffer->size() - simdjson::SIMDJSON_PADDING, false);
			if (document.error() != simdjson::SUCCESS)
			{
				Log::Error("UmaPyogin: Failed to parse localization file {}(error: {})",
				           entry.path().native(), document.error());
				continue;
			}

			const auto hashEntries = document.get_object();
			CHECK_ERROR(hashEntries);

			for (const auto& [key, value] : hashEntries)
			{
				const auto valueStr = value.get_string();
				CHECK_ERROR(valueStr);

				std::size_t hash;
				if (const auto ec = std::from_chars(key.data(), key.data() + key.size(), hash);
				    ec.ec != std::errc())
				{
					Log::Error("UmaPyogin: Failed to parse localization file {}(error: {})",
					           entry.path().native(), static_cast<int>(ec.ec));
					continue;
				}
				const auto localizedString = valueStr.value_unsafe();
				m_LocalizedStrings.emplace(hash, Misc::ToUTF16(localizedString));
			}
		}
	}

	const std::u16string* HashLocalization::Localize(std::size_t hash) const
	{
		if (const auto iter = m_LocalizedStrings.find(hash); iter != m_LocalizedStrings.end())
		{
			return &iter->second;
		}
		return nullptr;
	}

	StoryLocalization& StoryLocalization::GetInstance()
	{
		static StoryLocalization s_Instance;
		return s_Instance;
	}

	void StoryLocalization::LoadFrom(std::filesystem::path const& path)
	{
		constexpr const char StoryTimelinePrefix[] = "storytimeline_";
		constexpr const char StoryRacePrefix[] = "storyrace_";

		assert(std::filesystem::is_directory(path));

		for (const auto& entry : std::filesystem::recursive_directory_iterator(
		         path, std::filesystem::directory_options::follow_directory_symlink))
		{
			if (!entry.is_regular_file() || entry.path().extension() != ".json")
			{
				continue;
			}

			const auto filePath = entry.path();
			const auto fileStem = filePath.stem();
			if (fileStem.native().starts_with(StoryTimelinePrefix))
			{
				const auto timelineId =
				    std::stoi(fileStem.native().substr(sizeof(StoryTimelinePrefix) - 1));
				LoadTimeline(timelineId, filePath);
			}
			else if (fileStem.native().starts_with(StoryRacePrefix))
			{
				const auto raceId =
				    std::stoi(fileStem.native().substr(sizeof(StoryRacePrefix) - 1));
				LoadRace(raceId, filePath);
			}
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

	void StoryLocalization::LoadTimeline(std::size_t timelineId, std::filesystem::path const& path)
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
			Log::Error("UmaPyogin: Failed to parse localization file {}(error: {})",
			           path.native(), document.error());
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

		m_StoryTextDataMap.emplace(timelineId, std::move(data));
	}

	void StoryLocalization::LoadRace(std::size_t raceId, std::filesystem::path const& path)
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
			Log::Error("UmaPyogin: Failed to parse localization file {}(error: {})",
			           path.native(), document.error());
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

		m_RaceTextDataMap.emplace(raceId, std::move(data));
	}
} // namespace UmaPyogin::Localization
