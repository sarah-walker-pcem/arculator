Arculator 2.0
~~~~~~~~~~~~~


Changes since last release :
- FPA10 emulation
- Podule emulation. Current included podules : AKA31 SCSI Podule, AKD52 Hard Disc Podule,
  Computer Concepts Lark, HCCS Ultimate CD-ROM, ICS ideA, RISC Developments IDE Podule,
  Wild Vision MIDI Max, ZIDEFS
- Much better CPU/memory timing. Now emulates ARM3 cache and MEMC1/MEMC1a timings
- Added emulation of GamesPad, RTFM and Serial Port/Vertical Twist joysticks
- Improved sound filter emulation
- Re-implemented FDI support
- Added HostFS (ported from RPCemu)
- Disc drive noise
- Linux port
- Numerous bug fixes
- Many other changes



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

for four 512 kB files. Alternatively you could use

*SAVE rom 3800000+200000

for a single 2 MB file.

For RISC OS 2 and Arthur you only need

*SAVE rom 3800000+80000

for a single 512 kB file.


Configuration
~~~~~~~~~~~~~

Arculator requires you to create a machine configuration to emulate. Initially select a base
machine type, then configure the following options :

CPU - ARM2, ARM250, ARM3 of varying speeds
      Note that ARM2 and ARM250 run at the same speed as memory/MEMC. ARM3 has it's own CPU clock.
      ARM3 requires a MEMC1a

FPU - None or FPA10
      FPA10 requires an ARM3

Memory - Varies depending on machine
         Archimedes 305 & 310 support 512kB - 16MB
	 Archimedes 410/1, A3000 and A5000 support 1MB - 16MB
	 Archimedes 420/2 supports 2MB - 16MB
	 Archimedes 440, 440/1, 540 and A5000a support 4MB - 16MB
	 A3010 supports 1MB to 4MB
	 A3020 and A4000 support 2MB to 4MB

MEMC - MEMC1 at 8 MHz, MEMC1a at 8 MHz, 12 MHz, or 16 MHz
       MEMC1 is ~10% slower than MEMC1a, and does not support ARM3
       16 MHz is an overclock and not a standard speed

OS - Arthur 0.30 - RISC OS 3.19
     Archimedes 305, 310 and 440 can run all OS versions
     Archimedes 4x0/1 and A3000 can run RISC OS 2.00 and later
     Archimedes 540 can run RISC OS 2.01 and later
     A5000 can run RISC OS 3.00 and later
     A3010, A3020, A4000 and A5000a can run RISC OS 3.10 and later

Monitor - Standard, Multisync, VGA or Mono
          Standard supports lower resolution modes (288 lines and lower)
	  Multisync supports all modes except high res mono
	  VGA supports high resolution modes only (350 lines and up). Games will mostly run
            letterboxed
          Mono only supports high res mono (1152x896)

Unique ID - Unique machine ID implemented on A3010, A3020, A4000, A5000 and A5000a systems
            This is used by some copy protected software. Most users won't need to change this


Hard discs - Configures hard discs on internal interface (if present)
	Archimedes A440 and A4x0/1 have an internal ST-506 interface
	A3020, A4000, A5000 and A5000a have an internal IDE interface
	Other machines have no internal hard disc interface and need to use a podule

Joystick - Select emulated joystick interface. Available options :
	   A3010 - built in joystick ports. Supports 1 button per joystick
	   GamesPad - SNES controllers connected via printer port. Supports 8 buttons per joystick
	   RTFM - joystick interface installed into Econect socket. Supports 1 button per joystick
	   The Serial Port / Vertical Twist - joystick interface plugged into printer port.
	          Supports 2 buttons per joystick

Podules - Select and configure up to 4 podules per machine


Podules
~~~~~~~

Arculator supports add-on podules installed into the podules directory. It also has several
podules built in.

The podules supplied are :

- Acorn AKA31 SCSI Controller
	Supports hard discs and CD-ROM drives. SCSIFS only supports four hard discs, starting 
	from ID 0. The current podule emulates a single Toshiba XM-3301 drive, which can be at
	any ID, and is mapped to a physical or virtual drive.

	There are several versions of the ROM for this podule. I've seen an older version that
	works on RISC OS 2 and later, and a newer version that requires RISC OS 3 and includes the
	CDFS modules. There may be other versions.

- Acorn AKD52 Hard Disc Controller
	ST-506 podule, for use with Archimedes 305, 310 and 540 machines. Supports two hard drives.
	This will most likely not work on other machines.

- Arculator support modules
	Contains HostFS support modules, support module implementing MODE 99 (800x600x256), and 
	FPE v4.00 for use with FPA emulation.

- Computer Concepts Lark
	MIDI in/out, 16-bit sample playback/recording. Sample recording has a few issues, the
	other functions appear to work okay.

- HCCS Ultimate CD-ROM Podule
	Emulates a single Mitsumi CD-ROM drive, mapped to a physical or virtual drive.

- ICS ideA Hard Disc Interface
	Supports two hard drives, using IDEFS v3.10.

- RISC Developments IDE Controller
	Supports two hard drives, using IDEFS v1.15.

- Wild Vision MIDI Max
	MIDI in/out.

- ZIDEFS IDE Controller
	Supports two hard drives, using ZIDEFS.


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
	Border size :
		No borders     - Don't draw video borders
		Native borders - Draw borders as programmed by RISC OS
		Fixed (standard monitor) borders - Draw borders to the fixed size of a 'standard'
						   TV-resolution monitor.
	Blit method :
		Scanlines - Scale low resolution mode using blank scanlines
		Line doubling - Scale low resolution mode using line doubling
	Render driver - Select between Auto, Direct3D, OpenGL and Software rendering
	Scale filtering - Select between nearest sampling (blocky) and linear sampling (blurry)
			  when scaling up video
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
	Output filter - Control strength of sound low-pass filter. Reduced filtering sounds less
			muffled but may introduce aliasing noise
Settings :
	Configure machine - Configuration of emulated machine

	

Compatibility
~~~~~~~~~~~~~

The below OSes/applications/games demos are known to work on this version :

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
AudioWorks (v1.53)
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
POVRay (v2.2)
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
Warlocks
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