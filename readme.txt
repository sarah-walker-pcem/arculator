Arculator 2.2
~~~~~~~~~~~~~

Changes since last release :
- New podules: Acorn AEH50 Ethernet II, Acorn AEH54 Ethernet III, Acorn AKA32 SCSI Podule, Design IT Ethernet 200,
  Morley A3000 User and Analogue Port, Risc Developments High Density Floppy Controller
- Support for HFE v3 and SCP disc images
- Integrated debugger
- Optimisations
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

- Acorn AEH50 Ethernet II
	Supported on both RISC OS and RISCiX. See Readme-NETWORKING.txt for setup details.

- Acorn AEH54 Ethernet III
	Supported on RISC OS only. See Readme-NETWORKING.txt for setup details.

- Acorn AKA05 ROM Podule
	Supports up to 5 ROMs per podule, plus up to 2 banks of RAM. Battery backed RAM is not
	currently emulated.

- Acorn AKA10 IO Podule w/ AKA15 MIDI podule
	MIDI and analogue functionality are emulated. Analogue is mapped to joysticks. MIDI in
	has not been well tested.

- Acorn AKA12 MIDI/IO Podule (8-bit)
	MIDI and analogue functionality are emulated. Analogue is mapped to joysticks. MIDI in
	has not been well tested.

- Acorn AKA16 MIDI Podule
	MIDI in/out.

- Acorn AKA31/AKA32 SCSI Controller
	Supports hard discs and CD-ROM drives. SCSIFS only supports four hard discs, starting 
	from ID 0. The current podule emulates a single Toshiba XM-3301 drive, which can be at
	any ID, and is mapped to a physical or virtual drive.

	There are several versions of the ROM for this podule. AKA31 works on RISC OS 2 and later.
	AKA32 requires RISC OS 3 and includes the CDFS modules.

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

- Design IT Ethernet 200
	A3020/4000 specific networking. See Readme-NETWORKING.txt for setup details.

- HCCS Ultimate CD-ROM Podule
	Emulates a single Mitsumi CD-ROM drive, mapped to a physical or virtual drive.

- ICS ideA Hard Disc Interface
	Supports two hard drives, using IDEFS v3.10.

- Morley A3000 User and Analogue Port
	Analogue is mapped to joysticks.

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
Debugger :
	Enable debugger - Enable and disable debugger window
	Break - Trigger an immediate break to disassembler
	

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
RISCiX 1.21

2067BC
3D Chess
Aggressor
Air Supremacy
Aldebaran
Alerion (FDI)
Alien Invasion
Aliped
Alone in the Dark
Ankh (CD-ROM)
Apocalypse
ArcPinball (HFE)
Axis
Battle Chess (FDI)
Big Bang
Birds of War (HFE)
Black Angel (HFE)
Blitz (FDI)
Block Out
Bloxed
Blowpipe
Bobby Blockhead vs The Dark Planet
Boogie Buggy (FDI)
Botkiller
Botkiller 2
Brian Clough's Football Fortunes (FDI)
Bug Hunter
Bug Hunter in Space
Burn 'Out (HFE v3)
Cannon Fodder
Cataclysm
Chuck Rock
Command Ship (HFE)
Corruption
Cyber Ape (HFE)
Cycloids (HFE)
Darkwood (APD)
Days of Steam
Demon's Lair (FDI)
Diggers
Doom+
Dragonball
Drifter
Drop Ship
Dune II
Dune II (CD-ROM)
EGO: Repton 4
Elite
Empire Soccer 94
Enigma
Enter The Realm (HFE)
E-Type
Fine Racer
Fire & Ice (SCP)
Fireball II
Fish
Flashback
FRED (HFE)
FTT (HFE)
Galactic Dan (FDI)
Global Effect
Gorm
Grevious Bodily 'ARM (FDI)
Guile
Gyrinus II - Son of Gyrinus
Hamsters
Heimdall
Hero Quest (HFE)
Holed Out
Inertia
Inferno (HFE v3)
Interdictor
Interdictor 2
Iron Lord
Ixion (HFE)
Jahangir Kahn's World Championship Squash (HFE)
James Pond (HFE)
James Pond 2 : Robocod (HFE)
Jinxter
Karma - The Flight Trainer
Leeds United Champions
Lemmings
Lemmings 2 : The Tribes
Letrouve
Loopz
Lotus Turbo Challenge 2 (HFE)
Mad Professor Mariarti
Magic Pockets (APD)
Man At Arms
Manchester United
Manchester United Europe
Master Break (FDI)
Micro Drive
MiG-29 Fulcrum
Mirror Image (HFE)
Moonquake
Mr Doo
Nebulus
Nevryon (APD)
No Excuses
Oh No! More Lemmings (HFE)
Overload (Clares)
Overload (Paradise) (HFE v3)
Pac-Mania
Pandora's Box (APD)
Paradroid 2000
Phaethon (HFE)
Pipemania
Poizone
Populous (HFE)
Premier Manager (HFE)
Pushy II
Repton 3 (HFE)
Revelation
Revolver
Sensible Soccer (HFE)
Shanghai
SimCity (HFE)
SimCity 2000
Simon The Sorcerer
Simon The Sorcerer (CD-ROM)
Silver Ball
Small
Speedball 2
Spheres of Chaos
Spobbleoid Fantasy (HFE)
Sporting Triangles
Star Fighter 3000 (HFE)
Stunt Racer 2000
Super Snail
SWIV
Syndicate+ (CD-ROM)
Technodream (FDI)
The Chaos Engine
The Crystal Maze
The Dungeon (APD)
The Last Ninja (HFE)
Top Banana
Tower of Babel
Twinworld
Warlocks
WIMP Chess
Wolfenstein 3D (HFE)
Wonderland
World Championship Boxing Manager
Xenon II (HFE)
Zarch (HFE)
Zool (HFE)

1st Word Plus (v2.01)
Acorn Advance (v1.01)
Acorn C/C++
ArtWorks (v1.70)
AudioWorks (v1.38)
Browse (v2.01)
Eureka (v3.00)
Impression Publisher (v4.09)
Impression Style (v3.10)
Miracle (v0.10)
Music Studio 32
Notate
PC Emulator (v1.82)
Rhapsody 2
Rhapsody 3
Studio 24 (v1.10)
The Complete Animator (v1.04) (HFE)

Albert's House (HFE)
Crystal Rainforest 2
Darryl The Dragon (HFE)
Edwina's Energetic Elephant
Fun School 2 - Under 6s
Fun School 3 - Over 7s
Fun School 4 - 5-7s
Granny's Garden (HFE)
Hutchinson Multimedia Encyclopedia (CD-ROM)
KidPix
My World 2
Playdays (HFE)
Vikings! (CD-ROM)

Adept - Adept
Arc Angels - Megademo
ArcEmpire - Transmortal
Archiologics - Jojo
Archiologics & Icebird - Ostern Rulez!
ARM's Tech - Cakehead 2
Armaxess - Risc Dream
BASS - TNT
BASS et al - Xtreme
Bitshifters - Back By Popular Demand
Bitshifters - Tipsy Cube
Brothers in ARM - 0 Borders
Brothers in ARM - BIAtris
Brothers in ARM - Bounce
Brothers in ARM - Demo^2
Brothers in ARM - Ikosaeder
Brothers in ARM - Rotate
Brothers in ARM - Sister
Desire - Asynchronous
DHS - Signals
Expression - Insanity
Expression - Signum
FSG - Funky Demo
FSG - Funky Demo 2
Karl Morton - Coppersine
Nophobia - Come and See
progen - Bad Apple
progen - Reach
progen - I Heard You Like Dithering
Quantum - Liquid Dreams
Rabenauge & Bitshifters - Chipo Django
Rabenauge & Bitshifters - Chipo Django 2
Shifty - Brain Dead
Shifty - Sine Wave Demo
SICK - Ba!
Squoquo - Black Zone
Squoquo - Digital Prophecy
The Chip Duo - Powerscroll
The Master - Horizon
The Xperience - Xcentric
Xymox Project - Time's Up
Zarquon - Metamorphosis

PC card :
After Dark 3.0
Civilization for Windows
Microsoft Arcade
MS-DOS 6.22
Windows 95
Windows for Workgroups 3.11
Works for Windows 3.0

Sarah
b-em@bbcmicro.com
