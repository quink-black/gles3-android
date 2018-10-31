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

    static Coordinate TopLeft() {
        return {-1.0f, 1.0f};
    }
    static Coordinate TopMiddle() {
        return {0.0f, 1.0f};
    }
    static Coordinate TopRight() {
        return {1.0f, 1.0f};
    }
    static Coordinate MiddleLeft() {
        return {-1.0f, 0.0f};
    }
    static Coordinate Middle() {
        return {0.0f, 0.0f};
    }
    static Coordinate MiddleRight() {
        return {1.0f, 0.0f};
    }
    static Coordinate BottomLeft() {
        return {-1.0f, -1.0f};
    }
    static Coordinate BottomMiddle() {
        return {0.0f, -1.0f};
    }
    static Coordinate BottomRight() {
        return {1.0f, -1.0f};
    }
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
