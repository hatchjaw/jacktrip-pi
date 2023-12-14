/**
 * JackTrip client for bare-metal Raspberry Pi
 * Copyright (C) 2023 Thomas Rushton
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef JACKTRIP_PI_CONFIG_H
#define JACKTRIP_PI_CONFIG_H

// 0: 22050, 1: 32000, 2: 44100, 3: 48000
#define SR_FORMAT            3

// Format in which to exchange samples with JackTrip
// 0: u8, 1: s16, 2: s24, 3: u32 (See TSoundFormat)
#define SAMPLE_FORMAT        1

#if SR_FORMAT == 0
#define SAMPLE_RATE          22050
#define JACKTRIP_SAMPLE_RATE SR22
#elif SR_FORMAT == 1
#define SAMPLE_RATE          32000
#define JACKTRIP_SAMPLE_RATE SR32
#elif SR_FORMAT == 2
#define SAMPLE_RATE          44100
#define JACKTRIP_SAMPLE_RATE SR44
#elif SR_FORMAT == 3
#define SAMPLE_RATE          48000
#define JACKTRIP_SAMPLE_RATE SR48
#endif

#if SAMPLE_FORMAT == 0
#define JACKTRIP_BIT_RES     BIT8
#define TYPE                 u8
#define TYPE_SIZE            sizeof (u8)
#define FACTOR               ((1 << 7)-1)
#define NULL_LEVEL           (1 << 7)
#elif SAMPLE_FORMAT == 1
#define JACKTRIP_BIT_RES     BIT16
#define TYPE                 s16
#define TYPE_SIZE            sizeof (s16)
#define FACTOR               ((1 << 15)-1)
#define NULL_LEVEL           0
#elif SAMPLE_FORMAT == 2
#define JACKTRIP_BIT_RES     BIT24
#define TYPE		         s32
#define TYPE_SIZE	         (sizeof (u8)*3)
#define FACTOR		         ((1 << 23)-1)
#define NULL_LEVEL	         0
#elif SAMPLE_FORMAT == 3
#define JACKTRIP_BIT_RES     BIT32
#define TYPE                 u32
#define TYPE_SIZE            sizeof (u32)
#define FACTOR               ((1 << 31)-1)
#define NULL_LEVEL           (1 << 31)
#endif

// 1: Mono, 2: Stereo
#define WRITE_CHANNELS       2

#define AUDIO_BLOCK_FRAMES   32
#define QUEUE_SIZE_US        (AUDIO_BLOCK_FRAMES * 1000000 / SAMPLE_RATE)

// I2C slave address of the DAC (0 for auto probing)
#define DAC_I2C_ADDRESS      0

#define SERVER_IP            192,168,10,10
#define JACKTRIP_TCP_PORT    4464
// The Internet Assigned Numbers Authority (IANA) suggests the range 49152 to
// 65535 for dynamic or private ports.*/
#define DYNAMIC_PORT_START   49152
#define DYNAMIC_PORT_END     ((1 << 16) - 1)
#define DYNAMIC_PORT_RANGE   (DYNAMIC_PORT_END - DYNAMIC_PORT_START)

#define CHANNEL_QUEUE_SIZE   (AUDIO_BLOCK_FRAMES * TYPE_SIZE)
#define EXIT_PACKET_SIZE     63

#endif
