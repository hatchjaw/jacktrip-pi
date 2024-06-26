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

#ifndef JACKTRIP_PI_FIFO_H
#define JACKTRIP_PI_FIFO_H

#include <circle/types.h>

static const char FromFIFO[] = "fifo";

template<typename T>
class CFIFO
{
public:
    CFIFO(u8 numChannels, u16 length, int sampleMaxValue = FACTOR) :
            k_nChannels{numChannels},
            k_nLength{length},
            m_pBuffer{new T *[numChannels]}
    {
        for (int ch{0}; ch < k_nChannels; ++ch) {
            m_pBuffer[ch] = new T[k_nLength];
        }
        Clear();
    }

    ~CFIFO()
    {
        for (int i{0}; i < k_nChannels; ++i) {
            delete m_pBuffer[i];
        }
        delete[] m_pBuffer;
    }

    /**
     * Write samples to the fifo. Channel-interleaved, like JackTrip.
     * @param dataToWrite
     * @param numFrames
     */
    void Write(const T **dataToWrite, u16 numFrames)
    {
        auto reset{false};
        m_SpinLock.Acquire();

        for (int n{0}; n < numFrames; ++n) {
            for (int ch{0}; ch < k_nChannels; ++ch) {
                m_pBuffer[ch][m_nWriteIndex] = dataToWrite[ch][n];
            }

            ++m_nWriteIndex;

            if (m_nWriteIndex == m_nReadIndex) {
//                if (m_LogThrottle == 0) {
//                    CLogger::Get()->Write(FromFIFO, LogNotice, "Buffer full (Write); resetting.");
//                    m_LogThrottle = 10000;
//                }
                reset = true;
                Reset(Full);
            }

            if (m_nWriteIndex == k_nLength) {
                m_nWriteIndex = 0;
            }

            if (m_LogThrottle > 0) {
                --m_LogThrottle;
            }
        }

        m_SpinLock.Release();

        if (g_Verbose && reset) {
            CLogger::Get()->Write(FromFIFO, LogNotice, "Buffer full (Write); resetting.");
        }
    }

    /**
     * Read samples into a buffer. Sample-interleaved, like Circle.
     *
     * TODO: extract the sample-conversion logic into a dedicated class, or the sound device.
     *
     * @param bufferToFill The sample-interleaved buffer into which to write samples.
     * @param numFrames The number of frames to write, i.e. for each frame, a number of samples
     * equal to the number of channels in the fifo will be written to the buffer.
     * @param sampleMaxValue Different sound devices and sample rates may have differing bit
     * resolutions; the maximum permitted sample value is required here.
     * @param isI2S
     * @param debug
     */
    void Read(u32 *bufferToFill, u16 numFrames, int sampleMaxValue, bool isI2S, bool debug)
    {
        auto reset{false};
        float amp = AUDIO_VOLUME * sampleMaxValue / (isI2S ? 1.f : 2.f);
        float offset = isI2S ? 0.f : sampleMaxValue / 2.f;

        m_SpinLock.Acquire();

        for (u16 frame{0}; frame < numFrames; ++frame) {
            auto frameStart{frame * k_nChannels};

            for (u8 channel{0}; channel < k_nChannels; ++channel) {
                // Get sample in range [-32768, 32767]
                int sample{m_pBuffer[channel][m_nReadIndex]};
                // Convert to float [-1, 1)
                float fSample{static_cast<float>(sample) / static_cast<float>(1 << 15)};
                // Scale to u32 range
                int nSample{static_cast<int>(fSample * amp + offset)};

                if (debug && frame == 0 && channel == 0) {
                    CLogger::Get()->Write(FromFIFO, LogDebug, "sample = %d (%04x)", sample, sample);
                    CLogger::Get()->Write(FromFIFO, LogDebug, "fSample = %d / (1 << 15) = %f", sample, fSample);
                    CLogger::Get()->Write(FromFIFO, LogDebug, "amp = %f * %u / 2 = %f", AUDIO_VOLUME, sampleMaxValue, amp);
                    if (isI2S) {
                        CLogger::Get()->Write(FromFIFO, LogDebug, "nSample = %f * %f = %d (%08x)", fSample, amp, nSample, nSample);
                    } else {
                        CLogger::Get()->Write(FromFIFO, LogDebug, "nSample = %f * %f + %u = %d (%08x)", fSample, amp, sampleMaxValue / 2, nSample, nSample);
                    }
                }

                bufferToFill[frameStart + channel] = (u32) nSample;
            }

            ++m_nReadIndex;

            if (m_nReadIndex == m_nWriteIndex) {
//                if (m_LogThrottle == 0) {
//                    CLogger::Get()->Write(FromFIFO, LogNotice, "Buffer full (Read); resetting.");
//                    m_LogThrottle = 10000;
//                }
                reset = true;
                Reset(Empty);
            }

            if (m_nReadIndex == k_nLength) {
                m_nReadIndex = 0;
            }

            if (m_LogThrottle > 0) {
                --m_LogThrottle;
            }
        }

        m_SpinLock.Release();

        if (g_Verbose && reset) {
            CLogger::Get()->Write(FromFIFO, LogNotice, "Buffer full (Read); resetting.");
        }
    }

    /**
     * Write zeros to the fifo and reset the write and read indices.
     */
    void Clear()
    {
        m_SpinLock.Acquire();
//        memset(*m_pBuffer, 0, k_nChannels * k_nLength * sizeof(T));
        for (int ch = 0; ch < k_nChannels; ++ch) {
            memset(m_pBuffer[ch], 0, k_nLength * sizeof(T));
        }
        Reset();
        m_SpinLock.Release();

        if (g_Verbose) {
            CLogger::Get()->Write(FromFIFO, LogDebug, "Cleared buffer. Num channels %u, "
                                                      "num frames %u, write index %u, "
                                                      "read index %u",
                                  k_nChannels, k_nLength, m_nWriteIndex, m_nReadIndex);
        }
    }

private:
    enum TFIFOState
    {
        OK,
        Empty,
        Full
    };

    void Reset(TFIFOState state = OK)
    {
        int temp;

        switch (state) {
            case Empty:
                // No new samples left to read, so move the read-index back.
                temp = static_cast<int>(m_nReadIndex) - (k_nLength / 2);
                if (temp < 0) {
                    temp += k_nLength;
                }
                m_nReadIndex = temp;
                break;
            case Full:
                // No space to write new samples, so move the write-index back.
                temp = static_cast<int>(m_nWriteIndex) - (k_nLength / 2);
                if (temp < 0) {
                    temp += k_nLength;
                }
                m_nWriteIndex = temp;
                break;
            default:
                m_nWriteIndex = 0;
                m_nReadIndex = static_cast<u32>(k_nLength / 2);
                break;
        }
    }

    const u8 k_nChannels;
    const u32 k_nLength;

    T **m_pBuffer;
    u32 m_nWriteIndex{0}, m_nReadIndex{0};

    CSpinLock m_SpinLock;
    int m_LogThrottle{0};
};

#endif //JACKTRIP_PI_FIFO_H
