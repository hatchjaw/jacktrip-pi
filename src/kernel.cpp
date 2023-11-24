//
// kernel.cpp
//
// Circle - A C++ bare metal environment for Raspberry Pi
// Copyright (C) 2015-2017  R. Stange <rsta2@o2online.de>
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
#include "kernel.h"
#include <circle/net/in.h>
#include <circle/net/ntpdaemon.h>
#include <circle/net/syslogdaemon.h>
#include <circle/net/ipaddress.h>
#include <circle/util.h>
#include <assert.h>

// Syslog configuration
static const u8 SysLogServer[] = {192, 168, 10, 10};
static const u16 usServerPort = 8514;        // standard port is 514

static const u16 tcpServerPort = 4464;
static const u16 tcpClientBasePort = 49152;
static const u16 udpPort = 8888;

static const u16 udpLocalPort = 41814;
static const u16 udpRemotePort = 14841;
static const u8 multicastGroup[] = {244, 4, 244, 4};

// Time configuration
#undef USE_NTP

#ifdef USE_NTP
static const char NTPServer[]    = "pool.ntp.org";
static const int nTimeZone       = 1*60;		// minutes diff to UTC
#endif

// Network configuration`
#undef USE_DHCP

#ifndef USE_DHCP
static const u8 IPAddress[] = {192, 168, 10, 250};
static const u8 NetMask[] = {255, 255, 255, 0};
static const u8 DefaultGateway[] = {192, 168, 10, 1};
static const u8 DNSServer[] = {192, 168, 10, 1};
#endif

static const char FromKernel[] = "kernel";

CKernel::CKernel(void)
        : m_Screen(m_Options.GetWidth(), m_Options.GetHeight()),
          m_Timer(&m_Interrupt),
          mLogger(m_Options.GetLogLevel(), &m_Timer),
          m_USBHCI(&m_Interrupt, &m_Timer),
#ifndef USE_DHCP
          m_Net(IPAddress, NetMask, DefaultGateway, DNSServer),
#endif
          mTcpSocket(&m_Net, IPPROTO_TCP),
          mUdpSocket(&m_Net, IPPROTO_UDP) {
    m_ActLED.Blink(5, 150, 250);    // show we are alive
}

CKernel::~CKernel(void) {
}

boolean CKernel::Initialize(void) {
    boolean bOK = TRUE;

    if (bOK) {
        bOK = m_Screen.Initialize();
    }

    if (bOK) {
        bOK = m_Serial.Initialize(115200);
    }

    if (bOK) {
        CDevice *pTarget = m_DeviceNameService.GetDevice(m_Options.GetLogDevice(), FALSE);
        if (pTarget == 0) {
            pTarget = &m_Screen;
        }

        bOK = mLogger.Initialize(pTarget);
    }

    if (bOK) {
        bOK = m_Interrupt.Initialize();
    }

    if (bOK) {
        bOK = m_Timer.Initialize();
    }

    if (bOK) {
        bOK = m_USBHCI.Initialize();
    }

    if (bOK) {
        bOK = m_Net.Initialize();
    }

    return bOK;
}

TShutdownMode CKernel::Run(void) {
    mLogger.Write(FromKernel, LogNotice, "Compile time: " __DATE__ " " __TIME__);

#ifdef USE_NTP
    m_Timer.SetTimeZone (nTimeZone);
    new CNTPDaemon (NTPServer, &m_Net);
#endif

    CIPAddress ServerIP(SysLogServer);
    CString IPString;
    ServerIP.Format(&IPString);
    mLogger.Write(FromKernel, LogNotice, "Sending log messages to %s:%u",
                  (const char *) IPString, (unsigned) usServerPort);

    new CSysLogDaemon(&m_Net, ServerIP, usServerPort);

    // Connect to JackTrip server.
    if (!Connect()) {
        return ShutdownHalt;
    }

    while (mConnected) {
        // Send packets to the server.
        Send();

        // Receive packets from the server.
        Receive();

        m_Scheduler.usSleep(726);
//        m_Timer.usDelay(726);
    }

//    if (0 == mUdpSocket.Bind(udpLocalPort)) {
//        mLogger.Write(FromKernel, LogNotice, "UDP Socket successfully bound to port %u", udpLocalPort);
//    } else {
//        mLogger.Write(FromKernel, LogPanic, "Failed to bind UDP socket to port %u; system will halt now.", udpLocalPort);
//        return ShutdownHalt;
//    }
//
////    CIPAddress multicastIP(multicastGroup);
////    multicastIP.Format(&IPString);
//
//    if (0 == mUdpSocket.Connect(ServerIP, udpRemotePort)) {
////        m_Logger.Write(FromKernel, LogNotice, "Successfully joined multicast group %s:%u",
//        mLogger.Write(FromKernel, LogNotice, "Successfully set up socket with address %s:%u",
//                       (const char *) IPString, (unsigned) udpRemotePort);
//    } else {
//        mLogger.Write(FromKernel, LogNotice, "Failed to join multicast group %s:%u; system will halt now.",
//                       (const char *) IPString, (unsigned) udpRemotePort);
//        return ShutdownHalt;
//    }



//    u8 buffer[kUdpPacketSize];
//    int numBytesReceived;
//
//    for (unsigned i = 1; i <= 10; i++) {
//        m_Scheduler.Sleep(1);
//
//        mLogger.Write(FromKernel, LogNotice, "Hello syslog! (%u)", i);
//
//        numBytesReceived = mUdpSocket.Receive(buffer, sizeof buffer, MSG_DONTWAIT);
//        mLogger.Write(FromKernel, LogNotice, "%u: Received %d bytes.", i, numBytesReceived);
//
//        ++packetHeader.SeqNumber;
//        memcpy(buffer, &packetHeader, sizeof(JackTripPacketHeader));
//        mUdpSocket.Send(buffer, kUdpPacketSize, MSG_DONTWAIT);
//    }



    mLogger.Write(FromKernel, LogPanic, "System will halt now.");

    return ShutdownHalt;
}

boolean CKernel::Connect() {
    CIPAddress serverIP(SysLogServer);
    CString ipString;
    serverIP.Format(&ipString);

//    mTcpSocket = new CSocket(&m_Net, IPPROTO_TCP);

    auto clientPort = tcpClientBasePort + (CTimer::GetClockTicks() % (65535 - tcpClientBasePort));
    // Bind the TCP port.
    if (mTcpSocket.Bind(clientPort) < 0) {
        mLogger.Write(FromKernel, LogError, "Cannot bind TCP socket (port %u)", clientPort);
        return false;
    } else {
        mLogger.Write(FromKernel, LogNotice, "Successfully bound TCP socket (port %u)", clientPort);
    }

    if (mTcpSocket.Connect(serverIP, tcpServerPort) < 0) {
        mLogger.Write(FromKernel, LogWarning, "Cannot establish TCP connection to JackTrip server.");
        return false;
    } else {
        mLogger.Write(FromKernel, LogNotice, "TCP connection to server accepted.");
    }

    // Send the UDP port to the JackTrip server; block until sent.
    if (4 != mTcpSocket.Send(reinterpret_cast<const u8 *>(&udpPort), 4, 0)) {
        mLogger.Write(FromKernel, LogError, "Failed to send UDP port to server.");
        return false;
    } else {
        mLogger.Write(FromKernel, LogNotice, "Sent UDP port number %u to JackTrip server.", udpPort);
    }

    // Read the JackTrip server's UDP port; block until received.
    if (4 != mTcpSocket.Receive(reinterpret_cast<u8 *>(&mServerUdpPort), 4, 0)) {
        mLogger.Write(FromKernel, LogError, "Failed to read UDP port from server.");
        return false;
    } else {
        mLogger.Write(FromKernel, LogNotice, "Received port %u from JackTrip server.", mServerUdpPort);
    }

//    m_Timer.MsDelay(500);

    // Set up the UDP socket.
    if (mUdpSocket.Bind(udpPort) < 0) {
        mLogger.Write(FromKernel, LogError, "Failed to bind UDP socket to port %u.", udpPort);
        return ShutdownHalt;
    } else {
        mLogger.Write(FromKernel, LogNotice, "UDP Socket successfully bound to port %u", udpPort);
    }

    if (mUdpSocket.Connect(serverIP, mServerUdpPort) < 0) {
        mLogger.Write(FromKernel, LogError, "Failed to prepare UDP connection.");
        return false;
    } else {
        mLogger.Write(FromKernel, LogNotice, "UDP connection prepared with %s:%u",
                      (const char *) ipString, (unsigned) mServerUdpPort);
    }

    mLogger.Write(FromKernel, LogNotice, "Ready!");
    mConnected = true;

    return true;
}

void CKernel::Receive() {
//    mLogger.Write(FromKernel, LogDebug, "About to attempt to receive.");

    u8 buffer[kUdpPacketSize];

//    int nBytesReceived{mUdpSocket.Receive(mBuffer, sizeof mBuffer, MSG_DONTWAIT)};
    int nBytesReceived{mUdpSocket.Receive(buffer, sizeof buffer, MSG_DONTWAIT)};

    if (nBytesReceived != 144) {
        mLogger.Write(FromKernel, LogNotice, "Received %d bytes", nBytesReceived);
    }

    if (isExitPacket(nBytesReceived, buffer)) {
        mLogger.Write(FromKernel, LogNotice, "Exit packet received.");
        mConnected = false;
        return;
    } else {
//        mLogger.Write(FromKernel, LogNotice, "Received %d bytes", nBytesReceived);
    }
}

void CKernel::Send() {
    if (!mConnected) {
        return;
    }

    u8 packet[kUdpPacketSize];
    ++packetHeader.SeqNumber;
    memcpy(packet, &packetHeader, sizeof(JackTripPacketHeader));

//    mLogger.Write(FromKernel, LogNotice, "Sending packet %u to %s:%d", packetHeader.SeqNumber, (const char *) ipString, mServerUdpPort);

    auto nSent = mUdpSocket.Send(packet, kUdpPacketSize, MSG_DONTWAIT);

//    mLogger.Write(FromKernel, LogNotice, "Sent %d bytes.", nSent);
}

boolean CKernel::isExitPacket(const int size, const u8 *packet) const {
    if (size == EXIT_PACKET_SIZE) {
        for (auto i{0}; i < EXIT_PACKET_SIZE; ++i) {
            if (packet[i] != 0xff) {
                return false;
            }
        }
        return true;
    }
    return false;
}
