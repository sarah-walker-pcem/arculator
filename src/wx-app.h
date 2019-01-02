#ifndef SRC_WX_APP_H_
#define SRC_WX_APP_H_

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

class PopupMenuEvent;
class UpdateMenuEvent;

wxDECLARE_EVENT(WX_STOP_EMULATION_EVENT, wxCommandEvent);
wxDECLARE_EVENT(WX_POPUP_MENU_EVENT, PopupMenuEvent);
wxDECLARE_EVENT(WX_UPDATE_MENU_EVENT, UpdateMenuEvent);

#ifdef _WIN32
#ifndef LONG_PARAM
#define LONG_PARAM wxIntPtr
#endif

#ifndef INT_PARAM
#define INT_PARAM wxInt32
#endif

extern "C" void wx_winsendmessage(void *window, int msg, INT_PARAM wParam, LONG_PARAM lParam);

class WinSendMessageEvent;
wxDECLARE_EVENT(WX_WIN_SEND_MESSAGE_EVENT, WinSendMessageEvent);
class WinSendMessageEvent: public wxCommandEvent
{
public:
        WinSendMessageEvent(void* hwnd, int message, INT_PARAM wParam, LONG_PARAM lParam) : wxCommandEvent(WX_WIN_SEND_MESSAGE_EVENT)
        {
                this->hwnd = hwnd;
                this->message = message;
                this->wParam = wParam;
                this->lParam = lParam;
        }
        WinSendMessageEvent(const WinSendMessageEvent& event) : wxCommandEvent(event)
        {
                this->hwnd = event.GetHWND();
                this->message = event.GetMessage();
                this->wParam = event.GetWParam();
                this->lParam = event.GetLParam();
        }

        wxEvent* Clone() const { return new WinSendMessageEvent(*this); }

        void* GetHWND() const { return hwnd; }
        int GetMessage() const { return message; }
        INT_PARAM GetWParam() const { return wParam; }
        LONG_PARAM GetLParam() const { return lParam; }

private:
        void* hwnd;
        int message;
        INT_PARAM wParam;
        LONG_PARAM lParam;
};
#endif

class PopupMenuEvent: public wxCommandEvent
{
public:
        PopupMenuEvent(wxWindow *window, wxMenu *menu) : wxCommandEvent(WX_POPUP_MENU_EVENT)
        {
                this->window = window;
                this->menu = menu;
        }
        PopupMenuEvent(const PopupMenuEvent &event) : wxCommandEvent(event)
        {
                this->window = event.GetWindow();
                this->menu = event.GetMenu();
        }

        wxEvent *Clone() const { return new PopupMenuEvent(*this); }

        wxWindow *GetWindow() const { return window; }
        wxMenu *GetMenu() const { return menu; }

private:
        wxMenu *menu;
        wxWindow *window;
};

class UpdateMenuEvent: public wxCommandEvent
{
public:
        UpdateMenuEvent(wxMenu *menu) : wxCommandEvent(WX_UPDATE_MENU_EVENT)
        {
                this->menu = menu;
        }
        UpdateMenuEvent(const UpdateMenuEvent &event) : wxCommandEvent(event)
        {
                this->menu = event.GetMenu();
        }

        wxEvent *Clone() const { return new UpdateMenuEvent(*this); }

        wxMenu *GetMenu() const { return menu; }

private:
        wxMenu *menu;
};

class Frame;

class App: public wxApp
{
public:
        App();
        virtual bool OnInit();
        Frame* GetFrame()
        {
                return frame;
        }
private:
        Frame* frame;
};

class Frame: public wxFrame
{
public:
        Frame(App* app, const wxString& title, const wxPoint& pos,
                        const wxSize& size);

        virtual ~Frame() {}

        void Start();

        wxMenu* GetMenu();

private:
        void OnMenuCommand(wxCommandEvent& event);
        void OnStopEmulationEvent(wxCommandEvent& event);
        void OnPopupMenuEvent(PopupMenuEvent& event);
        void OnUpdateMenuEvent(UpdateMenuEvent& event);
#ifdef _WIN32
        void OnWinSendMessageEvent(WinSendMessageEvent &event);
#endif

        wxMenu* menu;

        void Quit(bool stop_emulator = 1);
        void ChangeDisc(int drive);
        void UpdateMenu(wxMenu *menu);

        wxDECLARE_EVENT_TABLE();
};

#endif /* SRC_WX_APP_H_ */
