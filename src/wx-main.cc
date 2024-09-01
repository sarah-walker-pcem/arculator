/*Arculator 2.1 by Sarah Walker
  Main function*/
#include "wx-app.h"
#include <SDL.h>
#include <wx/filename.h>
#include "wx-config_sel.h"

extern "C"
{
#ifndef _WIN32
	#include <X11/Xlib.h>
#endif
	#include "arc.h"
	#include "config.h"
	#include "podules.h"
}

int main(int argc, char **argv)
{
#ifndef _WIN32
	XInitThreads();
#endif

	strncpy(exname, argv[0], 511);
	char *p = (char *)get_filename(exname);
	*p = 0;

	if(argc > 1)
	{
		wxString config_path = GetConfigPath(argv[1]);

		if(wxFileName(config_path).Exists())
		{
			strcpy(machine_config_file, config_path.mb_str());
			strcpy(machine_config_name, argv[1]);
		}
		else
		{
			wxMessageBox("A configuration with the name '" + wxString(argv[1]) + "' does not exist", "Arculator", wxOK | wxCENTRE | wxSTAY_ON_TOP);
			exit(-1);
		}
	}

	podule_build_list();
	opendlls();
#ifdef _WIN32
	SDL_SetHint(SDL_HINT_WINDOWS_DISABLE_THREAD_NAMING, "1");
#endif

	wxApp::SetInstance(new App());
	wxEntry(argc, argv);

	return 0;
}
