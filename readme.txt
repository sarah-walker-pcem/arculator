Arculator 0.99
~~~~~~~~~~~~~~

Arculator emulates an Archimedes (old A3xx,A4xx,A3000,A540, or newer A5000,
A30x0, A4000) with an ARM2, ARM250 or ARM3, between 512k and 16 megabytes of 
RAM, up to four floppy drives, an IDE hard disc and a multisync monitor.

Changes since last release :

- Improved timing. Each model now performs much closer to the original machines (this means
   slower than previous versions).
- Can now emulate MEMC page sizes other than 32k, so all memory configurations from 512k to
   16 megs are now possible.
- Preliminary FDI support in 82c711 FDC.
- Better FDI support in WD1772 FDC, including FDI v2.1 support.
- Fast disc emulation in 82c711 FDC.
- Bugfixes to video code. Man United Europe now looks better. Less flicker in Populous, Zarch
  etc.
- Keyboard/mouse fix, now doesn't freeze in Lemmings 2 and some other games.
- Fixed broken sound in Sensible Soccer and Xenon 2.
- Sound now in stereo.
- Some minor optimisations.
- Seperate ROM + CMOS files for Arthur, RISC OS 2, RISC OS 3 (old FDC) and RISC OS 3 (new FDC).
- Added support for two MAME romsets that run on Archimedes hardware.
- Tracked WD1772 hanging bug down to an incorrectly set CMOS bit! Now fixed.


Files
~~~~~

arculator.exe   - The emulator itself
arc.cfg         - Configuration file
alleg42.dll     - Required DLL file
changes.txt     - Changelog
cmos\*\cmos.bin - CMOS RAM files
readme.txt      - This file
src\*           - Source (needs Ming/W and Allegro to compile)


Usage
~~~~~

Just run Arculator.exe. You will need to supply the RiscOS 3 ROM images as
either a single file rom, or four files ic24.rom-ic27.rom. They can be 
dumped from a real machine with the following sequence of commands :

*SAVE ic24 3800000+80000
*SAVE ic25 3880000+80000
*SAVE ic26 3900000+80000
*SAVE ic27 3980000+80000

Alternatively you could use

*SAVE rom 3800000+200000

but you'd have fun transfering it to a PC.

For RISC OS 2 and Arthur you only need

*SAVE rom 3800000+80000

Once you have the files then place them in the relevant ROM directories.

For speed reasons, it is best to have your desktop in 16-bit colour when running
Arculator.


Menus
~~~~~

File - Reset - soft-resets the Archimedes
       Exit  - exits back to Windows
Disc - Change disc - Loads a new disc into drives 0-3
       Remove disc - Unloads any disc that might be loaded from drives 0-3
       Fast disc loading - Increases speed of disc loading. Disc writing is not accelerated.
Machine - CPU Type - Select between :
		     ARM2   - 8mhz, used on A3xx, A4xx, A3000
		     ARM250 - 12mhz, used on A3010, A3020 and A4000
		     ARM3   - 25mhz (ish) used on A4 and A5000
          RAM Size - Select between 512k, 1, 2, 4, 8 and 16 megs of RAM.
          Operating System - Select between :
                     Arthur    - WD1772 FDC, max 4 megs RAM. Tested with Arthur 1.2
                     RISC OS 2 - WD1772 FDC, max 4 megs RAM. Tested with RISC OS 2.01
                     RISC OS 3 (old fdc) - WD1772 FDC. Tested with RISC OS 3.11
                     RISC OS 3 (new fdc) - 82c711 FDC. IDE hard disc supported.
                                           Tested with RISC OS 3.11. Limited FDI support.
		     Ertictac/Tactic - MAME ROM set 'ertictac'
                     Poizone         - MAME ROM set 'poizone'
Options - Sound enable - Enables/disables sound
          Stereo sound - Selects stereo or mono sound. Many music players sound quite odd
                         in stereo, especially on headphones, due to the Amiga-style panning.
	  Limit speed - Limit speed to what should be about 8/12/25mhz. Sound has to be disabled
                        as well as this for any speedup to happen.
Video - Full screen - Goes to a full screen. Use CTRL+END to return to windowed mode.
        Full borders - Toggles expansion of borders - useful for programs that overscan.
        Blit method - Selects between :
          Scanlines - Skip every other line to create a TV-style effect
          Software scale - Double each line in software. Reliable, but slow
          Hardware scale - Double each line in hardware. Fast, but some graphics cards 
    			   apply a filter effect.


Standard configurations
~~~~~~~~~~~~~~~~~~~~~~~

These are the standard configurations to emulate the various Archimedes models :

A3000, A3xx,  A4xx  - ARM2,   WD1772 FDC
A540                - ARM3,   WD1772 FDC
A3010, A3020, A4000 - ARM250, 82c711 FDC
A5000, A4           - ARM3,   82c711 FDC

The ARM3 emulation runs somewhere between 25 and 33 mhz (I haven't determined where yet).
The ARM2 and ARM250 run slightly faster than the real machines, but much closer than in
previous releases.


Ertictac/Tactic/Poizone
~~~~~~~~~~~~~~~~~~~~~~~

These are arcade games based on A3xx hardware. Controls for these games are :

1 	           - Player 1 start
2 		   - Player 2 start
5                  - Coin 1
6 		   - Coin 2
Up/down/left/right - Joystick
CTRL               - Fire


FAQ
~~~

Q : I'm at a supervisor prompt, how do I make it go to the desktop?
A : Type 'desktop'. If you want to avoid this, type 'configure language 10' and it
    will then go to the desktop at startup.

Q : I'm having trouble running a game
A : Some games are incompatible with RISC OS 3 (mainly pre-1990 games) and need to be run
    with RISC OS 2. Some (very old games) are incompatible with RISC OS and need to be run
    with Arthur. The only answer, short of finding a fixed version, is to track down the
    relevant OS.
    Many protected FDI images do not work as of yet. They may be made to work in the future
    however.
    Some stuff runs into other bugs in Arculator, eg Nebulus does not work with the 82c711
    FDC, for no apparent reason.
    And some just don't work (Bug Hunter, EGO : Repton 4).

Q : The picture looks blurry.
A : This is a side effect of using hardware scaling on many graphics cards. If you
    can spare the speed, go to Video->Blit method and choose one of the other blit
    modes.

Q : The sound is skipping.
A : Arculator is not able to run at full speed. Change to a slower processor emulation (ie
    avoid ARM3) or change to a faster blit mode (hardware scale is faster than scanlines,       
    which is faster than software scale). Try setting your Windows desktop to 16-bit colour
    - when it is not in 16-bit colour Arculator has to perform an extra video conversion,
    which slows it down.
    This can happen during IDE disc access, which is unavoidable. IDE emulation is _not_
    fast. FDI disc access is also very slow, as Arculator has to decompress the disc in
    real time (the alternative would have been a very long pause when the disc image had
    been selected).


Compatibility
~~~~~~~~~~~~~

RiscOS 3 - Boots. Supervisor useable. BASIC usable. Desktop usable.
RiscOS 2 - Seems okay. 82c711 doesn't work as ROS 2 doesn't support it.
Arthur   - Supervisor and BASIC usable.


Games :

Battle Chess (FDI)         - Playable
Black Angel (FDI)          - Playable
Boogie Buggy (FDI)         - Playable
Bug Hunter 2               - Playable
Cannon Fodder              - Playable
Cataclysm                  - Playable
Chuck Rock                 - Playable
Cycloids                   - Playable
Darkwood	           - Playable
Elite                      - Playable
Enter The Realm            - Playable, flickery on anything higher than ARM2
E-Type (+FDI)              - Playable
E-Type 2 (FDI)             - Playable
Flashback (demo)           - Playable
Gods                       - Playable
Grevious Bodily 'ARM (FDI) - Playable, screen too far to the right?
Gyrinus 2                  - Playable, no music unless run from desktop
Holed Out (FDI)            - Playable
Inertia	(FDI)		   - Playable
James Pond                 - Playable
Lander                     - Playable
Last Ninja (demo)          - Playable
Lemmings 2                 - Playable
Lemmings 2 (demo)          - Playable
Lemmings (+FDI)            - Playable
Lemings                    - Playable
Lotus 2                    - Playable, slightly flickery palette splits
Mad Professor Mariarti     - Playable
Magic Pockets (demo)       - Playable
Magic Pockets (FDI)        - Playable
Man United Europe (+FDI)   - Playable, flickery at the borders
Master Break               - Playable
Moondash                   - Playable
Nebulus                    - Playable, only with WD1772 FDC (ie not "RISC OS 3 (new FDC)")
Nevyron (not FDI)          - Playable
Oh No More Lemmings (+FDI) - Playable
Overload (FDI)             - Playable
Pacmania (+FDI)            - Playable
Paradroid 2000             - Playable
Phaethon                   - Playable
Pipemania (FDI)            - Playable
Populous                   - Playable
Premier Manager (FDI)      - Playable
Overload (FDI)             - Playable
Repton 3                   - Playable
Saloon Cars (FDI)          - Playable, RISC OS 2 only!
Saloon Cars Deluxe (FDI)   - Playable
Sensible Soccer (FDI)      - Playable, might want to enable full borders
Speedball 2                - Playable
Spheres of Chaos (demo)    - Playable
Starfighter 3000 (demo)    - Playable
Starfighter 3000 (FDI)     - Playable
Stunt Racer 2000 (FDI)     - Playable
Talisman (+FDI)            - Playable
Technodream (FDI)          - Playable
Terramex                   - Playable
Thundermonk (FDI)          - Playable
Top Banana (+FDI)          - Playable
Trivial Pursuit (FDI)      - Playable
Twinworld                  - Playable
Virtual Golf (FDI)         - Playable
Warlocks (demo)            - Playable
Wolfenstein 3D (FDI)       - Playable, if you install to RAM disc (!)
X-Fire (FDI)               - Playable
Xenon 2                    - Playable
Zarch (+FDI)               - Playable, don't set screen memory too high, flickery on anything higher than ARM2
Zelanites (demo)           - Playable
Zool (demo)                - Playable
Zool (FDI)                 - Playable

Wonderland                 - Stops responding to keyboard after a while. Still playable to
                             an extent, due to the extensive mouse control.

Bug Hunter        - Out of data error
EGO : Repton 4    - Hangs

Tactic/Ertictac (arcade) - Playable. DIP switched aren't configurable at the moment, so you can't change
                           coin/difficulty settings, or change Tactic into Ertictac.
Poizone         (arcade) - Playable. DIP switched aren't configurable at the moment, so you can't change
                           coin/difficulty settings.



Apps :

6502Em - Works
!65Host - Works
!ArmSI - Works. Results are a tad optimistic (but seemingly correct).
!ARPlayer - Works
Coconizer - Works
Impression    - Works
Impression Jr - Works. 
Logotron Pen Down 1.72 - Works
Notate - Works
ProArtisan - Works


Demos :

Arcangels  - Arcangels   - Works
Arcangels  - Megademo    - Works. Is best in ARM250 (won't load off 82c711 floppy in ARM2 + some parts
                           too slow, menu judders in ARM3)
Arcangels  - Spinguin    - Works
Arc Empire - Transmortal - Works
Armageddon - Demo Collection 3 - Works
Armaxess   - Megademo 2  - Works
Armaxess   - Risc Dream  - Works
BIA        - BIA^2       - Works. Screen too wide for Arculator window?
BIA        - Bounce      - Works
Bytepool   - Nirvana     - Works
TXP        - Xcentric    - Works. Screen too wide for Arculator window



Todo list :
~~~~~~~~~~~

Optimize. There are a few areas in the ARM emulation that I know of that can be
optimized, and probably the video emulation as well. 

Add FDI support to 82c711 FDC. Probably not that difficult, but after implementing it
on 1772 it's not something I really fancy doing at the minute.

Finish off ArculFS - make it read/write.


Tom Walker
b-em@bbcmicro.com

If you email me can you please state which emulator you are emailing about. Since Arculator
and RPCemu emulate similar machines it can be very difficult to tell which one you are
talking about.