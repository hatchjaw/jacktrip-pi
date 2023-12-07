//
// Created by tar on 05/12/23.
//

#include "JackTripClient.h"
#include <circle/sched/scheduler.h>
#include <circle/net/in.h>

static const char FromJTC[] = "jtclient";

JackTripClient::JackTripClient(CLogger *pLogger, CNetSubSystem *pNet, int sampleMaxValue) :
        m_Logger(*pLogger),
        m_FIFO{WRITE_CHANNELS, QUEUE_SIZE_FRAMES * 16, sampleMaxValue},
        m_pNet(pNet),
        m_pUdpSocket(nullptr) {
}

bool JackTripClient::Initialize(void) {
    return true;
}

bool JackTripClient::Connect(void) {
    const u8 ip[] = {SERVER_IP};
    CIPAddress serverIP{ip};
    CString ipString;
    serverIP.Format(&ipString);
    const u16 udpPort = UDP_PORT;

    // Pseudorandomise the client TCP port to minimise the likelihood of port number re-use.
    auto tcpClientPort = TCP_CLIENT_BASE_PORT + (CTimer::GetClockTicks() % TCP_PORT_RANGE);
    m_pTcpSocket = new CSocket(m_pNet, IPPROTO_TCP);
    assert(m_pTcpSocket);

    m_Logger.Write(FromJTC, LogNotice, "Looking for a JackTrip server at %s...", (const char *) ipString);

    // Bind the TCP port.
    if (m_pTcpSocket->Bind(tcpClientPort) < 0) {
        m_Logger.Write(FromJTC, LogError, "Cannot bind TCP socket (port %u)", tcpClientPort);
        Disconnect();
        return false;
    } else {
        m_Logger.Write(FromJTC, LogNotice, "Successfully bound TCP socket (port %u)", tcpClientPort);
    }

    if (m_pTcpSocket->Connect(serverIP, TCP_SERVER_PORT) < 0) {
        m_Logger.Write(FromJTC, LogWarning, "Cannot establish TCP connection to JackTrip server.");
        Disconnect();
        return false;
    } else {
        m_Logger.Write(FromJTC, LogNotice, "TCP connection with server accepted.");
    }

    // Send the UDP port to the JackTrip server; block until sent.
    if (4 != m_pTcpSocket->Send(reinterpret_cast<const u8 *>(&udpPort), 4, MSG_DONTWAIT)) {
        m_Logger.Write(FromJTC, LogError, "Failed to send UDP port to server.");
        Disconnect();
        return false;
    } else {
        m_Logger.Write(FromJTC, LogNotice, "Sent UDP port number %u to JackTrip server.", udpPort);
    }

    // Read the JackTrip server's UDP port; block until received.
    if (4 != m_pTcpSocket->Receive(reinterpret_cast<u8 *>(&m_ServerUdpPort), 4, 0)) {
        m_Logger.Write(FromJTC, LogError, "Failed to read UDP port from server.");
        Disconnect();
        return false;
    } else {
        m_Logger.Write(FromJTC, LogNotice, "Received port %u from JackTrip server.", m_ServerUdpPort);
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

    if (m_pUdpSocket->Connect(serverIP, m_ServerUdpPort) < 0) {
        m_Logger.Write(FromJTC, LogError, "Failed to prepare UDP connection.");
        Disconnect();
        return false;
    } else {
        m_Logger.Write(FromJTC, LogNotice, "UDP connection prepared with %s:%u",
                       (const char *) ipString, (unsigned) m_ServerUdpPort);
    }

    m_Logger.Write(FromJTC, LogNotice, "Ready!");
    m_Connected = true;

    return true;
}

void JackTripClient::Disconnect() {
    if (m_pTcpSocket != nullptr) {
        delete m_pTcpSocket;
        m_pTcpSocket = nullptr;
    }
    if (m_pUdpSocket != nullptr) {
        delete m_pUdpSocket;
        m_pUdpSocket = nullptr;
    }
    m_Connected = false;
    m_BufferCount = 0;
    m_ReceivedCount = 0;
    m_PacketHeader.SeqNumber = 0;
    m_FIFO.Clear();
}

void JackTripClient::Run(void) {
    if (!m_Connected) {
        m_ReceivedCount = 0;
        CScheduler::Get()->Sleep(2);
        Connect();
    }

    Send();
    Receive();
    CScheduler::Get()->usSleep(QUEUE_SIZE_US * .9);
}

void JackTripClient::Send() {
    if (!m_Connected) return;

    assert(m_pUdpSocket);

    u8 packet[k_UdpPacketSize];
    ++m_PacketHeader.SeqNumber;

    memcpy(packet, &m_PacketHeader, PACKET_HEADER_SIZE);
//    audioBuffer.read(reinterpret_cast<TYPE *>(packet + PACKET_HEADER_SIZE), QUEUE_SIZE_FRAMES);

//    mLogger.Write(FromKernel, LogNotice, "Sending packet %u to %s:%d", packetHeader.SeqNumber, (const char *) ipString, mServerUdpPort);

    auto nBytesSent = m_pUdpSocket->Send(packet, k_UdpPacketSize, MSG_DONTWAIT);

    if (nBytesSent != k_UdpPacketSize) {
        m_Logger.Write(FromJTC, LogWarning, "Sent %d bytes", nBytesSent);
    }
}

void JackTripClient::Receive() {
    if (!m_Connected) return;

    assert(m_pUdpSocket);

    u8 buffer8[k_UdpPacketSize];

    // TODO: check how long since last receive; disconnect if threshold exceeded
    int nBytesReceived{m_pUdpSocket->Receive(buffer8, sizeof buffer8, MSG_DONTWAIT)};//m_ReceivedCount == 0 ? MSG_DONTWAIT : 0)};

    if (nBytesReceived > 0) {
        ++m_ReceivedCount;
    }

    if (IsExitPacket(nBytesReceived, buffer8)) {
        m_Logger.Write(FromJTC, LogNotice, "Exit packet received.");
        Disconnect();
        return;
    } else if (nBytesReceived > 0 && nBytesReceived != k_UdpPacketSize) {
        m_Logger.Write(FromJTC, LogWarning, "Expected %u bytes; received %d bytes", k_UdpPacketSize, nBytesReceived);
    }

    const TYPE *buffer[WRITE_CHANNELS];
    for (int ch = 0; ch < WRITE_CHANNELS; ++ch) {
        buffer[ch] = reinterpret_cast<TYPE *>(buffer8 + PACKET_HEADER_SIZE + CHANNEL_QUEUE_SIZE * ch);
    }

//    b = reinterpret_cast<TYPE *>(localBuf + PACKET_HEADER_SIZE);

//    auto nFrames{((nBytesReceived - PACKET_HEADER_SIZE) / TYPE_SIZE) / WRITE_CHANNELS};
//    auto nFrames{((nBytesReceived) / TYPE_SIZE) / WRITE_CHANNELS};

    // Should be writing CHUNK_SIZE frames from the buffer to the fifo...
    m_FIFO.Write(buffer, CHUNK_SIZE);

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

    ++m_ReceivedCount;
}

bool JackTripClient::IsExitPacket(int size, const u8 *packet) const {
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

bool JackTripClient::ShouldLog() const { return m_BufferCount > 0 && m_BufferCount % 10000 == 0; }

void JackTripClient::HexDump(const u8 *buffer, unsigned int length, bool doHeader) {
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

//// PWM //////////////////////////////////////////////////////////////////////

JackTripClientPWM::JackTripClientPWM(CLogger *pLogger, CNetSubSystem *pNet, CInterruptSystem *pInterrupt) :
        JackTripClient(pLogger, pNet, GetRangeMax() - 1),
        CPWMSoundBaseDevice(pInterrupt, SAMPLE_RATE, CHUNK_SIZE * WRITE_CHANNELS) {
//    CPWMSoundBaseDevice::SetWriteFormat(TSoundFormat::SoundFormatSigned16, WRITE_CHANNELS);
}

unsigned int JackTripClientPWM::GetChunk(u32 *pBuffer, unsigned int nChunkSize) {
    unsigned nResult = nChunkSize;

    m_FIFO.Read(pBuffer, nChunkSize, ShouldLog(), amp ? (1 << 15) - 1 : -(1 << 15));

    if (ShouldLog()) {
        m_Logger.Write(FromJTC, LogDebug, "Output buffer");
        HexDump(reinterpret_cast<u8 *>(pBuffer), nChunkSize * sizeof(u32), false);
    }

    ++m_BufferCount;
    if (m_BufferCount % 4 == 0) {
        amp = !amp;
    }

    return nResult;
}

boolean JackTripClientPWM::Start(void) {
    return CPWMSoundBaseDevice::Start();
}

boolean JackTripClientPWM::IsActive(void) {
    return CPWMSoundBaseDevice::IsActive();
}


//// I2S //////////////////////////////////////////////////////////////////////

JackTripClientI2S::JackTripClientI2S(CLogger *pLogger, CNetSubSystem *pNet, CInterruptSystem *pInterrupt, CI2CMaster *pI2CMaster) :
        JackTripClient(pLogger, pNet, GetRangeMax() - 1),
        CI2SSoundBaseDevice(pInterrupt, SAMPLE_RATE, CHUNK_SIZE * WRITE_CHANNELS, FALSE, pI2CMaster, DAC_I2C_ADDRESS) {
//    CI2SSoundBaseDevice::SetWriteFormat(TSoundFormat::SoundFormatSigned16, WRITE_CHANNELS);
}

unsigned int JackTripClientI2S::GetChunk(u32 *pBuffer, unsigned int nChunkSize) {
    unsigned nResult = nChunkSize;

    m_FIFO.Read(pBuffer, nChunkSize, ShouldLog(), amp ? (1 << 15) - 1 : -(1 << 15));

    if (ShouldLog()) {
        m_Logger.Write(FromJTC, LogDebug, "Output buffer");
        HexDump(reinterpret_cast<u8 *>(pBuffer), nChunkSize * sizeof(u32), false);
    }

    ++m_BufferCount;
    if (m_BufferCount % 44 == 0) {
        amp = !amp;
    }

    return nResult;
}

boolean JackTripClientI2S::Start(void) {
    return CI2SSoundBaseDevice::Start();
}

boolean JackTripClientI2S::IsActive(void) {
    return CI2SSoundBaseDevice::IsActive();
}
