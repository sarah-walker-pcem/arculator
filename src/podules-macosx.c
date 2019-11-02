#include <dlfcn.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "arc.h"
#include "config.h"
#include "podules.h"

typedef struct dll_t
{
        void *lib;
        struct dll_t *next;
} dll_t;

static dll_t *dll_head = NULL;

static void closedlls(void)
{
        dll_t *dll = dll_head;
        
        while (dll)
        {
                dll_t *dll_next = dll->next;
                
                if (dll->lib)
                        dlclose(dll->lib);
                free(dll);
                
                dll = dll_next;
        }
}

void opendlls(void)
{
        char podule_path[512];
        DIR *dirp;
        struct dirent *dp;

        atexit(closedlls);
        
        append_filename(podule_path, exname, "podules/", sizeof(podule_path));
        rpclog("Looking for podules in %s\n", podule_path);
        dirp = opendir(podule_path);
        if (!dirp)
        {
                perror("opendir: ");
                fatal("Can't open rom dir %s\n", podule_path);
        }

        while (((dp = readdir(dirp)) != NULL))
        {
                const podule_header_t *(*podule_probe)(const podule_callbacks_t *callbacks, char *path);
                const podule_header_t *header;
		char so_fn[512], so_name[512];
                dll_t *dll = malloc(sizeof(dll_t));
                memset(dll, 0, sizeof(dll_t));

                if (dp->d_type != DT_DIR)
                {
                        free(dll);
                        continue;
                }

                sprintf(so_name, "/%s.dylib", dp->d_name);
                append_filename(so_fn, podule_path, dp->d_name, sizeof(so_fn));
                append_filename(so_fn, so_fn, so_name, sizeof(so_fn));
		
                dll->lib = dlopen(so_fn, RTLD_NOW);
                if (dll->lib == NULL)
                {
                        char *lasterror = dlerror();
                        rpclog("Failed to open dylib %s %s\n", dp->d_name, lasterror);
                        free(dll);
                        continue;
                }
                podule_probe = (const void *)dlsym(dll->lib, "podule_probe");
                if (podule_probe == NULL)
                {
                        rpclog("Couldn't find podule_probe in %s\n", dp->d_name);
                        dlclose(dll->lib);
                        free(dll);
                        continue;
                }
                append_filename(so_fn, podule_path, dp->d_name, sizeof(so_fn));
                append_filename(so_fn, so_fn, "/", sizeof(so_fn));
                header = podule_probe(&podule_callbacks_def, so_fn);
                if (!header)
                {
                        rpclog("podule_probe failed %s\n", dp->d_name);
                        dlclose(dll->lib);
                        free(dll);
                        continue;
                }
                rpclog("podule_probe returned %p\n", header);
                podule_add(header);
                dll->next = dll_head;
                dll_head = dll;
        }

	(void)closedir(dirp);
}
