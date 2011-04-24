/*
===========================================================================
ARX FATALIS GPL Source Code
Copyright (C) 1999-2010 Arkane Studios SA, a ZeniMax Media company.

This file is part of the Arx Fatalis GPL Source Code ('Arx Fatalis Source Code'). 

Arx Fatalis Source Code is free software: you can redistribute it and/or modify it under the terms of the GNU General Public 
License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

Arx Fatalis Source Code is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied 
warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with Arx Fatalis Source Code.  If not, see 
<http://www.gnu.org/licenses/>.

In addition, the Arx Fatalis Source Code is also subject to certain additional terms. You should have received a copy of these 
additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Arx 
Fatalis Source Code. If not, please request a copy in writing from Arkane Studios at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing Arkane Studios, c/o 
ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.
===========================================================================
*/

#include "audio/codec/WAV.h"

#include <windows.h>
#include <mmreg.h>

#include "io/PakManager.h"
#include "io/Logger.h"

#include "audio/codec/RAW.h"
#include "audio/codec/ADPCM.h"

namespace audio {

#define AS_FORMAT_PCM(x) ((WAVEFORMATEX *)x)
#define AS_FORMAT_ADPCM(x) ((ADPCMWAVEFORMAT *)x)

	///////////////////////////////////////////////////////////////////////////////
	//                                                                           //
	// Class ChunkFile                                                           //
	//                                                                           //
	///////////////////////////////////////////////////////////////////////////////
	class ChunkFile
	{
		public:
			//Constructor and destructor
			ChunkFile(PakFileHandle * file);
			~ChunkFile();
			//I/O
			aalSBool Read(void *, const aalULong &);
			aalSBool Skip(const aalULong &);
			aalSBool Find(const char *);
			aalSBool Check(const char *);
			aalULong Size()
			{
				return offset;
			};
			aalSBool Restart();
		private:
			//Data
			PakFileHandle * file;
			aalULong offset;
	};

	///////////////////////////////////////////////////////////////////////////////
	//                                                                           //
	// Constructor and destructor                                                //
	//                                                                           //
	///////////////////////////////////////////////////////////////////////////////
	StreamWAV::StreamWAV() :
		stream(NULL),
		codec(NULL),
		status(NULL),
		format(NULL),
		size(0), outsize(0),
		offset(0),
		cursor(0)
	{
	}

	StreamWAV::~StreamWAV()
	{
		if (codec) delete codec;

		free(status);
		free(format);
	}

	///////////////////////////////////////////////////////////////////////////////
	//                                                                           //
	// Setup                                                                     //
	//                                                                           //
	///////////////////////////////////////////////////////////////////////////////
	aalError StreamWAV::SetStream(PakFileHandle * _stream)
	{
		if(!_stream) {
			return AAL_ERROR_FILEIO;
		}

		stream = _stream;

		format = malloc(sizeof(WAVEFORMATEX));
		if(!format) {
			return AAL_ERROR_MEMORY;
		}

		ChunkFile wave(stream);

		// Check for 'RIFF' chunk id and skip file size
		if(wave.Check("RIFF") ||
		        wave.Skip(4) ||
		        wave.Check("WAVE") ||
		        wave.Find("fmt ") ||
		        wave.Read(&AS_FORMAT_PCM(format)->wFormatTag, 2) ||
		        wave.Read(&AS_FORMAT_PCM(format)->nChannels, 2) ||
		        wave.Read(&AS_FORMAT_PCM(format)->nSamplesPerSec, 4) ||
		        wave.Read(&AS_FORMAT_PCM(format)->nAvgBytesPerSec, 4) ||
		        wave.Read(&AS_FORMAT_PCM(format)->nBlockAlign, 2) ||
		        wave.Read(&AS_FORMAT_PCM(format)->wBitsPerSample, 2)) {
			return AAL_ERROR_FORMAT;
		}

		// Get codec specific infos from header for non-PCM format
		if (AS_FORMAT_PCM(format)->wFormatTag != WAVE_FORMAT_PCM)
		{
			void * ptr;

			//Load extra bytes from header
			if (wave.Read(&AS_FORMAT_PCM(format)->cbSize, 2)) return AAL_ERROR_FORMAT;

			ptr = realloc(format, sizeof(WAVEFORMATEX) + AS_FORMAT_PCM(format)->cbSize);

			if (!ptr) return AAL_ERROR_MEMORY;

			format = ptr;
			wave.Read((char *)format + sizeof(WAVEFORMATEX), AS_FORMAT_PCM(format)->cbSize);

			// Get sample count from the 'fact' chunk
			wave.Find("fact");
			wave.Read(&outsize, 4);
		}

		// Create codec
		switch (AS_FORMAT_PCM(format)->wFormatTag)
		{
			case WAVE_FORMAT_PCM   :
				codec = new CodecRAW;
				break;
			case WAVE_FORMAT_ADPCM :
				outsize <<= 1;
				codec = new CodecADPCM;
				break;
			default                :
				return AAL_ERROR_FORMAT;
		}

		// Check for 'data' chunk id, get data size and offset
		wave.Restart();
		wave.Skip(12);

		if (wave.Find("data")) return AAL_ERROR_FORMAT;

		size = wave.Size();

		if (AS_FORMAT_PCM(format)->wFormatTag == WAVE_FORMAT_PCM) outsize = size;
		else outsize *= AS_FORMAT_PCM(format)->nChannels;

		offset = PAK_ftell(stream);

		aalError error;
		error = codec->SetStream(stream);

		if (error) return error;

		error = codec->SetHeader(format);

		if (error) return error;

		return AAL_OK;
	}

	aalError StreamWAV::SetFormat(const aalFormat &)
	{
		return AAL_ERROR;
	}

	aalError StreamWAV::SetLength(const aalULong &)
	{
		return AAL_ERROR;
	}

	aalError StreamWAV::SetPosition(const aalULong & position)
	{
		if (position >= outsize) return AAL_ERROR_FILEIO;

		cursor = position;

		// Reset stream position at the begining of data chunk
		if (PAK_fseek(stream, offset, SEEK_SET) == -1) return AAL_ERROR_FILEIO;

		return codec->SetPosition(cursor);
	}

	///////////////////////////////////////////////////////////////////////////////
	//                                                                           //
	// Status                                                                    //
	//                                                                           //
	///////////////////////////////////////////////////////////////////////////////
	aalError StreamWAV::GetStream(PakFileHandle *&_stream)
	{
		_stream = stream;

		return AAL_OK;
	}

	aalError StreamWAV::GetFormat(aalFormat & _format)
	{
		_format.frequency = AS_FORMAT_PCM(format)->nSamplesPerSec;
		_format.channels = AS_FORMAT_PCM(format)->nChannels;

		switch (AS_FORMAT_PCM(format)->wFormatTag)
		{
			case WAVE_FORMAT_PCM   :
				_format.quality = AS_FORMAT_PCM(format)->wBitsPerSample;
				break;
			case WAVE_FORMAT_ADPCM :
				_format.quality = 16;
				break;
		}

		return AAL_OK;
	}

	aalError StreamWAV::GetLength(aalULong & _length)
	{
		_length = outsize;

		return AAL_OK;
	}

	aalError StreamWAV::GetPosition(aalULong & _position)
	{
		return codec->GetPosition(_position);
	}

	///////////////////////////////////////////////////////////////////////////////
	//                                                                           //
	// I/O                                                                       //
	//                                                                           //
	///////////////////////////////////////////////////////////////////////////////
	aalError StreamWAV::Read(void * buffer, const aalULong & to_read, aalULong & _read)
	{
		_read = 0;

		if (cursor >= outsize) return AAL_OK;

		aalULong count(cursor + to_read > outsize ? outsize - cursor : to_read);

		aalError error;

		error = codec->Read(buffer, count, _read);

		if (error) return error;

		cursor += _read;

		return AAL_OK;
	}

	aalError StreamWAV::Write(void *, const aalULong &, aalULong & write)
	{
		write = 0;

		return AAL_ERROR;
	}

	///////////////////////////////////////////////////////////////////////////////
	//                                                                           //
	// Constructor and destructor                                                //
	//                                                                           //
	///////////////////////////////////////////////////////////////////////////////
	// Constructor                                                               //
	ChunkFile::ChunkFile(PakFileHandle * ptr) : file(ptr), offset(0)
	{
	}

	ChunkFile::~ChunkFile()
	{
	}

	///////////////////////////////////////////////////////////////////////////////
	//                                                                           //
	// I/O                                                                       //
	//                                                                           //
	///////////////////////////////////////////////////////////////////////////////
	// Read!                                                                     //
	aalSBool ChunkFile::Read(void * buffer, const aalULong & size)
	{
		if (PAK_fread(buffer, 1, size, file) != size) return AAL_SFALSE;

		if (offset) offset -= size;

		return AAL_STRUE;
	}

	// Skip!                                                                     //
	aalSBool ChunkFile::Skip(const aalULong & size)
	{
		if (PAK_fseek(file, size, SEEK_CUR) == -1) return AAL_SFALSE;

		if (offset) offset -= size;

		return AAL_STRUE;
	}

	// Return AAL_OK if chunk is found                                        //
	aalSBool ChunkFile::Find(const char * id)
	{
		aalUByte cc[4];

		PAK_fseek(file, offset, SEEK_CUR);

		while (PAK_fread(cc, 4, 1, file))
		{
			if (!PAK_fread(&offset, 4, 1, file)) return AAL_SFALSE;

			if (!memcmp(cc, id, 4)) return AAL_STRUE;

			if (PAK_fseek(file, offset, SEEK_CUR) == -1) return AAL_SFALSE;
		}

		return AAL_SFALSE;
	}

	// Return AAL_OK if next four bytes = chunk id;                          //
	aalSBool ChunkFile::Check(const char * id)
	{
		aalUByte cc[4];

		if (!PAK_fread(cc, 4, 1, file)) return AAL_SFALSE;

		if (memcmp(cc, id, 4)) return AAL_SFALSE;

		if (offset) offset -= 4;

		return AAL_STRUE;
	}

	aalSBool ChunkFile::Restart()
	{
		offset = 0;

		if (PAK_fseek(file, 0, SEEK_SET) == -1) return AAL_SFALSE;

		return AAL_STRUE;
	}
} // namespace audio
