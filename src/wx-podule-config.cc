#include <wx/wxprec.h>

#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <wx/xrc/xmlres.h>

extern "C"
{
#include "arc.h"
#include "config.h"
#include "podules.h"
}
#define IDC_CONFIG_BASE 1000

class PoduleConfigDialog: public wxDialog
{
public:
	PoduleConfigDialog(wxWindow *parent, const podule_header_t *podule, bool running, int slot_nr);
private:
	void OnCommand(wxCommandEvent &event);
	
	const podule_header_t *podule;
	bool running;
	char section_name[20];
};

void PoduleConfigDialog::OnCommand(wxCommandEvent &event)
{
        switch (event.GetId())
        {
                case wxID_OK:
                {
                int id = IDC_CONFIG_BASE;
                const podule_config_t *config = podule->config;
                int c;
                int changed = 0;

                while (config->type != -1)
                {
                        podule_config_selection_t *selection = config->selection;
                        int val_int;
                        const char *val_str;

                        switch (config->type)
                        {
                                case CONFIG_BINARY:
                                {
                                wxCheckBox *cbox = (wxCheckBox *)this->FindWindow(id);
                                val_int = config_get_int(CFG_MACHINE, section_name, config->name, config->default_int);

                                if (val_int != cbox->GetValue())
                                        changed = 1;

                                id++;
                                }
                                break;

                                case CONFIG_SELECTION:
                                {
                                wxComboBox *cbox = (wxComboBox *)this->FindWindow(id + 1);
                                val_int = config_get_int(CFG_MACHINE, section_name, config->name, config->default_int);

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
                                val_str = config_get_string(CFG_MACHINE, section_name, config->name, config->default_string);

                                c = cbox->GetCurrentSelection();

                                for (; c > 0; c--)
                                        selection++;

                                if (strcmp(val_str, selection->value_string))
                                        changed = 1;

                                id += 2;
                                }
                                break;
                        }
                        config++;
                }

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
                config = podule->config;

                while (config->type != -1)
                {
                        podule_config_selection_t *selection = config->selection;

                        switch (config->type)
                        {
                                case CONFIG_BINARY:
                                {
                                wxCheckBox *cbox = (wxCheckBox *)this->FindWindow(id);
                                config_set_int(CFG_MACHINE, section_name, config->name, cbox->GetValue());

                                id++;
                                }
                                break;

                                case CONFIG_SELECTION:
                                {
                                wxComboBox *cbox = (wxComboBox *)this->FindWindow(id + 1);
                                c = cbox->GetCurrentSelection();

                                for (; c > 0; c--)
                                        selection++;
                                config_set_int(CFG_MACHINE, section_name, config->name, selection->value);

                                id += 2;
                                }
                                break;

                                case CONFIG_SELECTION_STRING:
                                {
                                wxComboBox *cbox = (wxComboBox *)this->FindWindow(id + 1);
                                c = cbox->GetCurrentSelection();

                                for (; c > 0; c--)
                                        selection++;
                                config_set_string(CFG_MACHINE, section_name, config->name, selection->value_string);

                                id += 2;
                                }
                                break;
                        }
                        config++;
                }

                saveconfig();
                if (running)
                        arc_reset();

                EndModal(0);
                break;
                }

                case wxID_CANCEL:
                EndModal(0);
                break;
        }
}

PoduleConfigDialog::PoduleConfigDialog(wxWindow *parent, const podule_header_t *podule_, bool running_, int slot_nr) :
        wxDialog(parent, -1, "Configure podule")
{
        char s[257];
        const podule_config_t *config;
        int id;
        int c;

        podule = podule_;
        running = running_;
        
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
        config = podule->config;
        while (config->type != -1)
        {
                switch (config->type)
                {
                        case CONFIG_BINARY:
                        sizer->Add(0, 0, 1, wxEXPAND, 5);
                        sizer->Add(new wxCheckBox(this, id++, config->description), 0, wxALL, 5);
                        break;

                        case CONFIG_SELECTION:
                        case CONFIG_SELECTION_STRING:
                        sprintf(s, "%s:", config->description);
                        sizer->Add(new wxStaticText(this, id++, s), 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
                        wxBoxSizer* comboSizer = new wxBoxSizer(wxHORIZONTAL);
                        sizer->Add(comboSizer, 1, wxEXPAND, 5);
                        wxComboBox* cb = new wxComboBox(this, id++);
                        cb->SetEditable(false);
                        comboSizer->Add(cb, 1, wxALL, 5);
                        break;
                }

                config++;
        }

        wxBoxSizer* okCancelSizer = new wxBoxSizer(wxHORIZONTAL);
        root->Add(okCancelSizer, 1, wxEXPAND, 5);

        okCancelSizer->Add(0, 0, 1, wxEXPAND, 5);
        okCancelSizer->Add(new wxButton(this, wxID_OK), 0, wxALL, 5);
        okCancelSizer->Add(new wxButton(this, wxID_CANCEL), 0, wxALL, 5);

        Bind(wxEVT_BUTTON, &PoduleConfigDialog::OnCommand, this);

        id = IDC_CONFIG_BASE;
        config = podule->config;
        while (config->type != -1)
        {
                podule_config_selection_t *selection = config->selection;
                int val_int;
                const char *val_str;
                        
                switch (config->type)
                {
                        case CONFIG_BINARY:
                        {
                        wxCheckBox *cbox = (wxCheckBox *)this->FindWindow(id);
                        val_int = config_get_int(CFG_MACHINE, section_name, config->name, config->default_int);

                        cbox->SetValue(val_int);

                        id++;
                        }
                        break;

                        case CONFIG_SELECTION:
                        {
                        wxComboBox *cbox = (wxComboBox *)this->FindWindow(id+1);

                        val_int = config_get_int(CFG_MACHINE, section_name, config->name, config->default_int);

                        c = 0;
                        while (selection->description[0])
                        {
                                cbox->Append(selection->description);
                                if (val_int == selection->value)
                                        cbox->Select(c);
                                selection++;
                                c++;
                        }

                        id += 2;
                        }
                        break;

                        case CONFIG_SELECTION_STRING:
                        {
                        wxComboBox *cbox = (wxComboBox *)this->FindWindow(id+1);

                        val_str = config_get_string(CFG_MACHINE, section_name, config->name, config->default_string);

                        c = 0;
                        while (selection->description[0])
                        {
                                cbox->Append(selection->description);
                                if (!strcmp(val_str, selection->value_string))
                                        cbox->Select(c);
                                selection++;
                                c++;
                        }

                        id += 2;
                        }
                        break;
                }
                config++;
        }

        Fit();
}

void ShowPoduleConfig(wxWindow *parent, const podule_header_t *podule, bool running, int slot_nr)
{
        PoduleConfigDialog dlg(parent, podule, running, slot_nr);
        
        dlg.ShowModal();
}
