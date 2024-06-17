#pragma once
#include <string>
#include "Snowflake.hpp"
#include "ContentType.hpp"

class Attachment
{
	enum {
		// The attachment was created using the remix tool on mobile
		IS_REMIX = 1 << 2,
	};

public:
	Snowflake m_id = 0;
	int m_width = 0;
	int m_height = 0;
	int m_previewWidth = 0;
	int m_previewHeight = 0;
	int m_size = 0;
	int m_flags = 0;
	std::string m_fileName = "";
	std::string m_proxyUrl = "";
	std::string m_actualUrl = "";
	ContentType::eType m_contentType = ContentType::BLOB;

public:
	void Load(nlohmann::json& j);

	void UpdatePreviewSize()
	{
		m_previewWidth = m_width;
		m_previewHeight = m_height;
		int maxWidth  = 300; // XXX hardcoded
		int maxHeight = 300; // XXX hardcoded
		if (m_previewHeight > maxHeight) {
			m_previewWidth = m_previewWidth * maxHeight / m_previewHeight;
			m_previewHeight = maxHeight;
		}
		if (m_previewWidth > maxWidth) {
			m_previewHeight = m_previewHeight * maxWidth / m_previewWidth;
			m_previewWidth = maxWidth;
		}
	}

	bool PreviewDifferent() const
	{
		return m_previewWidth != m_width || m_previewHeight != m_height;
	}

	bool IsImage() const
	{
		return
			m_contentType == ContentType::PNG ||
			m_contentType == ContentType::GIF ||
			m_contentType == ContentType::JPEG ||
			m_contentType == ContentType::WEBP;
	}
};
