#include "MessagePoll.hpp"
#include "../utils/Util.hpp"

MessagePoll::MessagePoll(const nlohmann::json& j)
{
	// Encapsulating JSON decomposition in a try-catch block because
	// I fear exceptions inside of constructors.
	try
	{
		m_bAllowMultiselect = j["allow_multiselect"];
		m_expiry = ParseTime(j["expiry"]);

		if (j.contains("question")) {
			m_question = GetFieldSafe(j["question"], "text");
		}

		if (j.contains("answers") && j["answers"].is_array()) {
			for (const auto& ans : j["answers"]) {
				MessagePollOption mpo;
				mpo.m_answerId = GetFieldSafeInt(j, "answer_id");

				if (!ans.contains("poll_media")) continue;
				const auto& pollMedia = ans["poll_media"];
				if (!pollMedia.is_object()) continue;

				mpo.m_text = GetFieldSafe(pollMedia, "text");
				if (pollMedia.contains("emoji")) {
					const auto& emoji = pollMedia["emoji"];
					if (emoji.is_object()) {
						mpo.m_emojiSF = GetSnowflake(emoji, "id");
						mpo.m_emojiUTF8 = GetFieldSafe(emoji, "name");
					}
				}

				m_options[mpo.m_answerId] = mpo;
			}
		}

		if (!j.contains("results")) return;
		const auto& results = j["results"];
		if (!results.is_object()) return;

		if (!results.contains("answer_counts")) return;
		const auto& answerCounts = results["answer_counts"];
		m_bIsFinalized = GetFieldSafeBool(results, "is_finalized", false);

		if (!answerCounts.is_array()) return;
		for (const auto& ac : answerCounts) {
			int id = GetFieldSafeInt(ac, "id");
			int count = GetFieldSafeInt(ac, "count");
			bool meVoted = GetFieldSafeBool(ac, "me_voted", false);

			auto& mpo = m_options[id];
			// reset, perhaps the entry was missing? maybe dont need to handle corrupt poll data
			mpo.m_answerId = id;
			mpo.m_bMeVoted = meVoted;
			mpo.m_voteCount = count;
		}
	}
	catch (...)
	{
		DbgPrintF("MessagePoll constructor: Json parsing threw an exception!");
	}

	DbgPrintF("yo");
}

MessagePoll::MessagePoll(const MessagePoll& oth) :
	m_options (oth.m_options),
	m_question(oth.m_question),
	m_expiry  (oth.m_expiry),
	m_bIsFinalized(oth.m_bIsFinalized),
	m_bAllowMultiselect(oth.m_bAllowMultiselect)
{
}
