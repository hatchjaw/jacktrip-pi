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
        m_FIFO{WRITE_CHANNELS, AUDIO_BLOCK_FRAMES * 16},
        m_pNet(pNet),
        m_pUdpSocket(pNet, IPPROTO_UDP),
//        m_pSendTask(&m_pUdpSocket, &m_Event, &m_Connected)
        m_pSendTask(nullptr)
{
}

CJackTripClient::~CJackTripClient()
{
//    delete m_pUdpSocket;
//    delete m_pSendTask;
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
    if (PORT_NUMBER_NUM_BYTES != tcpSocket->Send(
            reinterpret_cast<const u8 *>(&udpPort),
            PORT_NUMBER_NUM_BYTES,
            MSG_DONTWAIT
    )) {
        m_Logger.Write(FromJTC, LogError, "Failed to send UDP port to server.");
        Disconnect();
        return false;
    } else {
        m_Logger.Write(FromJTC, LogNotice, "Sent UDP port number %u to JackTrip server.", udpPort);
    }

    // Read the JackTrip server's UDP port; block until received.
    if (PORT_NUMBER_NUM_BYTES != tcpSocket->Receive(
            reinterpret_cast<u8 *>(&m_nServerUdpPort),
            PORT_NUMBER_NUM_BYTES,
            0
    )) {
        m_Logger.Write(FromJTC, LogError, "Failed to read UDP port from server.");
        Disconnect();
        return false;
    } else {
        m_Logger.Write(FromJTC, LogNotice, "Received port %u from JackTrip server.", m_nServerUdpPort);
    }

//    m_pUdpSocket = new CSocket(m_pNet, IPPROTO_UDP);
//    assert(m_pUdpSocket);

    // Free up the socket for re-binding.
    m_pUdpSocket = CSocket(m_pNet, IPPROTO_UDP);

    // Set up the UDP socket.
    if (m_pUdpSocket.Bind(udpPort) < 0) {
        m_Logger.Write(FromJTC, LogError, "Failed to bind UDP socket to port %u.", udpPort);
        Disconnect();
        return false;
    } else {
        m_Logger.Write(FromJTC, LogNotice, "UDP Socket successfully bound to port %u", udpPort);
    }

    if (m_pUdpSocket.Connect(serverIP, m_nServerUdpPort) < 0) {
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
    if (!m_Connected)
        return;

    m_Logger.Write(FromJTC, LogDebug, "Disconnecting");

    m_Connected = false;

    assert(m_pSendTask);

    // The send task will be waiting. Signal it, it'll find that
    // disconnection has occurred, and it'll terminate.
    m_Event.Set();
    m_Logger.Write(FromJTC, LogDebug, "Waiting for SendTask to terminate.");
    m_pSendTask->WaitForTermination();
    m_Logger.Write(FromJTC, LogDebug, "Terminated.");
    // DON'T DELETE THE TASK; THE SCHEDULER DOES THIS.
//        delete m_pSendTask;
    m_pSendTask = nullptr;

    m_Logger.Write(FromJTC, LogDebug, "Resetting fifo and counters.");
    m_BufferCount = 0;
    m_nPacketsReceived = 0;
    m_PacketHeader.nSeqNumber = 0;
    m_FIFO.Clear();
}

void CJackTripClient::Run()
{
    if (!m_Connected) {
        if (Connect()) {
            assert(!m_pSendTask);
            m_pSendTask = new CSendTask(&m_pUdpSocket, &m_Event, &m_Connected);
            m_Logger.Write(FromJTC, LogNotice, "Starting task %s.", m_pSendTask->GetName());
            m_nLastReceive = CTimer::Get()->GetUptime();
        } else {
            CScheduler::Get()->Sleep(2);
        }
    } else {
        Receive();
    }

    // Give the send task time to work.
    CScheduler::Get()->Yield();
}

void CJackTripClient::Receive()
{
    assert(m_Connected);

    u8 buffer8[UDP_PACKET_SIZE];

    // TODO: check how long since last receive; disconnect if threshold exceeded
    // TODO: probably need a spinlock here and one around Send
    int nBytesReceived{m_pUdpSocket.Receive(buffer8, sizeof buffer8, MSG_DONTWAIT)};//m_ReceivedCount == 0 ? MSG_DONTWAIT : 0)};

    if (nBytesReceived > 0) {
        if (IsExitPacket(nBytesReceived, buffer8)) {
            m_Logger.Write(FromJTC, LogNotice, "Exit packet received.");
            Disconnect();
            CScheduler::Get()->Sleep(2);
            return;
        } else if (nBytesReceived != UDP_PACKET_SIZE) {
            m_Logger.Write(FromJTC,
                           LogWarning,
                           "Malformed packet received. Expected %u bytes; received %d bytes.",
                           UDP_PACKET_SIZE,
                           nBytesReceived);
        } else {
            const TYPE *buffer[WRITE_CHANNELS];
            for (int ch = 0; ch < WRITE_CHANNELS; ++ch) {
                buffer[ch] = reinterpret_cast<TYPE *>(buffer8 + PACKET_HEADER_SIZE + CHANNEL_QUEUE_SIZE * ch);
            }

            m_FIFO.Write(buffer, AUDIO_BLOCK_FRAMES);

            ++m_nPacketsReceived;
            m_nLastReceive = CTimer::Get()->GetUptime();

            // Notify the send task to send a packet.
            m_Event.Set();

            if (ShouldLog()) {
                m_Logger.Write(FromJTC, LogDebug, "Received %d bytes via UDP", nBytesReceived);
                HexDump(buffer8, nBytesReceived, true);
            }
        }
    } else if (CTimer::Get()->GetUptime() - m_nLastReceive > RECEIVE_TIMEOUT_SEC) {
        m_Logger.Write(FromJTC, LogNotice, "Nothing received for %u seconds. Disconnecting.", RECEIVE_TIMEOUT_SEC);
        Disconnect();
        CScheduler::Get()->Sleep(2);
        return;
    }
}

bool CJackTripClient::IsExitPacket(int size, const u8 *packet)
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

static const char FromJTCSend[] = "jtcsend";

CJackTripClient::CSendTask::CSendTask(CSocket *pUdpSocket, CSynchronizationEvent *pEvent, bool *pConnected) :
//        CTask(TASK_STACK_SIZE, true),
        m_pUdpSocket(pUdpSocket),
        m_pEvent(pEvent),
        m_pConnected(*pConnected)
{
    SetName(FromJTCSend);
    CLogger::Get()->Write(FromJTCSend, LogDebug, "Constructing task jtcsend. Task is %s.", IsSuspended() ? "suspended" : "running");
}

CJackTripClient::CSendTask::~CSendTask(void)
{
//    CLogger::Get()->Write(FromJTCSend, LogDebug, "Destructing task %s.", GetName());
}

void CJackTripClient::CSendTask::Run(void)
{
    CLogger::Get()->Write(FromJTCSend, LogNotice, "Running task %s.", GetName());

    assert(m_pUdpSocket);

    u8 packet[UDP_PACKET_SIZE];
    memcpy(packet, &m_PacketHeader, PACKET_HEADER_SIZE);
    // The JackTrip server checks whether a datagram is available, and, if not,
    // sleeps for 100 ms and tries again. This process repeats until a global
    // timeout is exceeded, at which point it gives up. Just delaying before the
    // first send from the client doesn't appear to work; giving JackTrip a
    // moment to start listening for packets, sending once, then waiting a
    // little while does. Spamming the connection with an arbitrar number of
    // packets is an option, but results in a lot of ICMP "Destination
    // unreachable (Port unreachable)" warnings.
    CScheduler::Get()->MsSleep(100);
    // Send the zeroth packet.
    m_pUdpSocket->Send(packet, UDP_PACKET_SIZE, MSG_DONTWAIT);
    CScheduler::Get()->MsSleep(25);

    CLogger::Get()->Write(FromJTCSend, LogNotice, "Sending datagrams.");

    while (m_pConnected) {
        assert(m_pUdpSocket);

        ++m_PacketHeader.nSeqNumber;
        memcpy(packet, &m_PacketHeader, PACKET_HEADER_SIZE);

        m_pUdpSocket->Send(packet, UDP_PACKET_SIZE, MSG_DONTWAIT);

        m_pEvent->Clear();
        // Wait for a signal from the main (receive) task.
        m_pEvent->Wait();
    }

    CLogger::Get()->Write(FromJTCSend, LogDebug, "Disconnected; leaving CSendTask::Run");
}


//// PWM //////////////////////////////////////////////////////////////////////

JackTripClientPWM::JackTripClientPWM(CLogger *pLogger,
                                     CNetSubSystem *pNet,
                                     CInterruptSystem *pInterrupt,
                                     CDevice *pDevice) :
        CJackTripClient(pLogger, pNet, pDevice),
        CPWMSoundBaseDevice(pInterrupt, SAMPLE_RATE, AUDIO_BLOCK_FRAMES * WRITE_CHANNELS),
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
        CI2SSoundBaseDevice(pInterrupt, SAMPLE_RATE, AUDIO_BLOCK_FRAMES * WRITE_CHANNELS, FALSE, pI2CMaster, DAC_I2C_ADDRESS),
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
