#ifndef ToneMap_H
#define ToneMap_H

#include <memory>
#include "image.h"

class ToneMap {
public:
    static ToneMap *CreateToneMap();

    virtual ~ToneMap() = default;

    virtual int Init() = 0;
    virtual int UpLoadTexture(std::shared_ptr<ImageDecoder> img) = 0;
    virtual int Draw() = 0;
};

#endif
