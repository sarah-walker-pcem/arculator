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
		pending_s_mutex.Lock();

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

		pending_s_mutex.Unlock();
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
		pending_s_mutex.Lock();

		if (pending_s.empty())
		{
			pending_s_mutex.Unlock();
			return 0;
		}

		last_s = pending_s.Clone();
		strcpy(s, pending_s);
		pending_s.clear();

		pending_s_mutex.Unlock();

		return 1;
	}

private:
	wxDECLARE_EVENT_TABLE();

	wxString last_s;
	wxString pending_s;

	wxMutex pending_s_mutex;

	std::deque<wxString> scrollback;
	unsigned int scrollback_pos;
};

wxBEGIN_EVENT_TABLE(ConsoleInput, wxTextCtrl)
    EVT_TEXT_ENTER(wxID_ANY, ConsoleInput::OnTextEnter)
    EVT_KEY_DOWN(ConsoleInput::OnKeyDown)
wxEND_EVENT_TABLE()

wxDEFINE_EVENT(CONSOLE_INPUT_DISABLE_EVENT, wxCommandEvent);
wxDEFINE_EVENT(CONSOLE_INPUT_ENABLE_EVENT, wxCommandEvent);
wxDEFINE_EVENT(CONSOLE_WRITE_EVENT, wxCommandEvent);

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


		console_input->SetFont(wxFont(12, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
		console_output->SetFont(wxFont(12, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));

		main_sizer->Add(console_output, wxSizerFlags(1).Expand());
		main_sizer->Add(console_input, wxSizerFlags().Expand());

		SetSizer(main_sizer);
	}

	~ConsoleWindow()
	{
		console_window = NULL;
	}

	void OnConsoleWrite(wxCommandEvent &event)
	{
		wxString s = event.GetString().Clone();

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

	void OnInputDisable(wxCommandEvent &event)
	{
		console_input->Disable();
	}
	void OnInputEnable(wxCommandEvent &event)
	{
		console_input->Enable();
		console_input->SetFocus();
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
    EVT_COMMAND(wxID_ANY, CONSOLE_INPUT_DISABLE_EVENT, ConsoleWindow::OnInputDisable)
    EVT_COMMAND(wxID_ANY, CONSOLE_INPUT_ENABLE_EVENT, ConsoleWindow::OnInputEnable)
    EVT_COMMAND(wxID_ANY, CONSOLE_WRITE_EVENT, ConsoleWindow::OnConsoleWrite)
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

	wxCommandEvent *event = new wxCommandEvent(CONSOLE_WRITE_EVENT, wxID_ANY);
	event->SetString(s);
	wxQueueEvent((wxWindow *)console_window, event);
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

	wxCommandEvent *event = new wxCommandEvent(CONSOLE_INPUT_DISABLE_EVENT, wxID_ANY);
	wxQueueEvent((wxWindow *)console_window, event);
}

extern "C" void console_input_enable()
{
	if (!console_window_enabled)
		return;

	wxCommandEvent *event = new wxCommandEvent(CONSOLE_INPUT_ENABLE_EVENT, wxID_ANY);
	wxQueueEvent((wxWindow *)console_window, event);
}
