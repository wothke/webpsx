/*
* This file adapts "HighlyExperimental" to the interface expected by my generic JavaScript player..
*
* Copyright (C) 2015 Juergen Wothke
*
* LICENSE
* 
* This library is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2.1 of the License, or (at
* your option) any later version. This library is distributed in the hope
* that it will be useful, but WITHOUT ANY WARRANTY; without even the implied
* warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
* 
* You should have received a copy of the GNU General Public
* License along with this library; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
*/

#include <emscripten.h>
#include <stdio.h>
#include <stdlib.h>     /* malloc, free, rand */

#include <exception>
#include <iostream>
#include <fstream>

#include "psx.h"


// use heplug.c impl
extern "C" {
	struct DB_fileinfo_t;
	int he_init (struct DB_fileinfo_t *_info, const char * uri);
	void he_load (void);
	struct DB_fileinfo_t *he_open (uint32_t hints); 
	int he_read (struct DB_fileinfo_t *_info, char *bytes, int size);
	int he_install_bios(const char *he_bios_path);
}

DB_fileinfo_t* song_info= 0;

#ifdef EMSCRIPTEN
#define EMSCRIPTEN_KEEPALIVE __attribute__((used))
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

#define BUF_SIZE	1024
#define TEXT_MAX	255
#define NUM_MAX	15

// see Sound::Sample::CHANNELS
#define CHANNELS 2				
#define BYTES_PER_SAMPLE 2
#define SAMPLE_BUF_SIZE	8192
sint16 sample_buffer[SAMPLE_BUF_SIZE * CHANNELS];
int samples_available= 0;

const char* info_texts[7];

char title_str[TEXT_MAX];
char author_str[TEXT_MAX];
char desc_str[TEXT_MAX];
char program_str[TEXT_MAX];
char subpath_str[TEXT_MAX];
char tracks_str[NUM_MAX];
char current_track_str[NUM_MAX];

struct StaticBlock {
    StaticBlock(){
		info_texts[0]= title_str;
		info_texts[1]= author_str;
		info_texts[2]= desc_str;
		info_texts[3]= program_str;
		info_texts[4]= subpath_str;
		info_texts[5]= tracks_str;
		info_texts[6]= current_track_str;
    }
};

static StaticBlock g_emscripen_info;

void *psx_state= 0;
char * song_buffer= 0;
uint8 * bios_buffer= 0;

extern "C" void emu_teardown (void)  __attribute__((noinline));
extern "C" void EMSCRIPTEN_KEEPALIVE emu_teardown (void) {
}

extern "C" int emu_setup(char *bios_path) __attribute__((noinline));
extern "C" EMSCRIPTEN_KEEPALIVE int emu_setup(char *bios_path)
{
	he_load();	// basic init
	
	return he_install_bios(bios_path);	// the first installed BIOS stays..
}

extern "C" int emu_init(int sample_rate, char *basedir, char *songmodule) __attribute__((noinline));
extern "C" EMSCRIPTEN_KEEPALIVE int emu_init(int sample_rate, char *basedir, char *songmodule)
{
	he_load();	// basic init

	emu_teardown();

	if (!song_info) song_info= he_open(0);
	
	if (he_init (song_info, songmodule) == 0) {
	} else {
		return -1;
	}
	return 0;
}

extern "C" void emu_set_subsong(int subsong) __attribute__((noinline));
extern "C" void EMSCRIPTEN_KEEPALIVE emu_set_subsong(int subsong) {
}

extern "C" void set_name(const char *name) {
	snprintf(title_str, TEXT_MAX, "%s", name);
}
extern "C" void set_album(const char *name) {
	snprintf(author_str, TEXT_MAX, "%s", name);
}
extern "C" void set_date(const char *name) {
	snprintf(desc_str, TEXT_MAX, "%s", name);
}

extern "C" const char** emu_get_track_info() __attribute__((noinline));
extern "C" const char** EMSCRIPTEN_KEEPALIVE emu_get_track_info() {
	return info_texts;
}

extern "C" char* EMSCRIPTEN_KEEPALIVE emu_get_audio_buffer(void) __attribute__((noinline));
extern "C" char* EMSCRIPTEN_KEEPALIVE emu_get_audio_buffer(void) {
	return (char*)sample_buffer;
}

extern "C" long EMSCRIPTEN_KEEPALIVE emu_get_audio_buffer_length(void) __attribute__((noinline));
extern "C" long EMSCRIPTEN_KEEPALIVE emu_get_audio_buffer_length(void) {
	return samples_available;
}

extern "C" int emu_compute_audio_samples() __attribute__((noinline));
extern "C" int EMSCRIPTEN_KEEPALIVE emu_compute_audio_samples() {
	uint32 size = SAMPLE_BUF_SIZE;

	int ret=  he_read (song_info, (char*)sample_buffer, size);	// returns number of bytes

	if (ret < 0) {
		samples_available= 0;
		return -1;
	} else {
		samples_available= ret >>2; // 2*2bytes sample
		if (size) {
			return 0;
		} else {
			return 1;
		}		
	}
}

