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

#include "JackTripClient.h"
#include <circle/sched/scheduler.h>
#include <circle/net/in.h>
#include "math.h"

static u16 GenerateDynamicPortNumber(u16 seed = 0)
{
    return DYNAMIC_PORT_START + ((CTimer::GetClockTicks() + seed) % DYNAMIC_PORT_RANGE);
}

static const char FromJTC[] = "jtclient";

CJackTripClient::CJackTripClient(CLogger *pLogger, CNetSubSystem *pNet, CDevice *pDevice) :
        m_Logger(*pLogger),
        m_pDevice(pDevice),
        m_FIFO{WRITE_CHANNELS, QUEUE_SIZE_FRAMES * 16},
        m_pNet(pNet),
        m_pUdpSocket(nullptr),
        m_pSendTask(nullptr)
{
}

CJackTripClient::~CJackTripClient()
{
    delete m_pUdpSocket;
    delete m_pSendTask;
}

bool CJackTripClient::Initialize(void)
{
    return true;
}

bool CJackTripClient::Connect(void)
{
    const u8 ip[] = {SERVER_IP};
    CIPAddress serverIP{ip};
    CString ipString;
    serverIP.Format(&ipString);
    u16 tcpClientPort, udpPort;

    tcpClientPort = GenerateDynamicPortNumber();
    do {
        udpPort = GenerateDynamicPortNumber(tcpClientPort);
    } while (tcpClientPort == udpPort);

    auto tcpSocket = new CSocket(m_pNet, IPPROTO_TCP);
    assert(tcpSocket);

    m_Logger.Write(FromJTC, LogNotice, "Looking for a JackTrip server at %s...", (const char *) ipString);

    // Bind the TCP port.
    if (tcpSocket->Bind(tcpClientPort) < 0) {
        m_Logger.Write(FromJTC, LogError, "Cannot bind TCP socket (port %u)", tcpClientPort);
        Disconnect();
        return false;
    } else {
        m_Logger.Write(FromJTC, LogNotice, "Successfully bound TCP socket (port %u)", tcpClientPort);
    }

    if (tcpSocket->Connect(serverIP, JACKTRIP_TCP_PORT) < 0) {
        m_Logger.Write(FromJTC, LogWarning, "Cannot establish TCP connection to JackTrip server.");
        Disconnect();
        return false;
    } else {
        m_Logger.Write(FromJTC, LogNotice, "TCP connection with server accepted.");
    }

    // Send the UDP port to the JackTrip server; block until sent.
    if (4 != tcpSocket->Send(reinterpret_cast<const u8 *>(&udpPort), 4, MSG_DONTWAIT)) {
        m_Logger.Write(FromJTC, LogError, "Failed to send UDP port to server.");
        Disconnect();
        return false;
    } else {
        m_Logger.Write(FromJTC, LogNotice, "Sent UDP port number %u to JackTrip server.", udpPort);
    }

    // Read the JackTrip server's UDP port; block until received.
    if (4 != tcpSocket->Receive(reinterpret_cast<u8 *>(&m_nServerUdpPort), 4, 0)) {
        m_Logger.Write(FromJTC, LogError, "Failed to read UDP port from server.");
        Disconnect();
        return false;
    } else {
        m_Logger.Write(FromJTC, LogNotice, "Received port %u from JackTrip server.", m_nServerUdpPort);
    }

    m_pUdpSocket = new CSocket(m_pNet, IPPROTO_UDP);
    assert(m_pUdpSocket);

    // Set up the UDP socket.
    if (m_pUdpSocket->Bind(udpPort) < 0) {
        m_Logger.Write(FromJTC, LogError, "Failed to bind UDP socket to port %u.", udpPort);
        Disconnect();
        return false;
    } else {
        m_Logger.Write(FromJTC, LogNotice, "UDP Socket successfully bound to port %u", udpPort);
    }

    if (m_pUdpSocket->Connect(serverIP, m_nServerUdpPort) < 0) {
        m_Logger.Write(FromJTC, LogError, "Failed to prepare UDP connection.");
        Disconnect();
        return false;
    } else {
        m_Logger.Write(FromJTC, LogNotice, "Ready to send datagrams to %s:%u",
                       (const char *) ipString, (unsigned) m_nServerUdpPort);
    }

    m_Connected = true;

    return true;
}

void CJackTripClient::Disconnect()
{
    m_Logger.Write(FromJTC, LogDebug, "Disconnecting");

    m_Connected = false;

//    CScheduler::Get()->ListTasks(m_pDevice);

    if (m_pSendTask != nullptr) {
        // The send task will be waiting. Signal it, it'll find that
        // disconnection has occurred, and it'll terminate.
        m_Event.Set();
        m_Logger.Write(FromJTC, LogDebug, "Waiting for SendTask to terminate.");
        m_pSendTask->WaitForTermination();
//        delete m_pSendTask;
//        m_pSendTask = nullptr;
    }

//    CScheduler::Get()->ListTasks(m_pDevice);

    if (m_pUdpSocket != nullptr) {
        m_Logger.Write(FromJTC, LogDebug, "Deleting datagram socket.");
        delete m_pUdpSocket;
        m_pUdpSocket = nullptr;
    }

    m_Logger.Write(FromJTC, LogDebug, "Resetting fifo and counters.");
    m_BufferCount = 0;
    m_nPacketsReceived = 0;
    m_PacketHeader.nSeqNumber = 0;
    m_FIFO.Clear();

    m_Logger.Write(FromJTC, LogDebug, "Done.");
}

void CJackTripClient::Run()
{
    if (!m_Connected) {
        if (Connect()) {
//            CScheduler::Get()->MsSleep(250);
//            if (m_pSendTask == nullptr) {
            m_pSendTask = new CSendTask(m_pUdpSocket, m_Event, m_Connected);
//            } else if (m_pSendTask->IsSuspended()) {
//                m_pSendTask->Resume();
//            }
//            CScheduler::Get()->Yield();
        } else {
            CScheduler::Get()->Sleep(2);
        }
    } else {
        Receive();
    }

    // This is necessary to give the send task time to work.
    CScheduler::Get()->Yield();

//    Send();
//    Receive();
//    CScheduler::Get()->usSleep(QUEUE_SIZE_US * .925);
}

void CJackTripClient::Send()
{
    if (!m_Connected) return;

    assert(m_pUdpSocket);

    u8 packet[k_UdpPacketSize];
    ++m_PacketHeader.nSeqNumber;

    // Just send garbage with an appropriately-structured header.
    memcpy(packet, &m_PacketHeader, PACKET_HEADER_SIZE);

    auto nBytesSent = m_pUdpSocket->Send(packet, k_UdpPacketSize, MSG_DONTWAIT);

    if (nBytesSent != k_UdpPacketSize) {
        m_Logger.Write(FromJTC, LogWarning, "Sent %d bytes", nBytesSent);
    }
}

void CJackTripClient::Receive()
{
    if (!m_Connected) {
        return;
    }

    assert(m_pUdpSocket);

    u8 buffer8[k_UdpPacketSize];

    // TODO: check how long since last receive; disconnect if threshold exceeded
    // TODO: probably need a spinlock here and one around Send
    int nBytesReceived{m_pUdpSocket->Receive(buffer8, sizeof buffer8, MSG_DONTWAIT)};//m_ReceivedCount == 0 ? MSG_DONTWAIT : 0)};

    if (IsExitPacket(nBytesReceived, buffer8)) {
        m_Logger.Write(FromJTC, LogNotice, "Exit packet received.");
        Disconnect();
        return;
    } else if (nBytesReceived > 0 && nBytesReceived != k_UdpPacketSize) {
        m_Logger.Write(FromJTC, LogWarning, "Expected %u bytes; received %d bytes", k_UdpPacketSize, nBytesReceived);
    }

    if (nBytesReceived > 0) {
        const TYPE *buffer[WRITE_CHANNELS];
        for (int ch = 0; ch < WRITE_CHANNELS; ++ch) {
            buffer[ch] = reinterpret_cast<TYPE *>(buffer8 + PACKET_HEADER_SIZE + CHANNEL_QUEUE_SIZE * ch);
        }

//    b = reinterpret_cast<TYPE *>(localBuf + PACKET_HEADER_SIZE);

//    auto nFrames{((nBytesReceived - PACKET_HEADER_SIZE) / TYPE_SIZE) / WRITE_CHANNELS};
//    auto nFrames{((nBytesReceived) / TYPE_SIZE) / WRITE_CHANNELS};

        // Should be writing CHUNK_SIZE frames from the buffer to the fifo...
        m_FIFO.Write(buffer, CHUNK_SIZE);

        ++m_nPacketsReceived;

        // Notify the send task to send a packet.
        m_Event.Set();
    }

//    if (shouldLog()) {
    if (ShouldLog() && false) {
        m_Logger.Write(FromJTC, LogDebug, "Received %d bytes via UDP", nBytesReceived);
//        mLogger.Write(FromKernel, LogDebug, "Received net buffer:");
        HexDump(buffer8, nBytesReceived, true);

//        mLogger.Write(FromKernel, LogDebug, "Got audio buffer:");
//        hexDump(reinterpret_cast<u8 *>(b), WRITE_CHANNELS * CHANNEL_QUEUE_SIZE, false);
//        hexDump(reinterpret_cast<u8 *>(buffer), WRITE_CHANNELS * CHANNEL_QUEUE_SIZE, false);

//        for (unsigned frame = 0; frame < 8; ++frame) {
//            mLogger.Write(FromKernel, LogDebug, "In Receive(), frame %u = %04x", frame, b[frame]);
//        }

//        mLogger.Write(FromKernel, LogDebug, "audioBuffer, channels interleaved:");
//        CString log, chunk;
//        for (unsigned s = 0; s < QUEUE_SIZE_FRAMES; ++s) {
//            if (s > 0 && s % 4 == 0) {
//                mLogger.Write(FromKernel, LogDebug, log);
//                log = "";
//            }
//            chunk.Format("%04x ", audioBuffer[0][s]);
//            log.Append(chunk);
//            chunk.Format("%04x ", audioBuffer[1][s]);
//            log.Append(chunk);
//        }
//        log.Append("\n");
//        mLogger.Write(FromKernel, LogDebug, log);
    }
}

bool CJackTripClient::IsExitPacket(int size, const u8 *packet) const
{
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

bool CJackTripClient::ShouldLog() const { return false; }//m_BufferCount > 0 && m_BufferCount % 10000 == 0; }

void CJackTripClient::HexDump(const u8 *buffer, unsigned int length, bool doHeader)
{
    CString log{"\n"}, chunk;
    size_t word{doHeader ? PACKET_HEADER_SIZE : 0}, row{0};
    if (doHeader)log.Append("HEAD:");
    for (const u8 *p = buffer; word < length + (doHeader ? PACKET_HEADER_SIZE : 0); ++p, ++word) {
        if (word % 16 == 0 && !(doHeader && word == PACKET_HEADER_SIZE)) {
            if (row > 0 || doHeader) {
//                m_Logger.Write(FromJTC, LogDebug, log);
//                log = "";
                log.Append("\n");
            }
            chunk.Format("%04x: ", row);
            log.Append(chunk);
            ++row;
        } else if (word % 2 == 0) {
            log.Append(" ");
        }
        chunk.Format("%02x ", *p);
        log.Append(chunk);
    }

    log.Append("\n");
    m_Logger.Write(FromJTC, LogDebug, log);
}

//// SEND TASK ////////////////////////////////////////////////////////////////

CJackTripClient::CSendTask::CSendTask(CSocket *pUdpSocket, CSynchronizationEvent &pEvent, bool &connected) :
        m_pUdpSocket(pUdpSocket),
        m_Event(pEvent),
        m_Connected(connected)
{
    SetName("jtcsend");
}

CJackTripClient::CSendTask::~CSendTask(void)
{
//    CLogger::Get()->Write(FromJTC, LogDebug, "In CSendTask destructor");
}

void CJackTripClient::CSendTask::Run(void)
{
    u8 packet[k_nUdpPacketSize];

    assert(m_pUdpSocket);
    memcpy(packet, &m_PacketHeader, PACKET_HEADER_SIZE);
    // Send the zeroth packet.
    m_pUdpSocket->Send(packet, k_nUdpPacketSize, MSG_DONTWAIT);
    // The JackTrip server checks whether a datagram is available, and, if not,
    // sleeps for 100 ms and tries again. This process repeats until a global
    // timeout is exceeded, at which point it gives up. Delaying before the
    // first send from the client doesn't appear to work; sending once, then
    // waiting a little while does. Spamming the connection with an arbitrary
    // number of packets is an option, but results in a lot of ICMP
    // Destination unreachable (Port unreachable) warnings.
    // TODO: doesn't behave well if started before the server.
    CScheduler::Get()->MsSleep(100);

    CLogger::Get()->Write(FromJTC, LogNotice, "Sending datagrams.");

    while (m_Connected) {
        assert(m_pUdpSocket);

        ++m_PacketHeader.nSeqNumber;
        memcpy(packet, &m_PacketHeader, PACKET_HEADER_SIZE);

        m_pUdpSocket->Send(packet, k_nUdpPacketSize, MSG_DONTWAIT);

//        CScheduler::Get()->Yield();

        m_Event.Clear();
        m_Event.Wait();
    }

    CLogger::Get()->Write(FromJTC, LogDebug, "Not connected; leaving CSendTask::Run");
}


//// PWM //////////////////////////////////////////////////////////////////////

JackTripClientPWM::JackTripClientPWM(CLogger *pLogger,
                                     CNetSubSystem *pNet,
                                     CInterruptSystem *pInterrupt,
                                     CDevice *pDevice) :
        CJackTripClient(pLogger, pNet, pDevice),
        CPWMSoundBaseDevice(pInterrupt, SAMPLE_RATE, CHUNK_SIZE * WRITE_CHANNELS),
        m_nMaxLevel(GetRangeMax() - 1),
        m_nZeroLevel(m_nMaxLevel / 2)
{
}

unsigned int JackTripClientPWM::GetChunk(u32 *pBuffer, unsigned int nChunkSize)
{
    auto *b = pBuffer;
    // "Size of the buffer in words" -- numChannels * numFrames
    unsigned nResult = nChunkSize;
    auto sampleMaxValue = m_nMaxLevel; // GetRangeMax() - 1;
    auto sampleZeroValue = m_nZeroLevel; //sampleMaxValue / 2;

    if (m_DebugAudio) {
        if (m_BufferCount % 7 == 0) {
            m_Pulse = !m_Pulse;
        }
        float gain{.5f};
        float amp = gain * sampleMaxValue / 2.f;
        // Get current square wave sample.
        int sample{m_Pulse ? (1 << 15) - 1 : -(1 << 15)};
        // Convert to float [-1, 1)
        float fSample{static_cast<float>(sample) / static_cast<float>(1 << 15)};
        // Scale to u32 range
        int nSample{static_cast<int>(fSample * amp + sampleZeroValue)};
        if (ShouldLog()) {
            CLogger::Get()->Write(FromJTC, LogDebug, "sample = %d (%04x)", sample, sample);
            CLogger::Get()->Write(FromJTC, LogDebug, "fSample = %d / (1 << 15) = %f", sample, fSample);
            CLogger::Get()->Write(FromJTC, LogDebug, "amp = %f * %u / 2 = %f", gain, sampleMaxValue, amp);
            CLogger::Get()->Write(FromJTC, LogDebug, "nSample = %f * %f + %u = %d (%08x)", fSample, amp, sampleZeroValue, nSample, nSample);
        }
        for (; nChunkSize > 0; nChunkSize -= 2) {
            *pBuffer++ = (u32) nSample;
            *pBuffer++ = (u32) nSample;
        }
    } else {
        m_FIFO.Read(pBuffer, nChunkSize / WRITE_CHANNELS, sampleMaxValue, false, ShouldLog());
    }

    if (ShouldLog()) {
        m_Logger.Write(FromJTC, LogDebug, "Output buffer");
        HexDump(reinterpret_cast<u8 *>(b), nResult * sizeof(u32), false);
    }

    ++m_BufferCount;

    return nResult;
}

boolean JackTripClientPWM::Start(void)
{
    return CPWMSoundBaseDevice::Start();
}

boolean JackTripClientPWM::IsActive(void)
{
    return CPWMSoundBaseDevice::IsActive();
}


//// I2S //////////////////////////////////////////////////////////////////////

JackTripClientI2S::JackTripClientI2S(CLogger *pLogger,
                                     CNetSubSystem *pNet,
                                     CInterruptSystem *pInterrupt,
                                     CI2CMaster *pI2CMaster,
                                     CDevice *pDevice) :
        CJackTripClient(pLogger, pNet, pDevice),
        CI2SSoundBaseDevice(pInterrupt, SAMPLE_RATE, CHUNK_SIZE * WRITE_CHANNELS, FALSE, pI2CMaster, DAC_I2C_ADDRESS),
        k_nMinLevel(GetRangeMin() + 1),
        k_nMaxLevel(GetRangeMax() - 1)
{
}

unsigned int JackTripClientI2S::GetChunk(u32 *pBuffer, unsigned int nChunkSize)
{
    auto *b = pBuffer;
    // "Size of the buffer in words" -- numChannels * numFrames
    unsigned nResult = nChunkSize;
    auto sampleMaxValue = k_nMaxLevel;

    if (m_DebugAudio) {
        if (m_BufferCount % 7 == 0) {
            m_Pulse = !m_Pulse;
        }
        float gain{.1f};
        float amp = gain * sampleMaxValue;

        for (; nChunkSize > 0; nChunkSize -= 2) {
            // Get current sine wave sample.
            int sample{static_cast<int>((sin(m_fPhasor) + 1) * (1 << 15))};
            m_fPhasor += MATH_2_PI * m_fF0 / SAMPLE_RATE;
            if (m_fPhasor > MATH_PI) {
                m_fPhasor -= MATH_2_PI;
            }

            // Convert to float [-1, 1)
            float fSample{static_cast<float>(sample) / static_cast<float>(1 << 15)};
            // Scale to u32 range
            int nSample{static_cast<int>(fSample * amp)};

            *pBuffer++ = (u32) nSample;
            *pBuffer++ = (u32) nSample;
        }
//        if (ShouldLog()) {
//            CLogger::Get()->Write(FromJTC, LogDebug, "sample = %d (%04x)", sample, sample);
//            CLogger::Get()->Write(FromJTC, LogDebug, "fSample = %d / (1 << 15) = %f", sample, fSample);
//            CLogger::Get()->Write(FromJTC, LogDebug, "amp = %f * %u = %f", gain, sampleMaxValue, amp);
//            CLogger::Get()->Write(FromJTC, LogDebug, "nSample = %f * %f = %d (%08x)", fSample, amp, nSample, nSample);
//        }
    } else {
        m_FIFO.Read(pBuffer, nChunkSize / WRITE_CHANNELS, sampleMaxValue, true, ShouldLog());
    }

    if (ShouldLog()) {
        m_Logger.Write(FromJTC, LogDebug, "Output buffer");
        HexDump(reinterpret_cast<u8 *>(b), nResult * sizeof(u32), false);
    }

    ++m_BufferCount;

    return nResult;
}

boolean JackTripClientI2S::Start(void)
{
    return CI2SSoundBaseDevice::Start();
}

boolean JackTripClientI2S::IsActive(void)
{
    return CI2SSoundBaseDevice::IsActive();
}
