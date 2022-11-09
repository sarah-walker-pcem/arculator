#include <wx/wxprec.h>

#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <wx/xrc/xmlres.h>
#include "wx/vscroll.h"

#include <deque>

extern "C"
{
	#include "arc.h"
	#include "debugger.h"
}

static bool console_window_enabled = false;

class ConsoleWindow;
static ConsoleWindow *console_window = NULL;

#define SCROLLBACK_MAX_SIZE 500

class ConsoleInput: public wxTextCtrl
{
public:
	ConsoleInput(wxWindow *parent)
		: wxTextCtrl(parent, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER),
		  scrollback_pos(0)
	{
		scrollback.push_back("");
	}

	void OnTextEnter(wxCommandEvent &event)
	{
		if (pending_s.empty())
		{
			pending_s = GetValue();
			if (pending_s.empty())
				pending_s = last_s.Clone();

			if (scrollback.size() == SCROLLBACK_MAX_SIZE+1)
				scrollback.pop_front();

			scrollback_pos = scrollback.size();
			scrollback[scrollback_pos-1] = pending_s.Clone();
			scrollback.push_back("");

			Clear();
		}
	}

	void OnKeyDown(wxKeyEvent &event)
	{
		if (event.GetKeyCode() == WXK_UP)
		{
			if (scrollback_pos > 0)
			{
				if (scrollback_pos == scrollback.size() - 1)
					scrollback[scrollback_pos] = GetValue().Clone();
				scrollback_pos--;
				SetValue(scrollback[scrollback_pos]);
				SetInsertionPointEnd();
			}
		}
		else if (event.GetKeyCode() == WXK_DOWN)
		{
			if ((scrollback_pos + 1) < scrollback.size())
			{
				if (scrollback_pos == scrollback.size() - 1)
					scrollback[scrollback_pos] = GetValue().Clone();
				scrollback_pos++;
				SetValue(scrollback[scrollback_pos]);
				SetInsertionPointEnd();
			}
		}
		else
			event.Skip();
	}

	int GetInput(char *s)
	{
		if (pending_s.empty())
			return 0;

		last_s = pending_s.Clone();
		strcpy(s, pending_s);
		pending_s.clear();
		return 1;
	}

private:
	wxDECLARE_EVENT_TABLE();

	wxString last_s;
	wxString pending_s;

	std::deque<wxString> scrollback;
	unsigned int scrollback_pos;
};

wxBEGIN_EVENT_TABLE(ConsoleInput, wxTextCtrl)
    EVT_TEXT_ENTER(wxID_ANY, ConsoleInput::OnTextEnter)
    EVT_KEY_DOWN(ConsoleInput::OnKeyDown)
wxEND_EVENT_TABLE()


class ConsoleWindow: public wxFrame
{
public:
	ConsoleWindow(wxWindow *parent)
		: wxFrame(parent, wxID_ANY, "Arculator debugger", wxDefaultPosition, wxSize(720,480))
	{
		main_sizer = new wxBoxSizer(wxVERTICAL);
		console_input = new ConsoleInput(this);
		console_output = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
						wxDefaultSize, wxTE_READONLY | wxTE_MULTILINE | wxTE_DONTWRAP);


		console_input->SetFont(wxSystemSettings::GetFont(wxSYS_ANSI_FIXED_FONT));
		console_output->SetFont(wxSystemSettings::GetFont(wxSYS_ANSI_FIXED_FONT));

		main_sizer->Add(console_output, wxSizerFlags(1).Expand());
		main_sizer->Add(console_input, wxSizerFlags().Expand());

		SetSizer(main_sizer);
	}

	~ConsoleWindow()
	{
		console_window = NULL;
	}

	void Write(wxString s)
	{
		TextBuffer.Append(s);

		while (1)
		{
			int pos = TextBuffer.Find(wxString("\n"));

			if (pos == wxNOT_FOUND)
				break;

			console_output->AppendText(TextBuffer.Left(pos+1));
			TextBuffer = TextBuffer.Mid(pos+1);
		}
	}

	int GetInput(char *s)
	{
		return console_input->GetInput(s);
	}

	void OnClose(wxCloseEvent &event)
	{
		console_window_enabled = false;
		Destroy();
	}

	void InputDisable()
	{
		console_input->Disable();
	}
	void InputEnable()
	{
		console_input->Enable();
	}

private:
	wxDECLARE_EVENT_TABLE();

	ConsoleInput *console_input;
	wxTextCtrl *console_output;
	wxBoxSizer *main_sizer;

	wxString TextBuffer;
};

wxBEGIN_EVENT_TABLE(ConsoleWindow, wxFrame)
    EVT_CLOSE(ConsoleWindow::OnClose)
wxEND_EVENT_TABLE()

void ShowConsoleWindow(wxWindow *parent)
{
	if (!console_window)
		console_window = new ConsoleWindow(parent);
	console_window->Show(true);
	console_window_enabled = true;
}

void CloseConsoleWindow()
{
	if (!console_window_enabled)
		return;

	console_window_enabled = false;
	console_window->Destroy();
}

extern "C" void console_output(char *s)
{
	if (!console_window_enabled)
		return;

	console_window->Write(s);
}

extern "C" int console_input_get(char *s)
{
	int ret = 0;

	while (!ret)
	{
		if (!console_window_enabled)
			return CONSOLE_INPUT_GET_ERROR_WINDOW_CLOSED;
		if (debugger_in_reset)
			return CONSOLE_INPUT_GET_ERROR_IN_RESET;

		ret = console_window->GetInput(s);

		if (!ret)
			wxMilliSleep(50);
	}

	return ret;
}

extern "C" void console_input_disable()
{
	if (!console_window_enabled)
		return;

	console_window->InputDisable();
}

extern "C" void console_input_enable()
{
	if (!console_window_enabled)
		return;

	console_window->InputEnable();
}
