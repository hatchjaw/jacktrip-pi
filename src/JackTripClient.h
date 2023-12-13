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

#ifndef JACKTRIP_PI_JACKTRIPCLIENT_H
#define JACKTRIP_PI_JACKTRIPCLIENT_H


#include <circle/sound/pwmsoundbasedevice.h>
#include <circle/sound/i2ssoundbasedevice.h>
#include <circle/sched/task.h>
#include <circle/net/netsubsystem.h>
#include <circle/net/ipaddress.h>
#include <circle/net/socket.h>
#include <circle/util.h>
#include "config.h"
#include "fifo.h"
#include "PacketHeader.h"

class CJackTripClient
{
public:
    CJackTripClient(CLogger *pLogger, CNetSubSystem *pNet, CDevice *pDevice);

    bool Initialize(void);

    virtual boolean Start(void) = 0;

    virtual boolean IsActive(void) = 0;

    bool Connect();

    void Run();

    virtual ~CJackTripClient();

protected:
    void Send();

    void Receive();

    bool ShouldLog() const;

    void HexDump(const u8 *buffer, unsigned int length, bool doHeader);

    CLogger m_Logger;
    CDevice *m_pDevice;
    CFIFO<TYPE> m_FIFO;
    bool m_Connected{false};
    int m_BufferCount{0};
    bool m_Pulse{false};
    bool m_DebugAudio{false};
    float m_fPhasor{0.f}, m_fF0{440.f};
private:
    boolean IsExitPacket(int size, const u8 *packet) const;

    void Disconnect();

    CNetSubSystem *m_pNet;
    CSocket *m_pUdpSocket;
    CSynchronizationEvent m_Event;

    u16 m_nServerUdpPort{0};
    TJackTripPacketHeader m_PacketHeader{
            0,
            0,
            QUEUE_SIZE_FRAMES,
            TSamplingRate::SR44,
            AUDIO_BIT_RES * 8,
            WRITE_CHANNELS,
            WRITE_CHANNELS
    };
    const u16 k_UdpPacketSize{PACKET_HEADER_SIZE + WRITE_CHANNELS * QUEUE_SIZE_FRAMES * TYPE_SIZE};

    int m_nPacketsReceived{0};

    class CSendTask : public CTask
    {
    public:
        CSendTask(CSocket *pUdpSocket, CSynchronizationEvent &pEvent, bool &connected);

        ~CSendTask(void) override;

        void Run(void) override;

    private:
        const u16 k_nUdpPacketSize{PACKET_HEADER_SIZE + WRITE_CHANNELS * QUEUE_SIZE_FRAMES * TYPE_SIZE};

        CSocket *m_pUdpSocket;
        CSynchronizationEvent &m_Event;
        bool &m_Connected;
        TJackTripPacketHeader m_PacketHeader{0, 0, QUEUE_SIZE_FRAMES, TSamplingRate::SR44, AUDIO_BIT_RES * 8, WRITE_CHANNELS, WRITE_CHANNELS};
    };

    CSendTask *m_pSendTask;
};

//// PWM //////////////////////////////////////////////////////////////////////

class JackTripClientPWM : public CJackTripClient, public CPWMSoundBaseDevice
{
public:
    JackTripClientPWM(CLogger *pLogger, CNetSubSystem *pNet, CInterruptSystem *pInterrupt, CDevice *pDevice);

    boolean Start(void) override;

    boolean IsActive(void) override;

private:
    unsigned int GetChunk(u32 *pBuffer, unsigned int nChunkSize) override;

    unsigned m_nMaxLevel, m_nZeroLevel;
};

//// I2S //////////////////////////////////////////////////////////////////////

class JackTripClientI2S : public CJackTripClient, public CI2SSoundBaseDevice
{
public:
    JackTripClientI2S(CLogger *pLogger, CNetSubSystem *pNet, CInterruptSystem *pInterrupt, CI2CMaster *pI2CMaster,
                      CDevice *pDevice);

    boolean Start(void) override;

    boolean IsActive(void) override;

private:
    unsigned int GetChunk(u32 *pBuffer, unsigned int nChunkSize) override;

    const int k_nMinLevel, k_nMaxLevel;
};


#endif //JACKTRIP_PI_JACKTRIPCLIENT_H
