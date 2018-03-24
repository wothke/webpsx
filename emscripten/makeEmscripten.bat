rem  POOR MAN'S DOS PROMPT BUILD SCRIPT.. make sure to delete the respective *.bc files before building 
rem  existing *.bc files will not be recompiled. Unfortunately the script occasionally 
rem  fails for no good reason - this must be the wonderful world of DOS/Win... ;-)

rem  examples of windows bullshit bingo (i.e. the same compile works when started again..): 
rem  1) error msg: "'_zxtune.html' is not recognized as an internal or external command, 
rem  operable program or batch file." -> for some reason the last command line was arbitrarrily cut in two..
rem  2) The system cannot find the path specified.
rem  3) WindowsError: [Error 5] Access is denied
rem  4) cpp: error: CreateProcess: No such file or directory

rem Notice: for some reason Emscripten complains about "incompatible function pointer types" where 'tolower' is invoked
rem with a char... bloody bullshit :(

setlocal enabledelayedexpansion

SET ERRORLEVEL
VERIFY > NUL

:: **** use the "-s WASM" switch to compile WebAssembly output. warning: the SINGLE_FILE approach does NOT currently work in Chrome 63.. ****
set "OPT=   -s WASM=0 -s ASSERTIONS=2 -DBUILTIN_HEBIOS -s FORCE_FILESYSTEM=1 -Wcast-align -fno-strict-aliasing -s VERBOSE=0 -s SAFE_HEAP=0 -s DISABLE_EXCEPTION_CATCHING=0  -DEMU_COMPILE -DEMU_LITTLE_ENDIAN -DHAVE_STDINT_H -DNO_DEBUG_LOGS -Wno-pointer-sign -I. -I.. -I../Core -I../psflib  -I../zlib  -Os -O3 "

if not exist "built/he.bc" (
	call emcc.bat -DHAVE_STDINT_H %OPT% ../Core/psx.c ../Core/ioptimer.c ../Core/iop.c ../Core/bios.c ../Core/r3000dis.c ../Core/r3000asm.c ../Core/r3000.c ../Core/vfs.c ../Core/spucore.c ../Core/spu.c ../Core/mkhebios.c ../psflib/psf2fs.c  ../psflib/psflib.c -o built/he.bc
	IF !ERRORLEVEL! NEQ 0 goto :END
)

if not exist "built/zlib.bc" (
	call emcc.bat %OPT% ../zlib/adler32.c ../zlib/compress.c ../zlib/crc32.c ../zlib/gzio.c ../zlib/uncompr.c ../zlib/deflate.c ../zlib/trees.c ../zlib/zutil.c ../zlib/inflate.c ../zlib/infback.c ../zlib/inftrees.c ../zlib/inffast.c -o built/zlib.bc
	IF !ERRORLEVEL! NEQ 0 goto :END
)
call emcc.bat %OPT% --closure 1 --llvm-lto 1  -s TOTAL_MEMORY=134217728 --memory-init-file 0  built/he.bc built/zlib.bc heplug.c  adapter.cpp --js-library callback.js  -s EXPORTED_FUNCTIONS="['_emu_setup', '_emu_init','_emu_teardown','_emu_get_current_position','_emu_seek_position','_emu_get_max_position','_emu_set_subsong','_emu_get_track_info','_emu_get_sample_rate','_emu_get_audio_buffer','_emu_get_audio_buffer_length','_emu_compute_audio_samples', '_malloc', '_free']"  -o htdocs/psx.js  -s SINGLE_FILE=0 -s EXTRA_EXPORTED_RUNTIME_METHODS="['ccall', 'Pointer_stringify']"  -s BINARYEN_ASYNC_COMPILATION=1 -s BINARYEN_TRAP_MODE='clamp' && copy /b shell-pre.js + htdocs\psx.js + shell-post.js htdocs\web_psx3.js && del htdocs\psx.js && copy /b htdocs\web_psx3.js + psx_adapter.js htdocs\backend_psx.js && del htdocs\web_psx3.js

:END