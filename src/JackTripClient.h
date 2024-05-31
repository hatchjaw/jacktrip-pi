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
#include <circle/sched/scheduler.h>
#include <circle/bcmrandom.h>
#include "config.h"
#include "fifo.h"
#include "PacketHeader.h"

#define PORT_NUMBER_NUM_BYTES 4
#define UDP_PACKET_SIZE       (PACKET_HEADER_SIZE + WRITE_CHANNELS * AUDIO_BLOCK_FRAMES * TYPE_SIZE)
#define RECEIVE_TIMEOUT_SEC   5

class CJackTripClient
{
public:
    CJackTripClient(CLogger *pLogger, CNetSubSystem *pNet, CDevice *pDevice);

    virtual ~CJackTripClient() = default;

    bool Initialize(void);

    virtual boolean Start(void) = 0;

    virtual boolean IsActive(void) = 0;

    bool Connect();

    void Run();

protected:
    void Receive();

    bool ShouldLog() const;

    void HexDump(const u8 *buffer, unsigned int length, bool doHeader);

    CLogger m_Logger;
    CDevice *m_pDevice;
    CFIFO<TYPE> m_FIFO;
    bool m_Connected{false};
    int m_BufferCount{0};

    bool m_DebugAudio{false};
    float m_fPhasor{0.f}, m_fF0{440.f};
private:
    static bool IsExitPacket(int size, const u8 *packet) ;

    void Disconnect();

    CNetSubSystem *m_pNet;
    CSocket m_pUdpSocket;
    CSynchronizationEvent m_Event;
    CSpinLock m_SpinLock;

    u16 m_nServerUdpPort{0};
    TJackTripPacketHeader m_PacketHeader{
            0,
            0,
            AUDIO_BLOCK_FRAMES,
            JACKTRIP_SAMPLE_RATE,
            JACKTRIP_BIT_RES * 8,
            WRITE_CHANNELS,
            WRITE_CHANNELS
    };

    int m_nPacketsReceived{0};
    unsigned int m_nLastReceive{0};

    class CSendTask : public CTask
    {
    public:
        CSendTask(CSocket *pUdpSocket, CSynchronizationEvent *pEvent, bool *pConnected);

        ~CSendTask(void) override;

        void Run(void) override;

    private:
        CSocket *m_pUdpSocket;
        CSynchronizationEvent *m_pEvent;
        bool &m_pConnected;
        TJackTripPacketHeader m_PacketHeader{0, 0, AUDIO_BLOCK_FRAMES, JACKTRIP_SAMPLE_RATE, JACKTRIP_BIT_RES * 8, WRITE_CHANNELS, WRITE_CHANNELS};
    };

    class CClockTask : public CTask
    {
    public:
        CClockTask() : m_Clock(GPIOClockPCM, GPIOClockSourcePLLD) {}

        ~CClockTask(void) override = default;

        void Run(void) override {
            unsigned nSampleRate{SAMPLE_RATE};
            unsigned nClockFreq =
                    CMachineInfo::Get ()->GetGPIOClockSourceRate (GPIOClockSourcePLLD);
            CBcmRandomNumberGenerator rand;

            while (true)
            {
                CScheduler::Get()->MsSleep(5000);

                nSampleRate += (rand.GetNumber() % 1000 - 500);

                if (8000 <= nSampleRate && nSampleRate <= 192000) {
                    return;
                }

                // E.g. 500'000'000 / 64 / 48000 = 162.76... => 162
                unsigned nDivI = nClockFreq / (32*2) / nSampleRate;
                // E.g. 500'000'000 / 64 % 48000 = 36500
                unsigned nTemp = nClockFreq / (32*2) % nSampleRate;
                // E.g. (36500 * 4096 + 24000) / 48000 = 3115.166... => 3115
                unsigned nDivF = (nTemp * 4096 + nSampleRate/2) / nSampleRate;
                assert (nDivF <= 4096);
                if (nDivF > 4095)
                {
                    nDivI++;
                    nDivF = 0;
                }

                CLogger::Get()->Write("clocktask", LogDebug, "Setting Fs: %u, DivI: %u, DivF: %u",
                                      nSampleRate, nDivI, nDivF);

                m_Clock.Start (nDivI, nDivF, nDivF > 0 ? 1 : 0);
            }
        }

    private:
        CGPIOClock m_Clock;
    };

    CSendTask *m_pSendTask{nullptr};
    CClockTask *m_pClockTask{nullptr};
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
