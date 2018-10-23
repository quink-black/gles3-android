#ifndef ToneMap_H
#define ToneMap_H

class ToneMap {
public:
    static ToneMap *CreateToneMap();
    virtual ~ToneMap() = default;
    virtual int Init(const char *filename) = 0;
    virtual void Draw() = 0;
};

#endif
