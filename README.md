# webPSX

Copyright (C) 2015 Juergen Wothke

This is a JavaScript/WebAudio plugin of HighlyExperimental. This plugin is designed to work with my 
generic WebAudio ScriptProcessor music player (see separate project). 

It allows to play Playstation 1&2 music.(which can be found on pages
like these: http://www.vgmusic.com/music/console/sony/ps1/, http://www.zophar.net/music/psf.html, etc)

A live demo of this program can be found here: http://www.wothke.ch/webpsx/


## Credits
The project is based on version HighlyExperimental 2.09 & respective DeadBeef plugin (see https://github.com/kode54/deadbeef_he/).


## Project
It includes most of the original HE code including all the necessary 3rd party dependencies. All the "Web" specific 
additions (i.e. the whole point of this project) are contained in the "emscripten" subfolder. The main interface 
between the JavaScript/WebAudio world and the original C code is the adapter.c file. Some patches were necessary 
within the original codebase (these can be located by looking for EMSCRIPTEN if-defs). 


## Howto build

You'll need Emscripten (http://kripken.github.io/emscripten-site/docs/getting_started/downloads.html). The make script 
is designed for use of emscripten version 1.37.29 (unless you want to create WebAssembly output, older versions might 
also still work).

The below instructions assume that the webpsx project folder has been moved into the main emscripten 
installation folder (maybe not necessary) and that a command prompt has been opened within the 
project's "emscripten" sub-folder, and that the Emscripten environment vars have been previously 
set (run emsdk_env.bat).

The Web version is then built using the makeEmscripten.bat that can be found in this folder. The 
script will compile directly into the "emscripten/htdocs" example web folder, were it will create 
the backend_psx.js library. (To create a clean-build you have to delete any previously built libs in the 
'built' sub-folder!) The content of the "htdocs" can be tested by first copying it into some 
document folder of a web server. 

## Depencencies

The current version requires version 1.02 (older versions will not
support WebAssembly) of my https://github.com/wothke/webaudio-player.

This plugin requires an original PlayStation2 BIOS image - which is NOT bundled here due 
to copyright concerns (When installing the plugin on your own computer you can easily configure the index.html page to 
automatically use your own 4mb PS2 BIOS image - or a .gz version thereof.).

This project comes without any music files, so you'll also have to get your own and place them
in the htdocs/music folder (you can configure them in the 'songs' list in index.html).


## License

This library is free software; you can redistribute it and/or modify it
under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or (at
your option) any later version. This library is distributed in the hope
that it will be useful, but WITHOUT ANY WARRANTY; without even the implied
warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
