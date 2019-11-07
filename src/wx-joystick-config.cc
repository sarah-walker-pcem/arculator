/*Arculator 2.0 by Sarah Walker
  Joystick configuration dialogue
  This handles configuration of host joystick -> emulated joystick mappings*/
#include <wx/wxprec.h>

#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <wx/xrc/xmlres.h>
#include "wx-joystick-config.h"

extern "C" {
void rpclog(const char *format, ...);
#include "config.h"
#include "joystick.h"
#include "plat_joystick.h"
}

#define IDC_CONFIG_BASE 1000

class JoystickConfDialog: public wxDialog
{
public:
	JoystickConfDialog(wxWindow *parent, int joy_nr, int type);
	void OnInit();
	
private:
	void OnOK(wxCommandEvent &event);
	void OnCancel(wxCommandEvent &event);

	void OnDevice(wxCommandEvent &event);
	
	void rebuild_axis_button_selections();
        int get_axis(int id);
        int get_pov(int id);

	int joy_device;
        int joystick_nr;
        int joystick_config_type;
};

#define AXIS_STRINGS_MAX 3
static const char *axis_strings[AXIS_STRINGS_MAX] = {"X Axis", "Y Axis", "Z Axis"};


void JoystickConfDialog::rebuild_axis_button_selections()
{
        int id = IDC_CONFIG_BASE + 3;
        int c, d;

        for (c = 0; c < joystick_get_axis_count(joystick_config_type); c++)
        {
                int sel = c;

                wxComboBox *cbox = (wxComboBox *)this->FindWindow(id);
                cbox->Clear();

                if (joy_device)
                {
                        for (d = 0; d < plat_joystick_state[joy_device-1].nr_axes; d++)
                        {
                                cbox->Append(plat_joystick_state[joy_device-1].axis[d].name);
                                if (c < AXIS_STRINGS_MAX)
                                {
                                        if (!strcasecmp(axis_strings[c], plat_joystick_state[joy_device-1].axis[d].name))
                                                sel = d;
                                }
                        }
                        for (d = 0; d < plat_joystick_state[joy_device-1].nr_povs; d++)
                        {
                                char s[80];
                                
                                sprintf(s, "%s (X axis)", plat_joystick_state[joy_device-1].pov[d].name);
                                cbox->Append(s);
                                sprintf(s, "%s (Y axis)", plat_joystick_state[joy_device-1].pov[d].name);
                                cbox->Append(s);
                        }
                        cbox->SetSelection(sel);
                        cbox->Enable(true);
                }
                else
                        cbox->Enable(false);

                id += 2;
        }

        for (c = 0; c < joystick_get_button_count(joystick_config_type); c++)
        {
                wxComboBox *cbox = (wxComboBox *)this->FindWindow(id);
                cbox->Clear();

                if (joy_device)
                {
                        for (d = 0; d < plat_joystick_state[joy_device-1].nr_buttons; d++)
                                cbox->Append(plat_joystick_state[joy_device-1].button[d].name);
                        cbox->SetSelection(c);
                        cbox->Enable(true);
                }
                else
                        cbox->Enable(false);

                id += 2;
        }

        for (c = 0; c < joystick_get_pov_count(joystick_config_type)*2; c++)
        {
                int sel = c;
                                        
                wxComboBox *cbox = (wxComboBox *)this->FindWindow(id);
                cbox->Clear();

                if (joy_device)
                {
                        for (d = 0; d < plat_joystick_state[joy_device-1].nr_povs; d++)
                        {
                                char s[80];
                                
                                sprintf(s, "%s (X axis)", plat_joystick_state[joy_device-1].pov[d].name);
                                cbox->Append(s);
                                sprintf(s, "%s (Y axis)", plat_joystick_state[joy_device-1].pov[d].name);
                                cbox->Append(s);
                        }
                        for (d = 0; d < plat_joystick_state[joy_device-1].nr_axes; d++)
                                cbox->Append(plat_joystick_state[joy_device-1].axis[d].name);
                        cbox->SetSelection(sel);
                        cbox->Enable(true);
                }
                else
                        cbox->Enable(false);

                id += 2;
        }

}

int JoystickConfDialog::get_axis(int id)
{
        wxComboBox *cbox = (wxComboBox *)this->FindWindow(id);
        int axis_sel = cbox->GetCurrentSelection();
        int nr_axes = plat_joystick_state[joystick_state[joystick_nr].plat_joystick_nr-1].nr_axes;

        if (axis_sel < nr_axes)
                return axis_sel;
        
        axis_sel -= nr_axes;
        if (axis_sel & 1)
                return POV_Y | (axis_sel >> 1);
        else
                return POV_X | (axis_sel >> 1);
}

int JoystickConfDialog::get_pov(int id)
{
        wxComboBox *cbox = (wxComboBox *)this->FindWindow(id);
        int axis_sel = cbox->GetCurrentSelection();
        int nr_povs = plat_joystick_state[joystick_state[joystick_nr].plat_joystick_nr-1].nr_povs*2;

        if (axis_sel < nr_povs)
        {
                if (axis_sel & 1)
                        return POV_Y | (axis_sel >> 1);
                else
                        return POV_X | (axis_sel >> 1);
        }
        
        return axis_sel - nr_povs;
}

void JoystickConfDialog::OnInit()
{
        wxComboBox *cbox = (wxComboBox *)this->FindWindow(IDC_CONFIG_BASE+1);
        int c;
        int id = IDC_CONFIG_BASE + 3;
        int joystick = joystick_state[joystick_nr].plat_joystick_nr;

        cbox->SetSelection(joystick);
        joy_device = joystick;

        rebuild_axis_button_selections();

        if (joystick_state[joystick_nr].plat_joystick_nr)
        {
                int nr_axes = plat_joystick_state[joystick-1].nr_axes;
                int nr_povs = plat_joystick_state[joystick-1].nr_povs;
                for (c = 0; c < joystick_get_axis_count(joystick_config_type); c++)
                {
                        int mapping = joystick_state[joystick_nr].axis_mapping[c];

                        cbox = (wxComboBox *)this->FindWindow(id);
                        if (mapping & POV_X)
                                cbox->SetSelection(nr_axes + (mapping & 3)*2);
                        else if (mapping & POV_Y)
                                cbox->SetSelection(nr_axes + (mapping & 3)*2 + 1);
                        else
                                cbox->SetSelection(mapping);
                        id += 2;
                }
                for (c = 0; c < joystick_get_button_count(joystick_config_type); c++)
                {
                        cbox = (wxComboBox *)this->FindWindow(id);
                        cbox->SetSelection(joystick_state[joystick_nr].button_mapping[c]);
                        id += 2;
                }
                for (c = 0; c < joystick_get_pov_count(joystick_config_type); c++)
                {
                        int mapping;

                        cbox = (wxComboBox *)this->FindWindow(id);
                        mapping = joystick_state[joystick_nr].pov_mapping[c][0];
                        if (mapping & POV_X)
                                cbox->SetSelection((mapping & 3)*2);
                        else if (mapping & POV_Y)
                                cbox->SetSelection((mapping & 3)*2 + 1);
                        else
                                cbox->SetSelection(mapping + nr_povs*2);
                        id += 2;
                        cbox = (wxComboBox *)this->FindWindow(id);
                        mapping = joystick_state[joystick_nr].pov_mapping[c][1];
                        if (mapping & POV_X)
                                cbox->SetSelection((mapping & 3)*2);
                        else if (mapping & POV_Y)
                                cbox->SetSelection((mapping & 3)*2 + 1);
                        else
                                cbox->SetSelection(mapping + nr_povs*2);
                        id += 2;
                }
        }
}

void JoystickConfDialog::OnOK(wxCommandEvent &event)
{
        int c;
        int id = IDC_CONFIG_BASE + 3;
        wxComboBox *cbox = (wxComboBox *)this->FindWindow(IDC_CONFIG_BASE+1);

        joystick_state[joystick_nr].plat_joystick_nr = cbox->GetCurrentSelection();

        if (joystick_state[joystick_nr].plat_joystick_nr)
        {
                for (c = 0; c < joystick_get_axis_count(joystick_config_type); c++)
                {
                        joystick_state[joystick_nr].axis_mapping[c] = get_axis(id);
                        id += 2;
                }
                for (c = 0; c < joystick_get_button_count(joystick_config_type); c++)
                {
                        cbox = (wxComboBox *)this->FindWindow(id);
                        joystick_state[joystick_nr].button_mapping[c] = cbox->GetCurrentSelection();
                        id += 2;
                }
                for (c = 0; c < joystick_get_pov_count(joystick_config_type); c++)
                {
                        joystick_state[joystick_nr].pov_mapping[c][0] = get_pov(id);
                        id += 2;
                        joystick_state[joystick_nr].pov_mapping[c][1] = get_pov(id);
                        id += 2;
                }
        }

        EndModal(0);
}
void JoystickConfDialog::OnCancel(wxCommandEvent &event)
{
        EndModal(0);
}

void JoystickConfDialog::OnDevice(wxCommandEvent &event)
{
        wxComboBox *cbox = dynamic_cast<wxComboBox *>(event.GetEventObject());
        joy_device = cbox->GetCurrentSelection();
        
        rebuild_axis_button_selections();
}


JoystickConfDialog::JoystickConfDialog(wxWindow *parent, int joy_nr, int type) :
        wxDialog(parent, -1, "Configure joystick"),
        joystick_nr(joy_nr),
        joystick_config_type(type)
{
        char s[257];
        int c;

        wxFlexGridSizer* root = new wxFlexGridSizer(0, 1, 0, 0);
        root->SetFlexibleDirection(wxBOTH);
        root->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_SPECIFIED);
        SetSizer(root);

        wxFlexGridSizer* sizer = new wxFlexGridSizer(0, 2, 0, 0);
        sizer->SetFlexibleDirection(wxBOTH);
        sizer->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_SPECIFIED);
        sizer->AddGrowableCol(1);
        root->Add(sizer, 1, wxEXPAND, 5);

        int id = IDC_CONFIG_BASE;


        sizer->Add(new wxStaticText(this, id++, "Device:"), 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
        wxBoxSizer* comboSizer = new wxBoxSizer(wxHORIZONTAL);
        sizer->Add(comboSizer, 1, wxEXPAND, 5);
        wxComboBox* cb = new wxComboBox(this, id++);
        cb->SetEditable(false);
        comboSizer->Add(cb, 1, wxALL, 5);
        Bind(wxEVT_COMBOBOX, &JoystickConfDialog::OnDevice, this, id-1);

        cb->Append("None");
        for (c = 0; c < joysticks_present; c++)
                cb->Append(plat_joystick_state[c].name);

        for (c = 0; c < joystick_get_axis_count(type); c++)
        {
                sizer->Add(new wxStaticText(this, id++, joystick_get_axis_name(type, c)), 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
                wxBoxSizer* comboSizer = new wxBoxSizer(wxHORIZONTAL);
                sizer->Add(comboSizer, 1, wxEXPAND, 5);
                wxComboBox* cb = new wxComboBox(this, id++);
                cb->SetEditable(false);
                comboSizer->Add(cb, 1, wxALL, 5);
        }

        for (c = 0; c < joystick_get_button_count(type); c++)
        {
                sprintf(s, "Button %i:", c);
                sizer->Add(new wxStaticText(this, id++, joystick_get_button_name(type, c)), 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
                wxBoxSizer* comboSizer = new wxBoxSizer(wxHORIZONTAL);
                sizer->Add(comboSizer, 1, wxEXPAND, 5);
                wxComboBox* cb = new wxComboBox(this, id++);
                cb->SetEditable(false);
                comboSizer->Add(cb, 1, wxALL, 5);
        }

        for (c = 0; c < joystick_get_pov_count(type)*2; c++)
        {
                sprintf(s, "POV %i:", c);
                sizer->Add(new wxStaticText(this, id++, joystick_get_pov_name(type, c)), 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
                wxBoxSizer* comboSizer = new wxBoxSizer(wxHORIZONTAL);
                sizer->Add(comboSizer, 1, wxEXPAND, 5);
                wxComboBox* cb = new wxComboBox(this, id++);
                cb->SetEditable(false);
                comboSizer->Add(cb, 1, wxALL, 5);
        }

        wxBoxSizer* okCancelSizer = new wxBoxSizer(wxHORIZONTAL);
        root->Add(okCancelSizer, 1, wxEXPAND, 5);

        okCancelSizer->Add(0, 0, 1, wxEXPAND, 5);
        okCancelSizer->Add(new wxButton(this, wxID_OK), 0, wxALL, 5);
        okCancelSizer->Add(new wxButton(this, wxID_CANCEL), 0, wxALL, 5);

        Bind(wxEVT_BUTTON, &JoystickConfDialog::OnOK, this, wxID_OK);
        Bind(wxEVT_BUTTON, &JoystickConfDialog::OnCancel, this, wxID_CANCEL);
}

void ShowConfJoy(wxWindow *parent, int joy_nr, int type)
{
        rpclog("joysticks_present=%i\n", joysticks_present);
        JoystickConfDialog dlg(parent, joy_nr, type);

        dlg.Fit();
        dlg.OnInit();
        dlg.ShowModal();
}
