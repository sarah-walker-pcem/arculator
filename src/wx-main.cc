/*Arculator 2.0 by Sarah Walker
  Main function*/
#include "wx-app.h"
#include <SDL2/SDL.h>

extern "C"
{
        #include "arc.h"
        #include "config.h"
        #include "podules.h"
        #include "soundopenal.h"
}

int main(int argc, char **argv)
{
        al_init_main(0, NULL);

        strncpy(exname, argv[0], 511);
        char *p = (char *)get_filename(exname);
        *p = 0;

        podule_build_list();
        opendlls();
        SDL_SetHint(SDL_HINT_WINDOWS_DISABLE_THREAD_NAMING, "1");

        wxApp::SetInstance(new App());
        wxEntry(argc, argv);
        return 0;
}
