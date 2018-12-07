#pragma once

#include <memory>
#include "image.h"

namespace quink {

class Render {
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

    static Render *Create(const std::string &name);

    virtual ~Render() = default;

    virtual int Init() = 0;
    virtual int Init(const ImageCoord &coord) = 0;
    virtual int UploadTexture(std::shared_ptr<Image<uint8_t>> img) = 0;
    virtual int UploadTexture(std::shared_ptr<Image<float>> img) = 0;
    virtual int Draw() = 0;
};

}
