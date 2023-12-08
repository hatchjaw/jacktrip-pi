//
// Created by tar on 30/11/23.
//

#ifndef JACKTRIP_PI_FIFO_H
#define JACKTRIP_PI_FIFO_H

#include <circle/types.h>

template<typename T>
class FIFO {
public:
    FIFO(u8 numChannels, u16 length, int sampleMaxValue = (1 << 15) - 1) :
            kNumChannels{numChannels},
            kLength{length},
            buffer{new T *[numChannels]} {
        for (int ch{0}; ch < kNumChannels; ++ch) {
            buffer[ch] = new T[kLength];
        }
        Clear();
    }

    ~FIFO() {
        for (int i{0}; i < kNumChannels; ++i) {
            delete buffer[i];
        }
        delete[] buffer;
    }

    /**
     * Write
     * @param dataToWrite
     * @param numFrames
     */
    void Write(const T **dataToWrite, u16 numFrames) {
        spinLock.Acquire();
        for (int n{0}; n < numFrames; ++n, ++writeIndex) {
            if (writeIndex == kLength) {
                writeIndex = 0;
            }

            for (int ch{0}; ch < kNumChannels; ++ch) {
                buffer[ch][writeIndex] = dataToWrite[ch][n];
            }
        }
        spinLock.Release();
    }

    /**
     * Read channel-interleaved samples from the fifo.
     * @param bufferToFill
     * @param numFrames
     */
//    void read(T **bufferToFill, u16 numFrames) {
//        for (u16 n{0}; n < numFrames; ++n, ++readIndex) {
//            if (readIndex == kLength) {
//                readIndex = 0;
//            }
//
//            for (int ch{0}; ch < kNumChannels; ++ch) {
//                bufferToFill[ch][n] = buffer[ch][readIndex];
//            }
//        }
//    }

    /**
     * Read frame-interleaved samples from the fifo.
     * @param bufferToFill
     * @param numFrames
     */
//    void Read(T *bufferToFill, u16 numFrames) {
////        spinLock.Acquire();
//        for (u16 frame{0}; frame < numFrames; ++frame, ++readIndex) {
//            if (readIndex == kLength) {
//                readIndex = 0;
//            }
//
//            auto frameStart{frame * kNumChannels};
//
//            for (u8 channel{0}; channel < kNumChannels; ++channel) {
//                bufferToFill[frameStart + channel] = buffer[channel][readIndex];
//            }
//        }
////        spinLock.Release();
//    }

    void Read(u32 *bufferToFill, u16 numFrames, int sampleMaxValue, bool isI2S, bool debug) {
        float gain{.5f};
        float amp = gain * sampleMaxValue / (isI2S ? 1.f : 2.f);

        spinLock.Acquire();

        for (u16 frame{0}; frame < numFrames; ++frame, ++readIndex) {
            if (readIndex == kLength) {
                readIndex = 0;
            }

            auto frameStart{frame * kNumChannels};

            for (u8 channel{0}; channel < kNumChannels; ++channel) {
                // Get sample in range [-32768, 32767]
                int sample{buffer[channel][readIndex]};
                // Convert to float [-1, 1)
                float fSample{static_cast<float>(sample) / static_cast<float>(1 << 15)};
                // Scale to u32 range
                int nSample{static_cast<int>(fSample * amp + (isI2S ? 0 : sampleMaxValue / 2))};
                if (debug && frame == 0 && channel == 0) {
                    CLogger::Get()->Write("fifo", LogDebug, "sample = %d (%04x)", sample, sample);
                    CLogger::Get()->Write("fifo", LogDebug, "fSample = %d / (1 << 15) = %f", sample, fSample);
                    CLogger::Get()->Write("fifo", LogDebug, "amp = %f * %u / 2 = %f", gain, sampleMaxValue, amp);
                    if (isI2S) {
                        CLogger::Get()->Write("fifo", LogDebug, "nSample = %f * %f = %d (%08x)", fSample, amp, nSample, nSample);
                    } else {
                        CLogger::Get()->Write("fifo", LogDebug, "nSample = %f * %f + %u = %d (%08x)", fSample, amp, sampleMaxValue / 2, nSample, nSample);
                    }
                }

//                bufferToFill[frameStart + channel] = (u32) buffer[channel][readIndex];
                bufferToFill[frameStart + channel] = (u32) nSample;
            }
        }
        spinLock.Release();
    }

    /**
     * Write zeros to the fifo and reset the write and read indices.
     */
    void Clear() {
        memset(buffer[0], 0, kNumChannels * kLength * sizeof(T));
        writeIndex = 0;
        readIndex = static_cast<u16>(kLength / 2);

        CLogger::Get()->Write("fifo", LogDebug, "Cleared fifo. Num channels %u, "
                                                "num frames %u, write index %u, "
                                                "read index %u",
                              kNumChannels, kLength, writeIndex, readIndex);
    }

private:
    const u8 kNumChannels;
    const u16 kLength;

    T **buffer;
    u16 writeIndex{0}, readIndex{0};

    CSpinLock spinLock;
};


#endif //JACKTRIP_PI_FIFO_H
