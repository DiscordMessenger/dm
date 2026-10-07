#include "QrLogin.hpp"

#include <atomic>
#include <cstring>
#include <thread>
#include <vector>

#include <Xm/DialogS.h>
#include <Xm/DrawingA.h>
#include <Xm/Form.h>
#include <Xm/Label.h>
#include <Xm/PushB.h>
#include <Xm/Separator.h>

#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>

#include <boost/base64/base64.hpp>
#include <nlohmann/json.h>
#include <qrcodegen/qrcodegen.h>

#include "network/DiscordAPI.hpp"
#include "network/HTTPClient.hpp"
#include "network/WebsocketClient.hpp"
#include "utils/Util.hpp"
#include "posix/MainQueue.hpp"
#include "Fonts.hpp"
#include "Theme.hpp"

int AddVisualArgs(Arg* args, int n); // Main.cpp

using Json = nlohmann::json;

namespace
{
	const char* const GATEWAY_URL = "wss://remote-auth-gateway.discord.gg/?v=2";
	const int QR_AREA = 300;

	struct State
	{
		Widget shell = nullptr, area = nullptr, status = nullptr;
		const PixelFormat* fmt = nullptr;
		GC gc = nullptr;
		Canvas canvas;
		std::function<void(const std::string&)> done;
		std::function<void()> useToken;

		EVP_PKEY* key = nullptr;
		std::string publicKey; // base64 SubjectPublicKeyInfo (DER)
		XtIntervalId heartbeat = 0;
		int heartbeatMs = 0;
		std::vector<uint8_t> qr; // qrcodegen's buffer; empty until a code arrives
		bool waitingForPhone = false;
		int generation = 0;   // bumped by every new dialog
	};

	State* g_state;
	std::atomic<int> g_gateway(-1);
	int g_generation = 0;

	void SetStatus(const std::string& text)
	{
		if (!g_state)
			return;
		XmString xs = XmStringCreateLtoR((char*) Utf8ToLatin1(text).c_str(), (char*) XmFONTLIST_DEFAULT_TAG);
		XtVaSetValues(g_state->status, XmNlabelString, xs, NULL);
		XmStringFree(xs);
	}

	void Paint()
	{
		State* s = g_state;
		if (!s || !XtIsRealized(s->area))
			return;
		Display* dpy = XtDisplay(s->area);
		Window win = XtWindow(s->area);
		if (!s->gc)
			s->gc = XCreateGC(dpy, win, 0, NULL);
		Dimension w = 0, h = 0;
		XtVaGetValues(s->area, XmNwidth, &w, XmNheight, &h, NULL);
		if (s->canvas.Width() != w || s->canvas.Height() != h)
			s->canvas.Resize(w, h);

		Canvas& c = s->canvas;
		c.Fill(0, 0, w, h, 0xffffff);
		if (!s->qr.empty())
		{
			int n = qrcodegen_getSize(s->qr.data());
			int quiet = 4;
			int scale = std::max(1, std::min((int) w, (int) h) / (n + 2 * quiet));
			int x0 = ((int) w - n * scale) / 2, y0 = ((int) h - n * scale) / 2;
			for (int y = 0; y < n; y++)
				for (int x = 0; x < n; x++)
					if (qrcodegen_getModule(s->qr.data(), x, y))
						c.Fill(x0 + x * scale, y0 + y * scale, scale, scale, 0x000000);
			if (s->waitingForPhone) {
				// scanned: grey the code out, as discord.com does
				for (int y = 0; y < (int) h; y++)
					for (int x = 0; x < (int) w; x++)
						if ((x + y) % 3)
							c.Fill(x, y, 1, 1, 0xffffff);
			}
		}
		else
		{
			const char* text = "Preparing a code\xe2\x80\xa6";
			int tw = Fonts::Measure(text, FS_ITALIC, 14);
			Fonts::Draw(c, ((int) w - tw) / 2, (int) h / 2, text, FS_ITALIC, 14, 0x606060);
		}
		c.Present(*s->fmt, win, s->gc, 0, 0, w, h, 0, 0);
	}

	void ExposeCB(Widget, XtPointer, XtPointer)
	{
		Paint();
	}

	std::string Base64(const uint8_t* data, size_t n, bool url)
	{
		std::string out(base64::encoded_size(n), '\0');
		out.resize(base64::encode(&out[0], data, n));
		if (url) {
			for (auto& ch : out) {
				if (ch == '+') ch = '-';
				else if (ch == '/') ch = '_';
			}
			while (!out.empty() && out.back() == '=')
				out.pop_back();
		}
		return out;
	}

	std::vector<uint8_t> Unbase64(const std::string& s)
	{
		std::vector<uint8_t> out(base64::decoded_size(s.size()) + 4);
		auto r = base64::decode(out.data(), s.c_str(), s.size());
		out.resize(r.first);
		return out;
	}

	// RSA-OAEP with SHA-256, as the remote-auth gateway encrypts.
	bool Decrypt(EVP_PKEY* key, const std::string& b64, std::vector<uint8_t>& out)
	{
		std::vector<uint8_t> in = Unbase64(b64);
		EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(key, NULL);
		bool ok = ctx && EVP_PKEY_decrypt_init(ctx) > 0 &&
			EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING) > 0 &&
			EVP_PKEY_CTX_set_rsa_oaep_md(ctx, EVP_sha256()) > 0 &&
			EVP_PKEY_CTX_set_rsa_mgf1_md(ctx, EVP_sha256()) > 0;
		size_t n = 0;
		ok = ok && EVP_PKEY_decrypt(ctx, NULL, &n, in.data(), in.size()) > 0;
		if (ok) {
			out.resize(n);
			ok = EVP_PKEY_decrypt(ctx, out.data(), &n, in.data(), in.size()) > 0;
			out.resize(ok ? n : 0);
		}
		EVP_PKEY_CTX_free(ctx);
		return ok;
	}

	// A 2048-bit RSA key and its public half as base64 DER.  Slow on an old
	// CPU: runs on its own thread.
	EVP_PKEY* MakeKey(std::string& publicKey)
	{
		EVP_PKEY* key = nullptr;
		EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
		if (!ctx || EVP_PKEY_keygen_init(ctx) <= 0 ||
			EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 2048) <= 0 ||
			EVP_PKEY_keygen(ctx, &key) <= 0)
			key = nullptr;
		EVP_PKEY_CTX_free(ctx);
		if (!key)
			return nullptr;

		unsigned char* der = nullptr;
		int len = i2d_PUBKEY(key, &der);
		if (len <= 0) {
			EVP_PKEY_free(key);
			return nullptr;
		}
		publicKey = Base64(der, (size_t) len, false);
		OPENSSL_free(der);
		return key;
	}

	void Send(const Json& j)
	{
		int id = g_gateway;
		if (id >= 0)
			GetWebsocketClient()->SendMsg(id, j.dump());
	}

	void StopHeartbeat()
	{
		if (g_state && g_state->heartbeat) {
			XtRemoveTimeOut(g_state->heartbeat);
			g_state->heartbeat = 0;
		}
	}

	void HeartbeatCB(XtPointer, XtIntervalId*)
	{
		if (!g_state)
			return;
		g_state->heartbeat = XtAppAddTimeOut(XtWidgetToApplicationContext(g_state->shell),
			g_state->heartbeatMs, HeartbeatCB, NULL);
		Json j;
		j["op"] = "heartbeat";
		Send(j);
	}

	void CloseGateway()
	{
		StopHeartbeat();
		int id = g_gateway.exchange(-1);
		if (id >= 0)
			GetWebsocketClient()->Close(id, websocketpp::close::status::normal);
	}

	void Connect()
	{
		if (!g_state)
			return;
		g_state->qr.clear();
		g_state->waitingForPhone = false;
		Paint();
		SetStatus("Connecting to Discord\xe2\x80\xa6");
		int id = GetWebsocketClient()->Connect(GATEWAY_URL);
		g_gateway = id;
		if (id < 0)
			SetStatus("Could not reach Discord's login service.  Check the network, then try again.");
	}

	void ReconnectCB(XtPointer client, XtIntervalId*)
	{
		if (g_state && g_state->generation == (int) (long) client)
			Connect();
	}

	void ReconnectSoon(int ms)
	{
		CloseGateway();
		XtAppAddTimeOut(XtWidgetToApplicationContext(g_state->shell), ms, ReconnectCB, (XtPointer) (long) g_state->generation);
	}

	void Finish(const std::string& token, bool wantToken)
	{
		State* s = g_state;
		if (!s)
			return;
		g_state = nullptr;
		int id = g_gateway.exchange(-1);
		if (id >= 0)
			GetWebsocketClient()->Close(id, websocketpp::close::status::normal);
		if (s->heartbeat)
			XtRemoveTimeOut(s->heartbeat);
		if (s->key)
			EVP_PKEY_free(s->key);
		if (s->gc)
			XFreeGC(XtDisplay(s->area), s->gc);
		XtDestroyWidget(s->shell);
		auto done = s->done;
		auto useToken = s->useToken;
		delete s;
		if (wantToken)
			useToken();
		else
			done(token);
	}

	// The exchange of the ticket for the token comes back here (from the
	// HTTP thread, through the UI thread).
	void TicketResponse(NetRequest* req)
	{
		int result = req->result;
		std::string response = req->response;
		MainQueue::Post([result, response] {
			if (!g_state)
				return;
			if (result == 200) {
				try {
					Json j = Json::parse(response);
					std::vector<uint8_t> token;
					if (j.contains("encrypted_token") && Decrypt(g_state->key, j["encrypted_token"], token)) {
						Finish(std::string(token.begin(), token.end()), false);
						return;
					}
				}
				catch (...) {}
				SetStatus("Discord's answer could not be read.  Try again, or log in with a token.");
			}
			else if (response.find("captcha") != std::string::npos) {
				SetStatus("Discord wants a captcha for this login, which Discord Messenger cannot show.\nLog in with a token instead.");
			}
			else {
				SetStatus("Discord refused the login (" + std::to_string(result) + ").  Try again, or log in with a token.");
			}
			ReconnectSoon(4000);
		});
	}

	void QuitCB(Widget, XtPointer, XtPointer)
	{
		Finish("", false);
	}

	void UseTokenCB(Widget, XtPointer, XtPointer)
	{
		Finish("", true);
	}

	Widget MakeLabel(Widget form, const char* name, const char* text, Widget above, int offset)
	{
		XmString xs = XmStringCreateLtoR((char*) text, (char*) XmFONTLIST_DEFAULT_TAG);
		Widget w = XtVaCreateManagedWidget(name, xmLabelWidgetClass, form,
			XmNlabelString, xs,
			XmNalignment, XmALIGNMENT_BEGINNING,
			XmNtopAttachment, above ? XmATTACH_WIDGET : XmATTACH_FORM,
			XmNtopWidget, above,
			XmNleftAttachment, XmATTACH_FORM,
			XmNrightAttachment, XmATTACH_FORM,
			XmNtopOffset, offset,
			XmNleftOffset, 12,
			XmNrightOffset, 12,
			NULL);
		XmStringFree(xs);
		return w;
	}
}

int QrLogin::GatewayId()
{
	return g_gateway;
}

void QrLogin::OnGatewayMessage(const std::string& payload)
{
	State* s = g_state;
	if (!s)
		return;

	Json j;
	try {
		j = Json::parse(payload);
	}
	catch (...) {
		return;
	}
	std::string op = j.value("op", "");

	if (op == "hello")
	{
		s->heartbeatMs = j.value("heartbeat_interval", 41250);
		StopHeartbeat();
		s->heartbeat = XtAppAddTimeOut(XtWidgetToApplicationContext(s->shell), s->heartbeatMs, HeartbeatCB, NULL);
		Json init;
		init["op"] = "init";
		init["encoded_public_key"] = s->publicKey;
		Send(init);
	}
	else if (op == "nonce_proof")
	{
		// prove we hold the key: the SHA-256 of the decrypted nonce
		std::vector<uint8_t> nonce;
		if (!Decrypt(s->key, j.value("encrypted_nonce", ""), nonce)) {
			SetStatus("The login service's challenge could not be decrypted.");
			return;
		}
		unsigned char digest[32];
		unsigned int dlen = 0;
		EVP_Digest(nonce.data(), nonce.size(), digest, &dlen, EVP_sha256(), NULL);
		Json proof;
		proof["op"] = "nonce_proof";
		proof["proof"] = Base64(digest, dlen, true);
		Send(proof);
	}
	else if (op == "pending_remote_init")
	{
		std::string url = "https://discord.com/ra/" + j.value("fingerprint", "");
		std::vector<uint8_t> temp(qrcodegen_BUFFER_LEN_MAX);
		s->qr.assign(qrcodegen_BUFFER_LEN_MAX, 0);
		if (!qrcodegen_encodeText(url.c_str(), temp.data(), s->qr.data(), qrcodegen_Ecc_LOW,
				qrcodegen_VERSION_MIN, qrcodegen_VERSION_MAX, qrcodegen_Mask_AUTO, true))
			s->qr.clear();
		s->waitingForPhone = false;
		Paint();
		SetStatus("Scan this code with the Discord app on your phone:\n"
			"tap your avatar, then Scan QR Code.");
	}
	else if (op == "pending_ticket")
	{
		// "id:discriminator:avatar:username"
		std::vector<uint8_t> user;
		std::string name = "your phone";
		if (Decrypt(s->key, j.value("encrypted_user_payload", ""), user)) {
			std::string u(user.begin(), user.end());
			size_t p = u.find(':');
			p = p == std::string::npos ? p : u.find(':', p + 1);
			p = p == std::string::npos ? p : u.find(':', p + 1);
			if (p != std::string::npos)
				name = u.substr(p + 1);
		}
		s->waitingForPhone = true;
		Paint();
		SetStatus("Scanned by " + name + ".\nConfirm the login on your phone.");
	}
	else if (op == "pending_login")
	{
		SetStatus("Logging in\xe2\x80\xa6");
		Json body;
		body["ticket"] = j.value("ticket", "");
		GetHTTPClient()->PerformRequest(
			true,
			NetRequest::POST_JSON,
			GetDiscordAPI() + "users/@me/remote-auth/login",
			0,
			0,
			body.dump(),
			"",
			"",
			TicketResponse
		);
	}
	else if (op == "cancel")
	{
		SetStatus("The login was cancelled on the phone.  Getting a new code\xe2\x80\xa6");
		ReconnectSoon(1500);
	}
}

void QrLogin::OnGatewayClosed(int code, const std::string& reason)
{
	if (!g_state)
		return;
	g_gateway = -1;
	StopHeartbeat();
	// codes last a couple of minutes: get a new one
	SetStatus("The code expired.  Getting a new one\xe2\x80\xa6");
	XtAppAddTimeOut(XtWidgetToApplicationContext(g_state->shell), 1500, ReconnectCB, (XtPointer) (long) g_state->generation);
}

void QrLogin::Show(Widget parent, const PixelFormat& fmt, const std::string& message,
	std::function<void(const std::string&)> done, std::function<void()> useToken)
{
	if (g_state)
		return;
	State* s = new State;
	g_state = s;
	s->fmt = &fmt;
	s->done = done;
	s->useToken = useToken;
	s->generation = ++g_generation;

	Arg args[8];
	int n = 0;
	XtSetArg(args[n], XmNtitle, "Log in to Discord"); n++;
	XtSetArg(args[n], XmNdeleteResponse, XmDO_NOTHING); n++;
	n = AddVisualArgs(args, n);
	s->shell = XmCreateDialogShell(parent, (char*) "qrlogin", args, n);

	Widget form = XtVaCreateWidget("form", xmFormWidgetClass, s->shell,
		XmNdialogStyle, XmDIALOG_FULL_APPLICATION_MODAL,
		XmNautoUnmanage, False,
		NULL);

	Widget title = MakeLabel(form, "title", "Log in with a QR code", NULL, 10);
	Widget prev = title;
	if (!message.empty())
		prev = MakeLabel(form, "message", Utf8ToLatin1(message).c_str(), title, 6);

	s->area = XtVaCreateManagedWidget("code", xmDrawingAreaWidgetClass, form,
		XmNtopAttachment, XmATTACH_WIDGET,
		XmNtopWidget, prev,
		XmNleftAttachment, XmATTACH_POSITION,
		XmNleftPosition, 50,
		XmNleftOffset, -QR_AREA / 2,
		XmNtopOffset, 10,
		XmNwidth, QR_AREA,
		XmNheight, QR_AREA,
		XmNresizePolicy, XmRESIZE_NONE,
		NULL);
	XtAddCallback(s->area, XmNexposeCallback, ExposeCB, NULL);

	s->status = MakeLabel(form, "status", " \n ", s->area, 10);

	Widget sep = XtVaCreateManagedWidget("sep", xmSeparatorWidgetClass, form,
		XmNtopAttachment, XmATTACH_WIDGET,
		XmNtopWidget, s->status,
		XmNleftAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_FORM,
		XmNtopOffset, 10,
		NULL);

	Widget token = XtVaCreateManagedWidget("Use a Token Instead", xmPushButtonWidgetClass, form,
		XmNtopAttachment, XmATTACH_WIDGET,
		XmNtopWidget, sep,
		XmNleftAttachment, XmATTACH_FORM,
		XmNbottomAttachment, XmATTACH_FORM,
		XmNtopOffset, 10, XmNleftOffset, 12, XmNbottomOffset, 10,
		NULL);
	XtAddCallback(token, XmNactivateCallback, UseTokenCB, NULL);

	Widget quit = XtVaCreateManagedWidget("Quit", xmPushButtonWidgetClass, form,
		XmNtopAttachment, XmATTACH_WIDGET,
		XmNtopWidget, sep,
		XmNrightAttachment, XmATTACH_FORM,
		XmNbottomAttachment, XmATTACH_FORM,
		XmNtopOffset, 10, XmNrightOffset, 12, XmNbottomOffset, 10,
		XmNwidth, 90,
		NULL);
	XtAddCallback(quit, XmNactivateCallback, QuitCB, NULL);

	XtVaSetValues(form, XmNwidth, QR_AREA + 160, NULL);
	XtManageChild(form);
	SetStatus("Making a key for this login\xe2\x80\xa6\n ");

	// the key takes a while on an old CPU: off the UI thread
	int gen = s->generation;
	std::thread([gen] {
		std::string pub;
		EVP_PKEY* key = MakeKey(pub);
		MainQueue::Post([gen, key, pub] {
			if (!g_state || g_state->generation != gen) {
				if (key) EVP_PKEY_free(key);
				return;
			}
			if (!key) {
				SetStatus("Could not make a key for the login (OpenSSL).");
				return;
			}
			g_state->key = key;
			g_state->publicKey = pub;
			Connect();
		});
	}).detach();
}
