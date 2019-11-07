/*Arculator 2.0 by Sarah Walker
  Hard disc configuration dialogue*/
#include <wx/wxprec.h>

#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <wx/xrc/xmlres.h>
#include "wx-hd_conf.h"

static int MAX_CYLINDERS = 1024;
static int MAX_HEADS = 16;
static int MIN_SECTORS = 1;
static int MAX_SECTORS = 63;
static int SECTOR_SIZE = 512;
#define MAX_SIZE ((MAX_CYLINDERS * MAX_HEADS * MAX_SECTORS) / (1024 * 1024 / SECTOR_SIZE))

class HDConfDialog: public wxDialog
{
public:
	HDConfDialog(wxWindow *parent, int new_sectors, int new_heads, int new_cylinders);

        int new_sectors;
        int new_heads;
        int new_cylinders;

private:
	void OnOK(wxCommandEvent &event);
	void OnCancel(wxCommandEvent &event);

	void OnCHS(wxCommandEvent &event);
	void OnSize(wxCommandEvent &event);
};

HDConfDialog::HDConfDialog(wxWindow *parent, int new_sectors, int new_heads, int new_cylinders) :
                new_sectors(new_sectors),
                new_heads(new_heads),
                new_cylinders(new_cylinders)
{
        char temp_s[80];

        wxXmlResource::Get()->LoadDialog(this, parent, "HdConfDlg");

        Bind(wxEVT_BUTTON, &HDConfDialog::OnOK, this, wxID_OK);
        Bind(wxEVT_BUTTON, &HDConfDialog::OnCancel, this, wxID_CANCEL);
        Bind(wxEVT_TEXT, &HDConfDialog::OnCHS, this, XRCID("IDC_EDIT_SECTORS"));
        Bind(wxEVT_TEXT, &HDConfDialog::OnCHS, this, XRCID("IDC_EDIT_HEADS"));
        Bind(wxEVT_TEXT, &HDConfDialog::OnCHS, this, XRCID("IDC_EDIT_CYLINDERS"));
        Bind(wxEVT_TEXT, &HDConfDialog::OnSize, this, XRCID("IDC_EDIT_SIZE"));

        sprintf(temp_s, "%i", new_sectors);
        ((wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_SECTORS")))->SetValue(temp_s);
        sprintf(temp_s, "%i", new_heads);
        ((wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_HEADS")))->SetValue(temp_s);
        sprintf(temp_s, "%i", new_cylinders);
        ((wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_CYLINDERS")))->SetValue(temp_s);
}

void HDConfDialog::OnOK(wxCommandEvent &event)
{
        wxString temp_s;

        temp_s = ((wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_CYLINDERS")))->GetValue();
        new_cylinders = atoi(temp_s);
        temp_s = ((wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_HEADS")))->GetValue();
        new_heads = atoi(temp_s);
        temp_s = ((wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_SECTORS")))->GetValue();
        new_sectors = atoi(temp_s);

        EndModal(1);
}
void HDConfDialog::OnCancel(wxCommandEvent &event)
{
        EndModal(0);
}

static bool in_callback = false;

void HDConfDialog::OnCHS(wxCommandEvent &event)
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

void HDConfDialog::OnSize(wxCommandEvent &event)
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

int ShowConfHD(wxWindow *parent, int *new_sectors, int *new_heads, int *new_cylinders, char *new_fn, bool is_st506)
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

        FILE *f = fopen(new_fn, "rb");
        if (!f)
        {
                wxMessageBox("Could not access file", "Arculator", wxOK | wxCENTRE | wxSTAY_ON_TOP | wxICON_ERROR, parent);
                return 0;
        }

        fseek(f, -1, SEEK_END);
        int filesize = ftell(f) + 1;
        fseek(f, 0, SEEK_SET);

        int log2secsize, density;

        /*Try to detect drive size geometry disc record. Valid disc record
          will have log2secsize of 8 (256 bytes per sector) or 9 (512
          bytes per sector), density of 0, and sector and head counts
          within valid range*/
        /*Initially assume stupid 512 byte header created by RPCemu and
          older Arculator*/
        fseek(f, 0xFC0, SEEK_SET);
        log2secsize = getc(f);
        *new_sectors = getc(f);
        *new_heads = getc(f);
        density = getc(f);

        if ((log2secsize != 8 && log2secsize != 9) || !(*new_sectors) || !(*new_heads) || (*new_sectors) > MAX_SECTORS || (*new_heads) > MAX_HEADS || density != 0)
        {
                /*Invalid geometry, try without header*/
                fseek(f, 0xDC0, SEEK_SET);
                log2secsize = getc(f);
                *new_sectors = getc(f);
                *new_heads = getc(f);
                density = getc(f);

                if ((log2secsize != 8 && log2secsize != 9) || !(*new_sectors) || !(*new_heads) || (*new_sectors) > MAX_SECTORS || (*new_heads) > MAX_HEADS || density != 0)
                {
                        /*Invalid geometry, assume max*/
                        *new_sectors = MAX_SECTORS;
                        *new_heads = MAX_HEADS;
                }
        }
        else
                filesize -= 512; /*Account for header*/

        fclose(f);

//                rpclog("sectors=%i, heads=%i\n", new_sectors, new_heads);
        *new_cylinders = filesize / (SECTOR_SIZE * *new_sectors * *new_heads);

        HDConfDialog dlg(parent, *new_sectors, *new_heads, *new_cylinders);

        dlg.Fit();
        if (dlg.ShowModal())
        {
                *new_sectors = dlg.new_sectors;
                *new_heads = dlg.new_heads;
                *new_cylinders = dlg.new_cylinders;

                return 1;
        }
        else
                return 0;
}
