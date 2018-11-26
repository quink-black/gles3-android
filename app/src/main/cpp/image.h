#ifndef TONEMAP_IMAGE_H
#define TONEMAP_IMAGE_H

#include <stdint.h>
#include <memory>
#include <string>

enum class ImageType {
    common_ldr,
    openexr,
    hdr,
    pfm,
};

static inline std::string ImageTypeGetName(ImageType type) {
    switch (type) {
        case ImageType::common_ldr:
            return "common ldr";
        case ImageType::openexr:
            return "OpenEXR";
        case ImageType::hdr:
            return "HDR/RGBE";
        case ImageType::pfm:
            return "PFM";
        default:
            return "unknown";
    }
}

struct ImageDecoder {
    static std::shared_ptr<ImageDecoder> Create(ImageType type);
    static std::shared_ptr<ImageDecoder> Create(const std::string &file);

    ImageDecoder();
    virtual ~ImageDecoder();

    virtual int Decode(const std::string &file, const char *dataType) = 0;

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
