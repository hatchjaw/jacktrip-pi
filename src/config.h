//
// config.h
//
// Circle - A C++ bare metal environment for Raspberry Pi
// Copyright (C) 2017-2022  R. Stange <rsta2@o2online.de>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
#ifndef _config_h
#define _config_h

#define SAMPLE_RATE    44100        // overall system clock

#define WRITE_FORMAT    1        // 0: 8-bit unsigned, 1: 16-bit signed, 2: 24-bit signed
#define WRITE_CHANNELS    2        // 1: Mono, 2: Stereo

#define VOLUME        0.5        // [0.0, 1.0]

#define QUEUE_SIZE_MSECS 100        // size of the sound queue in milliseconds duration
#define CHUNK_SIZE   64 // (384 * 10)    // number of samples, written to sound device at once
#define QUEUE_SIZE_US (23 * CHUNK_SIZE)  // microseconds per buffer; chunk_size/Fs
#define QUEUE_SIZE_FRAMES CHUNK_SIZE

#define DAC_I2C_ADDRESS    0        // I2C slave address of the DAC (0 for auto probing)

#if WRITE_FORMAT == 0
#define FORMAT        SoundFormatUnsigned8
#define TYPE        u8
#define TYPE_SIZE    sizeof (u8)
#define FACTOR        ((1 << 7)-1)
#define NULL_LEVEL    (1 << 7)
#elif WRITE_FORMAT == 1
#define FORMAT        SoundFormatSigned16
#define TYPE        s16
#define TYPE_SIZE    sizeof (s16)
#define FACTOR        ((1 << 15)-1)
#define NULL_LEVEL    0
#elif WRITE_FORMAT == 2
#define FORMAT		SoundFormatSigned24
#define TYPE		s32
#define TYPE_SIZE	(sizeof (u8)*3)
#define FACTOR		((1 << 23)-1)
#define NULL_LEVEL	0
#endif

#define CHANNEL_QUEUE_SIZE (QUEUE_SIZE_FRAMES * TYPE_SIZE)
#define EXIT_PACKET_SIZE 63

#endif
