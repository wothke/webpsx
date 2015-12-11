/*
 psx_adapter.js: Adapts HighlyExperimental backend to generic WebAudio/ScriptProcessor player.
 
 version 1.0
 
 	Copyright (C) 2015 Juergen Wothke

 LICENSE
 
 This library is free software; you can redistribute it and/or modify it
 under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2.1 of the License, or (at
 your option) any later version. This library is distributed in the hope
 that it will be useful, but WITHOUT ANY WARRANTY; without even the implied
 warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
*/

PSXBackendAdapter = (function(){ var $this = function (presetBIOS) {
		$this.base.call(this, backend_PSX.Module, 2);
		this._manualSetupComplete= presetBIOS?false:true;
		this._undefined;
		this._currentPath;
		this._currentFile;
	}; 
	// HighlyExperimental's sample buffer contains 2-byte integer sample data (i.e. 
	// must be rescaled) of 2 interleaved channels
	extend(EmsHEAP16BackendAdapter, $this, {
		uploadFile: function(filename, options) {
			if (options.setBIOS === 'undefined') {
				console.log("unsupported uploadFile('"+filename+")' call" );
				return -1;
			} else {
				// if not explicitly set here.. then 'emu_init' will go for some default later..
				if (this.Module.ccall('emu_setup', 'number', ['string'], [filename]) ==0) {
					this._manualSetupComplete= true;
					if (!(this._observer === 'undefined')) this._observer.handleBackendEvent();
					return 0;
				} else {
					console.log("installBIOS failed");
					return -1;
				}				
			}
		},

		getAudioBuffer: function() {
			var ptr=  this.Module.ccall('emu_get_audio_buffer', 'number');			
			// make it a this.Module.HEAP16 pointer
			return ptr >> 1;	// 2 x 16 bit samples			
		},
		getAudioBufferLength: function() {
			var len= this.Module.ccall('emu_get_audio_buffer_length', 'number');
			return len;
		},
		computeAudioSamples: function() {
			return this.Module.ccall('emu_compute_audio_samples', 'number');
		},
		getMaxPlaybackPosition: function() { 
			return this.Module.ccall('emu_get_max_position', 'number');
		},
		getPlaybackPosition: function() {
			return this.Module.ccall('emu_get_current_position', 'number');
		},
		seekPlaybackPosition: function(pos) {
			var current= this.getPlaybackPosition();
			if (pos < current) {
				// hack: for some reason backward seeking fails ('he: execution error') if "built-in"
				// file reload if used... 
				var ret = this.Module.ccall('emu_init', 'number', 
							['string', 'string'], 
							[ this._currentPath, this._currentFile]);
			}
			this.Module.ccall('emu_seek_position', 'number', ['number'], [pos]);
		},
		getPathAndFilename: function(filename) {		
			var sp = filename.split('/');	// avoid folders
			var fn = sp[sp.length-1];					
			var path= '/'; // make it flat... filename.substring(0, filename.lastIndexOf("/"));	
			if (path.lenght) path= path+"/";
			return [path, fn];
		},
		registerFileData: function(pathFilenameArray, data) {
			return this.registerEmscriptenFileData(pathFilenameArray, data);
		},
		loadMusicData: function(sampleRate, path, filename, data, options) {
			var ret = this.Module.ccall('emu_init', 'number', 
								['string', 'string'], 
								[ path, filename]);

			if (ret == 0) {
				// PS2 always uses 48000hz / PS1 44100hz				
				var inputSampleRate = this.Module.ccall('emu_get_sample_rate', 'number');
				this.resetSampleRate(sampleRate, inputSampleRate); 
				this._currentPath= path;
				this._currentFile= filename;
			} else {
				this._currentPath= this._undefined;
				this._currentFile= this._undefined;
			}
			return ret;
		},
		evalTrackOptions: function(options) {
// is there any scenario with subsongs?
			if (typeof options.timeout != 'undefined') {
				ScriptNodePlayer.getInstance().setPlaybackTimeout(options.timeout*1000);
			}
			var id= (options && options.track) ? options.track : 0;		
			var boostVolume= (options && options.boostVolume) ? options.boostVolume : 0;		
			return this.Module.ccall('emu_set_subsong', 'number', ['number', 'number'], [id, boostVolume]);
		},				
		teardown: function() {
			this.Module.ccall('emu_teardown', 'number');	// just in case
		},
		getSongInfoMeta: function() {
			return {title: String,
					artist: String, 
					game: String, 
					year: String, 
					genre: String,
					copyright: String,
					psfby: String
					};
		},
		
		updateSongInfo: function(filename, result) {
			var numAttr= 7;
			var ret = this.Module.ccall('emu_get_track_info', 'number');

			var array = this.Module.HEAP32.subarray(ret>>2, (ret>>2)+numAttr);
			result.title= this.Module.Pointer_stringify(array[0]);
			if (!result.title.length) result.title= filename;		
			result.artist= this.Module.Pointer_stringify(array[1]);		
			result.game= this.Module.Pointer_stringify(array[2]);
			result.year= this.Module.Pointer_stringify(array[3]);
			result.genre= this.Module.Pointer_stringify(array[4]);
			result.copyright= this.Module.Pointer_stringify(array[5]);
			result.psfby= this.Module.Pointer_stringify(array[6]);
		}
	});	return $this; })();