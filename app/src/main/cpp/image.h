#ifndef TONEMAP_IMAGE_H
#define TONEMAP_IMAGE_H

#include <stdint.h>
#include <memory>
#include <string>

struct ImageDecoder {
    static std::shared_ptr<ImageDecoder> CreateByType(const char *fileType);
    static std::shared_ptr<ImageDecoder> CreateByName(const char *file);

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
    float mGamma;
    std::string mChannel;
    std::string mDataType;

protected:
    void Reset();
};

#endif
