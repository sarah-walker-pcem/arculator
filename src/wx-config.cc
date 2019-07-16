#include <wx/wxprec.h>

#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <wx/xrc/xmlres.h>
#include "wx-config.h"

extern "C"
{
        #include "arc.h"
        #include "arm.h"
        #include "config.h"
        #include "fpa.h"
        #include "memc.h"
        #include "podules.h"
        #include "st506.h"
};

enum
{
        CPU_ARM2 = 0,
        CPU_ARM250,
        CPU_ARM3_20,
        CPU_ARM3_25,
        CPU_ARM3_26,
        CPU_ARM3_30,
        CPU_ARM3_33,
        CPU_ARM3_35
};

enum
{
        FPU_NONE = 0,
        FPU_FPPC,
        FPU_FPA10
};

enum
{
        MEMC_MEMC1 = 0,
        MEMC_MEMC1A_8,
        MEMC_MEMC1A_12,
        MEMC_MEMC1A_16,
};

enum
{
        IO_OLD = 0,
        IO_OLD_ST506,
        IO_NEW
};

enum
{
        MEM_512K = 0,
        MEM_1M,
        MEM_2M,
        MEM_4M,
        MEM_8M,
        MEM_16M
};

enum
{
        ROM_ARTHUR = 0,
        ROM_RISCOS_2,
        ROM_RISCOS_3
};

static struct
{
        const char *name;
        int cpu, fpu, memc, io, mem;
} presets[] =
{
        {"Archimedes 305",   CPU_ARM2,    FPU_NONE, MEMC_MEMC1,     IO_OLD,       MEM_512K},
        {"Archimedes 310",   CPU_ARM2,    FPU_NONE, MEMC_MEMC1,     IO_OLD,       MEM_1M},
        {"Archimedes 440",   CPU_ARM2,    FPU_NONE, MEMC_MEMC1,     IO_OLD_ST506, MEM_4M},
        {"Archimedes 410/1", CPU_ARM2,    FPU_NONE, MEMC_MEMC1A_8,  IO_OLD_ST506, MEM_1M},
        {"Archimedes 420/1", CPU_ARM2,    FPU_NONE, MEMC_MEMC1A_8,  IO_OLD_ST506, MEM_2M},
        {"Archimedes 440/1", CPU_ARM2,    FPU_NONE, MEMC_MEMC1A_8,  IO_OLD_ST506, MEM_4M},
        {"A3000",            CPU_ARM2,    FPU_NONE, MEMC_MEMC1A_8,  IO_OLD,       MEM_1M},
        {"Archimedes 540",   CPU_ARM3_26, FPU_NONE, MEMC_MEMC1A_12, IO_OLD,       MEM_4M},
        {"A5000",            CPU_ARM3_25, FPU_NONE, MEMC_MEMC1A_12, IO_NEW,       MEM_2M},
        {"A3010",            CPU_ARM250,  FPU_NONE, MEMC_MEMC1A_12, IO_NEW,       MEM_1M},
        {"A3020",            CPU_ARM250,  FPU_NONE, MEMC_MEMC1A_12, IO_NEW,       MEM_2M},
        {"A4000",            CPU_ARM250,  FPU_NONE, MEMC_MEMC1A_12, IO_NEW,       MEM_2M},
        {"A5000a",           CPU_ARM3_33, FPU_NONE, MEMC_MEMC1A_12, IO_NEW,       MEM_4M},
        {"", 0, 0, 0, 0, 0}
};

class ConfigDialog: public wxDialog
{
public:
	ConfigDialog(wxWindow *parent, bool is_running);
	ConfigDialog(wxWindow *parent, bool is_running, int preset);
private:
	void OnOK(wxCommandEvent &event);
	void OnCancel(wxCommandEvent &event);
	void OnPreset(wxCommandEvent &event);
	void OnComboCPU(wxCommandEvent &event);
	void OnComboFPU(wxCommandEvent &event);
	void OnComboIO(wxCommandEvent &event);
	void OnComboMEMC(wxCommandEvent &event);
	void OnComboMemory(wxCommandEvent &event);
	void OnComboOS(wxCommandEvent &event);
        void OnComboPodule(wxCommandEvent &event);
	void OnHDSel(wxCommandEvent &event);
	void OnHDNew(wxCommandEvent &event);
	void OnHDEject(wxCommandEvent &event);

        void CommonInit(wxWindow *parent, bool is_running);
	void UpdateList(int cpu, int mem, int memc, int fpu, int io);
	void PopulatePoduleList(int nr, wxComboBox *cbox);
	void PopulatePoduleLists(void);
	
	int config_cpu, config_mem, config_memc, config_fpu, config_io, config_rom;
        wxString hd_fns[2];
        char config_podules[4][16];
	
	bool running;
};

void ConfigDialog::CommonInit(wxWindow *parent, bool is_running)
{
        running = is_running;
        wxXmlResource::Get()->LoadDialog(this, parent, "ConfigureDlg");

        Bind(wxEVT_BUTTON, &ConfigDialog::OnOK, this, wxID_OK);
        Bind(wxEVT_BUTTON, &ConfigDialog::OnCancel, this, wxID_CANCEL);
        Bind(wxEVT_BUTTON, &ConfigDialog::OnPreset, this, XRCID("IDC_LOAD_PRESET"));
        Bind(wxEVT_COMBOBOX, &ConfigDialog::OnComboCPU, this, XRCID("IDC_COMBO_CPU"));
        Bind(wxEVT_COMBOBOX, &ConfigDialog::OnComboFPU, this, XRCID("IDC_COMBO_FPU"));
        Bind(wxEVT_COMBOBOX, &ConfigDialog::OnComboIO, this, XRCID("IDC_COMBO_IO"));
        Bind(wxEVT_COMBOBOX, &ConfigDialog::OnComboMEMC, this, XRCID("IDC_COMBO_MEMC"));
        Bind(wxEVT_COMBOBOX, &ConfigDialog::OnComboMemory, this, XRCID("IDC_COMBO_MEMORY"));
        Bind(wxEVT_COMBOBOX, &ConfigDialog::OnComboOS, this, XRCID("IDC_COMBO_OS"));
        Bind(wxEVT_COMBOBOX, &ConfigDialog::OnComboPodule, this, XRCID("IDC_COMBO_PODULE0"));
        Bind(wxEVT_COMBOBOX, &ConfigDialog::OnComboPodule, this, XRCID("IDC_COMBO_PODULE1"));
        Bind(wxEVT_COMBOBOX, &ConfigDialog::OnComboPodule, this, XRCID("IDC_COMBO_PODULE2"));
        Bind(wxEVT_COMBOBOX, &ConfigDialog::OnComboPodule, this, XRCID("IDC_COMBO_PODULE3"));
        Bind(wxEVT_BUTTON, &ConfigDialog::OnHDSel, this, XRCID("IDC_SEL_HD4"));
        Bind(wxEVT_BUTTON, &ConfigDialog::OnHDSel, this, XRCID("IDC_SEL_HD5"));
        Bind(wxEVT_BUTTON, &ConfigDialog::OnHDNew, this, XRCID("IDC_NEW_HD4"));
        Bind(wxEVT_BUTTON, &ConfigDialog::OnHDNew, this, XRCID("IDC_NEW_HD5"));
        Bind(wxEVT_BUTTON, &ConfigDialog::OnHDEject, this, XRCID("IDC_EJECT_HD4"));
        Bind(wxEVT_BUTTON, &ConfigDialog::OnHDEject, this, XRCID("IDC_EJECT_HD5"));
        
        hd_fns[0] = wxString(hd_fn[0]);
        hd_fns[1] = wxString(hd_fn[1]);

        wxTextCtrl *tctrl = (wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_HD4"));
        tctrl->SetValue(hd_fns[0]);
        tctrl = (wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_HD5"));
        tctrl->SetValue(hd_fns[1]);
}

ConfigDialog::ConfigDialog(wxWindow *parent, bool is_running)
{
        int c;
        
        CommonInit(parent, is_running);

        config_cpu = arm_cpu_type;
        config_fpu = fpaena ? (fpu_type ? FPU_FPPC : FPU_FPA10) : FPU_NONE;
        config_memc = memc_type;
        config_io = fdctype ? IO_NEW : (st506_present ? IO_OLD_ST506 : IO_OLD);

        switch (memsize)
        {
                case   512:
                config_mem = MEM_512K;
                break;
                case   1024:
                config_mem = MEM_1M;
                break;
                case   2048:
                config_mem = MEM_2M;
                break;
                case   4096:
                config_mem = MEM_4M;
                break;
                case   8192:
                config_mem = MEM_8M;
                break;
                case   16384:
                config_mem = MEM_16M;
                break;
        }

        if (config_cpu == CPU_ARM250 || romset == 3)
                config_rom = ROM_RISCOS_3;
        else
                config_rom = romset;

        UpdateList(config_cpu, config_mem, config_memc, config_fpu, config_io);

        for (c = 0; c < 4; c++)
                strncpy(config_podules[c], podule_names[c], 15);

        PopulatePoduleLists();
}

ConfigDialog::ConfigDialog(wxWindow *parent, bool is_running, int preset)
{
        CommonInit(parent, is_running);

        config_cpu  = presets[preset].cpu;
        config_mem  = presets[preset].mem;
        config_memc = presets[preset].memc;
        config_fpu  = presets[preset].fpu;
        config_io   = presets[preset].io;
        
        UpdateList(config_cpu, config_mem, config_memc, config_fpu, config_io);
        
        /*Default to HostFS in the first podule slot, and the rest empty*/
        strncpy(config_podules[0], "arculator_rom", 15);
        strncpy(config_podules[1], "", 15);
        strncpy(config_podules[2], "", 15);
        strncpy(config_podules[3], "", 15);

        PopulatePoduleLists();
}

void ConfigDialog::UpdateList(int cpu, int mem, int memc, int fpu, int io)
{
        wxComboBox *cbox = (wxComboBox *)this->FindWindow(XRCID("IDC_COMBO_CPU"));
        cbox->SetValue("");
        cbox->Clear();
        cbox->Append("ARM2");
        cbox->Append("ARM250");
        cbox->Append("ARM3 @ 20 MHz");
        cbox->Append("ARM3 @ 25 MHz");
        cbox->Append("ARM3 @ 26 MHz");
        cbox->Append("ARM3 @ 30 MHz");
        cbox->Append("ARM3 @ 33 MHz");
        cbox->Append("ARM3 @ 35 MHz");
        cbox->Select(cpu);

        cbox = (wxComboBox *)this->FindWindow(XRCID("IDC_COMBO_MEMORY"));
        cbox->SetValue("");
        cbox->Clear();
        cbox->Append("512 kB");
        cbox->Append("1 MB");
        cbox->Append("2 MB");
        cbox->Append("4 MB");
        if (cpu != CPU_ARM250)
        {
                cbox->Append("8 MB");
                cbox->Append("16 MB");
        }
        cbox->Select(mem);
        
        cbox = (wxComboBox *)this->FindWindow(XRCID("IDC_COMBO_MEMC"));
        cbox->SetValue("");
        cbox->Clear();
        if (cpu == CPU_ARM2)
                cbox->Append("MEMC1");
        if (cpu != CPU_ARM250)
                cbox->Append("MEMC1a (8 MHz)");
        cbox->Append("MEMC1a (12 MHz)");
        cbox->Append("MEMC1a (16 MHz - overclocked)");
        cbox->Select((cpu == CPU_ARM250) ? memc-2 : ((cpu != CPU_ARM2) ? memc-1 : memc));

        cbox = (wxComboBox *)this->FindWindow(XRCID("IDC_COMBO_FPU"));
        cbox->SetValue("");
        cbox->Clear();
        cbox->Append("None");
        if (cpu == CPU_ARM2 && memc != MEMC_MEMC1)
                cbox->Append("FPPC");
        if (cpu != CPU_ARM2 && cpu != CPU_ARM250)
                cbox->Append("FPA10");
        cbox->Select(fpu ? 1 : 0);

        cbox = (wxComboBox *)this->FindWindow(XRCID("IDC_COMBO_IO"));
        cbox->SetValue("");
        cbox->Clear();
        if (cpu != CPU_ARM250)
        {
                cbox->Append("Old (1772)");
                cbox->Append("Old + ST506");
        }
        if (memc >= MEMC_MEMC1A_8 && cpu != CPU_ARM2)
                cbox->Append("New (SuperIO)");
        cbox->Select((cpu == CPU_ARM250) ? io-2 : io);

        cbox = (wxComboBox *)this->FindWindow(XRCID("IDC_COMBO_OS"));
        cbox->SetValue("");
        cbox->Clear();
        if (cpu != CPU_ARM250)
        {
                cbox->Append("Arthur");
                cbox->Append("RISC OS 2");
        }
        cbox->Append("RISC OS 3");
        cbox->Select((cpu == CPU_ARM250) ? config_rom-2 : config_rom);
}

void ConfigDialog::PopulatePoduleList(int slot_nr, wxComboBox *cbox)
{
        int c = 0;
        
        cbox->Clear();
        cbox->Append("None");
        cbox->SetValue("None");
        while (podule_get_name(c))
        {
                cbox->Append(podule_get_name(c));
                if (!strcmp(config_podules[slot_nr], podule_get_short_name(c)))
                        cbox->SetValue(podule_get_name(c));
                c++;
        }
}

void ConfigDialog::PopulatePoduleLists(void)
{
        PopulatePoduleList(0, (wxComboBox *)this->FindWindow(XRCID("IDC_COMBO_PODULE0")));
        PopulatePoduleList(1, (wxComboBox *)this->FindWindow(XRCID("IDC_COMBO_PODULE1")));
        PopulatePoduleList(2, (wxComboBox *)this->FindWindow(XRCID("IDC_COMBO_PODULE2")));
        PopulatePoduleList(3, (wxComboBox *)this->FindWindow(XRCID("IDC_COMBO_PODULE3")));

        ((wxComboBox *)this->FindWindow(XRCID("IDC_CONFIG_PODULE0")))->Enable(false);
        ((wxComboBox *)this->FindWindow(XRCID("IDC_CONFIG_PODULE1")))->Enable(false);
        ((wxComboBox *)this->FindWindow(XRCID("IDC_CONFIG_PODULE2")))->Enable(false);
        ((wxComboBox *)this->FindWindow(XRCID("IDC_CONFIG_PODULE3")))->Enable(false);
}

void ConfigDialog::OnOK(wxCommandEvent &event)
{
        int c;
        
        if (running)
        {
                if (wxMessageBox("This will reset Arculator!\nOkay to continue?", "Arculator", wxYES_NO | wxCENTRE | wxSTAY_ON_TOP, this) != wxYES)
                        return;
        }

        switch (config_memc)
        {
                case MEMC_MEMC1:
                memc_is_memc1 = 1;
                arm_mem_speed = 8;
                break;
                case MEMC_MEMC1A_8:
                memc_is_memc1 = 0;
                arm_mem_speed = 8;
                break;
                case MEMC_MEMC1A_12:
                memc_is_memc1 = 0;
                arm_mem_speed = 12;
                break;
                case MEMC_MEMC1A_16:
                memc_is_memc1 = 0;
                arm_mem_speed = 16;
                break;
        }
        memc_type = config_memc;

        switch (config_cpu)
        {
                case CPU_ARM2:
                arm_has_swp = arm_has_cp15 = 0;
                arm_cpu_speed = arm_mem_speed;
                break;
                case CPU_ARM250:
                arm_has_swp = 1;
                arm_has_cp15 = 0;
                arm_cpu_speed = arm_mem_speed;
                break;
                case CPU_ARM3_20:
                arm_has_swp = arm_has_cp15 = 1;
                arm_cpu_speed = 20;
                break;
                case CPU_ARM3_25:
                arm_has_swp = arm_has_cp15 = 1;
                arm_cpu_speed = 25;
                break;
                case CPU_ARM3_26:
                arm_has_swp = arm_has_cp15 = 1;
                arm_cpu_speed = 26;
                break;
                case CPU_ARM3_30:
                arm_has_swp = arm_has_cp15 = 1;
                arm_cpu_speed = 30;
                break;
                case CPU_ARM3_33:
                arm_has_swp = arm_has_cp15 = 1;
                arm_cpu_speed = 33;
                break;
                case CPU_ARM3_35:
                arm_has_swp = arm_has_cp15 = 1;
                arm_cpu_speed = 35;
                break;
        }
        arm_cpu_type = config_cpu;

        fpaena = (config_fpu == FPU_NONE) ? 0 : 1;
        fpu_type = (config_cpu >= CPU_ARM3_20) ? 0 : 1;
        fdctype = (config_io >= IO_NEW) ? 1 : 0;
        st506_present = (config_io == IO_OLD_ST506) ? 1 : 0;

        switch (config_mem)
        {
                case MEM_512K:
                memsize = 512;
                break;
                case MEM_1M:
                memsize = 1024;
                break;
                case MEM_2M:
                memsize = 2048;
                break;
                case MEM_4M:
                memsize = 4096;
                break;
                case MEM_8M:
                memsize = 8192;
                break;
                case MEM_16M:
                memsize = 16384;
                break;
        }

        if (config_io == IO_NEW && config_rom == ROM_RISCOS_3)
                romset = 3;
        else
                romset = config_rom;

        wxTextCtrl *tctrl = (wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_HD4"));
        strcpy(hd_fn[0], tctrl->GetValue().mb_str());
        tctrl = (wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_HD5"));
        strcpy(hd_fn[1], tctrl->GetValue().mb_str());

        for (c = 0; c < 4; c++)
                strncpy(podule_names[c], config_podules[c], 15);
                
        saveconfig();
        if (running)
                arc_reset();

        EndModal(0);
}
void ConfigDialog::OnCancel(wxCommandEvent &event)
{
        EndModal(-1);
}
void ConfigDialog::OnPreset(wxCommandEvent &event)
{
        int preset = ShowPresetList();
        
        if (preset != -1)
        {
                config_cpu  = presets[preset].cpu;
                config_mem  = presets[preset].mem;
                config_memc = presets[preset].memc;
                config_fpu  = presets[preset].fpu;
                config_io   = presets[preset].io;

                UpdateList(config_cpu, config_mem, config_memc, config_fpu, config_io);
        }
}

void ConfigDialog::OnComboCPU(wxCommandEvent &event)
{
        wxComboBox *cbox = (wxComboBox *)this->FindWindow(event.GetId());
        
        config_cpu = cbox->GetCurrentSelection();
        if (config_cpu == CPU_ARM250)
        {
                /*ARM250 only supports 4MB, no FPA, new IO*/
                if (config_memc < MEMC_MEMC1A_12)
                        config_memc = MEMC_MEMC1A_12;
                config_fpu = FPU_NONE;
                config_io = IO_NEW;
                if (config_mem > MEM_4M)
                        config_mem = MEM_4M;
        }
        else if (config_cpu == CPU_ARM2)
        {
                /*ARM2 does not support FPA*/
                if (config_fpu != FPU_NONE)
                        config_fpu = FPU_FPPC;
                if (config_io == IO_NEW)
                        config_io = IO_OLD_ST506;
        }
        else
        {
                /*ARM3 only supports MEMC1A*/
                if (config_fpu != FPU_NONE)
                        config_fpu = FPU_FPA10;
                if (config_memc == MEMC_MEMC1)
                        config_memc = MEMC_MEMC1A_8;
        }
        UpdateList(config_cpu, config_mem, config_memc, config_fpu, config_io);
}

void ConfigDialog::OnComboMemory(wxCommandEvent &event)
{
        wxComboBox *cbox = (wxComboBox *)this->FindWindow(event.GetId());

        config_mem = cbox->GetCurrentSelection();
}

void ConfigDialog::OnComboMEMC(wxCommandEvent &event)
{
        wxComboBox *cbox = (wxComboBox *)this->FindWindow(event.GetId());

        config_memc = cbox->GetCurrentSelection();
        if (config_cpu == CPU_ARM250)
                config_memc += 2;
        else if (config_cpu != CPU_ARM2)
                config_memc++;
        if (config_memc == MEMC_MEMC1)
                config_fpu = FPU_NONE;
        UpdateList(config_cpu, config_mem, config_memc, config_fpu, config_io);
}

void ConfigDialog::OnComboFPU(wxCommandEvent &event)
{
        wxComboBox *cbox = (wxComboBox *)this->FindWindow(event.GetId());

        config_fpu = cbox->GetCurrentSelection();
}

void ConfigDialog::OnComboIO(wxCommandEvent &event)
{
        wxComboBox *cbox = (wxComboBox *)this->FindWindow(event.GetId());

        config_io = cbox->GetCurrentSelection();
        if (config_cpu == CPU_ARM250)
                config_io = IO_NEW;
        UpdateList(config_cpu, config_mem, config_memc, config_fpu, config_io);
}

void ConfigDialog::OnComboOS(wxCommandEvent &event)
{
        wxComboBox *cbox = (wxComboBox *)this->FindWindow(event.GetId());

        config_rom = cbox->GetCurrentSelection();
        if (config_cpu == CPU_ARM250)
                config_rom = ROM_RISCOS_3;
        if (config_rom < ROM_RISCOS_3 && config_io == IO_NEW)
                config_io = IO_OLD_ST506;
        UpdateList(config_cpu, config_mem, config_memc, config_fpu, config_io);
}

void ConfigDialog::OnHDSel(wxCommandEvent &event)
{
        int hd_nr = (event.GetId() == XRCID("IDC_SEL_HD4")) ? 0 : 1;
        
        wxFileDialog dlg(NULL, "Select a disc image", "", hd_fns[hd_nr],
                        "HDF Disc Image|*.hdf|All Files|*.*",
                        wxFD_OPEN | wxFD_FILE_MUST_EXIST);

        if (dlg.ShowModal() == wxID_OK)
        {
                wxTextCtrl *tctrl;

                hd_fns[hd_nr] = dlg.GetPath();
                
                if (hd_nr)
                        tctrl = (wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_HD5"));
                else
                        tctrl = (wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_HD4"));
                tctrl->SetValue(hd_fns[hd_nr]);
        }
}
void ConfigDialog::OnHDNew(wxCommandEvent &event)
{
        int hd_nr = (event.GetId() == XRCID("IDC_NEW_HD4")) ? 0 : 1;

        wxFileDialog dlg(NULL, "New a disc image", "", hd_fns[hd_nr],
                        "HDF Disc Image|*.hdf|All Files|*.*",
                        wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

        if (dlg.ShowModal() == wxID_OK)
        {
                wxString new_fn_str = dlg.GetPath();
                FILE *f;

                f = fopen(new_fn_str.mb_str(), "wb");
                if (f)
                {
                        wxTextCtrl *tctrl;

                        putc(0, f);
                        fclose(f);

                        hd_fns[hd_nr] = new_fn_str;

                        if (hd_nr)
                                tctrl = (wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_HD5"));
                        else
                                tctrl = (wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_HD4"));
                        tctrl->SetValue(hd_fns[hd_nr]);
                }
                else
                        wxMessageBox("Could not create " + new_fn_str, "Arculator", wxOK | wxCENTRE | wxSTAY_ON_TOP, this);
        }
}
void ConfigDialog::OnHDEject(wxCommandEvent &event)
{
        wxTextCtrl *tctrl;
        int hd_nr = (event.GetId() == XRCID("IDC_EJECT_HD4")) ? 0 : 1;

        if (hd_nr)
                tctrl = (wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_HD5"));
        else
                tctrl = (wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_HD4"));
        tctrl->SetValue("");
}

void ConfigDialog::OnComboPodule(wxCommandEvent &event)
{
        wxComboBox *cbox = (wxComboBox *)this->FindWindow(event.GetId());
        int sel_nr = cbox->GetCurrentSelection();
        int slot_nr;
        
        if (event.GetId() == XRCID("IDC_COMBO_PODULE0"))
                slot_nr = 0;
        else if (event.GetId() == XRCID("IDC_COMBO_PODULE1"))
                slot_nr = 1;
        else if (event.GetId() == XRCID("IDC_COMBO_PODULE2"))
                slot_nr = 2;
        else
                slot_nr = 3;

        if (!sel_nr)
                strcpy(config_podules[slot_nr], "");
        else
        {
                strncpy(config_podules[slot_nr], podule_get_short_name(sel_nr-1), 15);

                if (podule_get_flags(sel_nr-1) & PODULE_FLAGS_UNIQUE)
                {
                        /*Clear out any duplicates*/
                        int c;

                        for (c = 0; c < 4; c++)
                        {
                                if (c != slot_nr && !strcmp(config_podules[c], config_podules[slot_nr]))
                                {
                                        strcpy(config_podules[c], "");
                                        switch (c)
                                        {
                                                case 0:
                                                cbox = (wxComboBox *)this->FindWindow(XRCID("IDC_COMBO_PODULE0"));
                                                break;
                                                case 1:
                                                cbox = (wxComboBox *)this->FindWindow(XRCID("IDC_COMBO_PODULE1"));
                                                break;
                                                case 2:
                                                cbox = (wxComboBox *)this->FindWindow(XRCID("IDC_COMBO_PODULE2"));
                                                break;
                                                case 3:
                                                cbox = (wxComboBox *)this->FindWindow(XRCID("IDC_COMBO_PODULE3"));
                                                break;
                                        }
                                        cbox->Select(0);
                                }
                        }
                }
        }
}



int ShowConfig(bool running)
{
        ConfigDialog dlg(NULL, running);

        return dlg.ShowModal();
}

int ShowPresetList()
{
        wxArrayString achoices;
        int c = 0;
        
        c = 0;
        while (presets[c].name[0])
                achoices.Add(presets[c++].name);

        return wxGetSingleChoiceIndex("Please select a machine type", "Arculator", achoices);
}

void ShowConfigWithPreset(int preset)
{
        ConfigDialog dlg(NULL, false, preset);

        dlg.ShowModal();
}
