#include "InstanceMutex.hpp"

void InstanceMutex::Close()
{
	if (m_handle)
	{
		CloseHandle(m_handle);
		m_handle = NULL;
	}
}

HRESULT InstanceMutex::Init()
{
#ifdef DISABLE_SINGLE_INSTANCE
	return 0;
#else

	SetLastError(NO_ERROR);
	m_handle = CreateMutex(NULL, TRUE, L"DiscordMessenger");

	const DWORD error = GetLastError();
	if (error == ERROR_ALREADY_EXISTS)
		Close();

	return error;
#endif
}

InstanceMutex::~InstanceMutex()
{
	Close();
}