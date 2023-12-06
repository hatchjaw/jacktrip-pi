//
// Created by tar on 05/12/23.
//

#ifndef HELLO_CIRCLE_JACKTRIPCLIENT_H
#define HELLO_CIRCLE_JACKTRIPCLIENT_H


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
    explicit JackTripClient(CLogger *pLogger, CNetSubSystem *pNet);

    bool Initialize(void);

    virtual boolean Start(void) = 0;

    virtual boolean IsActive(void) = 0;

    bool Connect();

    void Run(void);

protected:
    void Send();

    void Receive();

    FIFO<TYPE> m_FIFO;
    bool m_Connected{false};
    int m_BufferCount{0};

private:
    boolean IsExitPacket(int size, const u8 *packet) const;

    bool ShouldLog() const;

    CLogger m_Logger;
    CNetSubSystem *m_pNet;
    CSocket *m_pUdpSocket;
    u16 m_ServerUdpPort{0};
    JackTripPacketHeader m_PacketHeader{
            0,
            0,
            QUEUE_SIZE_FRAMES,
            samplingRateT::SR44,
            1 << (BIT16 + 2),
            WRITE_CHANNELS,
            WRITE_CHANNELS
    };
    const u16 k_UdpPacketSize{PACKET_HEADER_SIZE + WRITE_CHANNELS * QUEUE_SIZE_FRAMES * TYPE_SIZE};
    int m_ReceivedCount{0};

    void HexDump(const u8 *buffer, int length, bool doHeader);
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


#endif //HELLO_CIRCLE_JACKTRIPCLIENT_H
