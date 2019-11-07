/*Arculator 2.0 by Sarah Walker
  Configuration selector dialogue*/
#include <wx/wxprec.h>

#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <wx/xrc/xmlres.h>
#include "wx-config.h"
#include "wx-config_sel.h"

extern "C"
{
        #include "arc.h"
        #include "config.h"
        void rpclog(const char *format, ...);
};

class ConfigSelDialog: public wxDialog
{
public:
	ConfigSelDialog(wxWindow* parent);
private:
	void OnOK(wxCommandEvent &event);
	void OnCancel(wxCommandEvent &event);
	void OnNew(wxCommandEvent &event);
	void OnRename(wxCommandEvent &event);
	void OnCopy(wxCommandEvent &event);
	void OnDelete(wxCommandEvent &event);
	void OnConfig(wxCommandEvent &event);

        void BuildConfigList();
        wxString GetConfigPath(wxString config_name);
};

ConfigSelDialog::ConfigSelDialog(wxWindow* parent)
{
        wxXmlResource::Get()->LoadDialog(this, parent, "ConfigureSelectionDlg");

        Bind(wxEVT_BUTTON, &ConfigSelDialog::OnOK, this, wxID_OK);
        Bind(wxEVT_BUTTON, &ConfigSelDialog::OnCancel, this, wxID_CANCEL);
        Bind(wxEVT_BUTTON, &ConfigSelDialog::OnNew, this, XRCID("IDC_NEW"));
        Bind(wxEVT_BUTTON, &ConfigSelDialog::OnRename, this, XRCID("IDC_RENAME"));
        Bind(wxEVT_BUTTON, &ConfigSelDialog::OnCopy, this, XRCID("IDC_COPY"));
        Bind(wxEVT_BUTTON, &ConfigSelDialog::OnDelete, this, XRCID("IDC_DELETE"));
        Bind(wxEVT_BUTTON, &ConfigSelDialog::OnConfig, this, XRCID("IDC_CONFIG"));
        BuildConfigList();
}

void ConfigSelDialog::BuildConfigList()
{
        wxListBox* list = (wxListBox*)FindWindow(XRCID("IDC_LIST"));
        list->Clear();
        wxArrayString items;
        wxString path(exname);
        path += "configs/*.cfg";
        wxString f = wxFindFirstFile(path);
        while (!f.empty())
        {
                wxFileName file(f);
                items.Add(file.GetName());
                f = wxFindNextFile();
        }
        items.Sort();
        list->Set(items);
}
wxString ConfigSelDialog::GetConfigPath(wxString config_name)
{
        return wxString(exname) + "configs/" + config_name + ".cfg";
}

void ConfigSelDialog::OnOK(wxCommandEvent &event)
{
        wxListBox *list = (wxListBox*)FindWindow(XRCID("IDC_LIST"));
        wxString selection = list->GetStringSelection();
        if (!selection.IsEmpty())
        {
                wxString config_path = GetConfigPath(list->GetStringSelection());
                strcpy(machine_config_file, config_path.mb_str());
                strcpy(machine_config_name, list->GetStringSelection().mb_str());
                EndModal(0);
        }
}
void ConfigSelDialog::OnCancel(wxCommandEvent &event)
{
        EndModal(-1);
}
void ConfigSelDialog::OnNew(wxCommandEvent &event)
{
        wxTextEntryDialog dlg(this, "Enter name:", "New config");
        dlg.SetMaxLength(64);
        if (dlg.ShowModal() == wxID_OK)
        {
                wxString config_path = GetConfigPath(dlg.GetValue());
                
                if (wxFileName(config_path).Exists())
                {
                        wxMessageBox("A configuration with that name already exists", "Arculator", wxOK | wxCENTRE | wxSTAY_ON_TOP, this);
                }
                else
                {
                        int preset = ShowPresetList();
                        if (preset != -1)
                        {
                                strcpy(machine_config_file, config_path.mb_str());

                                loadconfig();
                                ShowConfigWithPreset(preset);
                                BuildConfigList();
                        }
                }
        }
}
void ConfigSelDialog::OnRename(wxCommandEvent &event)
{
        wxTextEntryDialog dlg(this, "Enter name:", "Rename config");
        dlg.SetMaxLength(64);
        if (dlg.ShowModal() == wxID_OK)
        {
                wxString new_config_path = GetConfigPath(dlg.GetValue());

                wxListBox *list = (wxListBox*)FindWindow(XRCID("IDC_LIST"));
                wxString old_config_path = GetConfigPath(list->GetStringSelection());

                if (wxFileName(new_config_path).Exists())
                {
                        wxMessageBox("A configuration with that name already exists", "Arculator", wxOK | wxCENTRE | wxSTAY_ON_TOP, this);
                }
                else
                {
                        wxRenameFile(old_config_path, new_config_path, false);
                        BuildConfigList();
                }
        }
}

void ConfigSelDialog::OnCopy(wxCommandEvent &event)
{
        wxTextEntryDialog dlg(this, "Enter name:", "Copy config");
        dlg.SetMaxLength(64);
        if (dlg.ShowModal() == wxID_OK)
        {
                wxString new_config_path = GetConfigPath(dlg.GetValue());

                wxListBox *list = (wxListBox*)FindWindow(XRCID("IDC_LIST"));
                wxString old_config_path = GetConfigPath(list->GetStringSelection());

                if (wxFileName(new_config_path).Exists())
                {
                        wxMessageBox("A configuration with that name already exists", "Arculator", wxOK | wxCENTRE | wxSTAY_ON_TOP, this);
                }
                else
                {
                        wxCopyFile(old_config_path, new_config_path, false);
                        BuildConfigList();
                }
        }
}
void ConfigSelDialog::OnDelete(wxCommandEvent &event)
{
        wxListBox *list = (wxListBox*)FindWindow(XRCID("IDC_LIST"));
        wxString config_name = list->GetStringSelection();

        if (wxMessageBox("Are you sure you want to delete " + config_name + "?", "Arculator", wxYES_NO | wxCENTRE | wxSTAY_ON_TOP, this) == wxYES)
        {
                wxString config_path = GetConfigPath(config_name);
                
                wxRemoveFile(config_path);
                BuildConfigList();
        }
}
void ConfigSelDialog::OnConfig(wxCommandEvent &event)
{
        wxListBox *list = (wxListBox*)FindWindow(XRCID("IDC_LIST"));
        wxString config_path = GetConfigPath(list->GetStringSelection());
        strcpy(machine_config_file, config_path.mb_str());

        loadconfig();
        ShowConfig(false);
}

int ShowConfigSelection()
{
        ConfigSelDialog dlg(NULL);
        
        return dlg.ShowModal();
}
