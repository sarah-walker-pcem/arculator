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



Sarah
b-em@bbcmicro.com

If you email me can you please state which emulator you are emailing about. Since Arculator
and RPCemu emulate similar machines it can be very difficult to tell which one you are
talking about.