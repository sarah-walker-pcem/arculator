/*Arculator 2.0 by Sarah Walker
  Machine configuration dialogue*/
#include <wx/wxprec.h>

#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <wx/xrc/xmlres.h>
#include "wx-config.h"
#include "wx-hd_conf.h"
#include "wx-hd_new.h"
#include "wx-joystick-config.h"
#include "wx-podule-config.h"

extern "C"
{
        #include "arc.h"
        #include "arm.h"
        #include "config.h"
        #include "fpa.h"
        #include "joystick.h"
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
        CPU_ARM3_35,
        CPU_MAX
};

const char *cpu_names[] =
{
        "ARM2",
        "ARM250",
        "ARM3 @ 20 MHz",
        "ARM3 @ 25 MHz",
        "ARM3 @ 26 MHz",
        "ARM3 @ 30 MHz",
        "ARM3 @ 33 MHz",
        "ARM3 @ 35 MHz"
};

enum
{
        CPU_MASK_ARM2    = (1 << CPU_ARM2),
        CPU_MASK_ARM250  = (1 << CPU_ARM250),
        CPU_MASK_ARM3_20 = (1 << CPU_ARM3_20),
        CPU_MASK_ARM3_25 = (1 << CPU_ARM3_25),
        CPU_MASK_ARM3_26 = (1 << CPU_ARM3_26),
        CPU_MASK_ARM3_30 = (1 << CPU_ARM3_30),
        CPU_MASK_ARM3_33 = (1 << CPU_ARM3_33),
        CPU_MASK_ARM3_35 = (1 << CPU_ARM3_35)
};

#define CPU_ARM2_AND_LATER (CPU_MASK_ARM2 | CPU_MASK_ARM3_20 | CPU_MASK_ARM3_25 | \
                CPU_MASK_ARM3_26 | CPU_MASK_ARM3_30 | CPU_MASK_ARM3_33 | \
                CPU_MASK_ARM3_35)

#define CPU_ARM250_ONLY (CPU_MASK_ARM250)

#define CPU_ARM3_25_AND_LATER (CPU_MASK_ARM3_25 | CPU_MASK_ARM3_26 | \
                CPU_MASK_ARM3_30 | CPU_MASK_ARM3_33 | CPU_MASK_ARM3_35)

#define CPU_ARM3_26_AND_LATER (CPU_MASK_ARM3_26 | CPU_MASK_ARM3_30 | \
                CPU_MASK_ARM3_33 | CPU_MASK_ARM3_35)

#define CPU_ARM3_33_AND_LATER (CPU_MASK_ARM3_33 | CPU_MASK_ARM3_35)

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
        MEMC_MAX
};

const char *memc_names[] =
{
        "MEMC1",
        "MEMC1a (8 MHz)",
        "MEMC1a (12 MHz)",
        "MEMC1a (16 MHz - overclocked)"
};

enum
{
        MEMC_MASK_MEMC1     = (1 << MEMC_MEMC1),
        MEMC_MASK_MEMC1A_8  = (1 << MEMC_MEMC1A_8),
        MEMC_MASK_MEMC1A_12 = (1 << MEMC_MEMC1A_12),
        MEMC_MASK_MEMC1A_16 = (1 << MEMC_MEMC1A_16)
};

#define MEMC_MIN_MEMC1     (MEMC_MASK_MEMC1 | MEMC_MASK_MEMC1A_8 | \
                MEMC_MASK_MEMC1A_12 | MEMC_MASK_MEMC1A_16)
#define MEMC_MIN_MEMC1A    (MEMC_MASK_MEMC1A_8 | MEMC_MASK_MEMC1A_12 | \
                MEMC_MASK_MEMC1A_16)
#define MEMC_MIN_MEMC1A_12 (MEMC_MASK_MEMC1A_12 | MEMC_MASK_MEMC1A_16)

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
        MEM_16M,
        MEM_MAX
};

const char *mem_names[] =
{
        "512 kB",
        "1 MB",
        "2 MB",
        "4 MB",
        "8 MB",
        "16 MB"
};

enum
{
        MEM_MASK_512K = (1 << MEM_512K),
        MEM_MASK_1M   = (1 << MEM_1M),
        MEM_MASK_2M   = (1 << MEM_2M),
        MEM_MASK_4M   = (1 << MEM_4M),
        MEM_MASK_8M   = (1 << MEM_8M),
        MEM_MASK_16M  = (1 << MEM_16M)
};

#define MEM_MIN_512K (MEM_MASK_512K | MEM_MASK_1M | MEM_MASK_2M | MEM_MASK_4M | \
                MEM_MASK_8M | MEM_MASK_16M)

#define MEM_MIN_1M (MEM_MASK_1M | MEM_MASK_2M | MEM_MASK_4M | MEM_MASK_8M | \
                MEM_MASK_16M)

#define MEM_MIN_2M (MEM_MASK_2M | MEM_MASK_4M | MEM_MASK_8M | MEM_MASK_16M)

#define MEM_MIN_4M (MEM_MASK_4M | MEM_MASK_8M | MEM_MASK_16M)

#define MEM_1M_4M  (MEM_MASK_1M | MEM_MASK_2M | MEM_MASK_4M)
#define MEM_2M_4M  (MEM_MASK_2M | MEM_MASK_4M)

const char *rom_names[] =
{
        "Arthur 0.30",
        "Arthur 1.20",
        "RISC OS 2.00",
        "RISC OS 2.01",
        "RISC OS 3.00",
        "RISC OS 3.10",
        "RISC OS 3.11",
        "RISC OS 3.19"
};

enum
{
        ROM_ARTHUR_030_MASK = (1 << ROM_ARTHUR_030),
        ROM_ARTHUR_120_MASK = (1 << ROM_ARTHUR_120),
        ROM_RISCOS_200_MASK = (1 << ROM_RISCOS_200),
        ROM_RISCOS_201_MASK = (1 << ROM_RISCOS_201),
        ROM_RISCOS_300_MASK = (1 << ROM_RISCOS_300),
        ROM_RISCOS_310_MASK = (1 << ROM_RISCOS_310),
        ROM_RISCOS_311_MASK = (1 << ROM_RISCOS_311),
        ROM_RISCOS_319_MASK = (1 << ROM_RISCOS_319)
};

/*Arthur and later - Archimedes 305, 310, 440*/
#define ROM_ALL (ROM_ARTHUR_030_MASK | ROM_ARTHUR_120_MASK | ROM_RISCOS_200_MASK | \
                ROM_RISCOS_201_MASK | ROM_RISCOS_300_MASK | ROM_RISCOS_310_MASK | \
                ROM_RISCOS_311_MASK | ROM_RISCOS_319_MASK)

/*RISC OS 2.00 and later - Archimedes 410/1, 420/1, 440/1, A3000*/
#define ROM_RISCOS (ROM_RISCOS_200_MASK | ROM_RISCOS_201_MASK | ROM_RISCOS_300_MASK | \
                ROM_RISCOS_310_MASK | ROM_RISCOS_311_MASK | ROM_RISCOS_319_MASK)

/*RISC OS 2.01 and later - Archimedes 540*/
#define ROM_RISCOS201 (ROM_RISCOS_201_MASK | ROM_RISCOS_300_MASK | ROM_RISCOS_310_MASK | \
                ROM_RISCOS_311_MASK | ROM_RISCOS_319_MASK)

/*RISC OS 3.00 and later - A5000*/
#define ROM_RISCOS3 (ROM_RISCOS_300_MASK | ROM_RISCOS_310_MASK | ROM_RISCOS_311_MASK | \
                ROM_RISCOS_319_MASK)

/*RISC OS 3.10 and later - A3010, A3020, A4000, A5000a*/
#define ROM_RISCOS31 (ROM_RISCOS_310_MASK | ROM_RISCOS_311_MASK | ROM_RISCOS_319_MASK)

const char *monitor_names[] =
{
        "Standard",
        "Multisync",
        "VGA",
        "High res mono"
};

enum
{
        MONITOR_STANDARD_MASK  = (1 << MONITOR_STANDARD),
        MONITOR_MULTISYNC_MASK = (1 << MONITOR_MULTISYNC),
        MONITOR_VGA_MASK       = (1 << MONITOR_VGA),
        MONITOR_MONO_MASK      = (1 << MONITOR_MONO)
};

/*Machines supporting high res mono - Archimedes 440, 4x0/1, 540*/
#define MONITOR_ALL (MONITOR_STANDARD_MASK | MONITOR_MULTISYNC_MASK | MONITOR_VGA_MASK | \
                MONITOR_MONO_MASK)

/*Machines not supporting high res mono - everythine else*/
#define MONITOR_NO_MONO (MONITOR_STANDARD_MASK | MONITOR_MULTISYNC_MASK | MONITOR_VGA_MASK)

typedef struct machine_preset_t
{
        const char *name;
        const char *config_name;
        const char *description;
        unsigned int allowed_cpu_mask;
        unsigned int allowed_mem_mask;
        unsigned int allowed_memc_mask;
        unsigned int allowed_romset_mask;
        unsigned int allowed_monitor_mask;
        int default_cpu, default_mem, default_memc, io;
} machine_preset_t;

static const machine_preset_t presets[] =
{
        {"Archimedes 305",   "a305",   "ARM2, 512kB RAM, MEMC1, Old IO, Arthur",               CPU_ARM2_AND_LATER,    MEM_MIN_512K, MEMC_MIN_MEMC1,     ROM_ALL,       MONITOR_NO_MONO, CPU_ARM2,    MEM_512K, MEMC_MEMC1,     IO_OLD},
        {"Archimedes 310",   "a310",   "ARM2, 1MB RAM, MEMC1, Old IO, Arthur",                 CPU_ARM2_AND_LATER,    MEM_MIN_1M,   MEMC_MIN_MEMC1,     ROM_ALL,       MONITOR_NO_MONO, CPU_ARM2,    MEM_1M,   MEMC_MEMC1,     IO_OLD},
        {"Archimedes 440",   "a440",   "ARM2, 1MB RAM, MEMC1, Old IO + ST-506 HD, Arthur",     CPU_ARM2_AND_LATER,    MEM_MIN_4M,   MEMC_MIN_MEMC1,     ROM_ALL,       MONITOR_ALL,     CPU_ARM2,    MEM_4M,   MEMC_MEMC1,     IO_OLD_ST506},
        {"Archimedes 410/1", "a410/1", "ARM2, 1MB RAM, MEMC1A, Old IO + ST-506 HD, RISC OS 2", CPU_ARM2_AND_LATER,    MEM_MIN_1M,   MEMC_MIN_MEMC1A,    ROM_RISCOS,    MONITOR_ALL,     CPU_ARM2,    MEM_1M,   MEMC_MEMC1A_8,  IO_OLD_ST506},
        {"Archimedes 420/1", "a420/1", "ARM2, 2MB RAM, MEMC1A, Old IO + ST-506 HD, RISC OS 2", CPU_ARM2_AND_LATER,    MEM_MIN_2M,   MEMC_MIN_MEMC1A,    ROM_RISCOS,    MONITOR_ALL,     CPU_ARM2,    MEM_2M,   MEMC_MEMC1A_8,  IO_OLD_ST506},
        {"Archimedes 440/1", "a440/1", "ARM2, 4MB RAM, MEMC1A, Old IO + ST-506 HD, RISC OS 2", CPU_ARM2_AND_LATER,    MEM_MIN_4M,   MEMC_MIN_MEMC1A,    ROM_RISCOS,    MONITOR_ALL,     CPU_ARM2,    MEM_4M,   MEMC_MEMC1A_8,  IO_OLD_ST506},
        {"A3000",            "a3000",  "ARM2, 1MB RAM, MEMC1A, Old IO, RISC OS 2",             CPU_ARM2_AND_LATER,    MEM_MIN_1M,   MEMC_MIN_MEMC1A,    ROM_RISCOS,    MONITOR_NO_MONO, CPU_ARM2,    MEM_1M,   MEMC_MEMC1A_8,  IO_OLD},
        {"Archimedes 540",   "a540",   "ARM3/26, 4MB RAM, MEMC1A, Old IO, RISC OS 2.01",       CPU_ARM3_26_AND_LATER, MEM_MIN_4M,   MEMC_MIN_MEMC1A_12, ROM_RISCOS201, MONITOR_ALL,     CPU_ARM3_26, MEM_4M,   MEMC_MEMC1A_12, IO_OLD},
        {"A5000",            "a5000",  "ARM3/25, 1MB RAM, MEMC1A, New IO, RISC OS 3.0",        CPU_ARM3_25_AND_LATER, MEM_MIN_1M,   MEMC_MIN_MEMC1A_12, ROM_RISCOS3,   MONITOR_NO_MONO, CPU_ARM3_25, MEM_2M,   MEMC_MEMC1A_12, IO_NEW},
        {"A3010",            "a3010",  "ARM250, 1MB RAM, MEMC1A, New IO, RISC OS 3.1",         CPU_ARM250_ONLY,       MEM_1M_4M,    MEMC_MIN_MEMC1A_12, ROM_RISCOS31,  MONITOR_NO_MONO, CPU_ARM250,  MEM_1M,   MEMC_MEMC1A_12, IO_NEW},
        {"A3020",            "a3020",  "ARM250, 2MB RAM, MEMC1A, New IO, RISC OS 3.1",         CPU_ARM250_ONLY,       MEM_2M_4M,    MEMC_MIN_MEMC1A_12, ROM_RISCOS31,  MONITOR_NO_MONO, CPU_ARM250,  MEM_2M,   MEMC_MEMC1A_12, IO_NEW},
        {"A4000",            "a4000",  "ARM250, 2MB RAM, MEMC1A, New IO, RISC OS 3.1",         CPU_ARM250_ONLY,       MEM_2M_4M,    MEMC_MIN_MEMC1A_12, ROM_RISCOS31,  MONITOR_NO_MONO, CPU_ARM250,  MEM_2M,   MEMC_MEMC1A_12, IO_NEW},
        {"A5000a",           "a5000a", "ARM3/33, 4MB RAM, MEMC1A, New IO, RISC OS 3.1",        CPU_ARM3_33_AND_LATER, MEM_MIN_4M,   MEMC_MIN_MEMC1A_12, ROM_RISCOS31,  MONITOR_NO_MONO, CPU_ARM3_33, MEM_4M,   MEMC_MEMC1A_12, IO_NEW},
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
	void OnMachine(wxCommandEvent &event);
	void OnComboCPU(wxCommandEvent &event);
	void OnComboFPU(wxCommandEvent &event);
	void OnComboMEMC(wxCommandEvent &event);
	void OnComboMemory(wxCommandEvent &event);
	void OnComboOS(wxCommandEvent &event);
	void OnComboMonitor(wxCommandEvent &event);
        void OnComboPodule(wxCommandEvent &event);
        void OnConfigPodule(wxCommandEvent &event);
	void OnHDSel(wxCommandEvent &event);
	void OnHDNew(wxCommandEvent &event);
	void OnHDEject(wxCommandEvent &event);
	void OnConfigJoystick(wxCommandEvent &event);
	void OnIDEdit(wxCommandEvent &event);

        void CommonInit(wxWindow *parent, bool is_running);
	void UpdateList(int cpu, int mem, int memc, int fpu, int io);
	void PopulatePoduleList(int nr, wxComboBox *cbox);
	void PopulatePoduleLists(void);
	bool PoduleGetConfigEnable(int slot_nr);
	
	int get_preset(char *machine);
	int get_preset_config(char *machine);

        int get_cpu(char *cpu);
        int get_memc(char *memc);
        int get_mem(char *mem);
        int get_rom(char *rom);
        int get_monitor(char *monitor);

	int config_preset;
	int config_cpu, config_mem, config_memc, config_fpu, config_io, config_rom, config_monitor;
	uint32_t config_unique_id;
        wxString hd_fns[2];
        char config_podules[4][16];
	
	bool running;

        bool is_a3010;
};

int ConfigDialog::get_preset(char *machine)
{
        int c = 0;

        while (presets[c].name[0])
        {
                if (!strcmp(presets[c].name, machine))
                        return c;

                c++;
        }

        return 0;
}

int ConfigDialog::get_preset_config(char *machine)
{
        int c = 0;
        
        while (presets[c].name[0])
        {
                if (!strcmp(presets[c].config_name, machine))
                        return c;
                        
                c++;
        }
        
        return 0;
}

int ConfigDialog::get_cpu(char *cpu)
{
        for (int c = 0; c < nr_elems(cpu_names); c++)
        {
                if (!strcmp(cpu_names[c], cpu))
                        return c;
        }

        return 0;
}
int ConfigDialog::get_memc(char *memc)
{
        for (int c = 0; c < nr_elems(memc_names); c++)
        {
                if (!strcmp(memc_names[c], memc))
                        return c;
        }

        return 0;
}
int ConfigDialog::get_mem(char *mem)
{
        for (int c = 0; c < nr_elems(mem_names); c++)
        {
                if (!strcmp(mem_names[c], mem))
                        return c;
        }

        return 0;
}
int ConfigDialog::get_rom(char *rom)
{
        for (int c = 0; c < nr_elems(rom_names); c++)
        {
                if (!strcmp(rom_names[c], rom))
                        return c;
        }

        return 0;
}
int ConfigDialog::get_monitor(char *monitor)
{
        for (int c = 0; c < nr_elems(monitor_names); c++)
        {
                if (!strcmp(monitor_names[c], monitor))
                        return c;
        }

        return 0;
}

void ConfigDialog::CommonInit(wxWindow *parent, bool is_running)
{
        running = is_running;
        wxXmlResource::Get()->LoadDialog(this, parent, "ConfigureDlg");

        Bind(wxEVT_BUTTON, &ConfigDialog::OnOK, this, wxID_OK);
        Bind(wxEVT_BUTTON, &ConfigDialog::OnCancel, this, wxID_CANCEL);
        Bind(wxEVT_COMBOBOX, &ConfigDialog::OnMachine, this, XRCID("IDC_COMBO_MACHINE"));
        Bind(wxEVT_COMBOBOX, &ConfigDialog::OnComboCPU, this, XRCID("IDC_COMBO_CPU"));
        Bind(wxEVT_COMBOBOX, &ConfigDialog::OnComboFPU, this, XRCID("IDC_COMBO_FPU"));
        Bind(wxEVT_COMBOBOX, &ConfigDialog::OnComboMEMC, this, XRCID("IDC_COMBO_MEMC"));
        Bind(wxEVT_COMBOBOX, &ConfigDialog::OnComboMemory, this, XRCID("IDC_COMBO_MEMORY"));
        Bind(wxEVT_COMBOBOX, &ConfigDialog::OnComboOS, this, XRCID("IDC_COMBO_OS"));
        Bind(wxEVT_COMBOBOX, &ConfigDialog::OnComboMonitor, this, XRCID("IDC_COMBO_MONITOR"));
        Bind(wxEVT_COMBOBOX, &ConfigDialog::OnComboPodule, this, XRCID("IDC_COMBO_PODULE0"));
        Bind(wxEVT_COMBOBOX, &ConfigDialog::OnComboPodule, this, XRCID("IDC_COMBO_PODULE1"));
        Bind(wxEVT_COMBOBOX, &ConfigDialog::OnComboPodule, this, XRCID("IDC_COMBO_PODULE2"));
        Bind(wxEVT_COMBOBOX, &ConfigDialog::OnComboPodule, this, XRCID("IDC_COMBO_PODULE3"));
        Bind(wxEVT_BUTTON, &ConfigDialog::OnConfigPodule, this, XRCID("IDC_CONFIG_PODULE0"));
        Bind(wxEVT_BUTTON, &ConfigDialog::OnConfigPodule, this, XRCID("IDC_CONFIG_PODULE1"));
        Bind(wxEVT_BUTTON, &ConfigDialog::OnConfigPodule, this, XRCID("IDC_CONFIG_PODULE2"));
        Bind(wxEVT_BUTTON, &ConfigDialog::OnConfigPodule, this, XRCID("IDC_CONFIG_PODULE3"));
        Bind(wxEVT_BUTTON, &ConfigDialog::OnHDSel, this, XRCID("IDC_SEL_HD4"));
        Bind(wxEVT_BUTTON, &ConfigDialog::OnHDSel, this, XRCID("IDC_SEL_HD5"));
        Bind(wxEVT_BUTTON, &ConfigDialog::OnHDNew, this, XRCID("IDC_NEW_HD4"));
        Bind(wxEVT_BUTTON, &ConfigDialog::OnHDNew, this, XRCID("IDC_NEW_HD5"));
        Bind(wxEVT_BUTTON, &ConfigDialog::OnHDEject, this, XRCID("IDC_EJECT_HD4"));
        Bind(wxEVT_BUTTON, &ConfigDialog::OnHDEject, this, XRCID("IDC_EJECT_HD5"));
        Bind(wxEVT_BUTTON, &ConfigDialog::OnConfigJoystick, this, XRCID("IDC_JOY1"));
        Bind(wxEVT_BUTTON, &ConfigDialog::OnConfigJoystick, this, XRCID("IDC_JOY2"));
        Bind(wxEVT_TEXT, &ConfigDialog::OnIDEdit, this, XRCID("IDC_EDIT_ID2"));
        
        hd_fns[0] = wxString(hd_fn[0]);
        hd_fns[1] = wxString(hd_fn[1]);

        char temp_s[80];
        wxTextCtrl *tctrl = (wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_HD4"));
        tctrl->SetValue(hd_fns[0]);
        sprintf(temp_s, "%i", hd_spt[0]);
        tctrl = (wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_SECTORS_4"));
        tctrl->SetValue(temp_s);
        sprintf(temp_s, "%i", hd_hpc[0]);
        tctrl = (wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_HEADS_4"));
        tctrl->SetValue(temp_s);
        sprintf(temp_s, "%i", hd_cyl[0]);
        tctrl = (wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_CYLINDERS_4"));
        tctrl->SetValue(temp_s);
        if (config_io == IO_NEW)
                sprintf(temp_s, "%i", (hd_cyl[0]*hd_hpc[0]*hd_spt[0]*512) / (1024*1024));
        else
                sprintf(temp_s, "%i", (hd_cyl[0]*hd_hpc[0]*hd_spt[0]*256) / (1024*1024));
        tctrl = (wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_SIZE_4"));
        tctrl->SetValue(temp_s);

        tctrl = (wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_HD5"));
        tctrl->SetValue(hd_fns[1]);
        sprintf(temp_s, "%i", hd_spt[1]);
        tctrl = (wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_SECTORS_5"));
        tctrl->SetValue(temp_s);
        sprintf(temp_s, "%i", hd_hpc[1]);
        tctrl = (wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_HEADS_5"));
        tctrl->SetValue(temp_s);
        sprintf(temp_s, "%i", hd_cyl[1]);
        tctrl = (wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_CYLINDERS_5"));
        tctrl->SetValue(temp_s);
        if (config_io == IO_NEW)
                sprintf(temp_s, "%i", (hd_cyl[1]*hd_hpc[1]*hd_spt[1]*512) / (1024*1024));
        else
                sprintf(temp_s, "%i", (hd_cyl[1]*hd_hpc[1]*hd_spt[1]*256) / (1024*1024));
        tctrl = (wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_SIZE_5"));
        tctrl->SetValue(temp_s);
}

ConfigDialog::ConfigDialog(wxWindow *parent, bool is_running)
{
        int c;
        
        config_preset = get_preset_config(machine);
        config_cpu = arm_cpu_type;
        config_fpu = fpaena ? (fpu_type ? FPU_FPPC : FPU_FPA10) : FPU_NONE;
        config_memc = memc_type;
        config_io = fdctype ? IO_NEW : (st506_present ? IO_OLD_ST506 : IO_OLD);
        config_monitor = monitor_type;
        config_unique_id = unique_id;

        CommonInit(parent, is_running);

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

        config_rom = romset;

        UpdateList(config_cpu, config_mem, config_memc, config_fpu, config_io);

        for (c = 0; c < 4; c++)
                strncpy(config_podules[c], podule_names[c], 15);

        PopulatePoduleLists();
}

ConfigDialog::ConfigDialog(wxWindow *parent, bool is_running, int preset)
{
        config_preset = preset;
        config_cpu  = presets[preset].default_cpu;
        config_mem  = presets[preset].default_mem;
        config_memc = presets[preset].default_memc;
        config_fpu  = FPU_NONE;
        config_io   = presets[preset].io;
        config_monitor = MONITOR_MULTISYNC;
        if (config_io == IO_NEW)
        {
                /*TODO - should replace with something better...*/
                srand(time(NULL));
                config_unique_id = rand() ^ (rand() << 16);
        }
        config_rom = ROM_RISCOS_311;

        CommonInit(parent, is_running);
        
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
        int c;
        wxComboBox *cbox = (wxComboBox *)this->FindWindow(XRCID("IDC_COMBO_MACHINE"));
        cbox->SetValue("");
        cbox->Clear();
        c = 0;
        while (presets[c].name[0])
        {
                if (romset_available_mask & presets[c].allowed_romset_mask)
                        cbox->Append(presets[c].name);
                c++;
        }
        cbox->SetValue(presets[config_preset].name);

        cbox = (wxComboBox *)this->FindWindow(XRCID("IDC_COMBO_CPU"));
        cbox->SetValue("");
        cbox->Clear();
        for (c = 0; c < nr_elems(cpu_names); c++)
        {
                if (presets[config_preset].allowed_cpu_mask & (1 << c))
                        cbox->Append(cpu_names[c]);
        }
        cbox->SetValue(cpu_names[cpu]);

        cbox = (wxComboBox *)this->FindWindow(XRCID("IDC_COMBO_MEMORY"));
        cbox->SetValue("");
        cbox->Clear();
        for (c = 0; c < nr_elems(mem_names); c++)
        {
                if (presets[config_preset].allowed_mem_mask & (1 << c))
                        cbox->Append(mem_names[c]);
        }
        cbox->SetValue(mem_names[mem]);

        cbox = (wxComboBox *)this->FindWindow(XRCID("IDC_COMBO_MEMC"));
        cbox->SetValue("");
        cbox->Clear();
        for (c = 0; c < nr_elems(memc_names); c++)
        {
                if (presets[config_preset].allowed_memc_mask & (1 << c))
                        cbox->Append(memc_names[c]);
        }
        cbox->SetValue(memc_names[memc]);

        cbox = (wxComboBox *)this->FindWindow(XRCID("IDC_COMBO_FPU"));
        cbox->SetValue("");
        cbox->Clear();
        cbox->Append("None");
        if (cpu == CPU_ARM2 && memc != MEMC_MEMC1)
                cbox->Append("FPPC");
        if (cpu != CPU_ARM2 && cpu != CPU_ARM250)
                cbox->Append("FPA10");
        cbox->Select(fpu ? 1 : 0);

        cbox = (wxComboBox *)this->FindWindow(XRCID("IDC_COMBO_OS"));
        cbox->SetValue("");
        cbox->Clear();
        for (c = 0; c < nr_elems(rom_names); c++)
        {
                if ((romset_available_mask & (1 << c)) && (presets[config_preset].allowed_romset_mask & (1 << c)))
                        cbox->Append(rom_names[c]);
        }
        if (romset_available_mask & (1 << romset))
                cbox->SetValue(rom_names[romset]);
        else
                wxMessageBox("Configured ROM set is not available.\nYou must select an available ROM set to run this machine.", "Arculator", wxOK | wxCENTRE | wxSTAY_ON_TOP, this);

        cbox = (wxComboBox *)this->FindWindow(XRCID("IDC_COMBO_MONITOR"));
        cbox->SetValue("");
        cbox->Clear();
        for (c = 0; c < nr_elems(monitor_names); c++)
        {
                if (presets[config_preset].allowed_monitor_mask & (1 << c))
                        cbox->Append(monitor_names[c]);
        }
        cbox->SetValue(monitor_names[monitor_type]);

        ((wxStaticText *)this->FindWindow(XRCID("IDC_TEXT_MACHINE")))->SetLabelText(presets[config_preset].description);

        wxTextCtrl *tctrl = (wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_ID2"));
        tctrl->SetMaxLength(8);
        tctrl->Enable(io == IO_NEW);
        char s[10];
        sprintf(s, "%08x", config_unique_id);
        tctrl->SetValue(s);

        is_a3010 = !strcmp(presets[config_preset].config_name, "a3010");
        cbox = (wxComboBox *)this->FindWindow(XRCID("IDC_COMBO_JOY"));
        cbox->Clear();
        cbox->Append("None");
        cbox->SetValue("None");
        c = 0;
        while (joystick_get_name(c))
        {
                if (strcmp(joystick_get_config_name(c), "a3010") || is_a3010)
                {
                        cbox->Append(joystick_get_name(c));
                        if (!strcmp(joystick_if, joystick_get_config_name(c)))
                                cbox->SetValue(joystick_get_name(c));
                }
                c++;
        }
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

bool ConfigDialog::PoduleGetConfigEnable(int slot_nr)
{
        const podule_header_t *podule = podule_find(config_podules[slot_nr]);
        
        if (podule && podule->config)
                return true;
                
        return false;
}

void ConfigDialog::PopulatePoduleLists(void)
{
        PopulatePoduleList(0, (wxComboBox *)this->FindWindow(XRCID("IDC_COMBO_PODULE0")));
        PopulatePoduleList(1, (wxComboBox *)this->FindWindow(XRCID("IDC_COMBO_PODULE1")));
        PopulatePoduleList(2, (wxComboBox *)this->FindWindow(XRCID("IDC_COMBO_PODULE2")));
        PopulatePoduleList(3, (wxComboBox *)this->FindWindow(XRCID("IDC_COMBO_PODULE3")));

        ((wxButton *)this->FindWindow(XRCID("IDC_CONFIG_PODULE0")))->Enable(PoduleGetConfigEnable(0));
        ((wxButton *)this->FindWindow(XRCID("IDC_CONFIG_PODULE1")))->Enable(PoduleGetConfigEnable(1));
        ((wxButton *)this->FindWindow(XRCID("IDC_CONFIG_PODULE2")))->Enable(PoduleGetConfigEnable(2));
        ((wxButton *)this->FindWindow(XRCID("IDC_CONFIG_PODULE3")))->Enable(PoduleGetConfigEnable(3));
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

        romset = config_rom;

        monitor_type = config_monitor;

        wxTextCtrl *tctrl = (wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_HD4"));
        strcpy(hd_fn[0], tctrl->GetValue().mb_str());
        wxString temp_s = ((wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_CYLINDERS_4")))->GetValue();
        hd_cyl[0] = atoi(temp_s);
        temp_s = ((wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_HEADS_4")))->GetValue();
        hd_hpc[0] = atoi(temp_s);
        temp_s = ((wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_SECTORS_4")))->GetValue();
        hd_spt[0] = atoi(temp_s);

        tctrl = (wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_HD5"));
        strcpy(hd_fn[1], tctrl->GetValue().mb_str());
        temp_s = ((wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_CYLINDERS_5")))->GetValue();
        hd_cyl[1] = atoi(temp_s);
        temp_s = ((wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_HEADS_5")))->GetValue();
        hd_hpc[1] = atoi(temp_s);
        temp_s = ((wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_SECTORS_5")))->GetValue();
        hd_spt[1] = atoi(temp_s);
        
        for (c = 0; c < 4; c++)
                strncpy(podule_names[c], config_podules[c], 15);

        unique_id = config_unique_id;
        
        char temp_s2[256];
        wxComboBox *cbox = (wxComboBox *)this->FindWindow(XRCID("IDC_COMBO_JOY"));
        strncpy(temp_s2, cbox->GetValue(), sizeof(temp_s2));
        strcpy(joystick_if, "none");
        
        c = 0;
        while (joystick_get_name(c))
        {
                if (!strcmp(temp_s2, joystick_get_name(c)))
                        strcpy(joystick_if, joystick_get_config_name(c));
                c++;
        }

        strncpy(machine, presets[config_preset].config_name, sizeof(machine));
        
        saveconfig();
        if (running)
                arc_reset();

        EndModal(0);
}
void ConfigDialog::OnCancel(wxCommandEvent &event)
{
        EndModal(-1);
}
void ConfigDialog::OnMachine(wxCommandEvent &event)
{
        char machine_c_s[256];
        wxString machine_s = ((wxComboBox *)this->FindWindow(XRCID("IDC_COMBO_MACHINE")))->GetValue();
        strncpy(machine_c_s, machine_s, sizeof(machine_c_s));

        config_preset = get_preset(machine_c_s);

        config_cpu  = presets[config_preset].default_cpu;
        config_mem  = presets[config_preset].default_mem;
        config_memc = presets[config_preset].default_memc;
        config_io   = presets[config_preset].io;

        UpdateList(config_cpu, config_mem, config_memc, config_fpu, config_io);
}

void ConfigDialog::OnComboCPU(wxCommandEvent &event)
{
        char cpu_c_s[256];
        wxString cpu_s = ((wxComboBox *)this->FindWindow(event.GetId()))->GetValue();
        strncpy(cpu_c_s, cpu_s, sizeof(cpu_c_s));

        rpclog("New CPU = %s\n", cpu_c_s);
        config_cpu = get_cpu(cpu_c_s);
        rpclog("config_cpu = %i\n", config_cpu);
        if (config_cpu == CPU_ARM2)
        {
                /*ARM2 does not support FPA*/
                if (config_fpu != FPU_NONE)
                        config_fpu = FPU_FPPC;
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
        char mem_c_s[256];
        wxString mem_s = ((wxComboBox *)this->FindWindow(event.GetId()))->GetValue();
        strncpy(mem_c_s, mem_s, sizeof(mem_c_s));

        config_mem = get_mem(mem_c_s);
}

void ConfigDialog::OnComboMEMC(wxCommandEvent &event)
{
        char memc_c_s[256];
        wxString memc_s = ((wxComboBox *)this->FindWindow(event.GetId()))->GetValue();
        strncpy(memc_c_s, memc_s, sizeof(memc_c_s));

        config_memc = get_memc(memc_c_s);
}

void ConfigDialog::OnComboFPU(wxCommandEvent &event)
{
        wxComboBox *cbox = (wxComboBox *)this->FindWindow(event.GetId());

        config_fpu = cbox->GetCurrentSelection();
}

void ConfigDialog::OnComboOS(wxCommandEvent &event)
{
        char rom_c_s[256];
        wxString rom_s = ((wxComboBox *)this->FindWindow(event.GetId()))->GetValue();
        strncpy(rom_c_s, rom_s, sizeof(rom_c_s));

        config_rom = get_rom(rom_c_s);
}

void ConfigDialog::OnComboMonitor(wxCommandEvent &event)
{
        char monitor_c_s[256];
        wxString monitor_s = ((wxComboBox *)this->FindWindow(event.GetId()))->GetValue();
        strncpy(monitor_c_s, monitor_s, sizeof(monitor_c_s));

        config_monitor = get_monitor(monitor_c_s);
}

void ConfigDialog::OnHDSel(wxCommandEvent &event)
{
        int hd_nr = (event.GetId() == XRCID("IDC_SEL_HD4")) ? 0 : 1;
        
        wxFileDialog dlg(NULL, "Select a disc image", "", hd_fns[hd_nr],
                        "HDF Disc Image|*.hdf|All Files|*.*",
                        wxFD_OPEN | wxFD_FILE_MUST_EXIST);

        if (dlg.ShowModal() == wxID_OK)
        {
                wxString new_fn = dlg.GetPath();
                char new_fn_c[256];
                int new_sectors, new_heads, new_cylinders;
                
                strncpy(new_fn_c, new_fn, sizeof(new_fn_c));
                
                if (ShowConfHD(this, &new_sectors, &new_heads, &new_cylinders, new_fn_c, config_io != IO_NEW))
                {
                        char temp_s[80];
                        int new_size;
                        
                        if (config_io != IO_NEW)
                                new_size = (new_cylinders * new_heads * new_sectors * 256) / (1024 * 1024);
                        else
                                new_size = (new_cylinders * new_heads * new_sectors * 512) / (1024 * 1024);

                        if (!hd_nr)
                        {
                                sprintf(temp_s, "%i", new_cylinders);
                                ((wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_CYLINDERS_4")))->SetValue(temp_s);
                                sprintf(temp_s, "%i", new_heads);
                                ((wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_HEADS_4")))->SetValue(temp_s);
                                sprintf(temp_s, "%i", new_sectors);
                                ((wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_SECTORS_4")))->SetValue(temp_s);
                                sprintf(temp_s, "%i", new_size);
                                ((wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_SIZE_4")))->SetValue(temp_s);
                                ((wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_HD4")))->SetValue(new_fn);
                        }
                        else
                        {
                                sprintf(temp_s, "%i", new_cylinders);
                                ((wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_CYLINDERS_5")))->SetValue(temp_s);
                                sprintf(temp_s, "%i", new_heads);
                                ((wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_HEADS_5")))->SetValue(temp_s);
                                sprintf(temp_s, "%i", new_sectors);
                                ((wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_SECTORS_5")))->SetValue(temp_s);
                                sprintf(temp_s, "%i", new_size);
                                ((wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_SIZE_5")))->SetValue(temp_s);
                                ((wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_HD5")))->SetValue(new_fn);
                        }
                }
        }
}
void ConfigDialog::OnHDNew(wxCommandEvent &event)
{
        int new_sectors, new_heads, new_cylinders;
        char new_fn[256];
        
        int ret = ShowNewHD(this, &new_sectors, &new_heads, &new_cylinders, new_fn, sizeof(new_fn), config_io != IO_NEW);
        
        if (ret)
        {
                char temp_s[256];
                int new_size;
                
                if (config_io != IO_NEW)
                        new_size = (new_cylinders * new_heads * new_sectors * 256) / (1024 * 1024);
                else
                        new_size = (new_cylinders * new_heads * new_sectors * 512) / (1024 * 1024);
                if (event.GetId() == XRCID("IDC_NEW_HD4"))
                {
                        sprintf(temp_s, "%i", new_cylinders);
                        ((wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_CYLINDERS_4")))->SetValue(temp_s);
                        sprintf(temp_s, "%i", new_heads);
                        ((wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_HEADS_4")))->SetValue(temp_s);
                        sprintf(temp_s, "%i", new_sectors);
                        ((wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_SECTORS_4")))->SetValue(temp_s);
                        sprintf(temp_s, "%i", new_size);
                        ((wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_SIZE_4")))->SetValue(temp_s);
                        ((wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_HD4")))->SetValue(new_fn);
                }
                else
                {
                        sprintf(temp_s, "%i", new_cylinders);
                        ((wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_CYLINDERS_5")))->SetValue(temp_s);
                        sprintf(temp_s, "%i", new_heads);
                        ((wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_HEADS_5")))->SetValue(temp_s);
                        sprintf(temp_s, "%i", new_sectors);
                        ((wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_SECTORS_5")))->SetValue(temp_s);
                        sprintf(temp_s, "%i", new_size);
                        ((wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_SIZE_5")))->SetValue(temp_s);
                        ((wxTextCtrl *)this->FindWindow(XRCID("IDC_EDIT_HD5")))->SetValue(new_fn);
                }
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
        wxButton *config_button;
        int slot_nr;
        
        if (event.GetId() == XRCID("IDC_COMBO_PODULE0"))
        {
                slot_nr = 0;
                config_button = (wxButton *)this->FindWindow(XRCID("IDC_CONFIG_PODULE0"));
        }
        else if (event.GetId() == XRCID("IDC_COMBO_PODULE1"))
        {
                slot_nr = 1;
                config_button = (wxButton *)this->FindWindow(XRCID("IDC_CONFIG_PODULE1"));
        }
        else if (event.GetId() == XRCID("IDC_COMBO_PODULE2"))
        {
                slot_nr = 2;
                config_button = (wxButton *)this->FindWindow(XRCID("IDC_CONFIG_PODULE2"));
        }
        else
        {
                slot_nr = 3;
                config_button = (wxButton *)this->FindWindow(XRCID("IDC_CONFIG_PODULE3"));
        }

        if (!sel_nr)
        {
                strcpy(config_podules[slot_nr], "");
                config_button->Enable(false);
        }
        else
        {
                strncpy(config_podules[slot_nr], podule_get_short_name(sel_nr-1), 15);
                config_button->Enable(PoduleGetConfigEnable(slot_nr));
                
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

void ConfigDialog::OnConfigPodule(wxCommandEvent &event)
{
        int slot_nr;

        if (event.GetId() == XRCID("IDC_CONFIG_PODULE0"))
                slot_nr = 0;
        else if (event.GetId() == XRCID("IDC_CONFIG_PODULE1"))
                slot_nr = 1;
        else if (event.GetId() == XRCID("IDC_CONFIG_PODULE2"))
                slot_nr = 2;
        else
                slot_nr = 3;

        const podule_header_t *podule = podule_find(config_podules[slot_nr]);
        ShowPoduleConfig(this, podule, podule->config, running, slot_nr);
}

void ConfigDialog::OnConfigJoystick(wxCommandEvent &event)
{
        int joy_nr = 0;

        if (event.GetId() == XRCID("IDC_JOY1"))
                joy_nr = 0;
        else if (event.GetId() == XRCID("IDC_JOY2"))
                joy_nr = 1;
                
        char temp_s[256];
        wxComboBox *cbox = (wxComboBox *)this->FindWindow(XRCID("IDC_COMBO_JOY"));
        strncpy(temp_s, cbox->GetValue(), sizeof(temp_s));

        int c = 0;
        int joy_type = 0;
        while (joystick_get_name(c))
        {
                if (!strcmp(temp_s, joystick_get_name(c)))
                        joy_type = c;
                c++;
        }

        ShowConfJoy(this, joy_nr, joy_type);
}

void ConfigDialog::OnIDEdit(wxCommandEvent &event)
{
        static bool skip_processing = false;
        
        if (skip_processing)
                return;
                
        wxTextCtrl *tctrl = (wxTextCtrl *)this->FindWindow(event.GetId());
        wxString s = tctrl->GetValue();
        unsigned long pos = tctrl->GetInsertionPoint();
        wxString s2 = "";
        config_unique_id = 0;
        for (unsigned int c = 0; c < s.Len(); c++)
        {
                wxUniChar uch = s.at(c);

                if (uch.IsAscii())
                {
                        char ch = uch;

                        /*Is character a valid hex digit?*/
                        if ((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F'))
                        {
                                s2 << uch;
                                config_unique_id <<= 4;
                                if (ch >= '0' && ch <= '9')
                                        config_unique_id |= (ch - '0');
                                else if (ch >= 'A' && ch <= 'F')
                                        config_unique_id |= ((ch - 'A') + 10);
                                else if (ch >= 'a' && ch <= 'f')
                                        config_unique_id |= ((ch - 'a') + 10);
                        }
                        else if (c < pos)
                                pos--;
                }
        }

        skip_processing = true;
        tctrl->SetValue(s2);
        tctrl->SetInsertionPoint(pos);
        skip_processing = false;
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
        int preset = -1;
        
        c = 0;
        while (presets[c].name[0])
                achoices.Add(presets[c++].name);

        while (1)
        {
                preset = wxGetSingleChoiceIndex("Please select a machine type", "Arculator", achoices);
                
                if (preset == -1)
                        break;

                if (romset_available_mask & presets[preset].allowed_romset_mask)
                        break;

                wxMessageBox("You do not have any of the ROM versions required for this machine", "Arculator", wxOK | wxCENTRE | wxSTAY_ON_TOP);
        }
        
        return preset;
}

void ShowConfigWithPreset(int preset)
{
        ConfigDialog dlg(NULL, false, preset);

        dlg.ShowModal();
}
