Arculator 0.99
~~~~~~~~~~~~~~


Changes since last release :



Usage
~~~~~

Arculator requires at least one RISC OS or Arthur ROM set. These should be placed
in the appropriate directory in the 'roms' directory. For most users RISC OS 3.11
will be the most useful.





Dumping ROMs
~~~~~~~~~~~~

If you want to dump ROMs from a real machine, this can be done from the RISC OS
command line :

*SAVE ic24 3800000+80000
*SAVE ic25 3880000+80000
*SAVE ic26 3900000+80000
*SAVE ic27 3980000+80000

Alternatively you could use

*SAVE rom 3800000+200000

For RISC OS 2 and Arthur you only need

*SAVE rom 3800000+80000



Menus
~~~~~

File :
	Hard reset - Hard-resets the Archimedes
      	Exit       - Exits the emulator
Disc :
	Change drive 0-3 - Loads a new disc image into drives 0-3
	Eject drive 0-3  - Unloads any disc image that might be loaded from drives 0-3
	Disc drive noise - Enable/disable and control volume of 3.5" disc drive noise
Video :
	Fullscreen - Switch to fullscreen mode. Use CTRL+END to exit back to windowed mode
	Resizeable window - Allow window to be resized. When disabled the window size will automatically adjust to the emulated resolution
	Border size :
		No borders     - Don't draw video borders
		Native borders - Draw borders as programmed by RISC OS
		Fixed (standard monitor) borders - Draw borders to the fixed size of a 'standard' TV-resolution monitor
	Blit method :
		Scanlines - Scale low resolution mode using blank scanlines
		Line doubling - Scale low resolution mode using line doubling
	Render driver - Select between Auto, Direct3D, OpenGL and Software rendering
	Scale filtering - Select between nearest sampling (blocky) and linear sampling (blurry) when scaling up video
	Output stretch-mode :
		None          - No limits on scaling
		4:3           - Force 4:3 aspect ratio when scaling
		Square pixels - Force square pixels when scaling
		Integer scale - Force integer pixel scaling
	Output scale - Allow fixed video scaling from 0.5x - 4x
Sound :
	Sound enable  - Enable/disable sound
	Stereo sound  - Enable/disable stereo
	Output level  - Control optional sound amplification from 0 to 18 dB
	Output filter - Control strength of sound low-pass filter. Reduced filtering sounds less muffled but may introduce aliasing noise
Settings :
	Configure machine - Configuration of emulated machine

	
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




Compatibility
~~~~~~~~~~~~~

Arthur 0.30
Arthur 1.20
RISC OS 2.00
RISC OS 2.01
RISC OS 3.00
RISC OS 3.10
RISC OS 3.11
RISC OS 3.19
RISC iX 1.2.1c

1st Word Plus (v2.01)
Acorn Advance (v1.01)
Acorn C/C++
ArtWorks (v1.7)
ClearView (v1.06)
Coconizer (v1.31)
EasiWriter Professional (v4.08)
FasterPC (v3.005)
Impression Publisher (v4.09)
Impression Style (v3.10)
Miracle (v0.10)
Music Studio 32 (v1.01)
Notate
Ovation (v1.42S)
PC Emulator (v1.82)
PenDown (v1.72)
ProArtisan
ProSound (v1.11b)
Rhapsody (v1.20)
Rhapsody 3 (v3.03)
SparkFS (v1.28)
Speculator (v1.03)
Studio24 (v1.10)
TechWriter Professional (v4.10)

3D Tanks
Aggressor
Air Supremacy
Aldebaran
Alerion (FDI)
Alien Invasion (FDI)
Aliped
Alone in the Dark
All In Boxing
Ankh
Apocalypse
Arcade Soccer
ArcElite
ARCticulate (FDI)
Asylum
Axis (FDI)
Bambuzle (FDI)
Battle Chess (FDI)
Battle Tanks (FDI)
Big Bang (FDI)
Black Angel (FDI)
Blaston
Blitz (FDI)
Blowpipe
Bloxed
Bobbie Blockhead vs The Dark Planet
Boogie Buggy (FDI)
Botkiller
Botkiller 2
Break 147
Brian Clough's Football Fortunes (FDI)
Bubble Fair
Bug Hunter
Bug Hunter II
Burn 'Out (FDI)
Cannon Fodder
Cataclysm (FDI)
Chess (FDI)
Chess 3D
Chocks Away
Chopper Force (FDI)
Chuck Rock
Conqueror
Creepie Crawlie
Cyber Chess (FDI)
Cycloids (APD)
Darkwood (APD)
Days of Steam (FDI)
Demons Lair (FDI)
Diggers
DinoSaw
Doom+
Drifter
Dune II (floppy & CD v1.30b)
Empire Soccer
Enigma
Enter the Realm (FDI)
E-Type (FDI)
E-Type 2 (FDI)
Fireball II
Flashback
FTT (APD)
Galactic Dan
Gods (FDI)
Grievous Bodily 'ARM (FDI)
Gyrinus II 
Hamsters
Heimdall
Hero Quest
Holed Out (FDI)
Hostages
Inertia (FDI)
Inferno
Iron Lord
Ixion
Jahangir Khan World Championship Squash (FDI)
James Pond
Leeds United Champions
Lemings
Lemmings
Lemmings 2 : The Tribes
Lotus 2 (FDI)
Mad Professor Mariati
Magic Pockets (FDI)
Man At Arms (FDI)
Manchester United (FDI)
Manchester United Europe (FDI)
Master Break (FDI)
MicroDrive (FDI)
MiG-29 Fulcrum
Moonquake
Mr Doo
Nebulus
Nevryon (APD)
No Excuses (FDI)
Oddball
Oh No! More Lemmings
Overload (Clares - FDI)
Overload (Paradise)
Pacmania
Pandora's Box (APD)
Paradroid 2000
Phaethon
Pipe Mania
Poizone
Populous (APD)
Premier Manager (APD)
Pysakki
Quest For Gold
RedShift (FDI)
Repton 3
Revelation
Revolver (FDI)
Saloon Cars (FDI)
Saloon Cars Deluxe (FDI)
Sensible Soccer (APD)
Silverball (APD)
SimCity
SimCity 2000
Simon the Sorcerer (CD)
Speedball II
Spheres of Chaos
Spy Snatcher (FDI)
Star Fighter 3000 (APD)
Starch
Stunt Racer 2000 (APD)
Super Foul Egg
Super Snail
SWIV
Talisman
Technodream (FDI)
Terramex
The Adventures of Sylvia Lane (FDI)
The Dungeon (APD)
The Last Ninja (APD)
The Olympics (FDI)
The Wimp Game (APD)
Time Zone
Top Banana
Tower of Babel
Trivial Pursuit
Twin World
Virtual Golf (FDI)
White Magic 2 (FDI)
Wolfenstein 3D (APD)
Wonderland
World Championship Boxing Manager
World Class Leaderboard (APD)
X-Fire (FDI)
Zarch (FDI)
Zool (FDI)

Address Exception - Demo6399
Arc Angels - Arc Angels
Arc Angels - Megademo
Arc Sailor - Adramort
Arc Sailor - ARMATAS
Arc Sailor - Fantasy Demo
ArcEmpire - Transmortal
Archiologics - Jojo
Archiologics/Icebird/Slompt - Ostern95
ARM's Tech - CakeHead 2
ARM's Tech - Damn!
ARMageddon - Demo Collection III
Armaxess - Risc Dream
Armie and Bert - Everybody
BASS et al - Xtreme (v1.02)
Brothers in ARM - BIATetris
Brothers in ARM - Bounce
Brothers in ARM - DEMO^2
Brothers in ARM - Rotate
Brothers in ARM - Sister
Bytepool - Nirvana
Daydream Software - Adept
Expression - Insanity
Expression - Signum
Network 23 - Grafitti Street
Quantum - Liquid Dreams
Shifty - Sinedemo
SICK - Ba
SquoQuo - Black Zone
TCD - Power Scroll
The Master - Horizon
The Obsessed Maniacs - Zerblast
The Xperience - Xcentric
Zarquon - Metamorphosis

Sarah
b-em@bbcmicro.com

If you email me can you please state which emulator you are emailing about. Since Arculator
and RPCemu emulate similar machines it can be very difficult to tell which one you are
talking about.