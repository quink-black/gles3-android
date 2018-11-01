#ifndef ToneMap_H
#define ToneMap_H

#include <memory>
#include "image.h"

class ToneMap {
public:
    struct Coordinate {
        float mX;
        float mY;
    };

    struct ImageCoord {
        Coordinate mTopLeft;
        Coordinate mBottomLeft;
        Coordinate mBottomRight;
        Coordinate mTopRight;
    };

    /* algorithm = "Hable", "" */
    static ToneMap *CreateToneMap(const std::string &algorithm);

    virtual ~ToneMap() = default;

    virtual int Init() = 0;
    virtual int Init(const ImageCoord &coord) = 0;
    virtual int UploadTexture(std::shared_ptr<ImageDecoder> img) = 0;
    virtual int Draw() = 0;
};

#endif
