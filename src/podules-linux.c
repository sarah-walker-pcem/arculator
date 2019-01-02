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
        char olddir[512],fn[512];
        podule tempp;
        int (*InitDll)();
        int dllnum=0;
        int i;
        DIR *dirp;
        struct dirent *dp;

        atexit(closedlls);
        for (dllnum = 0; dllnum < 8; dllnum++)
		hinstLib[dllnum] = NULL;
        dllnum = 0;
        
	getcwd(olddir, sizeof(olddir));
        append_filename(fn,exname,"podules/",sizeof(fn));
        if (chdir(fn))
	{
		error("Cannot find podules directory %s",fn);
		exit(-1);
	}
        dirp = opendir(".");
        if (!dirp)
        {
                perror("opendir: ");
                fatal("Can't open rom dir %s\n", fn);
        }

        while (((dp = readdir(dirp)) != NULL) && dllnum < 6)
        {
		char *ext;
		char so_fn[512];

                if (dp->d_type != DT_REG && dp->d_type != DT_LNK)
                        continue;
                ext = get_extension(dp->d_name);
                if (strcasecmp(ext, "so"))
			continue;

		sprintf(so_fn, "./%s", dp->d_name);
                hinstLib[dllnum] = dlopen(so_fn, RTLD_NOW);
                if (hinstLib[dllnum] == NULL)
                {
                        char *lasterror = dlerror();
                        rpclog("Failed to open SO %s %s\n", dp->d_name, lasterror);
                        continue;
                }
                InitDll = (const void *)dlsym(hinstLib[dllnum], "InitDll");
                if (InitDll == NULL)
                {
                        rpclog("Couldn't find InitDll in %s\n", dp->d_name);
                        continue;
                }
                InitDll();
                tempp.readb = (const void *)dlsym(hinstLib[dllnum],"readb");
                tempp.readw = (const void *)dlsym(hinstLib[dllnum],"readw");
                tempp.readl = (const void *)dlsym(hinstLib[dllnum],"readl");
                tempp.writeb = (const void *)dlsym(hinstLib[dllnum],"writeb");
                tempp.writew = (const void *)dlsym(hinstLib[dllnum],"writew");
                tempp.writel = (const void *)dlsym(hinstLib[dllnum],"writel");
                tempp.memc_readb = (const void *)dlsym(hinstLib[dllnum],"memc_readb");
                tempp.memc_readw = (const void *)dlsym(hinstLib[dllnum],"memc_readw");
                tempp.memc_writeb = (const void *)dlsym(hinstLib[dllnum],"memc_writeb");
                tempp.memc_writew = (const void *)dlsym(hinstLib[dllnum],"memc_writew");
                tempp.timercallback = (const void *)dlsym(hinstLib[dllnum],"timercallback");
                tempp.reset = (const void *)dlsym(hinstLib[dllnum],"reset");
                i=(dlsym(hinstLib[dllnum],"broken")!=NULL);
                rpclog("Podule is %s\n",(i)?"broken":"normal");
                rpclog("%08X %08X %08X %08X %08X %08X %08X %08X\n",tempp.writel,tempp.writew,tempp.writeb,tempp.readl,tempp.readw,tempp.readb,tempp.timercallback,tempp.reset);
                addpodule(tempp.writel, tempp.writew, tempp.writeb,
                          tempp.readl,  tempp.readw,  tempp.readb,
                          tempp.memc_writew, tempp.memc_writeb,
                          tempp.memc_readw,  tempp.memc_readb,                          
                          tempp.timercallback, tempp.reset, i);
                dllnum++;
        }

	(void)closedir(dirp);
	chdir(olddir);
}

