//
// Created by tar on 30/11/23.
//

#ifndef HELLO_CIRCLE_FIFO_H
#define HELLO_CIRCLE_FIFO_H

#include <circle/types.h>

template<typename T>
class FIFO {
public:
    FIFO(u8 numChannels, u16 length) :
            kNumChannels{numChannels},
            kLength{length},
            buffer{new T *[numChannels]} {
        for (int ch{0}; ch < kNumChannels; ++ch) {
            buffer[ch] = new T[kLength];
        }

        clear();
    }

    ~FIFO() {
        for (int i{0}; i < kNumChannels; ++i) {
            delete buffer[i];
        }
        delete[] buffer;
    }

    void write(const T **dataToWrite, u16 numFrames) {
        for (int n{0}; n < numFrames; ++n, ++writeIndex) {
            if (writeIndex == kLength) {
                writeIndex = 0;
            }

            for (int ch{0}; ch < kNumChannels; ++ch) {
                buffer[ch][writeIndex] = dataToWrite[ch][n];
            }
        }
    }

    /**
     * Read channel-interleaved
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
     * Read sample-interleaved
     * @param bufferToFill
     * @param numFrames
     */
    void read(T *bufferToFill, u16 numFrames) {
        for (u16 frame{0}; frame < numFrames; ++frame, ++readIndex) {
            auto frameStart{frame * kNumChannels};
            for (u8 channel{0}; channel < kNumChannels; ++channel) {
                if (readIndex == kLength) {
                    readIndex = 0;
                }

                bufferToFill[frameStart + channel] = buffer[channel][readIndex];
            }
        }
    }

    void clear() {
        memset(buffer[0], 0, kNumChannels * kLength * sizeof(T));
        writeIndex = 0;
        readIndex = static_cast<u16>(kLength / 2);
    }

private:
    const u8 kNumChannels;
    const u16 kLength;

    T **buffer;
    u16 writeIndex{0}, readIndex{0};
};


#endif //HELLO_CIRCLE_FIFO_H
