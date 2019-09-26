#include "wx-app.h"
#include <SDL2/SDL.h>

extern "C"
{
        #include "arc.h"
        #include "config.h"
        #include "plat_joystick.h"
        #include "podules.h"
        #include "romload.h"
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
        joystick_init();
        
        wxApp::SetInstance(new App());
        if (rom_establish_availability())
        {
                wxMessageBox("No ROMs available\nArculator needs at least one ROM set present to run", "Arculator", wxOK | wxCENTRE | wxSTAY_ON_TOP);
                return -1;
        }
        wxEntry(argc, argv);
        return 0;
}
