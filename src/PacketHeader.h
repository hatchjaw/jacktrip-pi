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

#ifndef JACKTRIP_PI_PACKETHEADER_H
#define JACKTRIP_PI_PACKETHEADER_H

enum TAudioBitResolution
{
    BIT8 = 1,
    BIT16 = 2,
    BIT24 = 3,
    BIT32 = 4
};

enum TSamplingRate
{
    SR22,
    SR32,
    SR44,
    SR48,
    SR88,
    SR96,
    SR192,
    UNDEF
};

struct TJackTripPacketHeader
{
public:
    u64 nTimeStamp;
    u16 nSeqNumber;
    u16 nBufferSize;
    u8 nSamplingRate;
    u8 nBitResolution;
    u8 nNumIncomingChannelsFromNet;
    u8 nNumOutgoingChannelsToNet;
};

#define PACKET_HEADER_SIZE sizeof(TJackTripPacketHeader)

#endif //JACKTRIP_PI_PACKETHEADER_H
