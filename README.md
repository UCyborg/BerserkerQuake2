# Berserker@Quake2 Engine

Original project hosted at: [Quake Gate](http://berserker.quakegate.ru/), [ModDB](http://www.moddb.com/mods/berserkerquake2)

This is a port of Berserker@Quake2 engine (1.45) to SDL2 library.
It is currently known to compile and run on Linux and Windows.

### Building on Linux

The makefile is provided in the root of this source tree, which should,
provided you have all needed libraries setup, produce "berserker"
executable file and game logic library "libgame.so". These libraries are:

libjpeg8  
libpng (v1.2.* or compatible)  
libogg  
libvorbis  
libvorbisfile  
SDL2  
zlib

Please note that you must install 32-bit versions of these libraries
because 64-bit issues are yet to be resolved. Depending on your
environment, you may need to make minor adjustments to the makefile
or your environment (eg. 64-bit Ubuntu doesn't create proper symlinks
to 32-bit libraries).

Code::Blocks project files are also provided.

### Building on Windows

Visual Studio 2015 solution is provided, along with header files and .libs
for linking with required libraries. Libraries are provided along with
precompiled engine binaries in the Releases section.

### Running the game

To make use of this engine, you will need original Quake II data. If you have Steam
version, you can find needed files in its install directory, in baseq2 folder. These are:

DIR players  
DIR video  
FILE pak0.pak  
FILE pak1.pak  
FILE pak2.pak  

Make a distintctive folder where you want to run Berserker from. The folder must be
writable, so somewhere under home folder should do for Linux users. Inside it, create
baseq2 folder in which you put the above mentioned files.

Then you will have to get some extra data that comes with this engine. You can get it
from my [Google Drive](https://drive.google.com/file/d/0B2DPRdP0LBDGemtyUTBKLVZUUkU/view?usp=sharing). Extract the zip in your Berserker directory.
The content itself is identical to those hosted at official website and ModDB, except
all file names were converted to lower case and references to them, as well as paths have
been fixed to use slashes instead of backslashes so hopefully there isn't any problem with
file access on Linux systems.

Now, you just need to put compiled executable files in their places:  
(assuming Quake2 is the folder where you put the data)

berserker or Berserker.exe -> Quake2  
libgame.so or Game.dll -> Quake2/baseq2

Finally, you can run the game. Make sure it's executed from its own directory so it can find the data.

One final tip, if you're on Windows and have NVIDIA graphics card, you should disable
threaded optimizations for the game, they seem to dramatically decrease the performance.
