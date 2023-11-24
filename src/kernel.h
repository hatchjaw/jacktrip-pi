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
    const int EXIT_PACKET_SIZE{63};

    // do not change this order
    CActLED m_ActLED;
    CKernelOptions m_Options;
    CDeviceNameService m_DeviceNameService;
    CScreenDevice m_Screen;
    CSerialDevice m_Serial;
    CExceptionHandler m_ExceptionHandler;
    CInterruptSystem m_Interrupt;
    CTimer m_Timer;
    CLogger mLogger;
    CUSBHCIDevice m_USBHCI;
    CScheduler m_Scheduler;
    CNetSubSystem m_Net;

    CSocket mTcpSocket;
    CSocket mUdpSocket;

    u16 mServerUdpPort;
    boolean mConnected{false};
    u8 mBuffer[FRAME_BUFFER_SIZE];
    JackTripPacketHeader packetHeader{
            0, 0, 32, samplingRateT::SR44, 1 << (BIT16 + 2), 2, 2
    };
    const u16 kUdpPacketSize{sizeof(JackTripPacketHeader) + 2 * 32 * sizeof(u16)};

    void Receive();

    void Send();

    bool isExitPacket(int size, const u8 *packet) const;

    boolean Connect();
};

#endif
