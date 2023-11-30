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
#include <circle/util.h>
#include "PacketHeader.h"
#include "config.h"
#include "oscillator.h"
#include "fifo.h"

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
    JackTripPacketHeader packetHeader{
            0,
            0,
            QUEUE_SIZE_FRAMES,
            samplingRateT::SR44,
            1 << (BIT16 + 2),
            WRITE_CHANNELS,
            WRITE_CHANNELS
    };
    const u16 kUdpPacketSize{PACKET_HEADER_SIZE + WRITE_CHANNELS * QUEUE_SIZE_FRAMES * TYPE_SIZE};

    CSoundBaseDevice *m_pSound;

    COscillator m_VFO;
    FIFO<TYPE> audioBuffer;
    int receivedCount;
    int bufferCount;

    void Receive();

    void Send();

    bool isExitPacket(int size, const u8 *packet) const;

    boolean Connect();

    bool StartAudio();

    void GetSoundData(void *pBuffer, unsigned int nFrames);

    void WriteSoundData(unsigned nFrames);

    void hexDump(const u8 *buffer, int length, bool doHeader = false);

    bool shouldLog() const;
};

#endif
