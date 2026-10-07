#include "Xm.hpp"
#include "LogonDialog.hpp"

#include <Xm/DialogS.h>
#include <Xm/Form.h>
#include <Xm/Label.h>
#include <Xm/PushB.h>
#include <Xm/Separator.h>
#include <Xm/Text.h>
#include <Xm/TextF.h>

#include "Theme.hpp"

int AddVisualArgs(Arg* args, int n); // Main.cpp

namespace
{
	struct LogonState
	{
		Widget shell;
		Widget text;
		std::string token; // what was typed; the field shows asterisks
		std::function<void(const std::string&)> done;
	};

	void Finish(LogonState* st, bool ok)
	{
		std::string token = ok ? st->token : "";
		// trim spaces and quotes pasted along with the token
		size_t a = token.find_first_not_of(" \t\r\n\"'"), b = token.find_last_not_of(" \t\r\n\"'");
		token = a == std::string::npos ? "" : token.substr(a, b - a + 1);
		if (ok && token.empty())
			return;

		auto done = st->done;
		XtDestroyWidget(st->shell);
		delete st;
		done(token);
	}

	void OkCB(Widget, XtPointer client, XtPointer)
	{
		Finish((LogonState*) client, true);
	}

	void CancelCB(Widget, XtPointer client, XtPointer)
	{
		Finish((LogonState*) client, false);
	}

	// Keeps the real text aside and shows asterisks.
	void MaskCB(Widget, XtPointer client, XtPointer call)
	{
		LogonState* st = (LogonState*) client;
		XmTextVerifyCallbackStruct* cbs = (XmTextVerifyCallbackStruct*) call;
		std::string& t = st->token;
		size_t start = (size_t) cbs->startPos, end = (size_t) cbs->endPos;
		if (start > t.size()) start = t.size();
		if (end > t.size()) end = t.size();
		std::string ins = cbs->text && cbs->text->ptr ? std::string(cbs->text->ptr, cbs->text->length) : "";
		t.replace(start, end - start, ins);
		for (int i = 0; cbs->text && i < cbs->text->length; i++)
			cbs->text->ptr[i] = '*';
	}

	Widget Label(Widget parent, const char* name, const std::string& text, Widget above)
	{
		XmString xs = XmStringCreateLtoR((char*) Utf8ToLatin1(text).c_str(), (char*) XmFONTLIST_DEFAULT_TAG);
		Widget w = XtVaCreateManagedWidget(name, xmLabelWidgetClass, parent,
			XmNlabelString, xs,
			XmNalignment, XmALIGNMENT_BEGINNING,
			XmNtopAttachment, above ? XmATTACH_WIDGET : XmATTACH_FORM,
			XmNtopWidget, above,
			XmNleftAttachment, XmATTACH_FORM,
			XmNrightAttachment, XmATTACH_FORM,
			XmNtopOffset, 10,
			XmNleftOffset, 12,
			XmNrightOffset, 12,
			NULL);
		XmStringFree(xs);
		return w;
	}
}

void ShowLogonDialog(Widget parent, const std::string& message, std::function<void(const std::string&)> done)
{
	LogonState* st = new LogonState;
	st->done = done;

	Arg args[8];
	int n = 0;
	XtSetArg(args[n], XmNtitle, "Log in to Discord"); n++;
	XtSetArg(args[n], XmNdeleteResponse, XmDO_NOTHING); n++;
	n = AddVisualArgs(args, n);
	st->shell = XmCreateDialogShell(parent, (char*) "logon", args, n);

	Widget form = XtVaCreateWidget("form", xmFormWidgetClass, st->shell,
		XmNdialogStyle, XmDIALOG_FULL_APPLICATION_MODAL,
		XmNautoUnmanage, False,
		NULL);

	Widget top = Label(form, "title", "Log in to Discord", NULL);
	Widget expl = Label(form, "explanation",
		"Discord Messenger logs in with your account's token.\n"
		"In a web browser logged in to discord.com, open the developer\n"
		"tools, and copy the \"authorization\" header of any request to\n"
		"discord.com/api; paste it below.  It is kept in\n"
		"~/.discordmessenger/settings.json.", top);
	Widget prev = expl;
	if (!message.empty())
		prev = Label(form, "message", message, expl);
	Widget tokenLabel = Label(form, "tokenLabel", "Token:", prev);

	st->text = XtVaCreateManagedWidget("token", xmTextFieldWidgetClass, form,
		XmNtopAttachment, XmATTACH_WIDGET,
		XmNtopWidget, tokenLabel,
		XmNleftAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_FORM,
		XmNtopOffset, 4,
		XmNleftOffset, 12,
		XmNrightOffset, 12,
		XmNcolumns, 50,
		NULL);
	XtAddCallback(st->text, XmNmodifyVerifyCallback, MaskCB, st);
	XtAddCallback(st->text, XmNactivateCallback, OkCB, st);

	Widget sep = XtVaCreateManagedWidget("sep", xmSeparatorWidgetClass, form,
		XmNtopAttachment, XmATTACH_WIDGET,
		XmNtopWidget, st->text,
		XmNleftAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_FORM,
		XmNtopOffset, 12,
		NULL);

	Widget ok = XtVaCreateManagedWidget("Log In", xmPushButtonWidgetClass, form,
		XmNtopAttachment, XmATTACH_WIDGET,
		XmNtopWidget, sep,
		XmNleftAttachment, XmATTACH_POSITION,
		XmNleftPosition, 20,
		XmNrightAttachment, XmATTACH_POSITION,
		XmNrightPosition, 45,
		XmNbottomAttachment, XmATTACH_FORM,
		XmNtopOffset, 10,
		XmNbottomOffset, 10,
		NULL);
	XtAddCallback(ok, XmNactivateCallback, OkCB, st);

	Widget cancel = XtVaCreateManagedWidget("Quit", xmPushButtonWidgetClass, form,
		XmNtopAttachment, XmATTACH_WIDGET,
		XmNtopWidget, sep,
		XmNleftAttachment, XmATTACH_POSITION,
		XmNleftPosition, 55,
		XmNrightAttachment, XmATTACH_POSITION,
		XmNrightPosition, 80,
		XmNbottomAttachment, XmATTACH_FORM,
		XmNtopOffset, 10,
		XmNbottomOffset, 10,
		NULL);
	XtAddCallback(cancel, XmNactivateCallback, CancelCB, st);

	XtVaSetValues(form, XmNdefaultButton, ok, XmNinitialFocus, st->text, NULL);
	XtManageChild(form);
}
