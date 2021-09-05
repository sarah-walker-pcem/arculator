Arculator 2.1
~~~~~~~~~~~~~

Changes since last release :
- New podules : Acorn AKA05 ROM Podule, Acorn AKA10 IO Podule (w/AKA15 MIDI Upgrade), Acorn
  AKA16 MIDI Podule, Aleph One 386PC/486PC Podule, Computer Concepts ColourCard, State Machine
  G16 Graphics Accelerator, Oak 16 Bit SCSI Interface
- Minipodule support for A30x0/A4000. Currently included : Acorn AKA12 MIDI/IO Podule, ICS
  A3INv5 IDE Interface, ZIDEFS IDE Interface
- Added A4 laptop emulation
- Added A500 prototype emulation
- Added HFE disc image support (read/write)
- Fixed MEMC1 timing
- Bugfixes


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
	 A4, A3020 and A4000 support 2MB to 4MB

MEMC - MEMC1 at 8 MHz, MEMC1a at 8 MHz, 12 MHz, or 16 MHz
       MEMC1 is ~10% slower than MEMC1a, and does not support ARM3
       16 MHz is an overclock and not a standard speed

OS - Arthur 0.30 - RISC OS 3.19
     Archimedes 305, 310 and 440 can run all OS versions
     Archimedes 4x0/1 and A3000 can run RISC OS 2.00 and later
     Archimedes 540 can run RISC OS 2.01 and later
     A5000 can run RISC OS 3.00 and later
     A4, A3010, A3020, A4000 and A5000a can run RISC OS 3.10 and later
     A500 has specific builds of Arthur, RISC OS 2.00 and RISC OS 3.10

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
	A3020, A4000, A4, A5000 and A5000a have an internal IDE interface
	Other machines have no internal hard disc interface and need to use a podule

Joystick - Select emulated joystick interface. Available options :
	   A3010 - built in joystick ports. Supports 1 button per joystick
	   GamesPad - SNES controllers connected via printer port. Supports 8 buttons per joystick
	   RTFM - joystick interface installed into Econect socket. Supports 1 button per joystick
	   The Serial Port / Vertical Twist - joystick interface plugged into printer port.
	          Supports 2 buttons per joystick

Podules - Select and configure up to 4 podules per machine. Slot 1 in A30x0 and A4000 machines is
	  a minipodule slot supporting 8-bit minipodules. A3010, A3020 and A4000 have no support
	  for podules in any other slot. A4 has no podule support.

5th column ROM - Select a 5th column ROM to use on A4 and A5000 machines. A4 will use the default
		 LCD/BMU support ROM if nothing else is configured.

Arculator support extension ROM - Enable Arculator support ROM, including HostFS. This is only
				  available on RISC OS 3.


Podules
~~~~~~~

Arculator supports add-on podules installed into the podules directory. It also has several
podules built in.

The podules supplied are :

- Acorn AKA05 ROM Podule
	Supports up to 5 ROMs per podule, plus up to 2 banks of RAM. Battery backed RAM is not
	currently emulated.

- Acorn AKA10 IO Podule w/ AKA15 MIDI podule
	Currently only the MIDI functionality is emulated, and MIDI in has not been well tested.

- Acorn AKA12 MIDI/IO Podule (8-bit)
	Currently only the MIDI functionality is emulated, and MIDI in has not been well tested.

- Acorn AKA16 MIDI Podule
	MIDI in/out.

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

- Aleph One 386/486PC Card
	PC card based on a 386SX design with 1 or 4 MB of RAM. Current emulation supports the base
	386SX25, higher spec 486SLC/25 and later 486SLC2/50 CPUs, optionally with a 387 FPU. !PC
	v2.06 was used; note that later versions may require RiscPC hardware and hence are not
	suitable here. Currently the original 'Diva'/'slow PAL' card is emulated.

- Computer Concepts ColourCard
	Supports higher desktop resolutions with up to 256 colours, and non-desktop resolutions
	with 16-bit colour.

	CC's FlipTop application is required to select the new modes. You must also load a module
	containing mode definitions, my copy of the software has 'AcornMulti' and 'Taxan795'
	modules. Either will work under emulation.

- Computer Concepts Lark
	MIDI in/out, 16-bit sample playback/recording. Sample recording has a few issues, the
	other functions appear to work okay.

- HCCS Ultimate CD-ROM Podule
	Emulates a single Mitsumi CD-ROM drive, mapped to a physical or virtual drive.

- ICS ideA Hard Disc Interface
	Supports two hard drives, using IDEFS v3.10.

- Oak 16-bit SCSI Interface
	Supports hard discs and CD-ROM drives. SCSIFS only supports four hard discs, starting 
	from ID 0. The current podule emulates a single Toshiba XM-3301 drive, which can be at
	any ID, and is mapped to a physical or virtual drive. The ROM image I have does not
	contain any CDFS modules, these will have to be loaded seperately.

	One quirk I discovered - Oak's !SCSIForm application works best with caps lock turned ON.
	Otherwise it may silently fail format/initialisation commands.

- RISC Developments IDE Controller
	Supports two hard drives, using IDEFS v1.15.

- State Machine G16 Graphic Accelerator
	Supports higher desktop resolutions with up to 256 colours, and non-desktop resolutions
	with 16-bit colour.

	The G16 does not have pass-through, so a podule configuration option determines whether
	the emulated monitor is connected to the Archimedes or G16 video out. Whether the card is
	in use or not depends on the RISC OS 'G8Monitor' configuration option; if 0 then the
	standard video is in use, otherwise the G16 is in use.

	The DeskMode application is required to select the new modes.

- Wild Vision MIDI Max
	MIDI in/out.

- ZIDEFS IDE Controller
	Supports two hard drives, using ZIDEFS.

- ZIDEFS 8-bit IDE Controller
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

The below OSes/applications/games/demos are known to work on this version :

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
AudioWorks (v1.38)
Impression Publisher (v4.09)
Impression Style (v3.10)
Music Studio 32 (v1.01)
Notate
!PC (r2.06sf)
PC Emulator (v1.82)
PenDown (v1.72)
ProArtisan
Render Bender (v1.14)
Rhapsody (v1.20)
Rhapsody 3 (v3.03)
Splosh (v1.09)
Studio24 (v1.10)
Talking Canvas Jr (v1.04)
The Complete Animator (v1.04) (HFE)

10 out of 10 - Junior Essentials
Crystal Rainforest 2
Fun School 2 - Under 6
Fun School 2 - 6-8
Fun School 3 - Over 7
My World 2

3D Chess (FDI)
Aldebaran
Alien Invasion (FDI)
Alone in the Dark
Alpha-Blockers
Ankh (CD)
Apocalypse
Arcade Soccer
Asylum
Axis (FDI)
Bambuzle
Battle Chess (FDI)
Battle Tank (FDI)
Big Bang (FDI)
Black Angel (FDI)
Block Out
Blowpipe
Bobby Blockhead vs The Dark Planet
Boogie Buggy (FDI)
BotKiller
BotKiller 2
Bug Hunter in Space
Burn 'Out (FDI)
Cannon Fodder
Cataclysm
Chocks Away (FDI)
Chopper Force (FDI)
Chuck Rock
Command Ship (HFE)
Corruption
Crystal Maze (FDI)
Cycloids (HFE)
Days of Steam (FDI)
Deeva
Demon's Lair (FDI)
Diggers
DinoSaw
Doom+
Drifter
Dune II
Dune II (CD)
EGO : Repton 4
Elite
Empire Soccer
Enter The Realm
E-Type (FDI)
Fine Racer
Fish!
Flashback
FTT (HFE)
Fugitive's Quest
Galactic Dan (FDI)
Global Effect
Gribbly's Day Out
Grievous Bodily 'ARM
Gods
Guile
Gyrinus II
Hamsters
Heimdall
Hero Quest
Holed Out
Hostages
Inertia (FDI)
Ixion
James Pond
James Pond 2 : Robocod
Jinxter
Karma - The Flight Trainer
Lander
Lemmings
Lemmings 2 : The Tribes
Letrouve (FDI)
Loopz
Lotus Turbo Challenge II (FDI)
Mad Professor Mariarti
Magic Pockets (FDI)
Man At Arms (FDI)
Manchester United Europe (FDI)
Master Break (FDI)
Merp (HFE)
Micro Drive (FDI)
Mr Doo
Nebulus
Nevryon
No Excuses
Oh No! More Lemmings (HFE)
Pacmania
Phaethon (HFE)
Poizone
Populous (APD)
Premier Manager (APD)
Psyanki (FDI)
Repton 3
Revelation
Revolver (FDI)
Quest for Gold
Saloon Cars (FDI)
Saloon Cars Deluxe (FDI)
Sensible Soccer (HFE)
Silverball
SimCity
SimCity 2000
Simon the Sorcerer
Simon the Sorcerer (CD)
Speedball 2
Star Fighter 3000 (FDI)
Star Trader (FDI)
Stranded!
Stunt Racer 2000 (APD)
Super Snail
Superior Golf (FDI)
SWIV (FDI)
Sylvia Lane (FDI)
Syndicate (CD)
Technodream (FDI)
The Chaos Engine
The Last Ninja (APD)
The Pawn
Top Banana
Trivial Pursuit (FDI)
Twinworld
Virtual Golf (FDI)
Warlocks
White Magic 2 (FDI)
Wolfenstein 3D (HFE)
Wonderland
World Championship Boxing Manager
Worra Battle
X-Fire (FDI)
Xenon 2
Zarch
Zelanites (FDI)
Zool (HFE)

Adept - Adept
Arc Angels - Arc Angels
Arc Angels - Megademo
ArcEmpire - Transmortal
Archiologics - Jojo
ARM's Tech - Damn!
ARMageddon - Demo Collection III
Armaxess - Megademo 2
Armaxess - Risc Dream
Armie and Bert - Everybody
BASS - TNT
BASS et al - Xtreme
Bitshifters - Back By Popular Demand
Brothers In ARM - 0 Borders
Brothers In ARM - BIA^2
Brothers In ARM - BIAtris
Brothers In ARM - Bounce
Brothers in ARM - Ikosaeder
Brothers In ARM - Rotate
Brothers In ARM - Sister
Byte Pool Productions - Nirvana
DFI - Fishtank
Expression - Insanity
Icebird & Archiologics & Slompt - Ostern95
John Graley - FunkyDemo
John Graley - FunkyDemo2
Network 23 - Graffiti Street
Nophobia - Come and See
Progen - Bad Apple
Progen - Reach
Quantum - Liquid Dreams
Shifty - Braindead
Shifty - Sindemo
Squoquo - Black Zone
The Arc Sailor - Adramort
The Arc Sailor - ARMATAS
The Arc Sailor - Fantasy Demo
The Chip Duo - Demo #1
The Chip Duo - Power Scroll
The Master - Horizon
The Xperience via The Vision Factory - fakeBlu
Xymox Project - Time's Up
Zarquon - Metamorphosis
??? - x-Blu

PC card :
MS-DOS 6.22
Windows 3.1
Windows 95
Works for Windows 3.0
Dune
Scorched Earth


Sarah
b-em@bbcmicro.com