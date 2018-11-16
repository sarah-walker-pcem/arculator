#include <windows.h>
#include <commctrl.h>
#include "arc.h"
#include "arm.h"
#include "config.h"
#include "fpa.h"
#include "memc.h"
#include "resources.h"
#include "win.h"

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
        char name[80];
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

static int config_cpu, config_mem, config_memc, config_fpu, config_io, config_rom;

static void update_list(HWND hdlg, int cpu, int mem, int memc, int fpu, int io)
{
        HWND h;
        
        h = GetDlgItem(hdlg, IDC_COMBO_CPU);
        SendMessage(h, CB_RESETCONTENT, 0, 0);
        SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)"ARM2");
        SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)"ARM250");
        SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)"ARM3 @ 20 MHz");
        SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)"ARM3 @ 25 MHz");
        SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)"ARM3 @ 26 MHz");
        SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)"ARM3 @ 30 MHz");
        SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)"ARM3 @ 33 MHz");
        SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)"ARM3 @ 35 MHz");
        SendMessage(h, CB_SETCURSEL, cpu, 0);
        
        h = GetDlgItem(hdlg, IDC_COMBO_MEMORY);
        SendMessage(h, CB_RESETCONTENT, 0, 0);
        SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)"512 kB");
        SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)"1 MB");
        SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)"2 MB");
        SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)"4 MB");
        if (cpu != CPU_ARM250)
        {
                SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)"8 MB");
                SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)"16 MB");
        }
        SendMessage(h, CB_SETCURSEL, mem, 0);
        
        h = GetDlgItem(hdlg, IDC_COMBO_MEMC);
        SendMessage(h, CB_RESETCONTENT, 0, 0);
        if (cpu == CPU_ARM2)
                SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)"MEMC1");
        if (cpu != CPU_ARM250)
                SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)"MEMC1a (8 MHz)");
        SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)"MEMC1a (12 MHz)");
        SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)"MEMC1a (16 MHz - overclocked)");
        SendMessage(h, CB_SETCURSEL, (cpu == CPU_ARM250) ? memc-2 : ((cpu != CPU_ARM2) ? memc-1 : memc), 0);
        
        h = GetDlgItem(hdlg, IDC_COMBO_FPU);
        SendMessage(h, CB_RESETCONTENT, 0, 0);
        SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)"None");
        if (cpu == CPU_ARM2 && memc != MEMC_MEMC1)
                SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)"FPPC");
        if (cpu != CPU_ARM2 && cpu != CPU_ARM250)
                SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)"FPA10");
        SendMessage(h, CB_SETCURSEL, fpu ? 1 : 0, 0);

        h = GetDlgItem(hdlg, IDC_COMBO_IO);
        SendMessage(h, CB_RESETCONTENT, 0, 0);
        if (cpu != CPU_ARM250)
        {
                SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)"Old (1772)");
                SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)"Old + ST506");
        }
        if (memc >= MEMC_MEMC1A_8 && cpu != CPU_ARM2)
                SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)"New (SuperIO)");
        SendMessage(h, CB_SETCURSEL, (cpu == CPU_ARM250) ? io-2 : io, 0);

        h = GetDlgItem(hdlg, IDC_COMBO_OS);
        SendMessage(h, CB_RESETCONTENT, 0, 0);
        if (cpu != CPU_ARM250)
        {
                SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)"Arthur");
                SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)"RISC OS 2");
        }
        SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)"RISC OS 3");
        SendMessage(h, CB_SETCURSEL, (cpu == CPU_ARM250) ? config_rom-2 : config_rom, 0);
}

static BOOL CALLBACK config_dlgproc(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam)
{
        HWND h;
        int c;
        int preset;
//        pclog("Dialog msg %i %08X\n",message,message);
        switch (message)
        {
                case WM_INITDIALOG:
                h = GetDlgItem(hdlg, IDC_COMBO_PRESET);
                c = 0;
                while (presets[c].name[0])
                {
                        SendMessage(h, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)presets[c].name);
                        c++;
                }

                if (!arm_has_swp)
                        config_cpu = CPU_ARM2;
                else if (!arm_has_cp15)
                        config_cpu = CPU_ARM250;
                else switch (arm_cpu_speed)
                {
                        case 20:
                        config_cpu = CPU_ARM3_20;
                        break;
                        case 26:
                        config_cpu = CPU_ARM3_26;
                        break;
                        case 30:
                        config_cpu = CPU_ARM3_30;
                        break;
                        case 33:
                        config_cpu = CPU_ARM3_33;
                        break;
                        case 35:
                        config_cpu = CPU_ARM3_35;
                        break;
                        case 25: default:
                        config_cpu = CPU_ARM3_25;
                        break;
                }

                config_fpu = fpaena ? (fpu_type ? FPU_FPPC : FPU_FPA10) : FPU_NONE;

                if (memc_is_memc1)
                        config_memc = MEMC_MEMC1;
                else switch (arm_mem_speed)
                {
                        case 12:
                        config_memc = MEMC_MEMC1A_12;
                        break;
                        case 16:
                        config_memc = MEMC_MEMC1A_16;
                        break;
                        case 8: default:
                        config_memc = MEMC_MEMC1A_8;
                        break;
                }
                
                config_io = fdctype ? IO_NEW : IO_OLD_ST506;
                rpclog("win-config %i %i\n", fdctype, config_io);

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

                update_list(hdlg, config_cpu, config_mem, config_memc, config_fpu, config_io);
                return TRUE;
                
                case WM_COMMAND:
                switch (LOWORD(wParam))
                {
                        case IDOK:
                        if (MessageBox(NULL, "This will reset Arculator!\nOkay to continue?", "Arculator", MB_OKCANCEL) != IDOK)
                                break;

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

                        saveconfig();
                        arc_reset();

                        case IDCANCEL:
                        EndDialog(hdlg, 0);
                        return TRUE;

                        case IDC_COMBO_PRESET:
                        h = GetDlgItem(hdlg, IDC_COMBO_PRESET);
                        preset = SendMessage(h, CB_GETCURSEL, 0, 0);
                        
                        config_cpu  = presets[preset].cpu;
                        config_mem  = presets[preset].mem;
                        config_memc = presets[preset].memc;
                        config_fpu  = presets[preset].fpu;
                        config_io   = presets[preset].io;
                        
                        update_list(hdlg, config_cpu, config_mem, config_memc, config_fpu, config_io);
                        break;
                        
                        case IDC_COMBO_CPU:
                        h = GetDlgItem(hdlg, IDC_COMBO_CPU);
                        config_cpu = SendMessage(h, CB_GETCURSEL, 0, 0);
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
                        update_list(hdlg, config_cpu, config_mem, config_memc, config_fpu, config_io);
                        break;

                        case IDC_COMBO_MEMORY:
                        h = GetDlgItem(hdlg, IDC_COMBO_MEMORY);
                        config_mem = SendMessage(h, CB_GETCURSEL, 0, 0);
                        break;

                        case IDC_COMBO_MEMC:
                        h = GetDlgItem(hdlg, IDC_COMBO_MEMC);
                        config_memc = SendMessage(h, CB_GETCURSEL, 0, 0);
                        if (config_cpu == CPU_ARM250)
                                config_memc += 2;
                        else if (config_cpu != CPU_ARM2)
                                config_memc++;
                        update_list(hdlg, config_cpu, config_mem, config_memc, config_fpu, config_io);
                        if (config_memc == MEMC_MEMC1)
                                config_fpu = FPU_NONE;
                        break;

                        case IDC_COMBO_FPU:
                        h = GetDlgItem(hdlg, IDC_COMBO_FPU);
                        config_fpu = SendMessage(h, CB_GETCURSEL, 0, 0);
                        break;

                        case IDC_COMBO_IO:
                        h = GetDlgItem(hdlg, IDC_COMBO_IO);
                        config_io = SendMessage(h, CB_GETCURSEL, 0, 0);
                        if (config_cpu == CPU_ARM250)
                                config_io = IO_NEW;
                        break;
                        
                        case IDC_COMBO_OS:
                        h = GetDlgItem(hdlg, IDC_COMBO_OS);
                        config_rom = SendMessage(h, CB_GETCURSEL, 0, 0);
                        if (config_cpu == CPU_ARM250)
                                config_rom = ROM_RISCOS_3;
                        if (config_rom < ROM_RISCOS_3 && config_io == IO_NEW)
                                config_io = IO_OLD_ST506;
                        update_list(hdlg, config_cpu, config_mem, config_memc, config_fpu, config_io);
                        break;                                
                }
                break;
        }
        return FALSE;
}

void config_open(HWND hwnd)
{
        DialogBox(hinstance, TEXT("ConfigureDlg"), hwnd, (DLGPROC)config_dlgproc);
}
