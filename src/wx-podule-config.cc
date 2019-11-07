/*Arculator 2.0 by Sarah Walker
  Podule configuration subsystem*/
#include <wx/wxprec.h>

#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <wx/xrc/xmlres.h>
#include <wx/filedlg.h>

#include <map>

#include "wx-podule-config.h"

extern "C"
{
#include "arc.h"
#include "config.h"
#include "podules.h"

void *podule_config_get_current(void *window_p, int id);
void podule_config_set_current(void *window_p, int id, void *val);
}
#define IDC_CONFIG_BASE 1000

class wxPointer: public wxObject
{
public:
        wxPointer(const void *p) : p(p) { }
        
        const void *p;
};

class PoduleConfigDialog: public wxDialog
{
public:
	PoduleConfigDialog(wxWindow *parent, const podule_header_t *podule, const podule_config_t *config_, bool running, int slot_nr, const char *prefix = NULL);

	std::map<int, int> id_map;
	std::map<int, int> type_map;

	const podule_header_t *podule;

	bool running;
	int slot_nr;

private:
	void OnCommand(wxCommandEvent &event);
	void OnButton(wxCommandEvent &event);
	void OnCombo(wxCommandEvent &event);
	void OnText(wxCommandEvent &event);
        const char *get_item_name(const podule_config_item_t *item, const char *prefix);
        
	char section_name[20];
	const char *prefix;
        char temp_name_s[256];
};

const char *PoduleConfigDialog::get_item_name(const podule_config_item_t *item, const char *prefix)
{
        if (item->flags & CONFIG_FLAGS_NAME_PREFIXED)
        {
                snprintf(temp_name_s, sizeof(temp_name_s), "%s%s", prefix, item->name);
                return temp_name_s;
        }
        else
                return item->name;
}

void PoduleConfigDialog::OnCommand(wxCommandEvent &event)
{
        wxPointer *userData = dynamic_cast<wxPointer *>(event.GetEventUserData());
        const podule_config_t *config = (const podule_config_t *)userData->p;
        
        switch (event.GetId())
        {
                case wxID_OK:
                {
                int id = IDC_CONFIG_BASE;
                const podule_config_item_t *item = config->items;
                int c;
                int changed = 0;
                
                while (item->type != -1)
                {
                        podule_config_selection_t *selection = item->selection;
                        int val_int;
                        const char *val_str;

                        if (!item->name)
                        {
                                switch (item->type)
                                {
                                        case CONFIG_BINARY:
                                        case CONFIG_BUTTON:
                                        id++;
                                        break;

                                        case CONFIG_SELECTION:
                                        case CONFIG_SELECTION_STRING:
                                        case CONFIG_STRING:
                                        id += 2;
                                        break;
                                }

                                item++;
                                continue;
                        }

                        switch (item->type)
                        {
                                case CONFIG_BINARY:
                                {
                                wxCheckBox *cbox = (wxCheckBox *)this->FindWindow(id);
                                val_int = config_get_int(CFG_MACHINE, section_name, get_item_name(item, prefix), item->default_int);

                                if (val_int != cbox->GetValue())
                                        changed = 1;

                                id++;
                                }
                                break;

                                case CONFIG_SELECTION:
                                {
                                wxComboBox *cbox = (wxComboBox *)this->FindWindow(id + 1);
                                val_int = config_get_int(CFG_MACHINE, section_name, get_item_name(item, prefix), item->default_int);

                                c = cbox->GetCurrentSelection();

                                for (; c > 0; c--)
                                        selection++;

                                if (val_int != selection->value)
                                        changed = 1;

                                id += 2;
                                }
                                break;

                                case CONFIG_SELECTION_STRING:
                                {
                                wxComboBox *cbox = (wxComboBox *)this->FindWindow(id + 1);
                                val_str = config_get_string(CFG_MACHINE, section_name, get_item_name(item, prefix), item->default_string);

                                c = cbox->GetCurrentSelection();

                                for (; c > 0; c--)
                                        selection++;

                                if (strcmp(val_str, selection->value_string))
                                        changed = 1;

                                id += 2;
                                }
                                break;

                                case CONFIG_STRING:
                                {
                                char temp_s[256];
                                wxTextCtrl *text = (wxTextCtrl *)this->FindWindow(id + 1);
                                strncpy(temp_s, (const char *)text->GetValue().mb_str(), sizeof(temp_s));

                                val_str = config_get_string(CFG_MACHINE, section_name, get_item_name(item, prefix), item->default_string);
                                
                                if ((!val_str && temp_s[0]) || (val_str && strcmp(val_str, temp_s)))
                                        changed = 1;

                                id += 2;
                                }
                                break;
                                
                                case CONFIG_BUTTON:
                                id++;
                                break;
                        }
                        item++;
                }
                
                if (config->close)
                        changed |= config->close(this);

                if (!changed)
                {
                        EndModal(0);
                        return;
                }

                if (running)
                {
                        if (wxMessageBox("This will reset Arculator!\nOkay to continue?", "Arculator", wxYES_NO | wxCENTRE | wxSTAY_ON_TOP, this) != wxYES)
                        {
                                EndModal(0);
                                return;
                        }
                }

                id = IDC_CONFIG_BASE;
                item = config->items;

                while (item->type != -1)
                {
                        podule_config_selection_t *selection = item->selection;

                        if (!item->name)
                        {
                                switch (item->type)
                                {
                                        case CONFIG_BINARY:
                                        case CONFIG_BUTTON:
                                        id++;
                                        break;

                                        case CONFIG_SELECTION:
                                        case CONFIG_SELECTION_STRING:
                                        case CONFIG_STRING:
                                        id += 2;
                                        break;
                                }

                                item++;
                                continue;
                        }

                        switch (item->type)
                        {
                                case CONFIG_BINARY:
                                {
                                wxCheckBox *cbox = (wxCheckBox *)this->FindWindow(id);
                                config_set_int(CFG_MACHINE, section_name, get_item_name(item, prefix), cbox->GetValue());

                                id++;
                                }
                                break;

                                case CONFIG_SELECTION:
                                {
                                wxComboBox *cbox = (wxComboBox *)this->FindWindow(id + 1);
                                c = cbox->GetCurrentSelection();

                                for (; c > 0; c--)
                                        selection++;
                                config_set_int(CFG_MACHINE, section_name, get_item_name(item, prefix), selection->value);

                                id += 2;
                                }
                                break;

                                case CONFIG_SELECTION_STRING:
                                {
                                wxComboBox *cbox = (wxComboBox *)this->FindWindow(id + 1);
                                c = cbox->GetCurrentSelection();

                                for (; c > 0; c--)
                                        selection++;
                                config_set_string(CFG_MACHINE, section_name, get_item_name(item, prefix), selection->value_string);

                                id += 2;
                                }
                                break;

                                case CONFIG_STRING:
                                {
                                char temp_s[256];
                                wxTextCtrl *text = (wxTextCtrl *)this->FindWindow(id + 1);
                                strncpy(temp_s, (const char *)text->GetValue().mb_str(), sizeof(temp_s));

                                config_set_string(CFG_MACHINE, section_name, get_item_name(item, prefix), temp_s);

                                id += 2;
                                }
                                break;

                                case CONFIG_BUTTON:
                                id++;
                                break;
                        }
                        item++;
                }

                saveconfig();
                if (running)
                        arc_reset();

                EndModal(1);
                break;
                }

                case wxID_CANCEL:
                EndModal(0);
                break;
        }
}

void PoduleConfigDialog::OnText(wxCommandEvent &event)
{
        wxObject *obj = dynamic_cast<wxObject *>(event.GetEventObject());
        wxPointer *userData = dynamic_cast<wxPointer *>(event.GetEventUserData());

        if (userData && obj)
        {
                const podule_config_item_t *item = (const podule_config_item_t *)userData->p;

                if (item->function)
                {
                        static bool in_callback = false;
                        int update = 0;

                        if (!in_callback)
                        {
                                in_callback = true;

                                switch (item->type)
                                {
                                        case CONFIG_STRING:
                                        update = item->function(this, item, (void *)(dynamic_cast<wxTextCtrl *>(obj)->GetValue().char_str()));
                                        break;
                                }

                                in_callback = false;
                        }

                        if (update)
                                Update();
                }
        }
}

void PoduleConfigDialog::OnCombo(wxCommandEvent &event)
{
        wxObject *obj = dynamic_cast<wxObject *>(event.GetEventObject());
        wxPointer *userData = dynamic_cast<wxPointer *>(event.GetEventUserData());
        
        if (userData && obj)
        {
                const podule_config_item_t *item = (const podule_config_item_t *)userData->p;

                if (item->function)
                {
                        static bool in_callback = false;
                        int update = 0;
                        
                        if (!in_callback)
                        {
                                in_callback = true;
                                
                                switch (item->type)
                                {
                                        case CONFIG_SELECTION_STRING:
                                        update = item->function(this, item, (void *)(dynamic_cast<wxComboBox *>(obj)->GetValue().char_str()));
                                        break;
                                }
                                
                                in_callback = false;
                        }
                        
                        if (update)
                                Update();
                }
        }
}

void PoduleConfigDialog::OnButton(wxCommandEvent &event)
{
        wxObject *obj = dynamic_cast<wxObject *>(event.GetEventObject());
        wxPointer *userData = dynamic_cast<wxPointer *>(event.GetEventUserData());
        
        if (userData && obj)
        {
                const podule_config_item_t *item = (const podule_config_item_t *)userData->p;

                if (item->function)
                        item->function(this, item, NULL);
        }
}

PoduleConfigDialog::PoduleConfigDialog(wxWindow *parent, const podule_header_t *podule, const podule_config_t *config, bool running, int slot_nr, const char *prefix) :
        wxDialog(parent, -1, "Configure podule"),
        podule(podule),
        running(running),
        slot_nr(slot_nr),
        prefix(prefix)
{
        const podule_config_item_t *item;
        char s[257];
        int id;
        int c;

        snprintf(section_name, 20, "%s.%i", podule->short_name, slot_nr);

        wxFlexGridSizer *root = new wxFlexGridSizer(0, 1, 0, 0);
        root->SetFlexibleDirection(wxBOTH);
        root->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_SPECIFIED);
        SetSizer(root);

        wxFlexGridSizer *sizer = new wxFlexGridSizer(0, 2, 0, 0);
        sizer->SetFlexibleDirection(wxBOTH);
        sizer->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_SPECIFIED);
        sizer->AddGrowableCol(1);
        root->Add(sizer, 1, wxEXPAND, 5);

        id = IDC_CONFIG_BASE;
        item = config->items;
        while (item->type != -1)
        {
                switch (item->type)
                {
                        case CONFIG_STRING:
//                        sizer->Add(0, 0, 1, wxEXPAND, 5);
                        sizer->Add(new wxStaticText(this, id++, item->description), 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
                        sizer->Add(new wxTextCtrl(this, id++), 0, wxALL, 5);
                        break;

                        case CONFIG_BINARY:
                        sizer->Add(0, 0, 1, wxEXPAND, 5);
                        sizer->Add(new wxCheckBox(this, id++, item->description), 0, wxALL, 5);
                        break;

                        case CONFIG_SELECTION:
                        case CONFIG_SELECTION_STRING:
                        {
                        sprintf(s, "%s:", item->description);
                        sizer->Add(new wxStaticText(this, id++, s), 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
                        wxBoxSizer* comboSizer = new wxBoxSizer(wxHORIZONTAL);
                        sizer->Add(comboSizer, 1, wxEXPAND, 5);
                        wxComboBox* cb = new wxComboBox(this, id++);
                        cb->SetEditable(false);
                        comboSizer->Add(cb, 1, wxALL, 5);
                        }
                        break;

                        case CONFIG_BUTTON:
                        sizer->Add(0, 0, 1, wxEXPAND, 5);
                        sizer->Add(new wxButton(this, id++, item->description), 0, wxALL, 5);
                        break;
                }

                item++;
        }

        wxBoxSizer* okCancelSizer = new wxBoxSizer(wxHORIZONTAL);
        root->Add(okCancelSizer, 1, wxEXPAND, 5);

        okCancelSizer->Add(0, 0, 1, wxEXPAND, 5);
        okCancelSizer->Add(new wxButton(this, wxID_OK), 0, wxALL, 5);
        okCancelSizer->Add(new wxButton(this, wxID_CANCEL), 0, wxALL, 5);

        Bind(wxEVT_BUTTON, &PoduleConfigDialog::OnCommand, this, wxID_OK, wxID_OK, new wxPointer(config));
        Bind(wxEVT_BUTTON, &PoduleConfigDialog::OnCommand, this, wxID_CANCEL, wxID_CANCEL, new wxPointer(config));

        id = IDC_CONFIG_BASE;
        item = config->items;
        while (item->type != -1)
        {
                podule_config_selection_t *selection = item->selection;
                int val_int;
                const char *val_str;

                type_map.insert(std::pair<int, int>(item->id, item->type));
                switch (item->type)
                {
                        case CONFIG_STRING:
                        {
                        wxTextCtrl *text = (wxTextCtrl *)this->FindWindow(id+1);
                        if (item->name)
                                val_str = config_get_string(CFG_MACHINE, section_name, get_item_name(item, prefix), item->default_string);
                        else
                                val_str = item->default_string;

                        text->SetValue(val_str);
                        if (item->flags & CONFIG_FLAGS_DISABLED)
                                text->Enable(false);
                        else
                                Bind(wxEVT_TEXT, &PoduleConfigDialog::OnText, this, id+1, id+1, new wxPointer(item));

                        id_map.insert(std::pair<int, int>(item->id, id+1));
                        id += 2;
                        }
                        break;

                        case CONFIG_BINARY:
                        {
                        wxCheckBox *cbox = (wxCheckBox *)this->FindWindow(id);
                        val_int = config_get_int(CFG_MACHINE, section_name, get_item_name(item, prefix), item->default_int);

                        cbox->SetValue(val_int);
                        if (item->flags & CONFIG_FLAGS_DISABLED)
                                cbox->Enable(false);

                        id_map.insert(std::pair<int, int>(item->id, id));
                        id++;
                        }
                        break;

                        case CONFIG_SELECTION:
                        {
                        wxComboBox *cbox = (wxComboBox *)this->FindWindow(id+1);

                        val_int = config_get_int(CFG_MACHINE, section_name, get_item_name(item, prefix), item->default_int);

                        c = 0;
                        while (selection->description[0])
                        {
                                cbox->Append(selection->description);
                                if (val_int == selection->value)
                                        cbox->Select(c);
                                selection++;
                                c++;
                        }
                        if (item->flags & CONFIG_FLAGS_DISABLED)
                                cbox->Enable(false);

                        id_map.insert(std::pair<int, int>(item->id, id+1));
                        id += 2;
                        }
                        break;

                        case CONFIG_SELECTION_STRING:
                        {
                        wxComboBox *cbox = (wxComboBox *)this->FindWindow(id+1);

                        val_str = config_get_string(CFG_MACHINE, section_name, get_item_name(item, prefix), item->default_string);

                        c = 0;
                        while (selection->description[0])
                        {
                                cbox->Append(selection->description);
                                if (!strcmp(val_str, selection->value_string))
                                        cbox->Select(c);
                                selection++;
                                c++;
                        }
                        if (item->flags & CONFIG_FLAGS_DISABLED)
                                cbox->Enable(false);
                        Bind(wxEVT_COMBOBOX, &PoduleConfigDialog::OnCombo, this, id+1, id+1, new wxPointer(item));
                        
                        id_map.insert(std::pair<int, int>(item->id, id+1));
                        id += 2;
                        }
                        break;
                        
                        case CONFIG_BUTTON:
                        Bind(wxEVT_BUTTON, &PoduleConfigDialog::OnButton, this, id, id, new wxPointer(item));
                        id++;
                        break;
                }
                item++;
        }

        Fit();
}

static wxWindow *find_wxWindow(void *window_p, int id, int *type)
{
        PoduleConfigDialog *dlg = static_cast<PoduleConfigDialog *>(window_p);
        int wx_id = dlg->id_map[id];

        *type = dlg->type_map[id];
        
        return dlg->FindWindow(wx_id);
}

static char temp_s[256];

void *podule_config_get_current(void *window_p, int id)
{
        int type;
        wxWindow *window = find_wxWindow(window_p, id, &type);
        
        switch (type)
        {
                case CONFIG_STRING:
                {
                        wxTextCtrl *text = dynamic_cast<wxTextCtrl *>(window);
                        strcpy(temp_s, (const char *)text->GetValue().mb_str());
                        return temp_s;
                }

                case CONFIG_SELECTION_STRING:
                {
                        wxComboBox *cbox = dynamic_cast<wxComboBox *>(window);
                        strcpy(temp_s, (const char *)cbox->GetValue().mb_str());
                        return temp_s;
                }
        }
        
        return NULL;
}

void podule_config_set_current(void *window_p, int id, void *val)
{
        int type;
        wxWindow *window = find_wxWindow(window_p, id, &type);

        switch (type)
        {
                case CONFIG_STRING:
                {
                        wxTextCtrl *text = dynamic_cast<wxTextCtrl *>(window);
                        text->SetValue((char *)val);
                        break;
                }

                case CONFIG_SELECTION_STRING:
                {
                        wxComboBox *cbox = dynamic_cast<wxComboBox *>(window);
                        cbox->SetValue((char *)val);
                        break;
                }
        }
}

int podule_config_file_selector(void *window_p, const char *title, const char *default_path, const char *default_fn, const char *default_ext, const char *wildcard, char *dest, int dest_len, int flags)
{
        PoduleConfigDialog *dlg = static_cast<PoduleConfigDialog *>(window_p);
        int wx_flags = 0;
        
        if (flags & CONFIG_FILESEL_SAVE)
                wx_flags = wxFD_SAVE | wxFD_OVERWRITE_PROMPT;
        else
                wx_flags = wxFD_OPEN | wxFD_FILE_MUST_EXIST;
                
        wxString fn = wxFileSelector(title, default_path, default_fn, default_ext,
                                     wildcard, wx_flags, dlg);

        if (!fn.empty())
        {
                strncpy(dest, fn.mb_str(), dest_len);
                return 0;
        }
        else
                return -1; /*Failure*/
}

int podule_config_open(void *window_p, podule_config_t *config, const char *prefix)
{
        PoduleConfigDialog *parent_dlg = static_cast<PoduleConfigDialog *>(window_p);
        
        PoduleConfigDialog dlg(parent_dlg, parent_dlg->podule, config, parent_dlg->running, parent_dlg->slot_nr, prefix);

        if (config->init)
                config->init(&dlg);

        return dlg.ShowModal();
}


void ShowPoduleConfig(wxWindow *parent, const podule_header_t *podule, podule_config_t *config, bool running, int slot_nr)
{
        PoduleConfigDialog dlg(parent, podule, config, running, slot_nr);
        
        if (config->init)
                config->init(&dlg);

        dlg.ShowModal();
}
