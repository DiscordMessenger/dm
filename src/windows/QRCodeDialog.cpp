#include <openssl/ssl.h>
#include <openssl/rsa.h>
#include <nlohmann/json.h>
#include <boost/base64/base64.hpp>
#include "QRCodeDialog.hpp"
#include "Main.hpp"
#include "../discord/WebsocketClient.hpp"

// NOTE - This is unused, probably won't be finished

#define C_RSA_BITS     (2048)
#define C_RSA_EXPONENT RSA_F4 // Node.js default exponent value

using Json = nlohmann::json;

// singleton
static QRCodeDialog g_qcd;
QRCodeDialog* GetQRCodeDialog() { return &g_qcd; }

static std::string g_gatewayUrl = "wss://remote-auth-gateway.discord.gg/?v=2";

bool QRCodeDialog::IsShown() const
{
	return m_hwnd != NULL;
}

void QRCodeDialog::HandleGatewayClose(int code)
{
	MessageBox(m_hwnd, TEXT("Error, remote auth gateway was closed.  Trying to reconnect."), TmGetTString(IDS_PROGRAM_NAME), MB_ICONERROR | MB_OK);

	// try again
	m_gatewayId = -1;
	AttemptConnect();
}

void CheckError()
{
	const char *fl = NULL, *fn = NULL, *da = NULL;
	int ln = 0, fa = 0;
	unsigned long er = ERR_get_error_all(&fl, &ln, &fn, &da, &fa);

	DbgPrintW("OpenSSL error(%lx): %s:%d, %s, %s, %d", er, fl, ln, fn, da, fa);
	assert(!"OpenSSL error!");
}

void QRCodeDialog::HandleGatewayMessage(const std::string& payload)
{
	Json j = Json::parse(payload);
	std::string op = j["op"];

	if (op == "heartbeat")
	{
		Json j2;
		j2["op"] = "heartbeat_ack";
		GetWebsocketClient()->SendMsg(m_gatewayId, j2.dump());
	}
	else if (op == "hello")
	{
		// send server our public key
		Json j2;
		j2["op"] = "init";
		j2["encoded_public_key"] = m_publicKeyBase64;
		GetWebsocketClient()->SendMsg(m_gatewayId, j2.dump());
	}
	else if (op == "nonce_proof")
	{
		char* data = nullptr, *dataEnc = nullptr;
		uint8_t* decryptedData = nullptr;
		EVP_PKEY_CTX* ctx = NULL;

		// separate scope for this bitch due to the goto
		{
			Json j2;

			// server sent us an encrypted Proof of Identity payload, decrypt it using our private key
			// send the server OUR proof which is the SHA256 hash of the proof of the decrypted nonce
			std::string dataB64 = j["encrypted_nonce"];

			// Buffer management is kind of crap here (grimacing)
			data = new char[base64::decoded_size(dataB64.size()) + 5];
			auto decsz = base64::decode(data, dataB64.c_str(), dataB64.size());
			size_t dataSz = decsz.first;
			int res = 0;

			ctx = EVP_PKEY_CTX_new(m_pkey, NULL);

			res = EVP_PKEY_decrypt_init(ctx);
			if (res <= 0) goto NONCE_PROOF_ERROR_LBL;

			if (EVP_PKEY_CTX_ctrl_str(ctx, "rsa_padding_mode", "oaep") <= 0) goto NONCE_PROOF_ERROR_LBL;
			if (EVP_PKEY_CTX_ctrl_str(ctx, "rsa_oaep_md", "sha256") <= 0) goto NONCE_PROOF_ERROR_LBL;
			if (EVP_PKEY_CTX_ctrl_str(ctx, "rsa_mgf1_md", "sha256") <= 0) goto NONCE_PROOF_ERROR_LBL;

			decryptedData = NULL;
			size_t decryptedSize = 0;
			res = EVP_PKEY_decrypt(ctx, NULL, &decryptedSize, (const uint8_t*)data, dataSz);
			if (res <= 0) goto NONCE_PROOF_ERROR_LBL;

			decryptedData = new uint8_t[decryptedSize];
			res = EVP_PKEY_decrypt(ctx, decryptedData, &decryptedSize, (const uint8_t*)data, dataSz);
			if (res <= 0) goto NONCE_PROOF_ERROR_LBL;

			dataEnc = new char[base64::encoded_size(decryptedSize) + 5];
			size_t strLen = base64::encode(dataEnc, decryptedData, decryptedSize);
			std::string finalData(dataEnc, strLen);

			for (auto& c : finalData) {
				if (c == '+')
					c = '-';
				if (c == '/')
					c = '_';
			}
			while (!finalData.empty() && finalData[finalData.size() - 1] == '=')
				finalData.resize(finalData.size() - 1);

			j2["op"] = "nonce_proof";
			j2["nonce"] = finalData;
			GetWebsocketClient()->SendMsg(m_gatewayId, j2.dump());
		}

		goto GOOD_CLEANUP;
	NONCE_PROOF_ERROR_LBL:
		CheckError();
	GOOD_CLEANUP:
		EVP_PKEY_CTX_free(ctx);
		delete[] dataEnc;
		delete[] decryptedData;
		delete[] data;
	}
	else if (op == "pending_remote_init")
	{
		// server finally sent us the new QR fingerprint
		std::string str = j["fingerprint"];
		DbgPrintW("Got Fingerprint: %s", str.c_str());
	}
}

void QRCodeDialog::AttemptConnect()
{
	m_gatewayId = GetWebsocketClient()->Connect(g_gatewayUrl);
	if (m_gatewayId < 0) {
		MessageBox(m_hwnd, TmGetTString(IDS_LOGIN_NO_INTERNET), TmGetTString(IDS_PROGRAM_NAME), MB_ICONERROR | MB_OK);
	}
}

INT_PTR QRCodeDialog::OnMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_INITDIALOG:
		{
			// N.B. Using version 2 of the Remote Auth Gateway API.
			CreateRSAKey();
			AttemptConnect();
			break;
		}
		case WM_COMMAND:
		{
			if (wParam == IDCANCEL) {
				EndDialog(m_hwnd, IDCANCEL);
				return TRUE;
			}
			break;
		}
		case WM_DESTROY:
		{
			m_hwnd = NULL;

			// close the gateway
			GetWebsocketClient()->Close(m_gatewayId, websocketpp::close::status::normal);
			m_gatewayId = -1;

			// trash the RSA keys
			OnDestroy();

			break;
		}
	}

	return FALSE;
}

INT_PTR CALLBACK QRCodeDialog::OnMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	GetQRCodeDialog()->m_hwnd = hWnd;
	return GetQRCodeDialog()->OnMessage(uMsg, wParam, lParam);
}

void QRCodeDialog::OnDestroy()
{
	if (m_pkey)
		EVP_PKEY_free(m_pkey);
	m_pkey = NULL;
}

void QRCodeDialog::CreateRSAKey()
{
	if (RAND_status() == 0) {
		DbgPrintW("QRCodeDialog: OpenSSL random is not seeded!!");
		assert(!"OpenSSL rand is not seeded!");
		return;
	}

	EVP_PKEY* pkey = nullptr;
	BUF_MEM* bptr = NULL;
	char* othdata = NULL;
	size_t encsz = 0;

	BIO *bio1 = NULL, *bio_der = NULL;
	BIGNUM* bn = NULL;

	// Generate RSA key pair
	EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
	if (!ctx) goto ERROR_LBL;

	if (EVP_PKEY_keygen_init(ctx) <= 0 ||
		EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, C_RSA_BITS) <= 0 ||
		EVP_PKEY_keygen(ctx, &pkey) <= 0)
		goto ERROR_LBL;

	m_pkey = pkey;
	if (!pkey) goto ERROR_LBL;

	// Export public key to DER format
	bio_der = BIO_new(BIO_s_mem());
	if (!bio_der) goto ERROR_LBL;
	if (i2d_PUBKEY_bio(bio_der, pkey) <= 0) goto ERROR_LBL;

	// Encode in base64
	BIO_get_mem_ptr(bio_der, &bptr);
	if (!bptr) goto ERROR_LBL;

	othdata = new char[base64::encoded_size(bptr->length) + 6];
	encsz = base64::encode(othdata, bptr->data, bptr->length);
	m_publicKeyBase64 = std::string(othdata, encsz);

	DbgPrintW("QRCodeDialog: Generated RSA key, encoded pkey is %s", m_publicKeyBase64.c_str());

	goto CLEANUPOK;

ERROR_LBL:
	CheckError();
	assert(!"Had to go cleanup because fail!");
	if (m_pkey) EVP_PKEY_free(m_pkey);
	m_pkey = NULL;
	m_publicKeyBase64 = "";

CLEANUPOK:
	if (ctx)     EVP_PKEY_CTX_free(ctx);
	if (bn)      BN_free(bn);
	if (bio1)    BIO_free_all(bio1);
	if (bio_der) BIO_free_all(bio_der);
}

void QRCodeDialog::Show()
{
	DialogBox(g_hInstance, MAKEINTRESOURCE(IDD_DIALOG_QRCODELOGIN), g_Hwnd, &QRCodeDialog::OnMessage);
}
