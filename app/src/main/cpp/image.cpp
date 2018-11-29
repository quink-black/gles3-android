#include "image.h"

#include <stdlib.h>
#include <stdio.h>

#define TINYEXR_IMPLEMENTATION
#include "tinyexr.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "log.h"

ImageDecoder::ImageDecoder()
:   mWidth(0), mHeight(0), mData(nullptr), mGamma(1.0f) {
}

ImageDecoder::~ImageDecoder() {
    free(mData);
}

void ImageDecoder::Reset() {
    mWidth = 0;
    mHeight = 0;
    free(mData); mData = nullptr;
    mGamma = 1.0f;
    mDataType = "";
}

struct OpenEXRImageDecoder : public ImageDecoder {
    int Decode(const std::string &file, const std::string &dataType) override;
};

struct HdrImageDecoder : public ImageDecoder {
    int Decode(const std::string &file, const std::string &dataType) override;
};

struct LdrImageDecoder : public ImageDecoder {
    int Decode(const std::string &file, const std::string &dataType) override;
};

struct PfmImageDecoder : public ImageDecoder {
    int Decode(const std::string &file, const std::string &dataType) override;
};

std::shared_ptr<ImageDecoder> ImageDecoder::Create(ImageType type) {
    switch (type) {
        case ImageType::common_ldr:
            return std::shared_ptr<ImageDecoder>(new LdrImageDecoder());
        case ImageType::openexr:
            return std::shared_ptr<ImageDecoder>(new OpenEXRImageDecoder());
        case ImageType::hdr:
            return std::shared_ptr<ImageDecoder>(new HdrImageDecoder());
        case ImageType::pfm:
            return std::shared_ptr<ImageDecoder>(new PfmImageDecoder());
        default:
            return nullptr;
    }
}

std::shared_ptr<ImageDecoder> ImageDecoder::Create(const std::string &file) {
    std::string filename = file;
    ImageType fileType = ImageType::common_ldr;

    auto suffix_pos = filename.rfind(".");
    if (suffix_pos != std::string::npos) {
        std::string suffix = filename.substr(suffix_pos);
        ALOGD("suffix %s", suffix.c_str());
        std::transform(suffix.begin(), suffix.end(), suffix.begin(), tolower);
        if (suffix == ".exr")
            fileType = ImageType::openexr;
        else if (suffix == ".hdr")
            fileType = ImageType::hdr;
        else if (suffix == ".pfm")
            fileType = ImageType::pfm;
    }

    ALOGD("create %s image decoder", ImageTypeGetName(fileType).c_str());

    return ImageDecoder::Create(fileType);
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

int OpenEXRImageDecoder::Decode(const std::string &file, const std::string &dataType) {
    Reset();

    mDataType = dataType;
    if (mDataType != "float" &&
            mDataType != "uint16_t" &&
            mDataType != "uint8_t") {
        mDataType = "";
        ALOGD("unsupported data type %s", mDataType.c_str());
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
    ParseEXRVersionFromFile(&version, file.c_str());
    if (ParseEXRHeaderFromFile(&header, &version, file.c_str(), &errMsg)) {
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
    if (LoadEXRImageFromFile(&img, &header, file.c_str(), &errMsg)) {
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

    if (idxR == -1 || idxG == -1 || idxB == -1) {
        ALOGE("channel is incomplete");
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

int HdrImageDecoder::Decode(const std::string &file, const std::string &dataType) {
    int comp = 0;

    Reset();

    if (!stbi_is_hdr(file.c_str())) {
        ALOGD("%s is not HDR?", file.c_str());
        return -1;
    }

    float *data = stbi_loadf(file.c_str(), &mWidth, &mHeight, &comp, 0);
    ALOGD("width %d, height %d, comp %d", mWidth, mHeight, comp);
    if (data == nullptr)
        return -1;
    if (comp != 3) {
        ALOGE("not support channel != 3");
        free(data);
        return -1;
    }

    mDataType = dataType;
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

int LdrImageDecoder::Decode(const std::string &file, const std::string &dataType) {
    int comp = 0;

    Reset();

    mDataU8 = stbi_load(file.c_str(), &mWidth, &mHeight, &comp, 3);
    ALOGD("width %d, height %d, comp %d", mWidth, mHeight, comp);
    if (mDataU8 == nullptr)
        return -1;
    if (comp < 3) {
        return -1;
    }

    (void)dataType;
    mDataType = "uint8_t";
    mGamma = 2.2f;

    return 0;
}

int PfmImageDecoder::Decode(const std::string &file, const std::string &dataType) {
    Reset();

    FILE *in = fopen(file.c_str(), "r");
    char buf[2];
    int ret;
    int n;
    float *data = nullptr;
    float f;

    if(fscanf(in, "%2c ", buf) != 1) {
        ret = -1;
        goto out;
    }
    if (buf[0] != 'P' || buf[1] != 'F') {
        ret = -1;
        goto out;
    }
    if (fscanf(in, "%d %d ", &mWidth, &mHeight) != 2) {
        ret = -1;
        goto out;
    }

    if (fscanf(in, "%f ", &f) != 1) {
        ret = -1;
        goto out;
    }

    n = mWidth * mHeight * 3;
    data = (float *)malloc(n * sizeof(float));
    if ((int)fread(data, sizeof(float), n, in) != n) {
        ALOGE("file incomplete?");
        ret = -1;
        goto out;
    }

    if (f > 0) {
        // data is in big endian
        union {
            uint8_t buf[4];
            float f;
        } tmp;
        for (int i = 0; i < n; i++) {
            tmp.f = data[i];
            std::swap(tmp.buf[0], tmp.buf[3]);
            std::swap(tmp.buf[1], tmp.buf[2]);
            data[i] = tmp.f;
        }
    }

    mDataType = dataType;
    if (mDataType == "float") {
        mDataF = data;
        data = nullptr;
    } else if (mDataType == "uint16_t") {
        mDataU16 = (uint16_t *)malloc(n * sizeof(uint16_t));
        for (int i = 0; i < mWidth * mHeight * 3; ++i) {
            mDataU16[i] = std::min<int>(data[i] * 255, UINT16_MAX);
        }
    } else if (mDataType == "uint8_t") {
        mDataU8 = (uint8_t *)malloc(n);
        for (int i = 0; i < mWidth * mHeight * 3; ++i) {
            mDataU8[i] = std::min<int>(data[i] * 255, UINT8_MAX);
        }
    }
    ret = 0;
    ALOGD("decoder %s success", file.c_str());

out:
    fclose(in);
    free(data);
    return ret;
}
