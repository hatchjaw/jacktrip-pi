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
//#include <circle/net/in.h>
//#include <circle/net/ntpdaemon.h>
//#include <circle/net/syslogdaemon.h>
//#include <circle/net/ipaddress.h>
#include <assert.h>
//#include <circle/sound/pwmsoundbasedevice.h>
//#include <circle/sound/i2ssoundbasedevice.h>
//#include <circle/sound/hdmisoundbasedevice.h>
//#include <circle/sound/usbsoundbasedevice.h>

// Syslog configuration
static const u8 SysLogServer[] = {192, 168, 10, 10};
static const u16 usServerPort = 8514;        // standard port is 514

//static const u16 tcpServerPort = 4464;
//static const u16 tcpClientBasePort = 49152;
//static const u16 tcpPortMax = (1 << 16) - 1;
//static const u16 tcpPortRange = tcpPortMax - tcpClientBasePort;
//static const u16 udpPort = 8888;

//static const u16 udpLocalPort = 41814;
//static const u16 udpRemotePort = 14841;
//static const u8 multicastGroup[] = {244, 4, 244, 4};

// Network configuration
static const u8 IPAddress[] = {192, 168, 10, 250};
static const u8 NetMask[] = {255, 255, 255, 0};
static const u8 DefaultGateway[] = {192, 168, 10, 1};
static const u8 DNSServer[] = {192, 168, 10, 1};

static const char FromKernel[] = "kernel";

CKernel::CKernel(void)
        : m_Screen(m_Options.GetWidth(), m_Options.GetHeight()),
          m_Timer(&m_Interrupt),
          m_Logger(m_Options.GetLogLevel(), &m_Timer),
          m_I2CMaster(CMachineInfo::Get()->GetDevice(DeviceI2CMaster), true),
          m_USBHCI(&m_Interrupt, &m_Timer, true),
          m_Net(IPAddress, NetMask, DefaultGateway, DNSServer),
          m_pJTC(nullptr)
//          m_UdpSocket(&m_Net, IPPROTO_UDP),
//          m_pSound(nullptr),
//          m_AudioBuffer(WRITE_CHANNELS, QUEUE_SIZE_FRAMES * 32)
{
    m_ActLED.Blink(5, 150, 250);    // show we are alive
}

CKernel::~CKernel(void) {
}

boolean CKernel::Initialize(void) {
    bool bOK = true;

    bOK = m_Screen.Initialize();

#if 0
    if (bOK) {
        bOK = m_Serial.Initialize(115200);
    }
#endif

    if (bOK) {
        CDevice *pTarget = m_DeviceNameService.GetDevice(m_Options.GetLogDevice(), FALSE);
        if (pTarget == 0) {
            pTarget = &m_Screen;
        }

        bOK = m_Logger.Initialize(pTarget);
    }

    if (bOK) {
        bOK = m_Interrupt.Initialize();
    }

    if (bOK) {
        bOK = m_Timer.Initialize();
    }

    if (bOK) {
        bOK = m_I2CMaster.Initialize();
    }

    if (bOK) {
        bOK = m_USBHCI.Initialize();
    }

    if (bOK) {
        bOK = m_Net.Initialize();
    }

    if (bOK) {
        const char *pSoundDevice = m_Options.GetSoundDevice();
        assert (pSoundDevice);
        if (strcmp(pSoundDevice, "sndi2s") == 0) {
            m_pJTC = new JackTripClientI2S(&m_Logger, &m_Net, &m_Interrupt, &m_I2CMaster);
        }
#if RASPPI >= 4
            else if (strcmp (pSoundDevice, "sndusb") == 0)
        {
            m_JTC = new JackTripClientUSB (&m_Interrupt);
        }
#endif
        else {
            pSoundDevice = "PWM";
            m_pJTC = new JackTripClientPWM(&m_Logger, &m_Net, &m_Interrupt);
        }

        assert (m_pJTC);

        m_Logger.Write(FromKernel, LogNotice, "Instantiated %s sound device", pSoundDevice);

        bOK = m_pJTC->Initialize();
    }

    return bOK;
}

TShutdownMode CKernel::Run(void) {
    m_Logger.Write(FromKernel, LogNotice, "Compile time: " __DATE__ " " __TIME__);

//    m_pJTC->Connect();

    if (!m_pJTC->Start()) {
        m_Logger.Write(FromKernel, LogPanic, "Failed to start JackTrip client.");
        return ShutdownHalt;
    } else {
        m_Logger.Write(FromKernel, LogNotice, "Started JackTrip client.");
    }

    while (m_pJTC->IsActive()) {
        m_pJTC->Run();
    }

//    unsigned nQueueSizeFrames = m_pSound->GetQueueSizeFrames();

//    // output sound data
//    for (unsigned nCount = 0; m_pSound->IsActive(); nCount++) {
//        m_Scheduler.MsSleep(QUEUE_SIZE_MSECS / 2);
//
//        // fill the whole queue free space with data
//        WriteSoundData(nQueueSizeFrames - m_pSound->GetQueueFramesAvail());
//    }

//    while (m_Connected) {
//        // Send packets to the server.
//        Send();
//
//        // Receive packets from the server.
//        Receive();
//
//        if (m_pSound->IsActive()) {
////            mLogger.Write(FromKernel, LogDebug, "nFrames: %u", nQueueSizeFrames - m_pSound->GetQueueFramesAvail());
//            WriteSoundData(nQueueSizeFrames - m_pSound->GetQueueFramesAvail());
//        }
//
//        m_Scheduler.usSleep(QUEUE_SIZE_US / 2);
////        m_Timer.usDelay(AUDIO_BLOCK_PERIOD_APPROX_US);
//
//        ++m_BufferCount;
//    }

//    for (unsigned i = 1; i <= 10; i++) {
//        m_Scheduler.Sleep(5);
//
//        m_Logger.Write(FromKernel, LogNotice, "Hello syslog! (%u)", i);
//    }

    m_Logger.Write(FromKernel, LogPanic, "System will halt now.");

    return ShutdownHalt;
}

//boolean CKernel::Connect() {
//    CIPAddress serverIP(SysLogServer);
//    CString ipString;
//    serverIP.Format(&ipString);
//
//    // Psuedorandomise the client TCP port to minimise the likelihood of port number re-use.
//    auto tcpClientPort = tcpClientBasePort + (CTimer::GetClockTicks() % tcpPortRange);
//    auto tcpSocket = new CSocket(&m_Net, IPPROTO_TCP);
//
//    // Bind the TCP port.
//    if (tcpSocket->Bind(tcpClientPort) < 0) {
//        m_Logger.Write(FromKernel, LogError, "Cannot bind TCP socket (port %u)", tcpClientPort);
//        return false;
//    } else {
//        m_Logger.Write(FromKernel, LogNotice, "Successfully bound TCP socket (port %u)", tcpClientPort);
//    }
//
//    if (tcpSocket->Connect(serverIP, tcpServerPort) < 0) {
//        m_Logger.Write(FromKernel, LogWarning, "Cannot establish TCP connection to JackTrip server.");
//        return false;
//    } else {
//        m_Logger.Write(FromKernel, LogNotice, "TCP connection to server accepted.");
//    }
//
//    // Send the UDP port to the JackTrip server; block until sent.
//    if (4 != tcpSocket->Send(reinterpret_cast<const u8 *>(&udpPort), 4, MSG_DONTWAIT)) {
//        m_Logger.Write(FromKernel, LogError, "Failed to send UDP port to server.");
//        return false;
//    } else {
//        m_Logger.Write(FromKernel, LogNotice, "Sent UDP port number %u to JackTrip server.", udpPort);
//    }
//
//    // Read the JackTrip server's UDP port; block until received.
//    if (4 != tcpSocket->Receive(reinterpret_cast<u8 *>(&m_ServerUdpPort), 4, 0)) {
//        m_Logger.Write(FromKernel, LogError, "Failed to read UDP port from server.");
//        return false;
//    } else {
//        m_Logger.Write(FromKernel, LogNotice, "Received port %u from JackTrip server.", m_ServerUdpPort);
//    }
//
//    // Set up the UDP socket.
//    if (m_UdpSocket.Bind(udpPort) < 0) {
//        m_Logger.Write(FromKernel, LogError, "Failed to bind UDP socket to port %u.", udpPort);
//        return ShutdownHalt;
//    } else {
//        m_Logger.Write(FromKernel, LogNotice, "UDP Socket successfully bound to port %u", udpPort);
//    }
//
//    if (m_UdpSocket.Connect(serverIP, m_ServerUdpPort) < 0) {
//        m_Logger.Write(FromKernel, LogError, "Failed to prepare UDP connection.");
//        return false;
//    } else {
//        m_Logger.Write(FromKernel, LogNotice, "UDP connection prepared with %s:%u",
//                       (const char *) ipString, (unsigned) m_ServerUdpPort);
//    }
//
//    m_Logger.Write(FromKernel, LogNotice, "Ready!");
//    m_Connected = true;
//
//    return true;
//}

//void CKernel::Receive() {
//    u8 buffer8[k_UdpPacketSize];
//
//    int nBytesReceived{m_UdpSocket.Receive(buffer8, sizeof buffer8, MSG_DONTWAIT)};
//
//    if (IsExitPacket(nBytesReceived, buffer8)) {
//        m_Logger.Write(FromKernel, LogNotice, "Exit packet received.");
//        m_Connected = false;
//        return;
//    } else if (nBytesReceived > 0 && nBytesReceived != k_UdpPacketSize) {
//        m_Logger.Write(FromKernel, LogWarning, "Expected %u bytes; received %d bytes", k_UdpPacketSize, nBytesReceived);
//    }
//
//    const TYPE *buffer[WRITE_CHANNELS];
//    for (int ch = 0; ch < WRITE_CHANNELS; ++ch) {
//        buffer[ch] = reinterpret_cast<TYPE *>(buffer8 + PACKET_HEADER_SIZE + CHANNEL_QUEUE_SIZE * ch);
//    }
//
////    b = reinterpret_cast<TYPE *>(localBuf + PACKET_HEADER_SIZE);
//static const char Msg[] = "Starting ...\n";

////    auto nFrames{((nBytesReceived - PACKET_HEADER_SIZE) / TYPE_SIZE) / WRITE_CHANNELS};
////    auto nFrames{((nBytesReceived) / TYPE_SIZE) / WRITE_CHANNELS};
//
//    // Should be writing CHUNK_SIZE frames from the buffer to the fifo...
//    m_AudioBuffer.Write(buffer, CHUNK_SIZE);
//
////    if (shouldLog()) {
//    if (ShouldLog()) {
//        m_Logger.Write(FromKernel, LogDebug, "Received %d bytes via UDP", nBytesReceived);
////        mLogger.Write(FromKernel, LogDebug, "Received net buffer:");
//        HexDump(buffer8, nBytesReceived, true);
//
////        mLogger.Write(FromKernel, LogDebug, "Got audio buffer:");
////        hexDump(reinterpret_cast<u8 *>(b), WRITE_CHANNELS * CHANNEL_QUEUE_SIZE, false);
////        hexDump(reinterpret_cast<u8 *>(buffer), WRITE_CHANNELS * CHANNEL_QUEUE_SIZE, false);
//
////        for (unsigned frame = 0; frame < 8; ++frame) {
////            mLogger.Write(FromKernel, LogDebug, "In Receive(), frame %u = %04x", frame, b[frame]);
////        }
//
////        mLogger.Write(FromKernel, LogDebug, "audioBuffer, channels interleaved:");
////        CString log, chunk;
////        for (unsigned s = 0; s < QUEUE_SIZE_FRAMES; ++s) {
////            if (s > 0 && s % 4 == 0) {
////                mLogger.Write(FromKernel, LogDebug, log);
////                log = "";
////            }
////            chunk.Format("%04x ", audioBuffer[0][s]);
////            log.Append(chunk);
////            chunk.Format("%04x ", audioBuffer[1][s]);
////            log.Append(chunk);
////        }
////        log.Append("\n");
////        mLogger.Write(FromKernel, LogDebug, log);
//    }
//
//    ++m_ReceivedCount;
//}

//bool CKernel::ShouldLog() const { return m_BufferCount > 0 && m_BufferCount % 5000 == 0; }

//void CKernel::Send() {
//    if (!m_Connected) {
//        return;
//    }
//
//    u8 packet[k_UdpPacketSize];
//    ++m_PacketHeader.SeqNumber;
//
//    memcpy(packet, &m_PacketHeader, PACKET_HEADER_SIZE);
////    audioBuffer.read(reinterpret_cast<TYPE *>(packet + PACKET_HEaudioBufferADER_SIZE), QUEUE_SIZE_FRAMES);
//
////    mLogger.Write(FromKernel, LogNotice, "Sending packet %u to %s:%d", packetHeader.SeqNumber, (const char *) ipString, mServerUdpPort);
//
//    auto nBytesSent = m_UdpSocket.Send(packet, k_UdpPacketSize, MSG_DONTWAIT);
//
//    if (nBytesSent != k_UdpPacketSize) {
//        m_Logger.Write(FromKernel, LogWarning, "Sent %d bytes", nBytesSent);
//    }
//
////    mLogger.Write(FromKernel, LogNotice, "Sent %d bytes.", nSent);
//}

//boolean CKernel::IsExitPacket(int size, const u8 *packet) const {
//    if (size == EXIT_PACKET_SIZE) {
//        for (auto i{0}; i < EXIT_PACKET_SIZE; ++i) {
//            if (packet[i] != 0xff) {
//                return false;
//            }
//        }
//        return true;
//    }
//    return false;
//}

//bool CKernel::StartAudio() {
//    const char *pSoundDevice = m_Options.GetSoundDevice();
//    assert (pSoundDevice);
//    if (strcmp(pSoundDevice, "sndi2s") == 0) {
//        m_pJTC = new JackTripClientI2S(&m_Logger, &m_Net, &m_Interrupt, &m_I2CMaster);
//    }
//#if RASPPI >= 4
//        else if (strcmp (pSoundDevice, "sndusb") == 0)
//        {
//            m_JTC = new JackTripClientUSB (&m_Interrupt);
//        }
//#endif
//    else {
//        pSoundDevice = "PWM";
//        m_pJTC = new JackTripClientPWM(&m_Logger, &m_Net, &m_Interrupt);
//    }
//
//    assert (m_pJTC);
//
//    m_Logger.Write(FromKernel, LogNotice, "Instantiated %s sound device", pSoundDevice);
//
//    m_pJTC->Initialize();

//    unsigned nQueueSizeFrames = m_JTC->GetQueueSizeFrames();
//
//    m_Logger.Write(FromKernel, LogNotice, "Audio queue size: %u frames", nQueueSizeFrames);

//    if (!m_pJTC->Start()) {
//        m_Logger.Write(FromKernel, LogPanic, "Failed to start JackTrip client.");
//        return false;
//    } else {
//        m_Logger.Write(FromKernel, LogPanic, "Started JackTrip client.");
//    }
//
//    return true;



//    // select the sound device
//    const char *pSoundDevice = m_Options.GetSoundDevice();
//    if (strcmp(pSoundDevice, "sndpwm") == 0) {
//        m_pSound = new CPWMSoundBaseDevice(&m_Interrupt, SAMPLE_RATE, CHUNK_SIZE);
//    } else if (strcmp(pSoundDevice, "sndi2s") == 0) {
//        m_pSound = new CI2SSoundBaseDevice(&m_Interrupt, SAMPLE_RATE, CHUNK_SIZE, FALSE,
//                                           &m_I2CMaster, DAC_I2C_ADDRESS);
//    } else if (strcmp(pSoundDevice, "sndhdmi") == 0) {
//        m_pSound = new CHDMISoundBaseDevice(&m_Interrupt, SAMPLE_RATE, CHUNK_SIZE);
//    }
//#if RASPPI >= 4
//        else if (strcmp (pSoundDevice, "sndusb") == 0)
//    {
//        m_pSound = new CUSBSoundBaseDevice (SAMPLE_RATE);
//    }
//#endif
//    else {
//#ifdef USE_VCHIQ_SOUND
//        m_pSound = new CVCHIQSoundBaseDevice (&m_VCHIQ, SAMPLE_RATE, CHUNK_SIZE,
//                    (TVCHIQSoundDestination) m_Options.GetSoundOption ());
//#else
//        m_pSound = new CPWMSoundBaseDevice(&m_Interrupt, SAMPLE_RATE, CHUNK_SIZE);
//        pSoundDevice = "PWM";
//#endif
//    }
//    assert (m_pSound != nullptr);
//    m_Logger.Write(FromKernel, LogNotice, "Instantiated %s sound device", pSoundDevice);
//
////    // initialize oscillators
////    m_LFO.SetWaveform (WaveformSine);
////    m_LFO.SetFrequency (10.0);
////
//    m_VFO.SetWaveform(WaveformSawtooth);
//    m_VFO.SetFrequency(1.0);
////    m_VFO.SetModulationVolume (0.25);
//
//    // configure sound device
//    if (!m_pSound->AllocateQueueFrames(QUEUE_SIZE_FRAMES)) {
//        m_Logger.Write(FromKernel, LogPanic, "Cannot allocate sound queue");
//        return false;
//    }
//
//    m_pSound->SetWriteFormat(FORMAT, WRITE_CHANNELS);
//
//    // initially fill the whole queue with data
//    unsigned nQueueSizeFrames = m_pSound->GetQueueSizeFrames();
//
//    m_Logger.Write(FromKernel, LogNotice, "Audio queue size: %u frames", nQueueSizeFrames);
//
//    WriteSoundData(nQueueSizeFrames);
//
//    // start sound device
//    if (!m_pSound->Start()) {
//        m_Logger.Write(FromKernel, LogPanic, "Cannot start sound device");
//        return false;
//    }
//
//    return true;
//}

//void CKernel::WriteSoundData(unsigned nFrames) {
//    const unsigned nFramesPerWrite = QUEUE_SIZE_FRAMES;
//    u8 buffer[nFramesPerWrite * WRITE_CHANNELS * TYPE_SIZE];
//
//    while (nFrames > 0) {
//        unsigned nWriteFrames = nFrames < nFramesPerWrite ? nFrames : nFramesPerWrite;
////        mLogger.Write(FromKernel, LogDebug, "nWriteFrames: %u", nWriteFrames);
//
////        GetSoundData(buffer, nWriteFrames);
//
//        m_AudioBuffer.Read(reinterpret_cast<TYPE *>(buffer), nFrames);
//
//        unsigned nWriteBytes = nWriteFrames * WRITE_CHANNELS * TYPE_SIZE;
////        if (nWriteBytes != WRITE_CHANNELS * TYPE_SIZE * QUEUE_SIZE_FRAMES)
////            mLogger.Write(FromKernel, LogDebug, "nWriteBytes: %u", nWriteBytes);
//
//        if (ShouldLog()) {
//            m_Logger.Write(FromKernel, LogDebug, "Audio buffer to write:");
//            HexDump(buffer, WRITE_CHANNELS * TYPE_SIZE * QUEUE_SIZE_FRAMES, false);
//        }
//
//        int nResult = m_pSound->Write(buffer, nWriteBytes);
//        if (nResult != (int) nWriteBytes) {
//            m_Logger.Write(FromKernel, LogError, "Sound data dropped");
//        }
//
//        nFrames -= nWriteFrames;
//
//        m_Scheduler.Yield();        // ensure the VCHIQ tasks can run
//    }
//}

//void CKernel::GetSoundData(void *pBuffer, unsigned nFrames) {
//    u8 *pBuffer8 = (u8 *) pBuffer;
//
//    unsigned nSamples = nFrames * WRITE_CHANNELS;
//
////    audioBuffer.read(reinterpret_cast<TYPE *>(pBuffer), nFrames);
//
////    if (shouldLog()) {
////        for (int frame = 0; frame < 8; ++frame) {
////            mLogger.Write(FromKernel, LogDebug, "Before memcpy, frame %u = %04x", frame, (TYPE *) pBuffer[frame]);
////        }
////    }
//
//
////    for (unsigned ch{0}; ch < WRITE_CHANNELS; ++ch) {
//////        for (unsigned i{0}; i < CHANNEL_QUEUE_SIZE; ++i) {
//////            auto typeOffset{TYPE_SIZE * i};
//////            memcpy(&pBuffer8[ch * typeOffset + typeOffset], &audioBuffer[ch][i], TYPE_SIZE);
//////        }
////        memcpy(pBuffer8 + CHANNEL_QUEUE_SIZE * ch, b + CHANNEL_QUEUE_SIZE * ch, CHANNEL_QUEUE_SIZE);
////
////        if (shouldLog()) {
////            mLogger.Write(FromKernel, LogDebug, "pBuffer ch %u", ch);
////            hexDump(pBuffer8 + CHANNEL_QUEUE_SIZE * ch,CHANNEL_QUEUE_SIZE, false);
////        }
////    }
//
////    if (shouldLog()) {
////        mLogger.Write(FromKernel, LogDebug, "Now have audio buffer:");
////        hexDump(reinterpret_cast<u8 *>(b), WRITE_CHANNELS * CHANNEL_QUEUE_SIZE, false);
////    }
//
//
//
//    // Output is sample interleaved; audioBuffer is channel interleaved.
//    for (unsigned sample = 0, frame = 0; sample < nSamples; ++frame) {
//        m_VFO.NextSample();
//
//        float fLevel = m_VFO.GetOutputLevel();
//        TYPE nLevel = (TYPE) (fLevel * VOLUME * FACTOR + NULL_LEVEL);
//
//        memcpy(&pBuffer8[sample++ * TYPE_SIZE], &nLevel, TYPE_SIZE);
//#if WRITE_CHANNELS == 2
//        memcpy(&pBuffer8[sample++ * TYPE_SIZE], &nLevel, TYPE_SIZE);
//#endif
//
//        if (ShouldLog() && frame < 8) {
//            m_Logger.Write(FromKernel, LogDebug, "Before memcpy, frame %u = %04x", frame, nLevel);
//        }
//    }
//
//
//
//////        auto fLevel = audioBuffer[ch][i]; //m_VFO.GetOutputLevel ();
//////        TYPE nLevel = (TYPE) (fLevel * VOLUME * FACTOR + NULL_LEVEL);
//////        mLogger.Write(FromKernel, LogDebug, "ch0 fr%u: %04x", frame, audioBuffer[0][frame]);
////        memcpy(&pBuffer8[sample * TYPE_SIZE], &b[sample], TYPE_SIZE);
////        ++sample;
////#if WRITE_CHANNELS == 2
////        //        fLevel = 0.f;//audioBuffer[ch][i]; //m_VFO.GetOutputLevel ();
////        //        nLevel = (TYPE) (fLevel * VOLUME * FACTOR + NULL_LEVEL);
////        memcpy(&pBuffer8[sample * TYPE_SIZE], &b[CHANNEL_QUEUE_SIZE + sample], TYPE_SIZE);
////        ++sample;
////#endif
////    }
//
////    if (shouldLog()) {
////        mLogger.Write(FromKernel, LogDebug, "Wrote %u samples", nSamples);
////    }
//}

//void CKernel::HexDump(const u8 *buffer, int length, bool doHeader) {
//    CString log, chunk;
//    size_t word{doHeader ? PACKET_HEADER_SIZE : 0}, row{0};
//    if (doHeader)log.Append("HEAD:");
//    for (const u8 *p = buffer; word < length + (doHeader ? PACKET_HEADER_SIZE : 0); ++p, ++word) {
//        if (word % 16 == 0 && !(doHeader && word == PACKET_HEADER_SIZE)) {
//            if (row > 0 || doHeader) {
//                m_Logger.Write(FromKernel, LogDebug, log);
//                log = "";
//            }
//            chunk.Format("%04x: ", row);
//            log.Append(chunk);
//            ++row;
//        } else if (word % 2 == 0) {
//            log.Append(" ");
//        }
//        chunk.Format("%02x ", *p);
//        log.Append(chunk);
//    }
//
//    log.Append("\n");
//    m_Logger.Write(FromKernel, LogDebug, log);
//}
