Arculator v2.0 Linux supplement


You will need the following libraries :

SDL2
wxWidgets 3.x
OpenAL

and their dependencies. ALSA is also needed to build podules.

Open a terminal window, navigate to the Arculator directory then enter

./configure --enable-release-build
make

then ./arculator to run.

configure options are :
  --enable-release-build : Generate release build. Recommended for regular use.
  --enable-debug         : Compile with debugging enabled.
  --disable-podules      : Don't build external podules

The menu is a pop-up menu in the Linux port. Right-click on the main window when mouse is not
captured.
