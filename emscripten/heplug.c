/*
	One of the first kode54 players that I had ported to WebAudio.. whereas the later ports
	are based on kode54's foobar2000 plugins, this one is based on some DeaDBeeF version..
	to which I later added hebios (compressed) from foo_qpsf (https://gitlab.kode54.net/kode54/foo_qpsf).

	For ease of maintenance it would be better to ditch this impl and replace it with the 
	"standard" foobar2000 kind of version.
	
	This lib can be compiled with or without a built-in hebios. (see BUILTIN_HEBIOS define). 
	
	PSF and PSF2 player based on Neill Corlett's Highly Experimental.
	Copyright (C) 2003-2012 Chris Moeller <kode54@gmail.com>
	Copyright (C) 2003-2012 Neill Corlett <neill@neillcorlett.com>
	
    DeaDBeeF - ultimate music player for GNU/Linux systems with X11
    Copyright (C) 2009-2012 Alexey Yakovenko <waker@users.sourceforge.net>

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.
    
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include <stdlib.h>
#include <string.h>

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "zlib.h"

#include "zhebios.h"

#include <psx.h>
#include <iop.h>
#include <r3000.h>
#include <bios.h>

#include <psflib.h>
#include <psf2fs.h>

#include <mkhebios.h>

# define strdup(s)							      \
  (__extension__							      \
    ({									      \
      const char *__old = (s);						      \
      size_t __len = strlen (__old) + 1;				      \
      char *__new = (char *) malloc (__len);			      \
      (char *) memcpy (__new, __old, __len);				      \
    }))


#define trace(...) { fprintf(stderr, __VA_ARGS__); }
//#define trace(fmt,...)

// implemented on JavaScript side (also see callback.js) for "on-demand" file load:
extern int psx_request_file(const char *filename);

size_t em_fgetlength( FILE * f) {
	int fd= fileno(f);
	struct stat buf;
	fstat(fd, &buf);
	return buf.st_size;	
}	
	
#define min(x,y) ((x)<(y)?(x):(y))
#define max(x,y) ((x)>(y)?(x):(y))

#define BORK_TIME 0xC0CAC01A

static unsigned get_le32( void const* p )
{
    return  (unsigned) ((unsigned char const*) p) [3] << 24 |
            (unsigned) ((unsigned char const*) p) [2] << 16 |
            (unsigned) ((unsigned char const*) p) [1] <<  8 |
            (unsigned) ((unsigned char const*) p) [0];
}

static unsigned long parse_time_crap(const char *input)
{
    if (!input) return BORK_TIME;
    int len = strlen(input);
    if (!len) return BORK_TIME;
    int value = 0;
    {
        int i;
        for (i = len - 1; i >= 0; i--)
        {
            if ((input[i] < '0' || input[i] > '9') && input[i] != ':' && input[i] != ',' && input[i] != '.')
            {
                return BORK_TIME;
            }
        }
    }

    char * foo = strdup( input );

    if ( !foo )
        return BORK_TIME;

    char * bar = foo;
    char * strs = bar + strlen( foo ) - 1;
    char * end;
    while (strs > bar && (*strs >= '0' && *strs <= '9'))
    {
        strs--;
    }
    if (*strs == '.' || *strs == ',')
    {
        // fraction of a second
        strs++;
        if (strlen(strs) > 3) strs[3] = 0;
        value = strtoul(strs, &end, 10);
        switch (strlen(strs))
        {
        case 1:
            value *= 100;
            break;
        case 2:
            value *= 10;
            break;
        }
        strs--;
        *strs = 0;
        strs--;
    }
    while (strs > bar && (*strs >= '0' && *strs <= '9'))
    {
        strs--;
    }
    // seconds
    if (*strs < '0' || *strs > '9') strs++;
    value += strtoul(strs, &end, 10) * 1000;
    if (strs > bar)
    {
        strs--;
        *strs = 0;
        strs--;
        while (strs > bar && (*strs >= '0' && *strs <= '9'))
        {
            strs--;
        }
        if (*strs < '0' || *strs > '9') strs++;
        value += strtoul(strs, &end, 10) * 60000;
        if (strs > bar)
        {
            strs--;
            *strs = 0;
            strs--;
            while (strs > bar && (*strs >= '0' && *strs <= '9'))
            {
                strs--;
            }
            value += strtoul(strs, &end, 10) * 3600000;
        }
    }
    free( foo );
    return value;
}

struct psf_tag
{
    char * name;
    char * value;
    struct psf_tag * next;
};

static struct psf_tag * add_tag( struct psf_tag * tags, const char * name, const char * value )
{
    struct psf_tag * tag = malloc( sizeof( struct psf_tag ) );
    if ( !tag ) return tags;

    tag->name = strdup( name );
    if ( !tag->name ) {
        free( tag );
        return tags;
    }
    tag->value = strdup( value );
    if ( !tag->value ) {
        free( tag->name );
        free( tag );
        return tags;
    }
    tag->next = tags;
    return tag;
}

static void free_tags( struct psf_tag * tags )
{
    struct psf_tag * tag, * next;

    tag = tags;

    while ( tag )
    {
        next = tag->next;
        free( tag->name );
        free( tag->value );
        free( tag );
        tag = next;
    }
}

struct psf_load_state
{
    void * emu;

    int first;

    int tag_song_ms;
    int tag_fade_ms;
    int refresh;

    int utf8;

    struct psf_tag *tags;
};

#define MAX_LIB_NAME 128
char requiredLib[MAX_LIB_NAME];
static int psf_lib_meta(void * context, const char * name, const char * value)
{
	if ( !strcasecmp( name, "_lib" ) )
    {
		snprintf(requiredLib, MAX_LIB_NAME, "%s", value);
    }
	return 0;
}
	
extern int psf_info_meta2(void * context, const char * name, const char * value);// define in adaper.cpp

static int psf_info_meta(void * context, const char * name, const char * value)
{
//fprintf(stderr, "dump k: %s v: %s\n", name, value);	
    struct psf_load_state * state = ( struct psf_load_state * ) context;

    if ( !strcasecmp( name, "length" ) )
    {
        unsigned long n = parse_time_crap( value );
        if ( n != BORK_TIME ) state->tag_song_ms = n;
    }
    else if ( !strcasecmp( name, "fade" ) )
    {
        unsigned long n = parse_time_crap( value );
        if ( n != BORK_TIME ) state->tag_fade_ms = n;
    }
    else if ( !strcasecmp( name, "_refresh" ) )
    {
        char * end;
        state->refresh = strtoul( value, &end, 10 );
    }
	psf_info_meta2(context, name, value);
	return 0;
}

static int psf_info_dump(void * context, const char * name, const char * value)
{
    struct psf_load_state * state = ( struct psf_load_state * ) context;

//fprintf(stderr, "dump k: %s v: %s\n", name, value);	
	
    if ( !strcasecmp( name, "length" ) )
    {
        unsigned long n = parse_time_crap( value );
        if ( n != BORK_TIME ) state->tag_song_ms = n;
    }
    else if ( !strcasecmp( name, "fade" ) )
    {
        unsigned long n = parse_time_crap( value );
        if ( n != BORK_TIME ) state->tag_fade_ms = n;
    }
    else if ( !strcasecmp( name, "_refresh" ) )
    {
        char * end;
        state->refresh = strtoul( value, &end, 10 );
    }
    else if ( !strcasecmp( name, "utf8" ) )
    {
        state->utf8 = 1;
    }
    else if ( *name != '_' )
    {
        if ( !strcasecmp( name, "game" ) ) name = "album";
        else if ( !strcasecmp( name, "year" ) ) name = "date";
        else if ( !strcasecmp( name, "tracknumber" ) ) name = "track";
        else if ( !strcasecmp( name, "discnumber" ) ) name = "disc";

        state->tags = add_tag( state->tags, name, value );
    }

    return 0;
}

typedef struct {
    uint32_t pc0;
    uint32_t gp0;
    uint32_t t_addr;
    uint32_t t_size;
    uint32_t d_addr;
    uint32_t d_size;
    uint32_t b_addr;
    uint32_t b_size;
    uint32_t s_ptr;
    uint32_t s_size;
    uint32_t sp,fp,gp,ret,base;
} exec_header_t;

typedef struct {
    char key[8];
    uint32_t text;
    uint32_t data;
    exec_header_t exec;
    char title[60];
} psxexe_hdr_t;

int psf1_load(void * context, const uint8_t * exe, size_t exe_size,
                                  const uint8_t * reserved, size_t reserved_size)
{
    struct psf_load_state * state = ( struct psf_load_state * ) context;

    psxexe_hdr_t *psx = (psxexe_hdr_t *) exe;

    if ( exe_size < 0x800 ) return -1;

    uint32_t addr = get_le32( &psx->exec.t_addr );
    uint32_t size = exe_size - 0x800;

    addr &= 0x1fffff;
    if ( ( addr < 0x10000 ) || ( size > 0x1f0000 ) || ( addr + size > 0x200000 ) ) return -1;

    void * pIOP = psx_get_iop_state( state->emu );
    iop_upload_to_ram( pIOP, addr, exe + 0x800, size );

    if ( !state->refresh )
    {
        if (!strncasecmp((const char *) exe + 113, "Japan", 5)) state->refresh = 60;
        else if (!strncasecmp((const char *) exe + 113, "Europe", 6)) state->refresh = 50;
        else if (!strncasecmp((const char *) exe + 113, "North America", 13)) state->refresh = 60;
    }

    if ( state->first )
    {
        void * pR3000 = iop_get_r3000_state( pIOP );
        r3000_setreg(pR3000, R3000_REG_PC, get_le32( &psx->exec.pc0 ) );
        r3000_setreg(pR3000, R3000_REG_GEN+29, get_le32( &psx->exec.s_ptr ) );
        state->first = 0;
    }

    return 0;
}

static void * psf_file_fopen( const char * uri )
{
	return (void*)fopen(uri, "r");
}

static size_t psf_file_fread( void * buffer, size_t size, size_t count, void * handle )
{
	return fread(buffer, size, count, (FILE*)handle );
}

static int psf_file_fseek( void * handle, int64_t offset, int whence )
{
	return fseek( (FILE*) handle, offset, whence );
}

static int psf_file_fclose( void * handle )
{
	fclose( (FILE *) handle  );
    return 0;
}

static long psf_file_ftell( void * handle )
{
	return  ftell( (FILE*) handle );
}

const psf_file_callbacks psf_file_system =
{
    "\\/|:",
    psf_file_fopen,
    psf_file_fread,
    psf_file_fseek,
    psf_file_fclose,
    psf_file_ftell
};

static int EMU_CALL virtual_readfile(void *context, const char *path, int offset, char *buffer, int length)
{
    return psf2fs_virtual_readfile(context, path, offset, buffer, length);
}

#define PATH_MAX 255

typedef struct {
	int channels;
	int bps;
	int samplerate;
	int channelmask;
} Format;

typedef struct {
	Format fmt;
	int readpos;
} DB_fileinfo_t;

typedef struct {
    DB_fileinfo_t info;
    const char *path;
    void *emu;
    void *psf2fs;
    int samples_played;
    int samples_to_play;
    int samples_to_fade;
} he_info_t;


DB_fileinfo_t *he_open (uint32_t hints) {
    DB_fileinfo_t *_info = (DB_fileinfo_t *)malloc (sizeof (he_info_t));
    memset (_info, 0, sizeof (he_info_t));
    return _info;
}

int he_get_sample_rate (DB_fileinfo_t *_info) {
    he_info_t *info = (he_info_t *)_info;
	return info->info.fmt.samplerate;
}

int he_get_samples_to_play (DB_fileinfo_t *_info) {
    he_info_t *info = (he_info_t *)_info;
	return info->samples_to_play;
}

int he_get_samples_played (DB_fileinfo_t *_info) {
    he_info_t *info = (he_info_t *)_info;
	return info->samples_played;
}
	
int isCompressed(const char *filename) {
	char* point;
	if((point = strrchr(filename,'.')) != NULL ) {
		return (strcmp(point,".gz") == 0);
	}
	return 0;
}

int inflate2(const void *src, int srcLen, void *dst, int dstLen) {
    z_stream strm  = {0};
    strm.total_in  = strm.avail_in  = srcLen;
    strm.total_out = strm.avail_out = dstLen;
    strm.next_in   = (Bytef *) src;
    strm.next_out  = (Bytef *) dst;

    strm.zalloc = Z_NULL;
    strm.zfree  = Z_NULL;
    strm.opaque = Z_NULL;

    int err = -1;
    int ret = -1;

    err = inflateInit2(&strm, (15 + 32)); //15 window bits, and the +32 tells zlib to to detect if using gzip or zlib
    if (err == Z_OK) {
        err = inflate(&strm, Z_FINISH);
        if (err == Z_STREAM_END) {
            ret = strm.total_out;
        }
        else {
             inflateEnd(&strm);
             return err;
        }
    }
    else {
        inflateEnd(&strm);
        return err;
    }

    inflateEnd(&strm);
    return ret;
}

void * tmp_bios_buffer = 0;


/* tmp util to compress hebios
void dumpComressed(uint8 * data, long size) {
	for (int i= 0; i<size; i++) {
		fprintf(stdout, "%d,", data[i]);
	}
}
void dumpComressedHeBios() {

	uint8 * dataDest= malloc(HEBIOS_SIZE);	// who cares about the leak here..
	long newSize= HEBIOS_SIZE;
	if (Z_OK != compress(dataDest, &newSize, hebios, HEBIOS_SIZE)) {
		fprintf(stderr, "ERROR!");	
	}
	
	fprintf(stdout, "#ifndef __HEBIOS_H\n");
	fprintf(stdout, "#define __HEBIOS_H\n");
	fprintf(stdout, "#define HEBIOS_SIZE 524288\n");
	
	fprintf(stdout, "static uint8_t zhebios[] = {\n");
	dumpComressed(dataDest, newSize);
	fprintf(stdout, "\n};\n");
	fprintf(stdout, "static uint32_t zhebios_size =%d;\n", newSize);
	
	fprintf(stdout, "#endif\n");
}
*/

// slightly refactored original heplug.c logic here...
int he_install_bios(const char *he_bios_path) {
	
//	dumpComressedHeBios();		// tmp utility

	if ( !bios_get_imagesize() ) {
//		if ((he_bios_path == 0) || !strlen(he_bios_path)) {
#ifdef BUILTIN_HEBIOS	
			// use built-in hebios
			void * he_bios = malloc( HEBIOS_SIZE );
			long hebios_size= HEBIOS_SIZE;
			if (Z_OK !=  uncompress(he_bios, &hebios_size, zhebios, zhebios_size)) {
				trace( "he: BIOS uncompress error\n" );
				return -1;
			}
			if (hebios_size != HEBIOS_SIZE) {
				trace( "he: BIOS uncompress mismatch\n" );
				return -1;
			}
			bios_set_image( he_bios, hebios_size );
			trace( "he: using built-in hebios\n" );
#else
//		} else {
			// install specific bios 
			if ( !*he_bios_path ) {
				trace( "he: no BIOS set\n" );
				return -1;
			}
			
			FILE * f = psf_file_fopen( he_bios_path );
			if ( !f ) {
				trace( "he: failed to open bios %s\n", he_bios_path );
				return -1;
			}

			void * ps2_bios = malloc( 0x400000 );
			if ( !ps2_bios ) {
				psf_file_fclose( f );
				trace( "he: out of memory\n" );
				return -1;
			}
			
			int compressionEnabled= 1;
			
			size_t ps2_bios_size= 0;
			if (compressionEnabled && isCompressed(he_bios_path)) {
				size_t compressed_size = em_fgetlength( f );

				if (tmp_bios_buffer) { free(tmp_bios_buffer);}
				tmp_bios_buffer= malloc( compressed_size );

				if ( psf_file_fread( tmp_bios_buffer, 1, compressed_size, f ) < compressed_size ) {
					free( tmp_bios_buffer );
					psf_file_fclose( f );
					trace( "he: error reading compressed bios\n" );
					return -1;
				}
				
				ps2_bios_size= inflate2(tmp_bios_buffer, compressed_size, ps2_bios, 0x400000);
				if (ps2_bios_size < 0x400000) {
					free( ps2_bios );
					psf_file_fclose( f );
					trace( "he: could not uncompress bios\n" );
					return -1;
				}		
			} else {
				size_t ps2_bios_size = em_fgetlength( f );
				if ( ps2_bios_size != 0x400000 ) {
					psf_file_fclose( f );
					trace( "he: bios is wrong size\n" );
					return -1;
				}

				if ( psf_file_fread( ps2_bios, 1, 0x400000, f ) < 0x400000 ) {
					free( ps2_bios );
					psf_file_fclose( f );
					trace( "he: error reading bios\n" );
					return -1;
				}

				psf_file_fclose( f );
			}
			int bios_size = 0x400000;
			void * he_bios = mkhebios_create( ps2_bios, &bios_size );

			trace( "he: fucko - %p, %p, %u\n", ps2_bios, he_bios, bios_size );

			free( ps2_bios );

			if ( !he_bios )
			{
				trace( "he: error processing bios\n" );
				return -1;
			}

			bios_set_image( he_bios, bios_size );
//		}
#endif
	}
	return 0;
}

void* set_emu(he_info_t *info, void *state) {
	if (info->emu) { 
		free(info->emu);
		info->emu= 0; 
	}
    info->emu = state;
	return state;
}	

int he_init (DB_fileinfo_t *_info, const char * uri) {
	
    he_info_t *info = (he_info_t *)_info;
	info->path = strdup( uri );
	
 //   int psf_version = psf_load( uri, &psf_file_system, 0, 0, 0, 0, 0, 0 );
	requiredLib[0]= 0;
    int psf_version = psf_load( uri, &psf_file_system, 0, 0, 0, psf_lib_meta, 0, 0 );
    if (psf_version < 0) {
        trace ("he: failed to open %s\n", uri);
        return -1;
    }
	
	if (strlen(requiredLib)) {
		// lib must be loaded here or else the psf loading will fail..
		// (enter "retry-mode" if something is missing)
		// make sure the file will be available in the FS when the song asks for it later..		
		char tmpFileName[PATH_MAX];   
		char *p= strrchr(uri, '/');		
		int pathlen= p?(p-uri+1):0;
		memcpy(tmpFileName, uri, pathlen);
		snprintf(tmpFileName+pathlen, PATH_MAX-pathlen, "%s", requiredLib);

		int r= psx_request_file(tmpFileName);	// trigger load & check if ready
		if (r <0) {
			return -1; // file not ready
		}
	}
	
    char he_bios_path[PATH_MAX];
	memset(he_bios_path, 0, PATH_MAX);	// set specific BIOS here if needed

	he_install_bios(he_bios_path);		
    psx_init();

    struct psf_load_state state;
    memset( &state, 0, sizeof(state) );

    state.first = 1;
	
	state.emu= set_emu(info, malloc( psx_get_state_size( psf_version ) ));
    if ( !state.emu ) {
        trace( "he: out of memory\n" );
        return -1;
    }

    psx_clear_state( state.emu, psf_version );

    if ( psf_version == 1 ) {
        if ( psf_load( uri, &psf_file_system, 1, psf1_load, &state, psf_info_meta, &state, 0 ) <= 0 ) {
            trace( "he: invalid PSF file\n" );
            return -1;
        }
    } else if ( psf_version == 2 ) {
        info->psf2fs = psf2fs_create();
        if ( !info->psf2fs ) {
            trace( "he: out of memory\n" );
            return -1;
        }
        if ( psf_load( uri, &psf_file_system, 2, psf2fs_load_callback, info->psf2fs, psf_info_meta, &state, 0 ) <= 0 ) {
            trace( "he: invalid PSF file\n" );
            return -1;
        }
        psx_set_readfile( info->emu, virtual_readfile, info->psf2fs );
    }

    if ( state.refresh )
        psx_set_refresh( info->emu, state.refresh );

    int tag_song_ms = state.tag_song_ms;
    int tag_fade_ms = state.tag_fade_ms;

    if (!tag_song_ms)
    {
        tag_song_ms = ( 2 * 60 + 50 ) * 1000;
        tag_fade_ms =            10   * 1000;
    }

    const int srate = psf_version == 2 ? 48000 : 44100;

    info->samples_played = 0;
    info->samples_to_play = (uint64_t)tag_song_ms * (uint64_t)srate / 1000;
    info->samples_to_fade = (uint64_t)tag_fade_ms * (uint64_t)srate / 1000;

    _info->fmt.channels = 2;
    _info->fmt.bps = 16;
    _info->fmt.samplerate = srate;
	_info->readpos = 0;

	free_tags(state.tags);
	
    return 0;
}

void he_free (DB_fileinfo_t *_info) {
    he_info_t *info = (he_info_t *)_info;
    if (info) {
        if (info->psf2fs) {
            psf2fs_delete( info->psf2fs );
            info->psf2fs = NULL;
        }
        if (info->emu) {
            free (info->emu);
            info->emu = NULL;
        }
        if (info->path) {
            free (info->path);
            info->path = NULL;
        }
        free (info);
    }
}

int he_read (DB_fileinfo_t *_info, char *bytes, int size) {
	
    he_info_t *info = (he_info_t *)_info;

	if (info->samples_played >= info->samples_to_play ) {
		return -1;		// end song	
	}	

    short * samples = (short *) bytes;
    uint32_t sample_count = size / ( 2 * sizeof(short) );

    if ( info->samples_played >= info->samples_to_play + info->samples_to_fade ) {
        return -1;
    }

    if ( psx_execute( info->emu, 0x7fffffff, samples, &sample_count, 0 ) < 0 ) {
        trace ( "he: execution error\n" );
        return -1;
    }

    int samples_start = info->samples_played;
    int samples_end   = info->samples_played += sample_count;

    if ( samples && ( samples_end > info->samples_to_play ) )
    {
        int fade_start = info->samples_to_play;
        if ( fade_start < samples_start ) fade_start = samples_start;
        int samples_length = info->samples_to_play + info->samples_to_fade;
        int fade_end = samples_length;
        if ( fade_end > samples_end ) fade_end = samples_end;

        for ( int i = fade_start; i < fade_end; i++ )
        {
            samples[ ( i - samples_start ) * 2 + 0 ] = (int64_t)samples[ ( i - samples_start ) * 2 + 0 ] * ( samples_length - i ) / info->samples_to_fade;
            samples[ ( i - samples_start ) * 2 + 1 ] = (int64_t)samples[ ( i - samples_start ) * 2 + 1 ] * ( samples_length - i ) / info->samples_to_fade;
        }

        if ( samples_end > samples_length ) samples_end = samples_length;
    }

    return ( samples_end - samples_start ) * 2 * sizeof(short);
}

int he_seek_sample (DB_fileinfo_t *_info, int sample) {
    he_info_t *info = (he_info_t *)_info;
    unsigned long int s = sample;
    if (s < info->samples_played) {
        struct psf_load_state state;
        memset( &state, 0, sizeof(state) );

        state.emu = info->emu;

        if ( !info->psf2fs ) {
            psx_clear_state( info->emu, 1 );
            if ( psf_load( info->path, &psf_file_system, 1, psf1_load, &state, psf_info_meta, &state, 0 ) <= 0 ) {
                trace( "he: invalid PSF file\n" );
                return -1;
            }
        } else {
            psx_clear_state( info->emu, 2 );
            if ( psf_load( info->path, &psf_file_system, 2, 0, 0, psf_info_meta, &state, 0 ) <= 0 ) {
                trace( "he: invalid PSF file\n" );
                return -1;
            }
            psx_set_readfile( info->emu, virtual_readfile, info->psf2fs );
        }

        if ( state.refresh )
            psx_set_refresh( info->emu, state.refresh );

        info->samples_played = 0;
		
		free_tags(state.tags);
    }
    while ( info->samples_played < s ) {
        int to_skip = s - info->samples_played;
        if ( to_skip > 32768 ) to_skip = 1024;
        if ( he_read( _info, NULL, to_skip * 2 * sizeof(short) ) < 0 ) {
            return -1;
        }
    }
    _info->readpos = s/(float)_info->fmt.samplerate;
    return 0;
}
/*
int he_seek (DB_fileinfo_t *_info, float time) {
    return he_seek_sample (_info, time * _info->fmt.samplerate);
}
*/

