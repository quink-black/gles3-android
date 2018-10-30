#include "image.h"

#include <stdlib.h>

#define TINYEXR_IMPLEMENTATION
#include "tinyexr.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "log.h"

ImageDecoder::ImageDecoder() 
:   mWidth(0), mHeight(0), mData(nullptr) {
}

ImageDecoder::~ImageDecoder() {
    free(mData);
}

void ImageDecoder::Reset() {
    mWidth = 0;
    mHeight = 0;
    free(mData); mData = nullptr;
    mDataType = "";
    mChannel = "";
}

struct OpenEXRImageDecoder : public ImageDecoder {
    int Decode(const char *file, const char *dataType) override;
};

struct HdrImageDecoder : public ImageDecoder {
    int Decode(const char *file, const char *dataType) override;
};

std::shared_ptr<ImageDecoder> ImageDecoder::CreateImageDecoder(const char *fileType) {
    if (!strcasecmp(fileType, "OpenEXR"))
        return std::shared_ptr<ImageDecoder>(new OpenEXRImageDecoder());
    else if (!strcasecmp(fileType, "hdr"))
        return std::shared_ptr<ImageDecoder>(new HdrImageDecoder());
    else
        return nullptr;
}

template<typename T, typename = typename std::enable_if<std::is_integral<T>::value>>
static void convert(void *p, int offset, int type, T *dst) {
    switch(type) {
    case TINYEXR_PIXELTYPE_UINT: {
        int32_t *pix = (int32_t *)p;
        *dst = std::min<int>(pix[offset], std::numeric_limits<T>::max());
        break;
    }
    case TINYEXR_PIXELTYPE_HALF:
    case TINYEXR_PIXELTYPE_FLOAT: {
        float *pix = (float *)p;
        *dst = std::min<int>(pix[offset] * 255, std::numeric_limits<T>::max());
        break;
    }
    default:
        *dst = 0;
        break;
    }
}

static void convert(void *p, int offset, int type, float *dst) {
    switch(type) {
    case TINYEXR_PIXELTYPE_UINT: {
        int32_t *pix = (int32_t *)p;
        *dst = pix[offset];
        break;
    }
    case TINYEXR_PIXELTYPE_HALF:
    case TINYEXR_PIXELTYPE_FLOAT: {
        float *pix = (float *)p;
        *dst = pix[offset];
        break;
    }
    default:
        *dst = 0.0;
        break;
    }
}

int OpenEXRImageDecoder::Decode(const char *file, const char *dataType) {
    Reset();

    mDataType = dataType;
    if (mDataType != "float" &&
            mDataType != "uint16_t" &&
            mDataType != "uint8_t") {
        mDataType = "";
        ALOGD("unsupported data type %s", dataType);
        return -1;
    }

    EXRImage img;
    EXRVersion version;
    EXRHeader header;
    const char *errMsg = nullptr;
    int ret;
    int num_channels;
    int idxR = -1, idxG = -1, idxB = -1;
    size_t pixels;

    InitEXRImage(&img);
    InitEXRHeader(&header);
    ParseEXRVersionFromFile(&version, file);
    if (ParseEXRHeaderFromFile(&header, &version, file, &errMsg)) {
        ALOGE("could not parser exr header: %s", errMsg);
        FreeEXRHeader(&header);
        FreeEXRImage(&img);
        ret = -1;
        goto out;
    }
    for (int i = 0; i < header.num_channels; ++i) {
        if (header.requested_pixel_types[i] == TINYEXR_PIXELTYPE_HALF)
            header.requested_pixel_types[i] = TINYEXR_PIXELTYPE_FLOAT;
    }
    if (LoadEXRImageFromFile(&img, &header, file, &errMsg)) {
        ALOGE("could not open exr file: %s", errMsg);
        ret = -1;
        goto out;
    }

    ALOGD("image width %d, height %d", img.width, img.height);
    mWidth = img.width;
    mHeight = img.height;

    num_channels = header.num_channels;
    if (num_channels > 3)
        num_channels = 3;
    else if (num_channels < 3) {
        ALOGE("not support %d channels", num_channels);
        ret = -1;
        goto out;
    }

    for (int c = 0; c < header.num_channels; ++c) {
        if (strcmp(header.channels[c].name, "R") == 0)
            idxR = c;
        else if (strcmp(header.channels[c].name, "G") == 0)
            idxG = c;
        else if (strcmp(header.channels[c].name, "B") == 0)
            idxB = c;
        else if (strcmp(header.channels[c].name, "A") == 0)
            ;
        else {
            ALOGE("channel name %s", header.channels[c].name);
            ret = -1;
            goto out;
        }
    }
    mChannel = "";
    if (idxR != -1)
        mChannel += "R";
    if (idxG != -1)
        mChannel += "G";
    if (idxB != -1)
        mChannel += "B";

    if (mChannel != "RGB") {
        ALOGE("channel is incomplete: %s", mChannel.c_str());
        ret = -1;
        goto out;
    }

    pixels = mWidth * mHeight * num_channels;
    if (mDataType == "float") {
        mDataF = (float *)malloc(pixels * sizeof(float));
        for (int i = 0; i < mHeight; ++i) {
            for (int j = 0; j < mWidth; ++j) {
                int index = i * mWidth + j;
                convert(img.images[idxR], index, header.pixel_types[idxR], &mDataF[index * 3 + 0]);
                convert(img.images[idxG], index, header.pixel_types[idxG], &mDataF[index * 3 + 1]);
                convert(img.images[idxB], index, header.pixel_types[idxB], &mDataF[index * 3 + 2]);
            }
        }
    } else if (mDataType == "uint16_t") {
        mDataU16 = (uint16_t *)malloc(pixels * sizeof(uint16_t));
        for (int i = 0; i < mHeight; ++i) {
            for (int j = 0; j < mWidth; ++j) {
                int index = i * mWidth + j;
                convert(img.images[idxR], index, header.pixel_types[idxR], &mDataU16[index * 3 + 0]);
                convert(img.images[idxG], index, header.pixel_types[idxG], &mDataU16[index * 3 + 1]);
                convert(img.images[idxB], index, header.pixel_types[idxB], &mDataU16[index * 3 + 2]);
            }
        }
    } else {
        mDataU8 = (uint8_t *)malloc(pixels * sizeof(uint8_t));
        for (int i = 0; i < mHeight; ++i) {
            for (int j = 0; j < mWidth; ++j) {
                int index = i * mWidth + j;
                convert(img.images[idxR], index, header.pixel_types[idxR], &mDataU8[index * 3 + 0]);
                convert(img.images[idxG], index, header.pixel_types[idxG], &mDataU8[index * 3 + 1]);
                convert(img.images[idxB], index, header.pixel_types[idxB], &mDataU8[index * 3 + 2]);
            }
        }
    }
    ret = 0;

out:
    FreeEXRHeader(&header);
    FreeEXRImage(&img);

    return ret;
}

int HdrImageDecoder::Decode(const char *file, const char *dataType) {
    int comp = 0;

    Reset();

    if (!stbi_is_hdr(file)) {
        ALOGD("%s is not HDR?", file);
        return -1;
    }

    float *data = stbi_loadf(file, &mWidth, &mHeight, &comp, 0);
    ALOGD("width %d, height %d, comp %d", mWidth, mHeight, comp);
    if (data == nullptr)
        return -1;
    if (comp != 3) {
        ALOGE("not support channel != 3");
        free(data);
        return -1;
    }

    mDataType = dataType;
    mChannel = "RGB";
    if (mDataType == "float") {
        mDataF = data;
        data = nullptr;
    } else if (mDataType == "uint16_t") {
        mDataU16 = (uint16_t *)malloc(mWidth * mHeight * 3 * sizeof(uint16_t));
        for (int i = 0; i < mWidth * mHeight * 3; ++i) {
            mDataU16[i] = std::min<int>(data[i] * 255, UINT16_MAX);
        }
    } else if (mDataType == "uint8_t") {
        mDataU8 = (uint8_t *)malloc(mWidth * mHeight * 3);
        for (int i = 0; i < mWidth * mHeight * 3; ++i) {
            mDataU8[i] = std::min<int>(data[i] * 255, UINT8_MAX);
        }
    }
    free(data);

    return 0;
}
