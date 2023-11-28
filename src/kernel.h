//
// kernel.h
//
// Circle - A C++ bare metal environment for Raspberry Pi
// Copyright (C) 2014-2017  R. Stange <rsta2@o2online.de>
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
#ifndef _kernel_h
#define _kernel_h

#include <circle/actled.h>
#include <circle/koptions.h>
#include <circle/devicenameservice.h>
#include <circle/screen.h>
#include <circle/serial.h>
#include <circle/exceptionhandler.h>
#include <circle/interrupt.h>
#include <circle/timer.h>
#include <circle/logger.h>
#include <circle/usb/usbhcidevice.h>
#include <circle/sched/scheduler.h>
#include <circle/net/netsubsystem.h>
#include <circle/types.h>
#include <circle/net/socket.h>
#include <circle/sound/soundbasedevice.h>
#include <circle/i2cmaster.h>
#include "PacketHeader.h"

enum TShutdownMode {
    ShutdownNone,
    ShutdownHalt,
    ShutdownReboot
};

class CKernel {
public:
    CKernel(void);

    ~CKernel(void);

    boolean Initialize(void);

    TShutdownMode Run(void);

private:
    static constexpr int AUDIO_BLOCK_SAMPLES{32};
//    static constexpr int AUDIO_BLOCK_PERIOD_APPROX_US{AUDIO_BLOCK_SAMPLES * 22};
    static constexpr int AUDIO_BLOCK_PERIOD_APPROX_US{650};
    static constexpr int NUM_CHANNELS{2};
    static constexpr int EXIT_PACKET_SIZE{63};
    static constexpr s16 CHANNEL_FRAME_SIZE{AUDIO_BLOCK_SAMPLES * sizeof(s16)};

    // do not change this order
    CActLED mActLED;
    CKernelOptions mOptions;
    CDeviceNameService m_DeviceNameService;
    CScreenDevice m_Screen;
    CSerialDevice m_Serial;
    CExceptionHandler m_ExceptionHandler;
    CInterruptSystem m_Interrupt;
    CTimer m_Timer;
    CLogger mLogger;
    CI2CMaster m_I2CMaster;
    CUSBHCIDevice m_USBHCI;
    CScheduler m_Scheduler;
    CNetSubSystem m_Net;

    CSocket mUdpSocket;

    u16 mServerUdpPort{0};
    boolean mConnected{false};
//    u8 mBuffer[FRAME_BUFFER_SIZE];
    JackTripPacketHeader packetHeader{
            0,
            0,
            AUDIO_BLOCK_SAMPLES,
            samplingRateT::SR44,
            1 << (BIT16 + 2),
            NUM_CHANNELS,
            NUM_CHANNELS
    };
    const u16 kUdpPacketSize{sizeof(JackTripPacketHeader) + NUM_CHANNELS * AUDIO_BLOCK_SAMPLES * sizeof(u16)};

    CSoundBaseDevice *m_pSound;

    s16** audioBuffer;

    void Receive();

    void Send();

    bool isExitPacket(int size, const u8 *packet) const;

    boolean Connect();

    bool StartAudio();

    void GetSoundData(void *pBuffer, unsigned int nFrames);

    void WriteSoundData(unsigned int nFrames);
};

#endif
