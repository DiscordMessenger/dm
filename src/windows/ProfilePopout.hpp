#pragma once

#include "Main.hpp"

struct ShowProfilePopoutParams
{
	POINT m_pt;
	Snowflake m_user;
	Snowflake m_guild;
	bool m_bRightJustify;
};

void DeferredShowProfilePopout(const ShowProfilePopoutParams& params);

void ShowProfilePopout(Snowflake user, Snowflake guild, int x, int y, bool rightJustify = false);

void DismissProfilePopout();

Snowflake GetProfilePopoutUser();

void UpdateProfilePopout();
