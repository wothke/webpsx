rem  POOR MAN'S DOS PROMPT BUILD SCRIPT.. make sure to delete the respective *.bc files before building 
rem  existing *.bc files will not be recompiled. Unfortunately the script occasionally 
rem  fails for no good reason - this must be the wonderful world of DOS/Win... ;-)

rem  examples of windows bullshit bingo (i.e. the same compile works when started again..): 
rem  1) error msg: "'_zxtune.html' is not recognized as an internal or external command, 
rem  operable program or batch file." -> for some reason the last command line was arbitrarrily cut in two..
rem  2) The system cannot find the path specified.
rem  3) WindowsError: [Error 5] Access is denied
rem  4) cpp: error: CreateProcess: No such file or directory


setlocal enabledelayedexpansion

SET ERRORLEVEL
VERIFY > NUL

set "OPT=  -Wcast-align -fno-strict-aliasing -s VERBOSE=0 -s SAFE_HEAP=0 -s DISABLE_EXCEPTION_CATCHING=0  -DEMU_COMPILE -DEMU_LITTLE_ENDIAN -DHAVE_STDINT_H -DNO_DEBUG_LOGS -Wno-pointer-sign -I. -I.. -I../Core -I../psflib  -I../zlib  -Os -O3"

if not exist "built/he.bc" (
	call emcc.bat -DHAVE_STDINT_H %OPT% ../Core/psx.c ../Core/ioptimer.c ../Core/iop.c ../Core/bios.c ../Core/r3000dis.c ../Core/r3000asm.c ../Core/r3000.c ../Core/vfs.c ../Core/spucore.c ../Core/spu.c ../Core/mkhebios.c ../psflib/psf2fs.c  ../psflib/psflib.c  -o built/he.bc
	IF !ERRORLEVEL! NEQ 0 goto :END
)

if not exist "built/zlib.bc" (
	call emcc.bat %OPT% ../zlib/adler32.c ../zlib/compress.c ../zlib/crc32.c ../zlib/gzio.c ../zlib/uncompr.c ../zlib/deflate.c ../zlib/trees.c ../zlib/zutil.c ../zlib/inflate.c ../zlib/infback.c ../zlib/inftrees.c ../zlib/inffast.c -o built/zlib.bc
	IF !ERRORLEVEL! NEQ 0 goto :END
)

if not exist "built/adapter.bc" (
	call emcc.bat   %OPT%   heplug.c adapter.cpp     -o built/adapter.bc
	IF !ERRORLEVEL! NEQ 0 goto :END
)
  
call emcc.bat %OPT% -s TOTAL_MEMORY=67108864 --closure 1  built/he.bc built/zlib.bc built/adapter.bc  -s EXPORTED_FUNCTIONS="['_alloc', '_emu_setup', '_emu_init','_emu_teardown','_emu_set_subsong','_emu_get_track_info','_emu_get_audio_buffer','_emu_get_audio_buffer_length','_emu_compute_audio_samples']"  -o htdocs/web_psx2.html && copy /b shell-pre.js + htdocs\web_psx2.js + shell-post.js htdocs\web_psx.js && del htdocs\web_psx2.html && del htdocs\web_psx2.js

:END