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
#include "config.h"
#include <circle/net/in.h>
#include <circle/net/ntpdaemon.h>
#include <circle/net/syslogdaemon.h>
#include <circle/net/ipaddress.h>
#include <circle/util.h>
#include <assert.h>
#include <circle/sound/pwmsoundbasedevice.h>
#include <circle/sound/i2ssoundbasedevice.h>
#include <circle/sound/hdmisoundbasedevice.h>
#include <circle/sound/usbsoundbasedevice.h>

// Syslog configuration
static const u8 SysLogServer[] = {192, 168, 10, 10};
static const u16 usServerPort = 8514;        // standard port is 514

static const u16 tcpServerPort = 4464;
static const u16 tcpClientBasePort = 49152;
static const u16 tcpPortMax = (1 << 16) - 1;
static const u16 tcpPortRange = tcpPortMax - tcpClientBasePort;
static const u16 udpPort = 8888;

//static const u16 udpLocalPort = 41814;
//static const u16 udpRemotePort = 14841;
//static const u8 multicastGroup[] = {244, 4, 244, 4};

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

#if WRITE_FORMAT == 0
#define FORMAT        SoundFormatUnsigned8
#define TYPE        u8
#define TYPE_SIZE    sizeof (u8)
#define FACTOR        ((1 << 7)-1)
#define NULL_LEVEL    (1 << 7)
#elif WRITE_FORMAT == 1
#define FORMAT        SoundFormatSigned16
#define TYPE        s16
#define TYPE_SIZE    sizeof (s16)
#define FACTOR        ((1 << 15)-1)
#define NULL_LEVEL    0
#elif WRITE_FORMAT == 2
#define FORMAT		SoundFormatSigned24
#define TYPE		s32
#define TYPE_SIZE	(sizeof (u8)*3)
#define FACTOR		((1 << 23)-1)
#define NULL_LEVEL	0
#endif

static const char FromKernel[] = "kernel";

CKernel::CKernel(void)
        : m_Screen(mOptions.GetWidth(), mOptions.GetHeight()),
          m_Timer(&m_Interrupt),
          mLogger(mOptions.GetLogLevel(), &m_Timer),
          m_I2CMaster(CMachineInfo::Get()->GetDevice(DeviceI2CMaster), true),
          m_USBHCI(&m_Interrupt, &m_Timer),
#ifdef USE_VCHIQ_SOUND
        m_VCHIQ (CMemorySystem::Get (), &m_Interrupt),
#endif
#ifndef USE_DHCP
          m_Net(IPAddress, NetMask, DefaultGateway, DNSServer),
#endif
          mUdpSocket(&m_Net, IPPROTO_UDP),
          m_pSound(nullptr) {
    audioBuffer = new s16*[NUM_CHANNELS];
    for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
        audioBuffer[ch] = new s16[AUDIO_BLOCK_SAMPLES];
    }
    mActLED.Blink(5, 150, 250);    // show we are alive
}

CKernel::~CKernel(void) {
}

boolean CKernel::Initialize(void) {
    bool bOK = true;

    bOK = m_Screen.Initialize();

    if (bOK) {
        bOK = m_Serial.Initialize(115200);
    }

    if (bOK) {
        CDevice *pTarget = m_DeviceNameService.GetDevice(mOptions.GetLogDevice(), FALSE);
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

    if (bOK) {
        bOK = m_I2CMaster.Initialize();
    }

#ifdef USE_VCHIQ_SOUND
    if (bOK)
    {
        bOK = m_VCHIQ.Initialize ();
    }
#endif

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

    if (!StartAudio()) {
        return ShutdownHalt;
    }

    // Connect to JackTrip server.
    if (!Connect()) {
        return ShutdownHalt;
    }

    unsigned nQueueSizeFrames = m_pSound->GetQueueSizeFrames();

//    // output sound data
//    for (unsigned nCount = 0; m_pSound->IsActive(); nCount++) {
//        m_Scheduler.MsSleep(QUEUE_SIZE_MSECS / 2);
//
//        // fill the whole queue free space with data
//        WriteSoundData(nQueueSizeFrames - m_pSound->GetQueueFramesAvail());
//    }

    while (mConnected) {
        // Send packets to the server.
        Send();

        // Receive packets from the server.
        Receive();

        if (m_pSound->IsActive()) {
            WriteSoundData(nQueueSizeFrames - m_pSound->GetQueueFramesAvail());
        }

        m_Scheduler.usSleep(AUDIO_BLOCK_PERIOD_APPROX_US);
//        m_Timer.usDelay(AUDIO_BLOCK_PERIOD_APPROX_US);
    }

    mLogger.Write(FromKernel, LogPanic, "System will halt now.");

    return ShutdownHalt;
}

boolean CKernel::Connect() {
    CIPAddress serverIP(SysLogServer);
    CString ipString;
    serverIP.Format(&ipString);

    // Psuedorandomise the client TCP port to minimise the likelihood of port number re-use.
    auto tcpClientPort = tcpClientBasePort + (CTimer::GetClockTicks() % tcpPortRange);
    auto tcpSocket = new CSocket(&m_Net, IPPROTO_TCP);

    // Bind the TCP port.
    if (tcpSocket->Bind(tcpClientPort) < 0) {
        mLogger.Write(FromKernel, LogError, "Cannot bind TCP socket (port %u)", tcpClientPort);
        return false;
    } else {
        mLogger.Write(FromKernel, LogNotice, "Successfully bound TCP socket (port %u)", tcpClientPort);
    }

    if (tcpSocket->Connect(serverIP, tcpServerPort) < 0) {
        mLogger.Write(FromKernel, LogWarning, "Cannot establish TCP connection to JackTrip server.");
        return false;
    } else {
        mLogger.Write(FromKernel, LogNotice, "TCP connection to server accepted.");
    }

    // Send the UDP port to the JackTrip server; block until sent.
    if (4 != tcpSocket->Send(reinterpret_cast<const u8 *>(&udpPort), 4, 0)) {
        mLogger.Write(FromKernel, LogError, "Failed to send UDP port to server.");
        return false;
    } else {
        mLogger.Write(FromKernel, LogNotice, "Sent UDP port number %u to JackTrip server.", udpPort);
    }

    // Read the JackTrip server's UDP port; block until received.
    if (4 != tcpSocket->Receive(reinterpret_cast<u8 *>(&mServerUdpPort), 4, 0)) {
        mLogger.Write(FromKernel, LogError, "Failed to read UDP port from server.");
        return false;
    } else {
        mLogger.Write(FromKernel, LogNotice, "Received port %u from JackTrip server.", mServerUdpPort);
    }

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
    u8 buffer[kUdpPacketSize];

    int nBytesReceived{mUdpSocket.Receive(buffer, sizeof buffer, MSG_DONTWAIT)};

    if (isExitPacket(nBytesReceived, buffer)) {
        mLogger.Write(FromKernel, LogNotice, "Exit packet received.");
        mConnected = false;
        return;
    } else if (nBytesReceived != kUdpPacketSize) {
        mLogger.Write(FromKernel, LogWarning, "Received %d bytes", nBytesReceived);
    }

    for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
//        audioBuffer[ch] = reinterpret_cast<s16 *>(buffer + PACKET_HEADER_SIZE + CHANNEL_FRAME_SIZE * ch);
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

    auto nBytesSent = mUdpSocket.Send(packet, kUdpPacketSize, MSG_DONTWAIT);

    if (nBytesSent != kUdpPacketSize) {
        mLogger.Write(FromKernel, LogWarning, "Sent %d bytes", nBytesSent);
    }

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

bool CKernel::StartAudio() {
    // select the sound device
    const char *pSoundDevice = mOptions.GetSoundDevice();
    if (strcmp(pSoundDevice, "sndpwm") == 0) {
        m_pSound = new CPWMSoundBaseDevice(&m_Interrupt, SAMPLE_RATE, CHUNK_SIZE);
    } else if (strcmp(pSoundDevice, "sndi2s") == 0) {
        m_pSound = new CI2SSoundBaseDevice(&m_Interrupt, SAMPLE_RATE, CHUNK_SIZE, FALSE,
                                           &m_I2CMaster, DAC_I2C_ADDRESS);
    } else if (strcmp(pSoundDevice, "sndhdmi") == 0) {
        m_pSound = new CHDMISoundBaseDevice(&m_Interrupt, SAMPLE_RATE, CHUNK_SIZE);
    }
#if RASPPI >= 4
        else if (strcmp (pSoundDevice, "sndusb") == 0)
    {
        m_pSound = new CUSBSoundBaseDevice (SAMPLE_RATE);
    }
#endif
    else {
#ifdef USE_VCHIQ_SOUND
        m_pSound = new CVCHIQSoundBaseDevice (&m_VCHIQ, SAMPLE_RATE, CHUNK_SIZE,
                    (TVCHIQSoundDestination) m_Options.GetSoundOption ());
#else
        m_pSound = new CPWMSoundBaseDevice(&m_Interrupt, SAMPLE_RATE, CHUNK_SIZE);
#endif
    }
    assert (m_pSound != 0);

//    // initialize oscillators
//    m_LFO.SetWaveform (WaveformSine);
//    m_LFO.SetFrequency (10.0);
//
//    m_VFO.SetWaveform (WaveformSine);
//    m_VFO.SetFrequency (440.0);
//    m_VFO.SetModulationVolume (0.25);

    // configure sound device
    if (!m_pSound->AllocateQueueFrames(QUEUE_SIZE_FRAMES)) {
        mLogger.Write(FromKernel, LogPanic, "Cannot allocate sound queue");
        return false;
    }

    m_pSound->SetWriteFormat(FORMAT, WRITE_CHANNELS);

    // initially fill the whole queue with data
    unsigned nQueueSizeFrames = m_pSound->GetQueueSizeFrames();

    mLogger.Write(FromKernel, LogNotice, "Audio queue size: %u frames", nQueueSizeFrames);

    WriteSoundData(nQueueSizeFrames);

    // start sound device
    if (!m_pSound->Start()) {
        mLogger.Write(FromKernel, LogPanic, "Cannot start sound device");
        return false;
    }

    return true;

//    mLogger.Write(FromKernel, LogNotice, "Playing modulated 440 Hz tone");

//    // output sound data
//    for (unsigned nCount = 0; m_pSound->IsActive(); nCount++) {
//        m_Scheduler.MsSleep(QUEUE_SIZE_MSECS / 2);
//
//        // fill the whole queue free space with data
//        WriteSoundData(nQueueSizeFrames - m_pSound->GetQueueFramesAvail());
//
//        m_Screen.Rotor(0, nCount);
//    }
}

void CKernel::WriteSoundData(unsigned nFrames) {
    const unsigned nFramesPerWrite = 32;
    u8 buffer[nFramesPerWrite * WRITE_CHANNELS * TYPE_SIZE];

    while (nFrames > 0) {
        unsigned nWriteFrames = nFrames < nFramesPerWrite ? nFrames : nFramesPerWrite;

        GetSoundData(buffer, nWriteFrames);

        unsigned nWriteBytes = nWriteFrames * WRITE_CHANNELS * TYPE_SIZE;

        int nResult = m_pSound->Write(buffer, nWriteBytes);
        if (nResult != (int) nWriteBytes) {
            mLogger.Write(FromKernel, LogError, "Sound data dropped");
        }

        nFrames -= nWriteFrames;

        m_Scheduler.Yield();        // ensure the VCHIQ tasks can run
    }
}

void CKernel::GetSoundData(void *pBuffer, unsigned nFrames) {
    u8 *pBuffer8 = (u8 *) pBuffer;

    unsigned nSamples = nFrames * WRITE_CHANNELS;

    for (unsigned i = 0; i < nSamples;) {
        unsigned ch{0};
//        m_LFO.NextSample ();
//        m_VFO.NextSample ();
//
        auto fLevel = audioBuffer[ch][i]; //m_VFO.GetOutputLevel ();
        TYPE nLevel = (TYPE) (fLevel * VOLUME * FACTOR + NULL_LEVEL);

        memcpy(&pBuffer8[i++ * TYPE_SIZE], &nLevel, TYPE_SIZE);
#if WRITE_CHANNELS == 2
        ++ch;
        fLevel = audioBuffer[ch][i]; //m_VFO.GetOutputLevel ();
        nLevel = (TYPE) (fLevel * VOLUME * FACTOR + NULL_LEVEL);
        memcpy(&pBuffer8[i++ * TYPE_SIZE], &nLevel, TYPE_SIZE);
#endif
    }
}
