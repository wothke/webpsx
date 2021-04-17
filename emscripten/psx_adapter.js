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

PSXBackendAdapter = (function(){ var $this = function (presetBIOS, modlandMode) {
		$this.base.call(this, backend_PSX.Module, 2);
		this._chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
		
		this._manualSetupComplete= presetBIOS?false:true;
		this._undefined;
		this._currentPath;
		this._currentFile;

		// aka dumshit ftp.modland.com mode:
		this.modlandMode= (typeof modlandMode != 'undefined') ? modlandMode : false;
		this.originalFile= "";
		this.modlandMap= {};	// mapping of weird shit filenames used on modland 
		
		if (!backend_PSX.Module.notReady) {
			// in sync scenario the "onRuntimeInitialized" has already fired before execution gets here,
			// i.e. it has to be called explicitly here (in async scenario "onRuntimeInitialized" will trigger
			// the call directly)
			this.doOnAdapterReady();
		}				
	}; 
	// HighlyExperimental's sample buffer contains 2-byte integer sample data (i.e. 
	// must be rescaled) of 2 interleaved channels
	extend(EmsHEAP16BackendAdapter, $this, {
		uploadFile: function(filename, options) {
			if (options.setBIOS === 'undefined') {
				return -1;
			} else {
				// if not explicitly set here.. then 'emu_init' will go for some default later..
				
				if (this.Module.ccall('emu_setup', 'number', ['string'], [filename]) ==0) {
					this._manualSetupComplete= true;
					if (!(this._observer === 'undefined')) this._observer.handleBackendEvent();
					return 0;
				} else {
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
			var ret= this.Module.ccall('emu_compute_audio_samples', 'number');
			return ret;
		},
		getMaxPlaybackPosition: function() { 
			return this.Module.ccall('emu_get_max_position', 'number');
		},
		getPlaybackPosition: function() {
			return this.Module.ccall('emu_get_current_position', 'number');
		},
		seekPlaybackPosition: function(pos) {
			var v= ScriptNodePlayer.getInstance().getVolume();
			ScriptNodePlayer.getInstance().setVolume(0);	// suppress any output while reset is in progress

			var current= this.getPlaybackPosition();
			if (pos < current) {
				// hack: for some reason backward seeking fails ('he: execution error') if "built-in"
				// file reload if used... 
				var ret = this.Module.ccall('emu_init', 'number', 
							['string', 'string'], 
							[ this._currentPath, this._currentFile]);
			}
			this.Module.ccall('emu_seek_position', 'number', ['number'], [pos]);
			ScriptNodePlayer.getInstance().setVolume(v);
		},
		
		/*
		* Creates the URL used to retrieve the song file.
		*/
		mapUrl: function(filename) {			
			// used transform the "internal filename" to a valid URL
			var uri= this.mapFs2Uri(filename);
			uri= decodeURI(uri);	// replace escape sequences... 
			return uri;
		},
		mapInternalFilename: function(overridePath, basePath, filename) {
			//map URLSs to FS	
			filename= this.mapUri2Fs(filename);	// treat all songs as "from outside"

			var f= ((overridePath)?overridePath:basePath) + filename;	// this._basePath ever needed?			
			if (this.modlandMode) this.originalFile= f;			
			return f;
		},
		
		getPathAndFilename: function(filename) {
			var sp = filename.split('/');
			var fn = sp[sp.length-1];					
			var path= filename.substring(0, filename.lastIndexOf("/"));	
			if (path.lenght) path= path+"/";	// FIXME XXX typo! why misspelled path.length still work here?
			
			return [path, fn];
		},
		
		mapBackendFilename: function (name) {
			// "name" comes from the C++ side 
			var input= this.Module.Pointer_stringify(name);
			if (this.modlandMode) {
				var tmpPathFilenameArray = new Array(2);	// do not touch original IO param			
				var p= input.lastIndexOf("/");
				if (p > 0) {
					tmpPathFilenameArray[0]= input.substring(0, p);
					tmpPathFilenameArray[1]= input.substring(p+1);
					
					var output= tmpPathFilenameArray[1].toLowerCase();	// idiots!
					if (tmpPathFilenameArray[1] != output) {	// remember the filename mapping (path is the same)
						this.modlandMap[output.replace(/^.*[\\\/]/, '')]= tmpPathFilenameArray[1].replace(/^.*[\\\/]/, '');	// needed to create FS expected by emu
						tmpPathFilenameArray[1]= output;
					}				
				} else  {
					tmpPathFilenameArray[0]= "";
					tmpPathFilenameArray[1]= input;
				}							
								
				input= tmpPathFilenameArray[0]+"/"+ tmpPathFilenameArray[1];
			}
			return input;
		},
		registerFileData: function(pathFilenameArray, data) {
			// input: the path is fixed to the basePath & the filename is actually still a path+filename
			var path= pathFilenameArray[0];
			var filename= pathFilenameArray[1];

			// MANDATORTY to move any path info still present in the "filename" to "path"
			var tmpPathFilenameArray = new Array(2);	// do not touch original IO param			
			var p= filename.lastIndexOf("/");
			if (p > 0) {
				tmpPathFilenameArray[0]= path + filename.substring(0, p);
				tmpPathFilenameArray[1]= filename.substring(p+1);
			} else  {
				tmpPathFilenameArray[0]= path;
				tmpPathFilenameArray[1]= filename;
			}

			if (this.modlandMode) {
				if (typeof this.modlandMap[tmpPathFilenameArray[1]] != 'undefined') {
					tmpPathFilenameArray[1]= this.modlandMap[tmpPathFilenameArray[1]];	// reverse map
				}
			}
			// setup data in our virtual FS (the next access should then be OK)
			return this.registerEmscriptenFileData(tmpPathFilenameArray, data);
		},
		loadMusicData: function(sampleRate, path, filename, data, options) {
			filename= path+"/"+ filename;
			var ret = this.Module.ccall('emu_init', 'number', 
								['string', 'string'], 
								[ "", filename]);	// separate path not used in emu

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
		// base64 decoding util
		findChar: function(str, c) {
			for (var i= 0; i<str.length; i++) {
				if (str.charAt(i) == c) {
					return i;
				}
			}
			return -1;
		},
		alphanumeric: function(inputtxt) {
			var letterNumber = /^[0-9a-zA-Z]+$/;
			return inputtxt.match(letterNumber);
		},
		is_base64: function(c) {
		  return (this.alphanumeric(""+c) || (c == '+') || (c == '/'));
		}, 
		base64Decode: function(encoded) {
			var in_len= encoded.length;
			var i= j= in_= 0;
			var arr4= new Array(4);
			var arr3= new Array(3);
			var ret= "";
			var carry=-1;

			while (in_len-- && ( encoded.charAt(in_) != '=') && this.is_base64(encoded.charAt(in_))) {
				arr4[i++]= encoded.charAt(in_); in_++;
				if (i ==4) {
					for (i = 0; i <4; i++) {
						arr4[i] = this.findChar(this._chars, arr4[i]);
					}
					arr3[0] = ( arr4[0] << 2       ) + ((arr4[1] & 0x30) >> 4);
					arr3[1] = ((arr4[1] & 0xf) << 4) + ((arr4[2] & 0x3c) >> 2);
					arr3[2] = ((arr4[2] & 0x3) << 6) +   arr4[3];

					for (i = 0; (i < 3); i++) {
						var val= arr3[i];
						
						if (carry > -1) {	// only allow 16bit max
							val= (carry << 8) + val;
							carry= -1;
							ret += String.fromCharCode(val)	// UNICODE
							
						} else if (val > 127) {	// treat as unicode
							carry= val;
						} else {
							ret += String.fromCharCode(val);	// ASCII
						}
					}
					i = 0;
				}
			}
			if (i) {
				for (j = 0; j < i; j++) {
					arr4[j] = this.findChar(this._chars, arr4[j]);
				}
				arr3[0] = (arr4[0] << 2) + ((arr4[1] & 0x30) >> 4);
				arr3[1] = ((arr4[1] & 0xf) << 4) + ((arr4[2] & 0x3c) >> 2);

				for (j = 0; (j < i - 1); j++) { 
					var val= arr3[j];
					
					if (carry > -1) {	// only allow 16bit max
						val= (carry << 8) + val;
						carry= -1;
						ret += String.fromCharCode(val)	// UNICODE
						
					} else if (val > 127) {	// treat as unicode
						carry= val;
					} else {
						ret += String.fromCharCode(val);	// ASCII
					}
				}
			}
			return ret;
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
			result.title= this.base64Decode(this.Module.Pointer_stringify(array[0]));
			if (!result.title.length) result.title= filename.replace(/^.*[\\\/]/, '').split('.').slice(0, -1).join('.');

			result.artist= this.base64Decode(this.Module.Pointer_stringify(array[1]));		
			result.game= this.base64Decode(this.Module.Pointer_stringify(array[2]));
			result.year= this.base64Decode(this.Module.Pointer_stringify(array[3]));
			result.genre= this.base64Decode(this.Module.Pointer_stringify(array[4]));
			result.copyright= this.base64Decode(this.Module.Pointer_stringify(array[5]));
			result.psfby= this.base64Decode(this.Module.Pointer_stringify(array[6]));
		}
	});	return $this; })();