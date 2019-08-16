#if defined WIN32 || defined _WIN32 || defined _WIN32
#include <dir.h>
#include <windows.h>
#include <stdio.h>
#include "arc.h"
#include "config.h"
#include "podules.h"

static HINSTANCE hinstLib[8];

static void closedlls(void)
{
        int c;
        
        for (c = 0; c < 8; c++)
        {
                if (hinstLib[c])
                        FreeLibrary(hinstLib[c]);
        }
}

void opendlls(void)
{
        char fn[512];
        char podule_path[512];
        struct _finddata_t finddata;
        int file;
        int dllnum=0;

        atexit(closedlls);
        memset(hinstLib, 0, sizeof(hinstLib));

        append_filename(podule_path, exname, "podules\\", sizeof(podule_path));
        append_filename(fn, podule_path, "*.", sizeof(fn));
        rpclog("Looking for podules in %s\n", fn);
        file = _findfirst(fn, &finddata);
        if (file == -1)
        {
                rpclog("Found nothing\n");
                return;
        }
        while (dllnum<6)
        {
                const podule_header_t *(*podule_probe)(const podule_callbacks_t *callbacks, char *path);
                const podule_header_t *header;
                char dll_name[256];

                sprintf(dll_name, "/%s.dll", finddata.name);
                append_filename(fn, podule_path, finddata.name, sizeof(fn));
                append_filename(fn, fn, dll_name, sizeof(fn));
                rpclog("Loading %s %s\n", finddata.name, fn);
                SetErrorMode(0);
                hinstLib[dllnum] = LoadLibrary(fn);
                if (hinstLib[dllnum] == NULL)
                {
                        DWORD lasterror = GetLastError();
                        rpclog("Failed to open DLL %s %x\n", finddata.name, lasterror);
                        goto nextdll;
                }
                
                podule_probe = (const void *)GetProcAddress(hinstLib[dllnum], "podule_probe");
                if (!podule_probe)
                {
                        rpclog("Couldn't find podule_probe in %s\n", finddata.name);
                        FreeLibrary(hinstLib[dllnum]);
                        goto nextdll;
                }
                append_filename(fn, podule_path, finddata.name, sizeof(fn));
                append_filename(fn, fn, "/", sizeof(fn));
                header = podule_probe(&podule_callbacks_def, fn);
                if (!header)
                {
                        rpclog("podule_probe failed\n", finddata.name);
                        FreeLibrary(hinstLib[dllnum]);
                        goto nextdll;
                }
                rpclog("podule_probe returned %p\n", header);
                podule_add(header);
                dllnum++;

nextdll:
                if (_findnext(file, &finddata))
                        break;
        }

        _findclose(file);

//        FreeLibrary(hinstLib);
}
#endif

