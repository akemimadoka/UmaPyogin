#ifndef UMAPYOGIN_LOCALIZATION_H
#define UMAPYOGIN_LOCALIZATION_H

#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace UmaPyogin::Localization
{
	class StaticLocalization
	{
	public:
		static StaticLocalization& GetInstance();

		void LoadFrom(std::filesystem::path const& path);

		const std::u16string* Localize(std::int32_t id) const;

		StaticLocalization(StaticLocalization const&) = delete;
		StaticLocalization& operator=(StaticLocalization const&) = delete;

	private:
		StaticLocalization() = default;

		std::vector<std::optional<std::u16string>> m_LocalizedStrings;
	};

	class StoryLocalization
	{
	public:
		struct StoryTextBlock
		{
			std::u16string Name;
			std::u16string Text;
			std::vector<std::u16string> ChoiceDataList;
			std::vector<std::u16string> ColorTextInfoList;
		};

		struct StoryTextData
		{
			std::u16string Title;
			std::vector<std::optional<StoryTextBlock>> TextBlockList;
		};

		struct RaceTextData
		{
			std::vector<std::u16string> textData;
		};

		static StoryLocalization& GetInstance();

		void LoadFrom(std::filesystem::path const& path);

		const StoryTextData* GetStoryTextData(std::size_t id) const;
		const RaceTextData* GetRaceTextData(std::size_t id) const;

		StoryLocalization(StoryLocalization const&) = delete;
		StoryLocalization& operator=(StoryLocalization const&) = delete;

	private:
		StoryLocalization() = default;

		std::unordered_map<std::size_t, StoryTextData> m_StoryTextDataMap;
		std::unordered_map<std::size_t, RaceTextData> m_RaceTextDataMap;

		void LoadTimeline(std::size_t timelineId, std::filesystem::path const& path,
		                  std::mutex& mutex);
		void LoadRace(std::size_t raceId, std::filesystem::path const& path, std::mutex& mutex);
	};

	class DatabaseLocalization
	{
	public:
		static DatabaseLocalization& GetInstance();

		void LoadFrom(std::filesystem::path const& textDataDictPath,
		              std::filesystem::path const& characterSystemTextDataDictPath,
		              std::filesystem::path const& raceJikkyoCommentDataDictPath,
		              std::filesystem::path const& raceJikkyoMessageDataDictPath);

		const std::u16string* GetTextData(std::size_t category, std::size_t index);
		const std::u16string* GetCharacterSystemTextData(std::size_t characterId,
		                                                 std::size_t voiceId);
		const std::u16string* GetRaceJikkyoCommentData(std::size_t id);
		const std::u16string* GetRaceJikkyoMessageData(std::size_t id);

		DatabaseLocalization(DatabaseLocalization const&) = delete;
		DatabaseLocalization& operator=(DatabaseLocalization const&) = delete;

	private:
		DatabaseLocalization() = default;

		// { category: { index: text } }
		std::unordered_map<std::size_t, std::unordered_map<std::size_t, std::u16string>>
		    m_TextDataMap;

		// { character_id: { voice_id: text } }
		std::unordered_map<std::size_t, std::unordered_map<std::size_t, std::u16string>>
		    m_CharacterSystemTextDataMap;

		// { id: text }
		std::unordered_map<std::size_t, std::u16string> m_RaceJikkyoCommentDataMap;

		// { id: text }
		std::unordered_map<std::size_t, std::u16string> m_RaceJikkyoMessageDataMap;
	};
} // namespace UmaPyogin::Localization

#endif
