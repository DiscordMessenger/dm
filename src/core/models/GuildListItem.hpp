#pragma once

#include <string>
#include <list>
#include "./Snowflake.hpp"
#include "../utils/Util.hpp"

class AbstractGuildItem
{
public:
	virtual ~AbstractGuildItem() {}

	// Gets the name of this guild item. Used by the windows frontend for tooltips.
	virtual std::string GetName() {
		return m_name;
	}

	// Whether this guild item represents a folder.
	virtual bool IsFolder() = 0;

	// If this is a folder, gets a list of its sub items.
	virtual std::list<AbstractGuildItem*>* GetItems() = 0;

	// Get this guild item's avatar icon.
	virtual std::string GetAvatar() = 0;

	// Get this guild item's ID.
	// For guild folders, this is the ID of the folder. For guilds, this is the ID of the guild.
	virtual Snowflake GetID() { return m_id; }

	// Erase a guild if this is a folder.
	virtual void EraseGuild(Snowflake sf) { }

	// Check if this guild folder contains an item.
	virtual bool ContainsGuild(Snowflake sf) { return false; }

protected:
	friend class FolderGuildItem;
	friend class GuildGuildItem;

	std::string m_name;
	Snowflake m_id;
};

class FolderGuildItem : public AbstractGuildItem
{
public:
	~FolderGuildItem() {
		for (auto& item : m_items)
			delete item;
	}

	FolderGuildItem(Snowflake id, const std::string& name) {
		m_id = id;
		m_name = name;
	}

	bool IsFolder() override { return true; }

	std::list<AbstractGuildItem*>* GetItems() override { return &m_items; }

	std::string GetAvatar() { return ""; }

	void EraseGuild(Snowflake guildId) override
	{
		for (auto iter = m_items.begin(); iter != m_items.end(); ++iter)
		{
			auto item = *iter;
			if (item->GetID() == guildId)
			{
				m_items.erase(iter);
				return;
			}
			else if (item->IsFolder())
			{
				item->EraseGuild(guildId);
			}
		}
	}

	bool ContainsGuild(Snowflake sf)
	{
		for (auto& item : m_items)
		{
			if (item->GetID() == sf)
				return true;

			if (item->ContainsGuild(sf))
				return true;
		}

		return false;
	}

private:
	std::list<AbstractGuildItem*> m_items;
};

class GuildGuildItem : public AbstractGuildItem
{
public:
	bool IsFolder() override { return false; }

	std::list<AbstractGuildItem*>* GetItems() override { return nullptr; }

	std::string GetAvatar() { return m_avatar; }

	GuildGuildItem(Snowflake guild, const std::string& name, const std::string& avatar) {
		m_name = name;
		m_avatar = avatar;
		m_id = guild;
	}

private:
	std::string m_avatar;
};

class GuildItemList
{
public:
	~GuildItemList() {
		Clear();
	}

	GuildItemList() {}
	
	GuildItemList(GuildItemList&& moved) {
		m_items = std::move(moved.m_items);
		moved.m_items.clear();
	}

	// Check if the guilds' order is the same.
	bool CompareOrder(GuildItemList& other) {
		// TODO
		return false;
	}

	void Clear() {
		for (auto& item : m_items)
			delete item;
	}

	void AddFolder(Snowflake folderId, const std::string& name)
	{
		// TODO: If needed, support folder depths >1? Discord doesn't (yet)
		m_items.push_back(new FolderGuildItem(folderId, name));
	}

	bool ContainsGuild(Snowflake snowflake)
	{
		for (auto& gi : m_items)
		{
			if (gi->GetID() == snowflake)
				return true;

			if (gi->ContainsGuild(snowflake))
				return true;
		}

		return false;
	}

	void Dump()
	{
		// Prints all of the guilds and guild folders.
		for (auto& gi : m_items)
		{
			if (gi->IsFolder())
			{
				DbgPrintF("Folder %s (%lld)", gi->GetName().c_str(), gi->GetID());

				auto items = gi->GetItems();
				for (auto& item : *items) {
					DbgPrintF("\tGuild  %s (%lld)", item->GetName().c_str(), item->GetID());
				}
			}
			else {
				DbgPrintF("Guild  %s (%lld)", gi->GetName().c_str(), gi->GetID());
			}
		}
	}

	void AddGuild(Snowflake folderId, Snowflake guildId, const std::string& name, const std::string& avatar)
	{
		// TODO: If needed, support folder depths >1? Discord doesn't (yet)
		if (folderId != 0)
		{
			for (auto& item : m_items)
			{
				if (item->GetID() == folderId) {
					item->GetItems()->push_back(new GuildGuildItem(guildId, name, avatar));
					return;
				}
			}

			DbgPrintF("No folder with id %lld, adding guild %lld to root.", folderId, guildId);
		}

		// Add it to the root.
		m_items.push_back(new GuildGuildItem(guildId, name, avatar));
	}

	void EraseGuild(Snowflake guildId)
	{
		for (auto iter = m_items.begin(); iter != m_items.end(); ++iter)
		{
			auto item = *iter;
			if (item->GetID() == guildId)
			{
				m_items.erase(iter);
				return;
			}
			else if (item->IsFolder())
			{
				item->EraseGuild(guildId);
			}
		}
	}

	std::list<AbstractGuildItem*>* GetItems()
	{
		return &m_items;
	}

private:
	std::list<AbstractGuildItem*> m_items;
};
