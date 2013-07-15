#if defined WIN32 || defined _WIN32 || defined _WIN32
#include <allegro.h>
#include <winalleg.h>
#include <stdio.h>
#include "arc.h"
#include "podules.h"

HINSTANCE hinstLib[8];

static void closedlls(void)
{
        int c;
        for (c=0;c<8;c++)
        {
                if (hinstLib[c]) FreeLibrary(hinstLib[c]);
        }
}

void opendlls(void)
{
        char olddir[512],fn[512];
        podule tempp;
        struct al_ffblk ff;
        int (*InitDll)();
        int finished;
        int dllnum=0;
        int i;
        
        atexit(closedlls);
        for (dllnum=0;dllnum<8;dllnum++) hinstLib[dllnum]=NULL;
        dllnum=0;
        
        getcwd(olddir,sizeof(olddir));
        append_filename(fn,exname,"podules",sizeof(fn));
        if (chdir(fn)) { error("Cannot find podules directory %s",fn); exit(-1); }
        finished=al_findfirst("*.dll",&ff,0xFFFF&~FA_DIREC);
        if (finished)
        {
                chdir(olddir);
                return;
        }
        while (!finished && dllnum<6)
        {
                rpclog("Loading %s\n",ff.name);
                hinstLib[dllnum]=LoadLibrary(ff.name);
                if (hinstLib[dllnum] == NULL)
                {
                        rpclog("Failed to open DLL %s\n",ff.name);
                        goto nextdll;
                }
                InitDll = (const void *) GetProcAddress(hinstLib[dllnum], "InitDll");
                if (InitDll == NULL)
                {
                        rpclog("Couldn't find InitDll in %s\n",ff.name);
                        goto nextdll;
                }
                InitDll();
                tempp.readb = (const void *) GetProcAddress(hinstLib[dllnum],"readb");
                tempp.readw = (const void *) GetProcAddress(hinstLib[dllnum],"readw");
                tempp.readl = (const void *) GetProcAddress(hinstLib[dllnum],"readl");
                tempp.writeb = (const void *) GetProcAddress(hinstLib[dllnum],"writeb");
                tempp.writew = (const void *) GetProcAddress(hinstLib[dllnum],"writew");
                tempp.writel = (const void *) GetProcAddress(hinstLib[dllnum],"writel");
                tempp.timercallback = (const void *) GetProcAddress(hinstLib[dllnum],"timercallback");
                tempp.reset = (const void *) GetProcAddress(hinstLib[dllnum],"reset");
                i=(GetProcAddress(hinstLib[dllnum],"broken")!=NULL);
                rpclog("Podule is %s\n",(i)?"broken":"normal");
                rpclog("%08X %08X %08X %08X %08X %08X %08X %08X\n",tempp.writel,tempp.writew,tempp.writeb,tempp.readl,tempp.readw,tempp.readb,tempp.timercallback,tempp.reset);
                addpodule(tempp.writel,tempp.writew,tempp.writeb,tempp.readl,tempp.readw,tempp.readb,tempp.timercallback,tempp.reset,i);
                dllnum++;
                
                nextdll:
                finished = al_findnext(&ff);
        }

        al_findclose(&ff);
        chdir(olddir);
        
//        FreeLibrary(hinstLib);
}
#endif

