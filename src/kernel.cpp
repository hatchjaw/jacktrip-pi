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

#include "kernel.h"
#include <assert.h>

// Syslog configuration
static const u8 SysLogServer[] = {192, 168, 10, 10};
static const u16 usServerPort = 8514;        // standard port is 514

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
          m_pJTC(nullptr) {
    m_ActLED.Blink(5, 150, 250);    // show we are alive
}

CKernel::~CKernel(void) {
}

boolean CKernel::Initialize(void) {
    bool bOK;

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
            m_pJTC = new JackTripClientI2S(&m_Logger, &m_Net, &m_Interrupt, &m_I2CMaster, &m_Screen);
        }
#if RASPPI >= 4
            else if (strcmp (pSoundDevice, "sndusb") == 0)
        {
            m_JTC = new JackTripClientUSB (&m_Interrupt);
        }
#endif
        else {
            pSoundDevice = "PWM";
            m_pJTC = new JackTripClientPWM(&m_Logger, &m_Net, &m_Interrupt, &m_Screen);
        }

        assert (m_pJTC);

        m_Logger.Write(FromKernel, LogNotice, "Instantiated %s sound device", pSoundDevice);

        bOK = m_pJTC->Initialize();
    }

    return bOK;
}

TShutdownMode CKernel::Run(void) {
    m_Logger.Write(FromKernel, LogNotice, "Compile time: " __DATE__ " " __TIME__);

    if (!m_pJTC->Start()) {
        m_Logger.Write(FromKernel, LogPanic, "Failed to start JackTrip client.");
        return ShutdownHalt;
    } else {
        m_Logger.Write(FromKernel, LogNotice, "Started JackTrip client.");
    }

    while (m_pJTC->IsActive()) {
        m_pJTC->Run();
    }

    m_Logger.Write(FromKernel, LogPanic, "System will halt now.");

    return ShutdownHalt;
}
