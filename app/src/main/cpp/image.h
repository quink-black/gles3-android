#ifndef TONEMAP_IMAGE_H
#define TONEMAP_IMAGE_H

#include <stdint.h>
#include <string>

struct ImageDecoder {
    static ImageDecoder *CreateImageDecoder(const char *fileType);

    ImageDecoder();
    virtual ~ImageDecoder();

    virtual int Decode(const char *file, const char *dataType) = 0;

    int mWidth;
    int mHeight;
    union {
        void *mData;
        float *mDataF;
        uint16_t *mDataU16;
        uint8_t *mDataU8;
    };
    std::string mChannel;
    std::string mDataType;
};

#endif