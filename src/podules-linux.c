#include <dlfcn.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "arc.h"
#include "config.h"
#include "podules.h"

void *hinstLib[8];

static void closedlls(void)
{
        int c;

        for (c = 0; c < 8; c++)
        {
                if (hinstLib[c])
			dlclose(hinstLib[c]);
        }
}

void opendlls(void)
{
        char podule_path[512];
        int dllnum=0;
        int i;
        DIR *dirp;
        struct dirent *dp;

        atexit(closedlls);
        for (dllnum = 0; dllnum < 8; dllnum++)
		hinstLib[dllnum] = NULL;
        dllnum = 0;
        
        append_filename(podule_path, exname, "podules/", sizeof(podule_path));
        dirp = opendir(podule_path);
        if (!dirp)
        {
                perror("opendir: ");
                fatal("Can't open rom dir %s\n", podule_path);
        }

        while (((dp = readdir(dirp)) != NULL) && dllnum < 6)
        {
                const podule_header_t *(*podule_probe)(const podule_callbacks_t *callbacks, char *path);
                const podule_header_t *header;
                char *ext;
		char so_fn[512];

                if (dp->d_type != DT_REG && dp->d_type != DT_LNK)
                        continue;
                ext = get_extension(dp->d_name);
                if (strcasecmp(ext, "so"))
			continue;

		sprintf(so_fn, "%s%s", podule_path, dp->d_name);
                hinstLib[dllnum] = dlopen(so_fn, RTLD_NOW);
                if (hinstLib[dllnum] == NULL)
                {
                        char *lasterror = dlerror();
                        rpclog("Failed to open SO %s %s\n", dp->d_name, lasterror);
                        continue;
                }
                podule_probe = (const void *)dlsym(hinstLib[dllnum], "podule_probe");
                if (podule_probe == NULL)
                {
                        rpclog("Couldn't find podule_probe in %s\n", dp->d_name);
                        continue;
                }
                header = podule_probe(&podule_callbacks_def, podule_path);
                if (!header)
                {
                        rpclog("podule_probe failed %s\n", dp->d_name);
                        dlclose(hinstLib[dllnum]);
                        continue;
                }
                rpclog("podule_probe returned %p\n", header);
                podule_add(header);
                dllnum++;
        }

	(void)closedir(dirp);
}

