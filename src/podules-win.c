/*Arculator 2.0 by Sarah Walker
  Windows podule loader*/
#if defined WIN32 || defined _WIN32 || defined _WIN32
#include <io.h>
#include <windows.h>
#include <stdio.h>
#include "arc.h"
#include "config.h"
#include "podules.h"

typedef struct dll_t
{
        HINSTANCE hinstance;
        struct dll_t *next;
} dll_t;

static dll_t *dll_head = NULL;

static void closedlls(void)
{
        dll_t *dll = dll_head;
        
        while (dll)
        {
                dll_t *dll_next = dll->next;
                
                if (dll->hinstance)
                        FreeLibrary(dll->hinstance);
                free(dll);
                
                dll = dll_next;
        }
}

void opendlls(void)
{
        char fn[512];
        char podule_path[512];
        struct _finddata_t finddata;
        int file;

        atexit(closedlls);

        append_filename(podule_path, exname, "podules\\", sizeof(podule_path));
        append_filename(fn, podule_path, "*.", sizeof(fn));
        rpclog("Looking for podules in %s\n", fn);
        file = _findfirst(fn, &finddata);
        if (file == -1)
        {
                rpclog("Found nothing\n");
                return;
        }
        while (1)
        {
                const podule_header_t *(*podule_probe)(const podule_callbacks_t *callbacks, char *path);
                const podule_header_t *header;
                char dll_name[256];
                dll_t *dll = malloc(sizeof(dll_t));
                memset(dll, 0, sizeof(dll_t));

                sprintf(dll_name, "/%s.dll", finddata.name);
                append_filename(fn, podule_path, finddata.name, sizeof(fn));
                append_filename(fn, fn, dll_name, sizeof(fn));
                rpclog("Loading %s %s\n", finddata.name, fn);
                SetErrorMode(0);
                dll->hinstance = LoadLibrary(fn);
                if (dll->hinstance == NULL)
                {
                        DWORD lasterror = GetLastError();
                        rpclog("Failed to open DLL %s %x\n", finddata.name, lasterror);
                        free(dll);
                        goto nextdll;
                }
                
                podule_probe = (const void *)GetProcAddress(dll->hinstance, "podule_probe");
                if (!podule_probe)
                {
                        rpclog("Couldn't find podule_probe in %s\n", finddata.name);
                        FreeLibrary(dll->hinstance);
                        free(dll);
                        goto nextdll;
                }
                append_filename(fn, podule_path, finddata.name, sizeof(fn));
                append_filename(fn, fn, "/", sizeof(fn));
                header = podule_probe(&podule_callbacks_def, fn);
                if (!header)
                {
                        rpclog("podule_probe failed\n", finddata.name);
                        FreeLibrary(dll->hinstance);
                        free(dll);
                        goto nextdll;
                }
                rpclog("podule_probe returned %p\n", header);
                podule_add(header);
                dll->next = dll_head;
                dll_head = dll;

nextdll:
                if (_findnext(file, &finddata))
                        break;
        }

        _findclose(file);

//        FreeLibrary(hinstLib);
}
#endif

