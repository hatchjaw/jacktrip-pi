//
// Created by tar on 20/11/23.
//

#ifndef HELLO_CIRCLE_PACKETHEADER_H
#define HELLO_CIRCLE_PACKETHEADER_H

enum audioBitResolutionT
{
    BIT8 = 1,  ///< 8 bits
    BIT16 = 2, ///< 16 bits (default)
    BIT24 = 3, ///< 24 bits
    BIT32 = 4  ///< 32 bits
};

enum samplingRateT
{
    SR22,  ///<  22050 Hz
    SR32,  ///<  32000 Hz
    SR44,  ///<  44100 Hz
    SR48,  ///<  48000 Hz
    SR88,  ///<  88200 Hz
    SR96,  ///<  96000 Hz
    SR192, ///< 192000 Hz
    UNDEF  ///< Undefined
};

struct JackTripPacketHeader
{
public:
    u64 TimeStamp;    ///< Time Stamp
    u16 SeqNumber;    ///< Sequence Number
    u16 BufferSize;   ///< Buffer Size in Samples
    u8 SamplingRate;  ///< Sampling Rate in JackAudioInterface::samplingRateT
    u8 BitResolution; ///< Audio Bit Resolution
    u8 NumIncomingChannelsFromNet; ///< Number of incoming Channels from the network
    u8 NumOutgoingChannelsToNet; ///< Number of outgoing Channels to the network
};

#endif //HELLO_CIRCLE_PACKETHEADER_H
