//
// Created by tar on 05/12/23.
//

#ifndef JACKTRIP_PI_JACKTRIPCLIENT_H
#define JACKTRIP_PI_JACKTRIPCLIENT_H


#include <circle/sound/pwmsoundbasedevice.h>
#include <circle/sound/i2ssoundbasedevice.h>
#include <circle/net/netsubsystem.h>
#include <circle/net/ipaddress.h>
#include <circle/net/socket.h>
#include <circle/util.h>
#include "config.h"
#include "fifo.h"
#include "PacketHeader.h"

class JackTripClient {
public:
    explicit JackTripClient(CLogger *pLogger, CNetSubSystem *pNet, int sampleMaxValue = (1 << 16) - 1);

    bool Initialize(void);

    virtual boolean Start(void) = 0;

    virtual boolean IsActive(void) = 0;

    bool Connect();

    void Run(void);

protected:
    void Send();

    void Receive();

    bool ShouldLog() const;

    void HexDump(const u8 *buffer, unsigned int length, bool doHeader);

    CLogger m_Logger;
    FIFO<TYPE> m_FIFO;
    bool m_Connected{false};
    int m_BufferCount{0};
    bool amp{false};
private:
    boolean IsExitPacket(int size, const u8 *packet) const;

    void Disconnect();

    CNetSubSystem *m_pNet;
    CSocket *m_pUdpSocket;
    CSocket *m_pTcpSocket;
    u16 m_ServerUdpPort{0};
    JackTripPacketHeader m_PacketHeader{
            0,
            0,
            QUEUE_SIZE_FRAMES,
            samplingRateT::SR44,
            AUDIO_BIT_RES * 8,
            WRITE_CHANNELS,
            WRITE_CHANNELS
    };
    const u16 k_UdpPacketSize{PACKET_HEADER_SIZE + WRITE_CHANNELS * QUEUE_SIZE_FRAMES * TYPE_SIZE};

    int m_ReceivedCount{0};
};

//// PWM //////////////////////////////////////////////////////////////////////

class JackTripClientPWM : public JackTripClient, public CPWMSoundBaseDevice {
public:
    JackTripClientPWM(CLogger *pLogger, CNetSubSystem *pNet, CInterruptSystem *pInterrupt);

    boolean Start(void) override;

    boolean IsActive(void) override;

private:
    unsigned int GetChunk(u32 *pBuffer, unsigned int nChunkSize) override;
};

//// I2S //////////////////////////////////////////////////////////////////////

class JackTripClientI2S : public JackTripClient, public CI2SSoundBaseDevice {
public:
    JackTripClientI2S(CLogger *pLogger, CNetSubSystem *pNet, CInterruptSystem *pInterrupt, CI2CMaster *pI2CMaster);

    boolean Start(void) override;

    boolean IsActive(void) override;

private:
    unsigned int GetChunk(u32 *pBuffer, unsigned int nChunkSize) override;
};


#endif //JACKTRIP_PI_JACKTRIPCLIENT_H
