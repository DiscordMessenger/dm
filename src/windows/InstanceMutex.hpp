#pragma once

#include <windows.h>

class InstanceMutex
{
private:
	HANDLE m_handle = NULL;

	void Close();

public:
	HRESULT Init();
	~InstanceMutex();
};
