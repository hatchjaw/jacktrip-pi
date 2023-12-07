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
#ifndef JACKTRIP_PI_CONFIG_H
#define JACKTRIP_PI_CONFIG_H

#define SAMPLE_RATE          44100

// 0: 8-bit unsigned, 1: 16-bit signed, 2: 24-bit signed, 3: u32 (See TSoundFormat)
#define WRITE_FORMAT         1
// 1: Mono, 2: Stereo
#define WRITE_CHANNELS       2

// [0.0, 1.0]
#define VOLUME               0.5

// size of the sound queue in milliseconds duration
#define QUEUE_SIZE_MSECS     100
// number of samples, written to sound device at once
#define CHUNK_SIZE           64 // (384 * 10)
// microseconds per buffer; chunk_size/Fs
#define QUEUE_SIZE_US        (23 * CHUNK_SIZE)
#define QUEUE_SIZE_FRAMES    CHUNK_SIZE

// I2C slave address of the DAC (0 for auto probing)
#define DAC_I2C_ADDRESS      0

#if WRITE_FORMAT == 0
#define AUDIO_BIT_RES        BIT8
#define FORMAT               SoundFormatUnsigned8
#define TYPE                 u8
#define TYPE_SIZE            sizeof (u8)
#define FACTOR               ((1 << 7)-1)
#define NULL_LEVEL           (1 << 7)
#elif WRITE_FORMAT == 1
#define AUDIO_BIT_RES        BIT16
#define FORMAT               SoundFormatSigned16
#define TYPE                 s16
#define TYPE_SIZE            sizeof (s16)
#define FACTOR               ((1 << 15)-1)
#define NULL_LEVEL           0
#elif WRITE_FORMAT == 2
#define AUDIO_BIT_RES        BIT24
#define FORMAT		         SoundFormatSigned24
#define TYPE		         s32
#define TYPE_SIZE	         (sizeof (u8)*3)
#define FACTOR		         ((1 << 23)-1)
#define NULL_LEVEL	         0
#elif WRITE_FORMAT == 3
#define AUDIO_BIT_RES        BIT32
#define FORMAT               SoundFormatUnsigned32
#define TYPE                 u32
#define TYPE_SIZE            sizeof (u32)
#define FACTOR               ((1 << 31)-1)
#define NULL_LEVEL           (1 << 31)
#endif

#define SERVER_IP            192,168,10,10
#define TCP_SERVER_PORT      4464
#define TCP_CLIENT_BASE_PORT 49152
#define TCP_PORT_MAX         ((1 << 16) - 1)
#define TCP_PORT_RANGE       (TCP_PORT_MAX - TCP_CLIENT_BASE_PORT)
#define UDP_PORT             31713

#define CHANNEL_QUEUE_SIZE   (QUEUE_SIZE_FRAMES * TYPE_SIZE)
#define EXIT_PACKET_SIZE     63

#endif
