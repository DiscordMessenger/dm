#include "SettingsManager.hpp"
#include "Frontend.hpp"
#include "DiscordInstance.hpp"

static SettingsManager g_SMSingleton;
SettingsManager* GetSettingsManager()
{
	return &g_SMSingleton;
}

SettingsManager::SettingsManager()
{
	m_pSettingsMessage = new Protobuf::ObjectBaseMessage;
}

SettingsManager::~SettingsManager()
{
	delete m_pSettingsMessage;
}

void SettingsManager::LoadData(const uint8_t* data, size_t sz)
{
	using namespace Protobuf;
	ObjectBaseMessage* pMsg = new ObjectBaseMessage;
	pMsg->SetDecodingHint(CreateHint()); // gonna get deleted below

	try
	{
		DecodeBlock(data, sz, pMsg);
		m_pSettingsMessage->MergeWith(pMsg);
		m_pSettingsMessage->MarkClean();

		delete pMsg;
	}
	catch (Protobuf::ErrorCode code)
	{
		delete pMsg;
		GetFrontend()->OnProtobufError(code);
	}
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

	const char* possibleUpdates[] = {
		"online",
		"idle",
		"dnd",
		"invisible",
	};
	assert(status >= 0 && status <= _countof(possibleUpdates));
	std::string state(possibleUpdates[int(status)]);

	if (pState->GetContent() != state)
	{
		pState->SetContent(state);

		// mark dirty so it's still sent,  so that it doesn't accidentally remove your status message
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
	if (text.empty() && emoji.empty())
	{
		using namespace Protobuf;

		// delete that sucker
		ObjectMessage* pActivity = m_pSettingsMessage
			->GetFieldObjectDefault<ObjectMessage>(Settings::FIELD_ACTIVITY);

		pActivity->EraseField(Settings::Activity::FIELD_CUSTOM_STATUS);
	}
	else
	{
		using namespace Protobuf;

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

		// so it gets pushed too alongside the update
		m_pSettingsMessage
			->GetFieldObjectDefault<ObjectMessage>(Settings::FIELD_ACTIVITY)
			->GetFieldObjectDefault<ObjectString>(Settings::Activity::FIELD_INDICATOR)
			->MarkDirty();
	}
}

std::string SettingsManager::GetCustomStatusText()
{
	Protobuf::ObjectBase* pObject = m_pSettingsMessage
		->GetFieldObjectDefault<Protobuf::ObjectMessage>(Settings::FIELD_ACTIVITY)
		->GetFieldObjectDefault<Protobuf::ObjectMessage>(Settings::Activity::FIELD_CUSTOM_STATUS)
		->GetFieldObject(Settings::Activity::CustomStatus::FIELD_TEXT);

	if (!pObject)
		return "";

	if (!pObject->IsStringObject())
		return ""; // failed

	return dynamic_cast<Protobuf::ObjectString*>(pObject)->GetContent();
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
	return
		m_pSettingsMessage
		->GetFieldObjectDefault<Protobuf::ObjectMessage>(Settings::FIELD_ACTIVITY)
		->GetFieldObjectDefault<Protobuf::ObjectMessage>(Settings::Activity::FIELD_CUSTOM_STATUS)
		->GetFieldObjectDefault<Protobuf::ObjectFixed64>(Settings::Activity::CustomStatus::FIELD_EXPIRY)
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
	// XXX: big endian support?
	std::vector<uint8_t> data;
	data.resize(guilds.size() * sizeof(Snowflake));
	memcpy(data.data(), guilds.data(), data.size());

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
	// XXX: big endian support?
	std::vector<uint8_t> data = m_pSettingsMessage
		->GetFieldObjectDefault<Protobuf::ObjectMessage>(Settings::FIELD_PRIVACY)
		->GetFieldObjectDefault<Protobuf::ObjectBytes>(Settings::Privacy::FIELD_DM_BLOCK_GUILDS)
		->GetContent();

	assert(data.size() % sizeof(Snowflake) == 0);
	guilds.resize(data.size() / sizeof(Snowflake));
	memcpy(guilds.data(), data.data(), data.size());
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
	return (bool)m_pSettingsMessage
		->GetFieldObjectDefault<Protobuf::ObjectMessage>(Settings::FIELD_PRIVACY)
		->GetFieldObjectDefault<Protobuf::ObjectVarInt>(Settings::Privacy::FIELD_DM_BLOCK_DEFAULT)
		->GetValue();
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
	return (bool)m_pSettingsMessage
		->GetFieldObjectDefault<Protobuf::ObjectMessage>(Settings::FIELD_TEXT_AND_IMAGES)
		->GetFieldObjectDefault<Protobuf::ObjectMessage>(Settings::TextAndImages::FIELD_DISPLAY_COMPACT)
		->GetFieldObjectDefault<Protobuf::ObjectVarInt>(Settings::TextAndImages::DisplayCompact::FIELD_VALUE)
		->GetValue();
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
	if (!items) {
		DbgPrintF("Failed to fetch guild folder items.");
		return v;
	}

	for (auto& item : *items)
	{
		auto pBytesBase = item->GetFieldObject(Settings::GuildFolders::Item::FIELD_GUILD_IDS);

		// TODO: Don't support guild folders OR reordering right now, so this will do.
		if (!pBytesBase) {
			DbgPrintF("Guild folders: No guilds!");
			continue;
		}
		if (!pBytesBase->IsByteArrayObject()) {
			DbgPrintF("Guild folders: This ain't a byte array!");
			continue;
		}

		auto pBytes = reinterpret_cast<Protobuf::ObjectBytes*>(pBytesBase);
		std::vector<uint8_t> content = pBytes->GetContent();
		if (content.size() % 8 != 0) {
			DbgPrintF("Guild folders: This ain't a list of 64-bit integers!");
			continue;
		}

		for (size_t i = 0; i < content.size(); i += 8)
		{
			Snowflake sf = *reinterpret_cast<Snowflake*>(content.data() + i);
			v.push_back(sf);
		}
	}

	return v;
}

Protobuf::DecodeHint* SettingsManager::CreateHint()
{
	// Create a decode hint and use it
	using Protobuf::DecodeHint;
	DecodeHint* pHint = new DecodeHint(DecodeHint::O_MESSAGE); // root

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
	uint8_t* pData = new uint8_t[base64::decoded_size(userSettings.size())];
	auto szPair = base64::decode(pData, userSettings.c_str(), userSettings.size());
	GetSettingsManager()->LoadData(pData, szPair.first);
	delete[] pData;
}
