/*
* webPSX
* =======
*
* 	Copyright (C) 2015 Juergen Wothke
*
* Based on original C code of HighlyExperimental & respective DeadBeef plugin (see https://github.com/kode54/deadbeef_he/).
*
* LICENSE
*
* This library is free software; you can redistribute it and/or modify it
* under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation; either version 2.1 of the License, or (at
* your option) any later version. This library is distributed in the hope
* that it will be useful, but WITHOUT ANY WARRANTY; without even the implied
* warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with this library; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
*/

This is a JavaScript/WebAudio version of HighlyExperimental: It allows to play Playstation 1&2 music.(which can be found on pages
like these: http://www.vgmusic.com/music/console/sony/ps1/, http://www.zophar.net/music/psf.html, etc)

A live demo of this program can be found here: http://www.wothke.ch/webpsx/

The project is based on version HighlyExperimental 2.09. It includes most of the original code including all the necessary 3rd party dependencies. All the "Web" specific additions (i.e. the whole point of this project) are contained in the "emscripten" subfolder. The main interface between the JavaScript/WebAudio world and the original C code is the adapter.c file. Some patches were necessary within the original codebase (these can be located by looking for EMSCRIPTEN if-defs).



Howto build:

You'll need Emscripten (I used the win installer on WinXP: emsdk-1.13.0-full-32bit.exe which could be found here: 
http://kripken.github.io/emscripten-site/docs/getting_started/downloads.html) I did not need to perform 
ANY additions or manual changes on the installation. The below instructions assume that the web project 
folder has been moved into the main emscripten folder (maybe not necessary) and that a command prompt has been 
opened within the webpsx/emscripten sub-folder, and that the Emscripten environment vars have been set (run emsdk_env.bat).

Running the makeEmscripten.bat will generate a JavaScript 'HighlyExperimental' library (web_psx.js) including the above mentioned 
interface APIs. This lib is directly written into the web-page example in the "htdocs" sub-folder. (This generated lib is 
used from some manually written JavaScript/WebAudio code, see htdocs/sample_player.js). Study the example in "htdocs" 
for how the player is used. This player requires an original PlayStation2 BIOS image - which is NOT bundled here due to copyright concerns (When installing the player on your own computer you can easily configure the index.html page to automatically use your own 4mb PS2 BIOS image - or a .gz version thereof.).

This project comes without any music files, so you'll also have to get your own and place them
in the htdocs/music folder (you can configure them in the 'songs' list in index.html).


