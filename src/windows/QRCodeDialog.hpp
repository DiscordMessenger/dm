#pragma once

#define WIN32_LEAN_AND_MEAN
#include "windows.h"

#include <openssl/rsa.h>

class QRCodeDialog
{
public:
	void Show();
	bool IsShown() const;
	void HandleGatewayClose(int code);
	void HandleGatewayMessage(const std::string& payload);

	int GetGatewayID() const {
		return m_gatewayId;
	}

private:
	int m_gatewayId = -1;
	HWND m_hwnd = NULL;

	void AttemptConnect();
	INT_PTR OnMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
	static INT_PTR CALLBACK OnMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	void OnDestroy();
	void CreateRSAKey();

private:
	EVP_PKEY* m_pkey = NULL;
	std::string m_publicKeyBase64 = "iProgram's a doofus if you see this while running";
};

QRCodeDialog* GetQRCodeDialog();
