#include <cassert>
#include <cstdint>

#include <filesystem>
#include <fstream>
#include <regex>
#include <string_view>
#include <thread>
#include <unordered_set>
#include <variant>

#include "Hook.h"
#include "Il2Cpp.h"
#include "Localization.h"
#include "Log.h"
#include "Misc.h"
#include "Plugin.h"

using namespace UmaPyogin;
using namespace Il2CppSymbols;

using namespace std::literals;

#define DEFINE_HOOK(returnType, name, params)                                                      \
	using name##_Type = returnType(*) params;                                                      \
	name##_Type name##_Addr = nullptr;                                                             \
	name##_Type name##_Orig = nullptr;                                                             \
	returnType name##_Hook params

namespace
{
	template <typename Predicate>
	Il2CppClass* FindNestedClass(Il2CppClass* klass, Predicate&& predicate)
	{
		void* iter{};
		while (const auto curNestedClass = il2cpp_class_get_nested_types(klass, &iter))
		{
			if (std::forward<Predicate>(predicate)(curNestedClass))
			{
				return curNestedClass;
			}
		}

		return nullptr;
	}

	Il2CppClass* FindNestedClassFromName(Il2CppClass* klass, std::string_view name)
	{
		return FindNestedClass(klass, [&](Il2CppClass* nestedClass) {
			return il2cpp_class_get_name(nestedClass) == name;
		});
	}

	template <typename Predicate>
	const MethodInfo* FindMethod(Il2CppClass* klass, Predicate&& predicate)
	{
		void* iter{};
		while (const auto curMethod = il2cpp_class_get_methods(klass, &iter))
		{
			if (std::forward<Predicate>(predicate)(curMethod))
			{
				return curMethod;
			}
		}

		return nullptr;
	}

	// 有多个实现的情况下，选择第一个
	const MethodInfo* FindMethodFromName(Il2CppClass* klass, std::string_view name)
	{
		return FindMethod(klass, [&](const MethodInfo* method) {
			return std::string_view(method->name).ends_with(name);
		});
	}

	Il2CppClass* IListClass;
	Il2CppString* (*Environment_get_StackTrace)();

	template <typename Receiver>
	void IterateIList(Il2CppObject* container, Receiver&& receiver)
	{
		using namespace std::literals::string_view_literals;

		const auto containerClass = il2cpp_object_get_class(container);
		if (!il2cpp_class_is_assignable_from(IListClass, containerClass))
		{
			Log::Error("UmaPyogin: IterateIList: container({}) is not IList",
			           il2cpp_class_get_name(containerClass));
			return;
		}
		std::int32_t size;
		const auto get_Count_Method = FindMethodFromName(containerClass, "get_Count");
		if (get_Count_Method)
		{
			size = reinterpret_cast<std::int32_t (*)(Il2CppObject*)>(
			    get_Count_Method->methodPointer)(container);
		}
		else
		{
			Log::Error("UmaPyogin: IterateIList: Failed to get get_Count method from class {}",
			           il2cpp_class_get_name(containerClass));
			return;
		}

		const auto get_Item_Method = FindMethodFromName(containerClass, "get_Item");
		if (!get_Item_Method)
		{
			Log::Error("UmaPyogin: IterateIList: Failed to get get_Item method from class {}.",
			           il2cpp_class_get_name(containerClass));
			return;
		}

		for (std::int32_t i = 0; i < size; ++i)
		{
			const auto item = reinterpret_cast<Il2CppObject* (*) (Il2CppObject*, std::int32_t)>(
			    get_Item_Method->methodPointer)(container, i);
			std::forward<Receiver>(receiver)(i, item);
		}
	}

	template <typename Receiver>
	void IterateIEnumerable(Il2CppObject* enumerable, Receiver&& receiver)
	{
		using namespace std::literals::string_view_literals;

		const auto enumerableClass = il2cpp_object_get_class(enumerable);
		const auto get_Enumerator_Method =
		    il2cpp_class_get_method_from_name(enumerableClass, "GetEnumerator", 0);
		if (!get_Enumerator_Method)
		{
			Log::Error("UmaPyogin: Failed to get GetEnumerator method.");
			return;
		}

		const auto enumerator = reinterpret_cast<Il2CppObject* (*) (Il2CppObject*)>(
		    get_Enumerator_Method->methodPointer)(enumerable);
		const auto enumeratorClass = il2cpp_object_get_class(enumerator);
		const auto move_Next_Method =
		    il2cpp_class_get_method_from_name(enumeratorClass, "MoveNext", 0);
		if (!move_Next_Method)
		{
			Log::Error("UmaPyogin: Failed to get MoveNext method.");
			return;
		}

		const auto get_Current_Method =
		    il2cpp_class_get_method_from_name(enumeratorClass, "get_Current", 0);
		if (!get_Current_Method)
		{
			Log::Error("UmaPyogin: Failed to get get_Current method.");
			return;
		}

		while (
		    reinterpret_cast<bool (*)(Il2CppObject*)>(move_Next_Method->methodPointer)(enumerator))
		{
			const auto item = reinterpret_cast<Il2CppObject* (*) (Il2CppObject*)>(
			    get_Current_Method->methodPointer)(enumerator);
			std::forward<Receiver>(receiver)(item);
		}
	}

	[[maybe_unused]] Il2CppString* ToIl2CppString(std::u8string_view str)
	{
		return il2cpp_string_new(reinterpret_cast<const char*>(str.data()));
	}

	Il2CppString* ToIl2CppString(std::u16string_view str)
	{
		return il2cpp_string_new_utf16(str.data(), str.size());
	}

	DEFINE_HOOK(Il2CppString*, LocalizeJP_Get, (std::int32_t id))
	{
		const auto localizedString = Localization::StaticLocalization::GetInstance().Localize(id);
		if (localizedString)
		{
			return il2cpp_string_new_utf16(localizedString->c_str(), localizedString->size());
		}
		return LocalizeJP_Get_Orig(id);
	}

	Il2CppClass* StoryTimelineDataClass;
	FieldInfo* StoryTimelineDataClass_StoryIdField;
	FieldInfo* StoryTimelineDataClass_TitleField;
	FieldInfo* StoryTimelineDataClass_BlockListField;
	Il2CppClass* StoryTimelineTextClipDataClass;
	FieldInfo* StoryTimelineTextClipDataClass_NameField;
	FieldInfo* StoryTimelineTextClipDataClass_TextField;
	FieldInfo* StoryTimelineTextClipDataClass_ChoiceDataList;
	Il2CppClass* StoryTimelineTextClipDataClass_ChoiceDataClass;
	FieldInfo* StoryTimelineTextClipDataClass_ChoiceDataClass_TextField;
	FieldInfo* StoryTimelineTextClipDataClass_ColorTextInfoListField;
	Il2CppClass* StoryTimelineTextClipDataClass_ColorTextInfoClass;
	FieldInfo* StoryTimelineTextClipDataClass_ColorTextInfoClass_TextField;
	Il2CppClass* StoryTimelineBlockDataClass;
	FieldInfo* StoryTimelineBlockDataClass_TextTrackField;
	Il2CppClass* StoryTimelineTextTrackDataClass;
	FieldInfo* StoryTimelineTrackDataClass_ClipListField;
	Il2CppClass* StoryTimelineClipDataClass;

	void LocalizeStoryTimelineData(Il2CppObject* timelineData)
	{
		const auto storyIdStr = reinterpret_cast<Il2CppString*>(
		    il2cpp_field_get_value_object(StoryTimelineDataClass_StoryIdField, timelineData));
		const auto storyId = std::stoull(Misc::ToUTF8(storyIdStr->chars));

		const auto localizedStory =
		    Localization::StoryLocalization::GetInstance().GetStoryTextData(storyId);
		if (!localizedStory)
		{
			return;
		}

		il2cpp_field_set_value_object(
		    timelineData, StoryTimelineDataClass_TitleField,
		    reinterpret_cast<Il2CppObject*>(ToIl2CppString(localizedStory->Title)));

		const auto blockList =
		    il2cpp_field_get_value_object(StoryTimelineDataClass_BlockListField, timelineData);
		IterateIList(blockList, [&](std::size_t i, Il2CppObject* block) {
			const auto& textClip = localizedStory->TextBlockList[i];
			if (!textClip)
			{
				return;
			}

			const auto textTrack =
			    il2cpp_field_get_value_object(StoryTimelineBlockDataClass_TextTrackField, block);
			if (!textTrack)
			{
				return;
			}

			const auto clipList =
			    il2cpp_field_get_value_object(StoryTimelineTrackDataClass_ClipListField, textTrack);
			IterateIList(clipList, [&](std::size_t, Il2CppObject* clipData) {
				il2cpp_field_set_value_object(
				    clipData, StoryTimelineTextClipDataClass_NameField,
				    reinterpret_cast<Il2CppObject*>(ToIl2CppString(textClip->Name)));
				il2cpp_field_set_value_object(
				    clipData, StoryTimelineTextClipDataClass_TextField,
				    reinterpret_cast<Il2CppObject*>(ToIl2CppString(textClip->Text)));

				const auto choiceDataList = il2cpp_field_get_value_object(
				    StoryTimelineTextClipDataClass_ChoiceDataList, clipData);
				IterateIList(choiceDataList, [&](std::size_t i, Il2CppObject* choiceData) {
					const auto& choiceText = textClip->ChoiceDataList[i];
					il2cpp_field_set_value_object(
					    choiceData, StoryTimelineTextClipDataClass_ChoiceDataClass_TextField,
					    reinterpret_cast<Il2CppObject*>(ToIl2CppString(choiceText)));
				});

				const auto colorTextInfoList = il2cpp_field_get_value_object(
				    StoryTimelineTextClipDataClass_ColorTextInfoListField, clipData);
				IterateIList(colorTextInfoList, [&](std::size_t i, Il2CppObject* colorTextInfo) {
					const auto& colorText = textClip->ColorTextInfoList[i];
					il2cpp_field_set_value_object(
					    colorTextInfo, StoryTimelineTextClipDataClass_ColorTextInfoClass_TextField,
					    reinterpret_cast<Il2CppObject*>(ToIl2CppString(colorText)));
				});
			});
		});
	}

	Il2CppClass* StoryRaceTextAssetClass;
	FieldInfo* StoryRaceTextAssetClass_textDataField;
	Il2CppClass* StoryRaceTextAssetClass_KeyClass;
	FieldInfo* StoryRaceTextAssetClass_KeyClass_textField;

	void LocalizeStoryRaceTextAsset(Il2CppObject* raceTextAsset, std::size_t raceId)
	{
		const auto localizedRaceData =
		    Localization::StoryLocalization::GetInstance().GetRaceTextData(raceId);
		if (!localizedRaceData)
		{
			return;
		}

		const auto textData =
		    il2cpp_field_get_value_object(StoryRaceTextAssetClass_textDataField, raceTextAsset);
		IterateIList(textData, [&](std::size_t i, Il2CppObject* key) {
			const auto& text = localizedRaceData->textData[i];
			il2cpp_field_set_value_object(key, StoryRaceTextAssetClass_KeyClass_textField,
			                              reinterpret_cast<Il2CppObject*>(ToIl2CppString(text)));
		});
	}

	std::uint32_t ExtraAssetBundleHandle;

	void LoadResources();

	struct TransparentStringHash : std::hash<std::u16string>, std::hash<std::u16string_view>
	{
		using is_transparent = void;

		using std::hash<std::u16string>::operator();
		using std::hash<std::u16string_view>::operator();
	};

	std::unordered_set<std::u16string, TransparentStringHash, std::equal_to<void>>
	    ExtraAssetBundleAssetNameSet;

	Il2CppObject* (*AssetBundle_LoadFromFile)(Il2CppString* path);
	Il2CppObject* (*AssetBundle_GetAllAssetNames)(Il2CppObject* self);

	DEFINE_HOOK(Il2CppObject*, AssetBundle_LoadAsset,
	            (Il2CppObject * self, Il2CppString* name, Il2CppReflectionType* type))
	{
		if (!ExtraAssetBundleHandle)
		{
			LoadResources();
		}
		const auto extraAssetBundle = il2cpp_gchandle_get_target(ExtraAssetBundleHandle);
		if (ExtraAssetBundleAssetNameSet.find(
#if __cpp_lib_generic_unordered_lookup >= 201811L
		        std::u16string_view(name->chars)
#else
		        std::u16string(name->chars)
#endif
		            ) != ExtraAssetBundleAssetNameSet.end())
		{
			return AssetBundle_LoadAsset_Orig(extraAssetBundle, name, type);
		}
		const auto asset = AssetBundle_LoadAsset_Orig(self, name, type);
		if (asset)
		{
			const auto assetClass = il2cpp_object_get_class(asset);
			if (assetClass == StoryTimelineDataClass)
			{
				LocalizeStoryTimelineData(asset);
			}
			else if (assetClass == StoryRaceTextAssetClass)
			{
				const auto assetPath = std::filesystem::path(name->chars).stem();
#ifdef _WIN32
				const auto assetNameStr = assetPath.string();
				const auto assetName = static_cast<std::string_view>(assetNameStr);
#else
				const auto assetName = static_cast<std::string_view>(assetPath.native());
#endif
				constexpr const char RacePrefix[] = "storyrace_";
				assert(assetName.starts_with(RacePrefix));
				const auto raceId = static_cast<std::size_t>(
				    std::atoll(assetName.substr(std::size(RacePrefix) - 1).data()));
				LocalizeStoryRaceTextAsset(asset, raceId);
			}
		}
		return asset;
	}

	DEFINE_HOOK(void, Application_set_targetFrameRate, (int value))
	{
		if (const auto overrideFPS = Plugin::GetInstance().GetConfig().OverrideFPS)
		{
			Application_set_targetFrameRate_Orig(overrideFPS);
		}
		else
		{
			Application_set_targetFrameRate_Orig(value);
		}
	}

	bool (*Object_IsNativeObjectAlive)(Il2CppObject* object);

	Il2CppReflectionType* Font_Type;
	void (*Text_set_font)(Il2CppObject* self, Il2CppObject* font);
	void (*Text_AssignDefaultFont)(Il2CppObject* self);
	void (*Text_set_horizontalOverflow)(Il2CppObject* self, std::int32_t value);
	void (*Text_set_verticalOverflow)(Il2CppObject* self, std::int32_t value);
	void (*Text_set_fontStyle)(Il2CppObject* self, std::int32_t value);
	void (*Text_set_lineSpacing)(Il2CppObject* self, float value);

	std::uint32_t ReplaceFontHandle;

	DEFINE_HOOK(void, TextCommon_Awake, (Il2CppObject * self))
	{
		TextCommon_Awake_Orig(self);

		Il2CppObject* replaceFont{};
		if (!ExtraAssetBundleHandle)
		{
			LoadResources();
		}
		if (ReplaceFontHandle)
		{
			replaceFont = il2cpp_gchandle_get_target(ReplaceFontHandle);
			if (Object_IsNativeObjectAlive(replaceFont))
			{
				goto FontAlive;
			}
			else
			{
				il2cpp_gchandle_free(std::exchange(ReplaceFontHandle, 0));
			}
		}
		{
			const auto extraAssetBundle = il2cpp_gchandle_get_target(ExtraAssetBundleHandle);
			replaceFont = AssetBundle_LoadAsset_Orig(
			    extraAssetBundle,
			    il2cpp_string_new(Plugin::GetInstance().GetConfig().ReplaceFontPath.c_str()),
			    Font_Type);
		}
		if (replaceFont)
		{
			ReplaceFontHandle = il2cpp_gchandle_new(replaceFont, false);
		}
		else
		{
			Log::Warn("UmaPyogin: Failed to load replace font.");
		}

	FontAlive:
		if (replaceFont)
		{
			Text_set_font(self, replaceFont);
		}
		else
		{
			Text_AssignDefaultFont(self);
		}

		Text_set_horizontalOverflow(self, 1);
		Text_set_verticalOverflow(self, 1);
		Text_set_fontStyle(self, 1);
		Text_set_lineSpacing(self, 1.03f);
	}

	int (*Query_GetInt)(void* self, int idx);

	struct ColumnIndex
	{
		std::size_t Value;
		std::optional<std::size_t> QueryResult;

		explicit ColumnIndex(std::size_t value) : Value(value)
		{
		}
	};

	struct BindingParam
	{
		std::size_t Value;
		std::optional<std::size_t> BindedValue;

		explicit BindingParam(std::size_t value) : Value(value)
		{
		}
	};

	using QueryIndex = std::variant<std::monostate, ColumnIndex, BindingParam>;

	struct ILocalizationQuery
	{
		virtual ~ILocalizationQuery() = default;

		virtual void AddColumn(std::size_t index, std::string_view column)
		{
		}

		virtual void AddParam(std::size_t index, std::string_view param)
		{
		}

		virtual void Bind(std::size_t index, std::size_t value)
		{
		}

		virtual void Step(void* query)
		{
		}

		virtual const std::u16string* GetString(std::size_t index) = 0;
	};

	struct TextDataQuery : ILocalizationQuery
	{
		QueryIndex Category;
		QueryIndex Index;

		QueryIndex Text;

		void AddColumn(std::size_t index, std::string_view column) override
		{
			if (column == "text"sv)
			{
				Text.emplace<ColumnIndex>(index);
			}
		}

		void AddParam(std::size_t index, std::string_view param) override
		{
			if (param == "category"sv)
			{
				Category.emplace<BindingParam>(index);
			}
			else if (param == "index"sv)
			{
				Index.emplace<BindingParam>(index);
			}
		}

		void Bind(std::size_t index, std::size_t value) override
		{
			if (const auto p = std::get_if<BindingParam>(&Category))
			{
				if (index == p->Value)
				{
					p->BindedValue.emplace(value);
					return;
				}
			}

			if (const auto p = std::get_if<BindingParam>(&Index))
			{
				if (index == p->Value)
				{
					p->BindedValue.emplace(value);
				}
			}
		}

		const std::u16string* GetString(std::size_t index) override
		{
			if (index == std::get<ColumnIndex>(Text).Value)
			{
				const auto category = std::get<BindingParam>(Category).BindedValue.value();
				const auto index = std::get<BindingParam>(Index).BindedValue.value();
				return Localization::DatabaseLocalization::GetInstance().GetTextData(category,
				                                                                     index);
			}
			return nullptr;
		}
	};

	struct CharacterSystemTextQuery : ILocalizationQuery
	{
		QueryIndex CharacterId;
		QueryIndex VoiceId;

		QueryIndex Text;

		void AddColumn(std::size_t index, std::string_view column) override
		{
			if (column == "text"sv)
			{
				Text.emplace<ColumnIndex>(index);
			}
			else if (column == "voice_id"sv)
			{
				VoiceId.emplace<ColumnIndex>(index);
			}
		}

		void AddParam(std::size_t index, std::string_view param) override
		{
			if (param == "character_id"sv)
			{
				CharacterId.emplace<BindingParam>(index);
			}
			else if (param == "voice_id"sv)
			{
				VoiceId.emplace<BindingParam>(index);
			}
		}

		void Bind(std::size_t index, std::size_t value) override
		{
			if (const auto p = std::get_if<BindingParam>(&CharacterId))
			{
				if (index == p->Value)
				{
					p->BindedValue.emplace(value);
					return;
				}
			}

			if (const auto p = std::get_if<BindingParam>(&VoiceId))
			{
				if (index == p->Value)
				{
					p->BindedValue.emplace(value);
				}
			}
		}

		void Step(void* query) override
		{
			assert(std::holds_alternative<BindingParam>(CharacterId));
			if (const auto p = std::get_if<ColumnIndex>(&VoiceId))
			{
				const auto voiceId = Query_GetInt(query, p->Value);
				p->QueryResult.emplace(voiceId);
			}
		}

		const std::u16string* GetString(std::size_t index) override
		{
			if (index == std::get<ColumnIndex>(Text).Value)
			{
				const auto characterId = std::get<BindingParam>(CharacterId).BindedValue.value();
				const auto voiceId = [&] {
					if (const auto column = std::get_if<ColumnIndex>(&VoiceId))
					{
						return column->QueryResult.value();
					}
					else
					{
						return std::get<BindingParam>(VoiceId).BindedValue.value();
					}
				}();

				return Localization::DatabaseLocalization::GetInstance().GetCharacterSystemTextData(
				    characterId, voiceId);
			}
			return nullptr;
		}
	};

	struct RaceJikkyoCommentQuery : ILocalizationQuery
	{
		QueryIndex Id;

		QueryIndex Message;

		void AddColumn(std::size_t index, std::string_view column) override
		{
			if (column == "message"sv)
			{
				Message.emplace<ColumnIndex>(index);
			}
			else if (column == "id"sv)
			{
				Id.emplace<ColumnIndex>(index);
			}
		}

		void AddParam(std::size_t index, std::string_view param) override
		{
			if (param == "id"sv)
			{
				Id.emplace<BindingParam>(index);
			}
		}

		void Bind(std::size_t index, std::size_t value) override
		{
			if (const auto p = std::get_if<BindingParam>(&Id))
			{
				if (index == p->Value)
				{
					p->BindedValue.emplace(value);
				}
			}
		}

		void Step(void* query) override
		{
			if (const auto p = std::get_if<ColumnIndex>(&Id))
			{
				const auto id = Query_GetInt(query, p->Value);
				p->QueryResult.emplace(id);
			}
		}

		const std::u16string* GetString(std::size_t index) override
		{
			if (index == std::get<ColumnIndex>(Message).Value)
			{
				const auto id = [&] {
					if (const auto column = std::get_if<ColumnIndex>(&Id))
					{
						return column->QueryResult.value();
					}
					else
					{
						return std::get<BindingParam>(Id).BindedValue.value();
					}
				}();
				return Localization::DatabaseLocalization::GetInstance().GetRaceJikkyoCommentData(
				    id);
			}
			return nullptr;
		}
	};

	struct RaceJikkyoMessageQuery : ILocalizationQuery
	{
		QueryIndex Id;

		QueryIndex Message;

		void AddColumn(std::size_t index, std::string_view column) override
		{
			if (column == "message"sv)
			{
				Message.emplace<ColumnIndex>(index);
			}
			else if (column == "id"sv)
			{
				Id.emplace<ColumnIndex>(index);
			}
		}

		void AddParam(std::size_t index, std::string_view param) override
		{
			if (param == "id"sv)
			{
				Id.emplace<BindingParam>(index);
			}
		}

		void Bind(std::size_t index, std::size_t value) override
		{
			if (const auto p = std::get_if<BindingParam>(&Id))
			{
				if (index == p->Value)
				{
					p->BindedValue.emplace(value);
				}
			}
		}

		void Step(void* query) override
		{
			if (const auto p = std::get_if<ColumnIndex>(&Id))
			{
				const auto id = Query_GetInt(query, p->Value);
				p->QueryResult.emplace(id);
			}
		}

		const std::u16string* GetString(std::size_t index) override
		{
			if (index == std::get<ColumnIndex>(Message).Value)
			{
				const auto id = [&] {
					if (const auto column = std::get_if<ColumnIndex>(&Id))
					{
						return column->QueryResult.value();
					}
					else
					{
						return std::get<BindingParam>(Id).BindedValue.value();
					}
				}();
				return Localization::DatabaseLocalization::GetInstance().GetRaceJikkyoMessageData(
				    id);
			}
			return nullptr;
		}
	};

	std::unordered_map<void*, std::unique_ptr<ILocalizationQuery>> TextQueries;

	DEFINE_HOOK(void*, Query_ctor, (void* self, void* conn, Il2CppString* sql))
	{
		const auto sqlStr = Misc::ToUTF8(sql->chars);

		static const std::regex statementPattern(R"(SELECT (.+?) FROM `(.+?)`(?: WHERE (.+))?;)");
		static const std::regex columnPattern(R"(,?`(\w+)`)");
		static const std::regex whereClausePattern(R"((?:AND )?`(\w+)=?`)");

		std::cmatch matches;
		if (std::regex_match(sqlStr.c_str(), matches, statementPattern))
		{
			const auto columns = matches[1].str();
			const auto table = matches[2].str();
			const auto whereClause =
			    matches.size() == 4 ? std::optional(matches[3].str()) : std::nullopt;

			std::unique_ptr<ILocalizationQuery> query;

			if (table == "text_data"sv)
			{
				query = std::make_unique<TextDataQuery>();
			}
			else if (table == "character_system_text"sv)
			{
				query = std::make_unique<CharacterSystemTextQuery>();
			}
			else if (table == "race_jikkyo_comment"sv)
			{
				query = std::make_unique<RaceJikkyoCommentQuery>();
			}
			else if (table == "race_jikkyo_message"sv)
			{
				query = std::make_unique<RaceJikkyoMessageQuery>();
			}
			else
			{
				goto NormalPath;
			}

			auto columnsPtr = columns.c_str();
			std::size_t columnIndex{};
			while (std::regex_search(columnsPtr, matches, columnPattern))
			{
				const auto column = matches[1].str();
				query->AddColumn(columnIndex++, column);

				columnsPtr = matches.suffix().first;
			}

			// 有 WHERE 子句的查询
			if (whereClause)
			{
				auto whereClausePtr = whereClause->c_str();
				std::size_t paramIndex = 1;
				while (std::regex_search(whereClausePtr, matches, whereClausePattern))
				{
					const auto param = matches[1].str();
					query->AddParam(paramIndex++, param);

					whereClausePtr = matches.suffix().first;
				}
			}

			TextQueries.emplace(self, std::move(query));
		}

	NormalPath:
		return Query_ctor_Orig(self, conn, sql);
	}

	DEFINE_HOOK(void, PreparedQuery_BindInt, (void* self, std::int32_t idx, std::int32_t value))
	{
		if (const auto iter = TextQueries.find(self); iter != TextQueries.end())
		{
			iter->second->Bind(idx, value);
		}

		PreparedQuery_BindInt_Orig(self, idx, value);
	}

	DEFINE_HOOK(bool, Query_Step, (void* self))
	{
		const auto result = Query_Step_Orig(self);

		if (const auto iter = TextQueries.find(self); iter != TextQueries.end())
		{
			iter->second->Step(self);
		}

		return result;
	}

	DEFINE_HOOK(Il2CppString*, Query_GetText, (void* self, std::int32_t idx))
	{
		if (const auto iter = TextQueries.find(self); iter != TextQueries.end())
		{
			if (const auto localizedStr = iter->second->GetString(idx))
			{
				return ToIl2CppString(*localizedStr);
			}
		}

		return Query_GetText_Orig(self, idx);
	}

	DEFINE_HOOK(void, Query_Dispose, (void* self))
	{
		TextQueries.erase(self);
		Query_Dispose_Orig(self);
	}

#define CHECK_NULL(expr)                                                                           \
	if (!(expr))                                                                                   \
	{                                                                                              \
		Log::Error("UmaPyogin: " #expr " is null");                                                \
		return;                                                                                    \
	}

	HookFunction g_Hook;

#define HOOK_FUNC(name, addr)                                                                      \
	name##_Addr = reinterpret_cast<name##_Type>(addr);                                             \
	CHECK_NULL(name##_Addr);                                                                       \
	hookInstaller->InstallHook(reinterpret_cast<OpaqueFunctionPointer>(name##_Addr),               \
	                           reinterpret_cast<OpaqueFunctionPointer>(name##_Hook),               \
	                           reinterpret_cast<OpaqueFunctionPointer*>(&name##_Orig))

	void InjectFunctions()
	{
		const auto hookInstaller = Plugin::GetInstance().GetHookInstaller();

		Log::Error("UmaPyogin: InjectFunctions");

		const auto domain = il2cpp_domain_get();
		CHECK_NULL(domain);

		// mscorlib.dll
		{
			const auto corlib = il2cpp_get_corlib();
			CHECK_NULL(corlib);

			IListClass = il2cpp_class_from_name(corlib, "System.Collections", "IList");
			CHECK_NULL(IListClass);

			const auto environmentClass = il2cpp_class_from_name(corlib, "System", "Environment");
			CHECK_NULL(environmentClass);

			const auto Environment_get_StackTrace_Method =
			    il2cpp_class_get_method_from_name(environmentClass, "get_StackTrace", 0);
			CHECK_NULL(Environment_get_StackTrace_Method);

			Environment_get_StackTrace = reinterpret_cast<decltype(Environment_get_StackTrace)>(
			    Environment_get_StackTrace_Method->methodPointer);
			CHECK_NULL(Environment_get_StackTrace);
		}

		// umamusume.dll
		{
			const auto umamusumeAssembly = il2cpp_domain_assembly_open(domain, "umamusume.dll");
			CHECK_NULL(umamusumeAssembly);

			const auto umamusumeImage = il2cpp_assembly_get_image(umamusumeAssembly);
			CHECK_NULL(umamusumeImage);

			const auto localizeClass = il2cpp_class_from_name(umamusumeImage, "Gallop", "Localize");
			CHECK_NULL(localizeClass);

			const auto localizeJPClass = FindNestedClassFromName(localizeClass, "JP");
			CHECK_NULL(localizeJPClass);

			const auto getMethod = il2cpp_class_get_method_from_name(localizeJPClass, "Get", 1);
			CHECK_NULL(getMethod);

			HOOK_FUNC(LocalizeJP_Get, getMethod->methodPointer);

			StoryTimelineDataClass =
			    il2cpp_class_from_name(umamusumeImage, "Gallop", "StoryTimelineData");
			CHECK_NULL(StoryTimelineDataClass);
			StoryTimelineDataClass_StoryIdField =
			    il2cpp_class_get_field_from_name(StoryTimelineDataClass, "StoryId");
			CHECK_NULL(StoryTimelineDataClass_StoryIdField);
			StoryTimelineDataClass_TitleField =
			    il2cpp_class_get_field_from_name(StoryTimelineDataClass, "Title");
			CHECK_NULL(StoryTimelineDataClass_TitleField);
			StoryTimelineDataClass_BlockListField =
			    il2cpp_class_get_field_from_name(StoryTimelineDataClass, "BlockList");
			CHECK_NULL(StoryTimelineDataClass_BlockListField);

			StoryTimelineTextClipDataClass =
			    il2cpp_class_from_name(umamusumeImage, "Gallop", "StoryTimelineTextClipData");
			CHECK_NULL(StoryTimelineTextClipDataClass);
			StoryTimelineTextClipDataClass_NameField =
			    il2cpp_class_get_field_from_name(StoryTimelineTextClipDataClass, "Name");
			CHECK_NULL(StoryTimelineTextClipDataClass_NameField);
			StoryTimelineTextClipDataClass_TextField =
			    il2cpp_class_get_field_from_name(StoryTimelineTextClipDataClass, "Text");
			CHECK_NULL(StoryTimelineTextClipDataClass_TextField);
			StoryTimelineTextClipDataClass_ChoiceDataList =
			    il2cpp_class_get_field_from_name(StoryTimelineTextClipDataClass, "ChoiceDataList");
			CHECK_NULL(StoryTimelineTextClipDataClass_ChoiceDataList);
			StoryTimelineTextClipDataClass_ColorTextInfoListField =
			    il2cpp_class_get_field_from_name(StoryTimelineTextClipDataClass,
			                                     "ColorTextInfoList");
			CHECK_NULL(StoryTimelineTextClipDataClass_ColorTextInfoListField);

			StoryTimelineTextClipDataClass_ChoiceDataClass =
			    FindNestedClassFromName(StoryTimelineTextClipDataClass, "ChoiceData");
			CHECK_NULL(StoryTimelineTextClipDataClass_ChoiceDataClass);
			StoryTimelineTextClipDataClass_ChoiceDataClass_TextField =
			    il2cpp_class_get_field_from_name(StoryTimelineTextClipDataClass_ChoiceDataClass,
			                                     "Text");
			CHECK_NULL(StoryTimelineTextClipDataClass_ChoiceDataClass_TextField);

			StoryTimelineTextClipDataClass_ColorTextInfoClass =
			    FindNestedClassFromName(StoryTimelineTextClipDataClass, "ColorTextInfo");
			CHECK_NULL(StoryTimelineTextClipDataClass_ColorTextInfoClass);
			StoryTimelineTextClipDataClass_ColorTextInfoClass_TextField =
			    il2cpp_class_get_field_from_name(StoryTimelineTextClipDataClass_ColorTextInfoClass,
			                                     "Text");
			CHECK_NULL(StoryTimelineTextClipDataClass_ColorTextInfoClass_TextField);

			StoryTimelineBlockDataClass =
			    il2cpp_class_from_name(umamusumeImage, "Gallop", "StoryTimelineBlockData");
			CHECK_NULL(StoryTimelineBlockDataClass);
			StoryTimelineBlockDataClass_TextTrackField =
			    il2cpp_class_get_field_from_name(StoryTimelineBlockDataClass, "TextTrack");
			CHECK_NULL(StoryTimelineBlockDataClass_TextTrackField);

			StoryTimelineTextTrackDataClass =
			    il2cpp_class_from_name(umamusumeImage, "Gallop", "StoryTimelineTextTrackData");
			CHECK_NULL(StoryTimelineTextTrackDataClass);
			StoryTimelineTrackDataClass_ClipListField =
			    il2cpp_class_get_field_from_name(StoryTimelineTextTrackDataClass, "ClipList");
			CHECK_NULL(StoryTimelineTrackDataClass_ClipListField);

			StoryTimelineClipDataClass =
			    il2cpp_class_from_name(umamusumeImage, "Gallop", "StoryTimelineClipData");
			CHECK_NULL(StoryTimelineClipDataClass);

			StoryRaceTextAssetClass =
			    il2cpp_class_from_name(umamusumeImage, "Gallop", "StoryRaceTextAsset");
			CHECK_NULL(StoryRaceTextAssetClass);
			StoryRaceTextAssetClass_textDataField =
			    il2cpp_class_get_field_from_name(StoryRaceTextAssetClass, "textData");
			CHECK_NULL(StoryRaceTextAssetClass_textDataField);

			StoryRaceTextAssetClass_KeyClass =
			    FindNestedClassFromName(StoryRaceTextAssetClass, "Key");
			CHECK_NULL(StoryRaceTextAssetClass_KeyClass);
			StoryRaceTextAssetClass_KeyClass_textField =
			    il2cpp_class_get_field_from_name(StoryRaceTextAssetClass_KeyClass, "text");
			CHECK_NULL(StoryRaceTextAssetClass_KeyClass_textField);

			const auto textCommonClass =
			    il2cpp_class_from_name(umamusumeImage, "Gallop", "TextCommon");
			CHECK_NULL(textCommonClass);
			const auto TextCommon_Awake_Method =
			    il2cpp_class_get_method_from_name(textCommonClass, "Awake", 0);
			CHECK_NULL(TextCommon_Awake_Method);

			HOOK_FUNC(TextCommon_Awake, TextCommon_Awake_Method->methodPointer);
		}

		// LibNative.Runtime.dll
		{
			const auto libNativeAssembly =
			    il2cpp_domain_assembly_open(domain, "LibNative.Runtime.dll");
			CHECK_NULL(libNativeAssembly);

			const auto libNativeImage = il2cpp_assembly_get_image(libNativeAssembly);
			CHECK_NULL(libNativeImage);

			const auto QueryClass =
			    il2cpp_class_from_name(libNativeImage, "LibNative.Sqlite3", "Query");
			CHECK_NULL(QueryClass);

			const auto Query_ctor_Method =
			    il2cpp_class_get_method_from_name(QueryClass, ".ctor", 2);
			CHECK_NULL(Query_ctor_Method);

			HOOK_FUNC(Query_ctor, Query_ctor_Method->methodPointer);

			const auto Query_GetInt_Method =
			    il2cpp_class_get_method_from_name(QueryClass, "GetInt", 1);
			CHECK_NULL(Query_GetInt_Method);

			Query_GetInt =
			    reinterpret_cast<decltype(Query_GetInt)>(Query_GetInt_Method->methodPointer);
			CHECK_NULL(Query_GetInt);

			const auto Query_GetText_Method =
			    il2cpp_class_get_method_from_name(QueryClass, "GetText", 1);
			CHECK_NULL(Query_GetText_Method);

			HOOK_FUNC(Query_GetText, Query_GetText_Method->methodPointer);

			const auto Query_Step_Method = il2cpp_class_get_method_from_name(QueryClass, "Step", 0);
			CHECK_NULL(Query_Step_Method);

			HOOK_FUNC(Query_Step, Query_Step_Method->methodPointer);

			const auto Query_Dispose_Method =
			    il2cpp_class_get_method_from_name(QueryClass, "Dispose", 0);
			CHECK_NULL(Query_Dispose_Method);

			HOOK_FUNC(Query_Dispose, Query_Dispose_Method->methodPointer);

			const auto PreparedQueryClass =
			    il2cpp_class_from_name(libNativeImage, "LibNative.Sqlite3", "PreparedQuery");
			CHECK_NULL(PreparedQueryClass);

			const auto PreparedQuery_BindInt_Method =
			    il2cpp_class_get_method_from_name(PreparedQueryClass, "BindInt", 2);
			CHECK_NULL(PreparedQuery_BindInt_Method);

			HOOK_FUNC(PreparedQuery_BindInt, PreparedQuery_BindInt_Method->methodPointer);
		}

		// UnityEngine.AssetBundleModule.dll
		{
			const auto unityAssetBundleModuleAssembly =
			    il2cpp_domain_assembly_open(domain, "UnityEngine.AssetBundleModule.dll");
			CHECK_NULL(unityAssetBundleModuleAssembly);

			const auto unityAssetBundleModuleImage =
			    il2cpp_assembly_get_image(unityAssetBundleModuleAssembly);
			CHECK_NULL(unityAssetBundleModuleImage);

			const auto assetBundleClass =
			    il2cpp_class_from_name(unityAssetBundleModuleImage, "UnityEngine", "AssetBundle");
			CHECK_NULL(assetBundleClass);

			const auto loadAssetMethod =
			    il2cpp_class_get_method_from_name(assetBundleClass, "LoadAsset", 2);
			CHECK_NULL(loadAssetMethod);

			HOOK_FUNC(AssetBundle_LoadAsset, loadAssetMethod->methodPointer);

			const auto loadFromFileMethod =
			    il2cpp_class_get_method_from_name(assetBundleClass, "LoadFromFile", 1);
			CHECK_NULL(loadFromFileMethod);
			AssetBundle_LoadFromFile = reinterpret_cast<decltype(AssetBundle_LoadFromFile)>(
			    loadFromFileMethod->methodPointer);
			CHECK_NULL(AssetBundle_LoadFromFile);

			const auto getAllAssetNamesMethod =
			    il2cpp_class_get_method_from_name(assetBundleClass, "GetAllAssetNames", 0);
			CHECK_NULL(getAllAssetNamesMethod);
			AssetBundle_GetAllAssetNames = reinterpret_cast<decltype(AssetBundle_GetAllAssetNames)>(
			    getAllAssetNamesMethod->methodPointer);
			CHECK_NULL(AssetBundle_GetAllAssetNames);
		}

		// UnityEngine.CoreModule.dll
		{
			const auto unityCoreModuleAssembly =
			    il2cpp_domain_assembly_open(domain, "UnityEngine.CoreModule.dll");
			CHECK_NULL(unityCoreModuleAssembly);

			const auto unityCoreModuleImage = il2cpp_assembly_get_image(unityCoreModuleAssembly);
			CHECK_NULL(unityCoreModuleImage);

			const auto objectClass =
			    il2cpp_class_from_name(unityCoreModuleImage, "UnityEngine", "Object");
			CHECK_NULL(objectClass);

			const auto Object_IsNativeObjectAliveMethod =
			    il2cpp_class_get_method_from_name(objectClass, "IsNativeObjectAlive", 1);
			CHECK_NULL(Object_IsNativeObjectAliveMethod);
			Object_IsNativeObjectAlive = reinterpret_cast<decltype(Object_IsNativeObjectAlive)>(
			    Object_IsNativeObjectAliveMethod->methodPointer);
			CHECK_NULL(Object_IsNativeObjectAlive);

			const auto applicationClass =
			    il2cpp_class_from_name(unityCoreModuleImage, "UnityEngine", "Application");
			CHECK_NULL(applicationClass);
			const auto Application_set_targetFrameRateMethod =
			    il2cpp_class_get_method_from_name(applicationClass, "set_targetFrameRate", 1);
			CHECK_NULL(Application_set_targetFrameRateMethod);

			HOOK_FUNC(Application_set_targetFrameRate,
			          Application_set_targetFrameRateMethod->methodPointer);
		}

		// UnityEngine.UI.dll
		{
			const auto unityUiAssembly = il2cpp_domain_assembly_open(domain, "UnityEngine.UI.dll");
			CHECK_NULL(unityUiAssembly);

			const auto unityUiImage = il2cpp_assembly_get_image(unityUiAssembly);
			CHECK_NULL(unityUiImage);

			const auto textClass = il2cpp_class_from_name(unityUiImage, "UnityEngine.UI", "Text");
			CHECK_NULL(textClass);

			const auto Text_set_font_Method =
			    il2cpp_class_get_method_from_name(textClass, "set_font", 1);
			CHECK_NULL(Text_set_font_Method);
			Text_set_font =
			    reinterpret_cast<decltype(Text_set_font)>(Text_set_font_Method->methodPointer);
			CHECK_NULL(Text_set_font);

			const auto Text_AssignDefaultFont_Method =
			    il2cpp_class_get_method_from_name(textClass, "AssignDefaultFont", 0);
			CHECK_NULL(Text_AssignDefaultFont_Method);
			Text_AssignDefaultFont = reinterpret_cast<decltype(Text_AssignDefaultFont)>(
			    Text_AssignDefaultFont_Method->methodPointer);
			CHECK_NULL(Text_AssignDefaultFont);

			const auto Text_set_horizontalOverflow_Method =
			    il2cpp_class_get_method_from_name(textClass, "set_horizontalOverflow", 1);
			CHECK_NULL(Text_set_horizontalOverflow_Method);
			Text_set_horizontalOverflow = reinterpret_cast<decltype(Text_set_horizontalOverflow)>(
			    Text_set_horizontalOverflow_Method->methodPointer);
			CHECK_NULL(Text_set_horizontalOverflow);

			const auto Text_set_verticalOverflow_Method =
			    il2cpp_class_get_method_from_name(textClass, "set_verticalOverflow", 1);
			CHECK_NULL(Text_set_verticalOverflow_Method);
			Text_set_verticalOverflow = reinterpret_cast<decltype(Text_set_verticalOverflow)>(
			    Text_set_verticalOverflow_Method->methodPointer);
			CHECK_NULL(Text_set_verticalOverflow);

			const auto Text_set_fontStyle_Method =
			    il2cpp_class_get_method_from_name(textClass, "set_fontStyle", 1);
			CHECK_NULL(Text_set_fontStyle_Method);
			Text_set_fontStyle = reinterpret_cast<decltype(Text_set_fontStyle)>(
			    Text_set_fontStyle_Method->methodPointer);
			CHECK_NULL(Text_set_fontStyle);

			const auto Text_set_lineSpacing_Method =
			    il2cpp_class_get_method_from_name(textClass, "set_lineSpacing", 1);
			CHECK_NULL(Text_set_lineSpacing_Method);
			Text_set_lineSpacing = reinterpret_cast<decltype(Text_set_lineSpacing)>(
			    Text_set_lineSpacing_Method->methodPointer);
			CHECK_NULL(Text_set_lineSpacing);
		}

		// UnityEngine.TextRenderingModule.dll
		{
			const auto unityTextRenderingModuleAssembly =
			    il2cpp_domain_assembly_open(domain, "UnityEngine.TextRenderingModule.dll");
			CHECK_NULL(unityTextRenderingModuleAssembly);

			const auto unityTextRenderingModuleImage =
			    il2cpp_assembly_get_image(unityTextRenderingModuleAssembly);
			CHECK_NULL(unityTextRenderingModuleImage);

			const auto fontClass =
			    il2cpp_class_from_name(unityTextRenderingModuleImage, "UnityEngine", "Font");
			CHECK_NULL(fontClass);

			const auto fontType = il2cpp_class_get_type(fontClass);
			Font_Type = reinterpret_cast<Il2CppReflectionType*>(il2cpp_type_get_object(fontType));
		}
	}

	void LoadResources()
	{
		Log::Info("UmaPyogin: LoadResources");

		const auto& config = Plugin::GetInstance().GetConfig();

		const auto extraAssetBundle =
		    AssetBundle_LoadFromFile(il2cpp_string_new(config.ExtraAssetBundlePath.c_str()));
		CHECK_NULL(extraAssetBundle);

		const auto allAssetNames = AssetBundle_GetAllAssetNames(extraAssetBundle);
		CHECK_NULL(allAssetNames);

		IterateIList(allAssetNames, [](std::size_t, Il2CppObject* name) {
			ExtraAssetBundleAssetNameSet.emplace(reinterpret_cast<Il2CppString*>(name)->chars);
		});

		ExtraAssetBundleHandle = il2cpp_gchandle_new(extraAssetBundle, false);
	}

	DEFINE_HOOK(int, il2cpp_init, (const char* domain_name))
	{
		const auto ret = il2cpp_init_Orig(domain_name);
		InjectFunctions();

		Log::Info("UmaPyogin: Loading localization files");

		const auto& config = Plugin::GetInstance().GetConfig();

		const std::filesystem::path staticLocalizationFilePath = config.StaticLocalizationFilePath;
		if (std::filesystem::is_regular_file(staticLocalizationFilePath))
		{
			auto& staticLocalization = Localization::StaticLocalization::GetInstance();
			staticLocalization.LoadFrom(staticLocalizationFilePath);
		}

		const std::filesystem::path storyLocalizationDirPath = config.StoryLocalizationDirPath;
		if (std::filesystem::is_directory(storyLocalizationDirPath))
		{
			auto& storyLocalization = Localization::StoryLocalization::GetInstance();
			storyLocalization.LoadFrom(storyLocalizationDirPath);
		}

		auto& databaseLocalization = Localization::DatabaseLocalization::GetInstance();
		databaseLocalization.LoadFrom(
		    config.TextDataDictPath, config.CharacterSystemTextDataDictPath,
		    config.RaceJikkyoCommentDataDictPath, config.RaceJikkyoMessageDataDictPath);

		Log::Info("UmaPyogin: Initialized");
		return ret;
	}
} // namespace

namespace UmaPyogin::Hook
{
	Il2CppString* LocalizeJP_Get(std::int32_t id)
	{
		return LocalizeJP_Get_Orig(id);
	}

	void Install()
	{
		const auto hookInstaller = Plugin::GetInstance().GetHookInstaller();

		Log::Info("UmaPyogin: Installing hook");

		LoadIl2CppSymbols();

		HOOK_FUNC(il2cpp_init,
		          Plugin::GetInstance().GetHookInstaller()->LookupSymbol("il2cpp_init"));

		Log::Info("UmaPyogin: Hook installed");
	}
} // namespace UmaPyogin::Hook
