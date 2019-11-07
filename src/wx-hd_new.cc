/*Arculator 2.0 by Sarah Walker
  New hard disc dialogue*/
#include <wx/wxprec.h>

#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <wx/xrc/xmlres.h>
#include "wx-hd_new.h"

static int MAX_CYLINDERS = 1024;
static int MAX_HEADS = 16;
static int MIN_SECTORS = 1;
static int MAX_SECTORS = 63;
static int SECTOR_SIZE = 512;
#define MAX_SIZE ((MAX_CYLINDERS * MAX_HEADS * MAX_SECTORS) / (1024 * 1024 / SECTOR_SIZE))

class HDNewDialog: public wxDialog
{
public:
	HDNewDialog(wxWindow *parent);

        int new_sectors;
        int new_heads;
        int new_cylinders;
        char new_fn[256];
        
private:
	void OnOK(wxCommandEvent &event);
	void OnCancel(wxCommandEvent &event);

	void OnFile(wxCommandEvent &event);
	void OnCHS(wxCommandEvent &event);
	void OnSize(wxCommandEvent &event);
};

HDNewDialog::HDNewDialog(wxWindow *parent)
{
        wxXmlResource::Get()->LoadDialog(this, parent, "HdNewDlg");

        Bind(wxEVT_BUTTON, &HDNewDialog::OnOK, this, wxID_OK);
        Bind(wxEVT_BUTTON, &HDNewDialog::OnCancel, this, wxID_CANCEL);
        Bind(wxEVT_BUTTON, &HDNewDialog::OnFile, this, XRCID("IDC_CFILE"));
        Bind(wxEVT_TEXT, &HDNewDialog::OnCHS, this, XRCID("IDC_EDIT_SECTORS"));
        Bind(wxEVT_TEXT, &HDNewDialog::OnCHS, this, XRCID("IDC_EDIT_HEADS"));
        Bind(wxEVT_TEXT, &HDNewDialog::OnCHS, this, XRCID("IDC_EDIT_CYLINDERS"));
        Bind(wxEVT_TEXT, &HDNewDialog::OnSize, this, XRCID("IDC_EDIT_SIZE"));

        if (SECTOR_SIZE == 512)
        {
                ((wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_SECTORS")))->SetValue("63");
                ((wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_HEADS")))->SetValue("16");
                ((wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_CYLINDERS")))->SetValue("100");
        }
        else
        {
                ((wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_SECTORS")))->SetValue("32");
                ((wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_HEADS")))->SetValue("8");
                ((wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_CYLINDERS")))->SetValue("615");
        }
        ((wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_SIZE")))->SetValue("40");
}

void HDNewDialog::OnOK(wxCommandEvent &event)
{
        wxString temp_s;
        
        temp_s = ((wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_CYLINDERS")))->GetValue();
        new_cylinders = atoi(temp_s);
        temp_s = ((wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_HEADS")))->GetValue();
        new_heads = atoi(temp_s);
        temp_s = ((wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_SECTORS")))->GetValue();
        new_sectors = atoi(temp_s);
        temp_s = ((wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_FN")))->GetValue();
        strncpy(new_fn, temp_s, sizeof(new_fn));
        
        int total_sectors = new_cylinders * new_heads * new_sectors;
        FILE *f = fopen(new_fn, "wb");
        if (f)
        {
                uint8_t sector_buf[512];
                
                memset(sector_buf, 0, 512);
                
                for (int c = 0; c < total_sectors; c++)
                        fwrite(sector_buf, SECTOR_SIZE, 1, f);

                fclose(f);
                EndModal(1);
        }
        else
        {
                wxMessageBox("Could not create file", "Arculator", wxOK | wxCENTRE | wxSTAY_ON_TOP | wxICON_ERROR, this);
        }
}
void HDNewDialog::OnCancel(wxCommandEvent &event)
{
        EndModal(0);
}

static bool in_callback = false;

void HDNewDialog::OnFile(wxCommandEvent &event)
{
        wxString fn = wxFileSelector("New disc image", "", "", "",
                                     "HDF Disc Image|*.hdf|All Files|*.*", wxFD_SAVE | wxFD_OVERWRITE_PROMPT, this);

        if (!fn.empty())
        {
                ((wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_FN")))->SetValue(fn);
        }
}

void HDNewDialog::OnCHS(wxCommandEvent &event)
{
        wxString temp_s;
        char size_s[80];
        int cylinders, heads, sectors;
        int size;

        if (in_callback)
                return;
        in_callback = true;

        temp_s = ((wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_CYLINDERS")))->GetValue();
        cylinders = atoi(temp_s);
        if (cylinders > MAX_CYLINDERS)
        {
                cylinders = MAX_CYLINDERS;
                snprintf(size_s, sizeof(size_s), "%i", cylinders);
                ((wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_CYLINDERS")))->SetValue(size_s);
        }

        temp_s = ((wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_HEADS")))->GetValue();
        heads = atoi(temp_s);
        if (heads > MAX_HEADS)
        {
                heads = MAX_HEADS;
                snprintf(size_s, sizeof(size_s), "%i", heads);
                ((wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_HEADS")))->SetValue(size_s);
        }

        temp_s = ((wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_SECTORS")))->GetValue();
        sectors = atoi(temp_s);
        if (sectors > MAX_SECTORS)
        {
                sectors = MAX_SECTORS;
                snprintf(size_s, sizeof(size_s), "%i", sectors);
                ((wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_SECTORS")))->SetValue(size_s);

        }
        else if (sectors < MIN_SECTORS)
        {
                sectors = MIN_SECTORS;
                snprintf(size_s, sizeof(size_s), "%i", sectors);
                ((wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_SECTORS")))->SetValue(size_s);

        }

        size = (cylinders * heads * sectors) / (1024 * 1024 / SECTOR_SIZE);
        snprintf(size_s, sizeof(size_s), "%i", size);
        ((wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_SIZE")))->SetValue(size_s);

        in_callback = false;
}

void HDNewDialog::OnSize(wxCommandEvent &event)
{
        wxString temp_s;
        char size_s[80];
        int cylinders, heads, sectors;
        int size;

        if (in_callback)
                return;
        in_callback = true;

        temp_s = ((wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_SIZE")))->GetValue();
        size = atoi(temp_s);
        if (size > MAX_SIZE)
        {
                size = MAX_SIZE;
                snprintf(size_s, sizeof(size_s), "%i", size);
                ((wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_SIZE")))->SetValue(size_s);
        }

        heads = MAX_HEADS;
        sectors = MAX_SECTORS;
        cylinders = (size * (1024 * 1024 / SECTOR_SIZE)) / (MAX_HEADS * MAX_SECTORS);

        snprintf(size_s, sizeof(size_s), "%i", cylinders);
        ((wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_CYLINDERS")))->SetValue(size_s);
        snprintf(size_s, sizeof(size_s), "%i", heads);
        ((wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_HEADS")))->SetValue(size_s);
        snprintf(size_s, sizeof(size_s), "%i", sectors);
        ((wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_SECTORS")))->SetValue(size_s);

        in_callback = false;
}

int ShowNewHD(wxWindow *parent, int *new_sectors, int *new_heads, int *new_cylinders, char *new_fn, int new_fn_size, bool is_st506)
{
        if (is_st506)
        {
                MAX_CYLINDERS = 1024;
                MAX_HEADS = 8;
                MIN_SECTORS = 32;
                MAX_SECTORS = 32;
                SECTOR_SIZE = 256;
        }
        else
        {
                MAX_CYLINDERS = 1024;
                MAX_HEADS = 16;
                MIN_SECTORS = 1;
                MAX_SECTORS = 63;
                SECTOR_SIZE = 512;
        }

        HDNewDialog dlg(parent);
        
        dlg.Fit();
        if (dlg.ShowModal())
        {
                *new_sectors = dlg.new_sectors;
                *new_heads = dlg.new_heads;
                *new_cylinders = dlg.new_cylinders;
                strncpy(new_fn, dlg.new_fn, new_fn_size);
                
                return 1;
        }
        else
                return 0;
}
