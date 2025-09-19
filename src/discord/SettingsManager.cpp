#include "SettingsManager.hpp"
#include "Frontend.hpp"
#include "DiscordInstance.hpp"

SettingsManager* GetSettingsManager()
{
	static SettingsManager s_instance;
	return &s_instance;
}

SettingsManager::SettingsManager()
	: m_pSettingsMessage(std::make_unique<Protobuf::ObjectBaseMessage>())
{
}

SettingsManager::~SettingsManager() = default;

void SettingsManager::LoadData(const uint8_t* data, size_t sz)
{
	using namespace Protobuf;
	if (!data || sz == 0) return;

	auto pMsg = std::make_unique<ObjectBaseMessage>();
	pMsg->SetDecodingHint(CreateHint()); // gonna get deleted below

	auto decodeRes = DecodeBlock(reinterpret_cast<const char*>(data), sz, pMsg.get());
	if (!decodeRes.has_value()) {
		GetFrontend()->OnProtobufError(decodeRes.error());
		return;
	}

	auto mergeRes = m_pSettingsMessage->MergeWith(pMsg.get());
	if (!mergeRes.has_value()) {
		GetFrontend()->OnProtobufError(mergeRes.error());
		return;
	}


	m_pSettingsMessage->MarkClean();
}

void SettingsManager::FlushSettings()
{
	std::vector<uint8_t> data;
	m_pSettingsMessage->EncodeDirty(data);
	m_pSettingsMessage->MarkClean();

	GetDiscordInstance()->SendSettingsProto(data);
}

void SettingsManager::SetOnlineIndicator(eActiveStatus status)
{
	using namespace Protobuf;

	ObjectString* pState = m_pSettingsMessage
		->GetFieldObjectDefault<ObjectMessage>(Settings::FIELD_ACTIVITY)
		->GetFieldObjectDefault<ObjectMessage>(Settings::Activity::FIELD_INDICATOR)
		->GetFieldObjectDefault<ObjectString>(Settings::Activity::Indicator::FIELD_STATE);

	static constexpr const char* possibleUpdates[] = {
		"invisible",
		"online",
		"idle",
		"dnd",
	};
	
	static_assert(_countof(possibleUpdates) == int(STATUS_MAX), "Update this array if you add more status states!");

	std::string state(possibleUpdates[int(status)]);

	if (pState->GetContent() != state)
	{
		pState->SetContent(state);

		// mark dirty so it's still sent, so that it doesn't accidentally remove your status message
		m_pSettingsMessage
			->GetFieldObjectDefault<ObjectMessage>(Settings::FIELD_ACTIVITY)
			->GetFieldObjectDefault<ObjectMessage>(Settings::Activity::FIELD_INDICATOR)
			->MarkDirtyAndChildren();
	}
}

eActiveStatus SettingsManager::GetOnlineIndicator()
{
	return GetStatusFromString(
		m_pSettingsMessage
		->GetFieldObjectDefault<Protobuf::ObjectMessage>(Settings::FIELD_ACTIVITY)
		->GetFieldObjectDefault<Protobuf::ObjectMessage>(Settings::Activity::FIELD_INDICATOR)
		->GetFieldObjectDefault<Protobuf::ObjectString>(Settings::Activity::Indicator::FIELD_STATE)
		->GetContent()
	);
}

void SettingsManager::SetCustomStatus(const std::string& text, const std::string& emoji, uint64_t timeExpiry)
{
	using namespace Protobuf;

	if (text.empty() && emoji.empty())
	{
		// remove the custom-status field if both empty
		ObjectMessage* pActivity = m_pSettingsMessage
			->GetFieldObjectDefault<ObjectMessage>(Settings::FIELD_ACTIVITY);

		pActivity->EraseField(Settings::Activity::FIELD_CUSTOM_STATUS);
	}
	else
	{
		ObjectMessage *pCustomStatus = m_pSettingsMessage
			->GetFieldObjectDefault<ObjectMessage>(Settings::FIELD_ACTIVITY)
			->GetFieldObjectDefault<ObjectMessage>(Settings::Activity::FIELD_CUSTOM_STATUS);

		pCustomStatus
			->GetFieldObjectDefault<ObjectString>(Settings::Activity::CustomStatus::FIELD_TEXT)
			->SetContent(text);

		pCustomStatus
			->GetFieldObjectDefault<ObjectString>(Settings::Activity::CustomStatus::FIELD_EMOJI)
			->SetContent(emoji);

		pCustomStatus
			->GetFieldObjectDefault<ObjectFixed64>(Settings::Activity::CustomStatus::FIELD_EXPIRY)
			->SetValue(timeExpiry);

		// ensure indicator string is marked dirty so server receives the update
		m_pSettingsMessage
			->GetFieldObjectDefault<ObjectMessage>(Settings::FIELD_ACTIVITY)
			->GetFieldObjectDefault<ObjectString>(Settings::Activity::FIELD_INDICATOR)
			->MarkDirty();
	}
}

std::string SettingsManager::GetCustomStatusText()
{
	using namespace Protobuf;

	ObjectBase* pObject = m_pSettingsMessage
		->GetFieldObjectDefault<ObjectMessage>(Settings::FIELD_ACTIVITY)
		->GetFieldObjectDefault<ObjectMessage>(Settings::Activity::FIELD_CUSTOM_STATUS)
		->GetFieldObject(Settings::Activity::CustomStatus::FIELD_TEXT);


	if (!pObject || !pObject->IsStringObject())
		return {};

	return static_cast<ObjectString*>(pObject)->GetContent();
}

std::string SettingsManager::GetCustomStatusEmoji()
{
	return
		m_pSettingsMessage
		->GetFieldObjectDefault<Protobuf::ObjectMessage>(Settings::FIELD_ACTIVITY)
		->GetFieldObjectDefault<Protobuf::ObjectMessage>(Settings::Activity::FIELD_CUSTOM_STATUS)
		->GetFieldObjectDefault<Protobuf::ObjectString>(Settings::Activity::CustomStatus::FIELD_EMOJI)
		->GetContent();
}

uint64_t SettingsManager::GetCustomStatusExpiry()
{
	using namespace Protobuf;

	return m_pSettingsMessage
		->GetFieldObjectDefault<ObjectMessage>(Settings::FIELD_ACTIVITY)
		->GetFieldObjectDefault<ObjectMessage>(Settings::Activity::FIELD_CUSTOM_STATUS)
		->GetFieldObjectDefault<ObjectFixed64>(Settings::Activity::CustomStatus::FIELD_EXPIRY)
		->GetValue();
}

void SettingsManager::SetExplicitFilter(eExplicitFilter filter)
{
	int flt = int(filter);

	m_pSettingsMessage
		->GetFieldObjectDefault<Protobuf::ObjectMessage>(Settings::FIELD_TEXT_AND_IMAGES)
		->GetFieldObjectDefault<Protobuf::ObjectMessage>(Settings::TextAndImages::FIELD_EXPLICIT_FILTER)
		->GetFieldObjectDefault<Protobuf::ObjectVarInt>(Settings::TextAndImages::ExplicitFilter::FIELD_VALUE)
		->SetValue(flt);
}

eExplicitFilter SettingsManager::GetExplicitFilter()
{
	return eExplicitFilter(
		m_pSettingsMessage
		->GetFieldObjectDefault<Protobuf::ObjectMessage>(Settings::FIELD_TEXT_AND_IMAGES)
		->GetFieldObjectDefault<Protobuf::ObjectMessage>(Settings::TextAndImages::FIELD_EXPLICIT_FILTER)
		->GetFieldObjectDefault<Protobuf::ObjectVarInt>(Settings::TextAndImages::ExplicitFilter::FIELD_VALUE)
		->GetValue()
	);
}

void SettingsManager::SetGuildDMBlocklist(const std::vector<Snowflake>& guilds)
{
	// pack Snowflake vector into a byte vector safely
	std::vector<uint8_t> data;
	if (!guilds.empty()) {
		data.resize(guilds.size() * sizeof(Snowflake));
		std::memcpy(data.data(), guilds.data(), data.size());
	}

	m_pSettingsMessage
		->GetFieldObjectDefault<Protobuf::ObjectMessage>(Settings::FIELD_PRIVACY)
		->GetFieldObjectDefault<Protobuf::ObjectBytes>(Settings::Privacy::FIELD_DM_BLOCK_GUILDS)
		->SetContent(data);

	m_pSettingsMessage
		->GetFieldObjectDefault<Protobuf::ObjectMessage>(Settings::FIELD_PRIVACY)
		->GetFieldObjectDefault<Protobuf::ObjectVarInt>(Settings::Privacy::FIELD_DM_BLOCK_DEFAULT)
		->MarkDirty();
}

void SettingsManager::GetGuildDMBlocklist(std::vector<Snowflake>& guilds)
{
	std::vector<uint8_t> data = m_pSettingsMessage
		->GetFieldObjectDefault<Protobuf::ObjectMessage>(Settings::FIELD_PRIVACY)
		->GetFieldObjectDefault<Protobuf::ObjectBytes>(Settings::Privacy::FIELD_DM_BLOCK_GUILDS)
		->GetContent();

	if (data.empty()) { guilds.clear(); return; }

	assert(data.size() % sizeof(Snowflake) == 0);
	size_t count = data.size() / sizeof(Snowflake);
	guilds.resize(count);

	// copy per-element to avoid unaligned reads on platforms that care
	for (size_t i = 0; i < count; ++i) {
		Snowflake sf = 0;
		std::memcpy(&sf, data.data() + i * sizeof(Snowflake), sizeof(Snowflake));
		guilds[i] = sf;
	}
}

void SettingsManager::SetDMBlockDefault(bool b)
{
	m_pSettingsMessage
		->GetFieldObjectDefault<Protobuf::ObjectMessage>(Settings::FIELD_PRIVACY)
		->GetFieldObjectDefault<Protobuf::ObjectVarInt>(Settings::Privacy::FIELD_DM_BLOCK_DEFAULT)
		->SetValue(b);

	m_pSettingsMessage
		->GetFieldObjectDefault<Protobuf::ObjectMessage>(Settings::FIELD_PRIVACY)
		->GetFieldObjectDefault<Protobuf::ObjectBytes>(Settings::Privacy::FIELD_DM_BLOCK_GUILDS)
		->MarkDirty();
}

bool SettingsManager::GetDMBlockDefault()
{
	return static_cast<bool>(m_pSettingsMessage
		->GetFieldObjectDefault<Protobuf::ObjectMessage>(Settings::FIELD_PRIVACY)
		->GetFieldObjectDefault<Protobuf::ObjectVarInt>(Settings::Privacy::FIELD_DM_BLOCK_DEFAULT)
		->GetValue());
}

void SettingsManager::SetMessageCompact(bool b)
{
	m_pSettingsMessage
		->GetFieldObjectDefault<Protobuf::ObjectMessage>(Settings::FIELD_TEXT_AND_IMAGES)
		->GetFieldObjectDefault<Protobuf::ObjectMessage>(Settings::TextAndImages::FIELD_DISPLAY_COMPACT)
		->GetFieldObjectDefault<Protobuf::ObjectVarInt>(Settings::TextAndImages::DisplayCompact::FIELD_VALUE)
		->SetValue(b);
}

bool SettingsManager::GetMessageCompact()
{
	return static_cast<bool>(m_pSettingsMessage
		->GetFieldObjectDefault<Protobuf::ObjectMessage>(Settings::FIELD_TEXT_AND_IMAGES)
		->GetFieldObjectDefault<Protobuf::ObjectMessage>(Settings::TextAndImages::FIELD_DISPLAY_COMPACT)
		->GetFieldObjectDefault<Protobuf::ObjectVarInt>(Settings::TextAndImages::DisplayCompact::FIELD_VALUE)
		->GetValue());
}

std::vector<Snowflake> SettingsManager::GetGuildFolders()
{
	std::vector<Snowflake> v;

	auto pgfRoot = m_pSettingsMessage->GetFieldObjectDefault<Protobuf::ObjectMessage>(Settings::FIELD_GUILD_FOLDERS);
	if (!pgfRoot) {
		DbgPrintF("No guild folder root");
		return v;
	}
	auto items = pgfRoot->GetFieldObjects(1);
	if (items.empty()) {
		DbgPrintF("Failed to fetch guild folder items.");
		return v;
	}

	for (auto& item : items)
	{
		auto pBytesBase = item->GetFieldObject(Settings::GuildFolders::Item::FIELD_GUILD_IDS);

		if (!pBytesBase) {
			DbgPrintF("Guild folders: No guilds!");
			continue;
		}
		if (!pBytesBase->IsByteArrayObject()) {
			DbgPrintF("Guild folders: Not a byte array!");
			continue;
		}

		auto pBytes = static_cast<Protobuf::ObjectBytes*>(pBytesBase);
		std::vector<uint8_t> content = pBytes->GetContent();
		if (content.size() % sizeof(Snowflake) != 0) {
			DbgPrintF("Guild folders: Not a list of 64-bit integers!");
			continue;
		}

		for (size_t i = 0; i < content.size(); i += sizeof(Snowflake))
		{
			Snowflake sf = 0;
			std::memcpy(&sf, content.data() + i, sizeof(Snowflake));
			v.push_back(sf);
		}
	}

	return v;
}

void SettingsManager::GetGuildFoldersEx(std::map<Snowflake, std::string>& folders, std::vector<std::pair<Snowflake, Snowflake>>& guilds)
{
	folders.clear();
	guilds.clear();

	auto pgfRoot = m_pSettingsMessage->GetFieldObjectDefault<Protobuf::ObjectMessage>(Settings::FIELD_GUILD_FOLDERS);
	if (!pgfRoot) {
		DbgPrintF("No guild folder root");
		return;
	}
	auto items = pgfRoot->GetFieldObjects(Settings::GuildFolders::FIELD_ITEMS);
	if (items.empty()) {
		DbgPrintF("Failed to fetch guild folder items.");
		return;
	}

	for (auto& item : items)
	{
		// note: I don't understand why this isn't just the fixed64/varint
		Snowflake folderId = 0;
		std::string folderName = "";
		auto pFolderIdPar = item->GetFieldObject(Settings::GuildFolders::Item::FIELD_ID);
		
		// note: if pFolderIdPar isn't present, that means that this is likely
		// just a folderless single guild or list thereof
		if (pFolderIdPar)
		{
			// Fetch its ID. I don't know why this is the encapsulation. Discord....
			auto pFolderId = pFolderIdPar->GetFieldObjectDefault<Protobuf::ObjectVarInt>(1);

			if (pFolderId) {
				folderId = pFolderId->GetValue();
			}
		}
		
		// Also check if its name exists.
		auto pFolderNamePar = item->GetFieldObject(Settings::GuildFolders::Item::FIELD_NAME);
		if (pFolderNamePar)
		{
			auto pFolderName = pFolderNamePar->GetFieldObjectDefault<Protobuf::ObjectString>(1);

			if (pFolderName) {
				folderName = pFolderName->GetContent();
			}
		}

		if (folderId) {
			folders[folderId] = folderName;
		}

		auto pBytesBase = item->GetFieldObject(Settings::GuildFolders::Item::FIELD_GUILD_IDS);

		if (!pBytesBase) {
			DbgPrintF("Guild folders: No guilds!");
			continue;
		}
		if (!pBytesBase->IsByteArrayObject()) {
			DbgPrintF("Guild folders: This ain't a byte array!");
			continue;
		}

		auto pBytes = static_cast<Protobuf::ObjectBytes*>(pBytesBase);
		std::vector<uint8_t> content = pBytes->GetContent();
		if (content.size() % sizeof(Snowflake) != 0) {
			DbgPrintF("Guild folders: Not a list of 64-bit integers!");
			continue;
		}

		for (size_t i = 0; i < content.size(); i += sizeof(Snowflake))
		{
			Snowflake sf = 0;
			std::memcpy(&sf, content.data() + i, sizeof(Snowflake));
			guilds.emplace_back(folderId, sf);
		}
	}
}

Protobuf::DecodeHint* SettingsManager::CreateHint()
{
	// Create a decode hint and use it
	using Protobuf::DecodeHint;
	auto pHint = new DecodeHint(DecodeHint::O_MESSAGE); // root

	DecodeHint* pFolderHint = pHint
		->AddChild(Settings::FIELD_GUILD_FOLDERS, DecodeHint::O_MESSAGE)
		->AddChild(Settings::GuildFolders::FIELD_ITEMS, DecodeHint::O_MESSAGE);

	pFolderHint->AddChild(Settings::GuildFolders::Item::FIELD_GUILD_IDS, DecodeHint::O_BYTES);
	pFolderHint->AddChild(Settings::GuildFolders::Item::FIELD_NAME, DecodeHint::O_MESSAGE);
	return pHint;
}

#include <boost/base64/base64.hpp>

void SettingsManager::LoadDataBase64(const std::string& userSettings)
{
	std::vector<uint8_t> buffer(base64::decoded_size(userSettings.size()));
	auto szPair = base64::decode(buffer.data(), userSettings.c_str(), userSettings.size());
	if (szPair.first > 0)
		GetSettingsManager()->LoadData(buffer.data(), szPair.first);
}
