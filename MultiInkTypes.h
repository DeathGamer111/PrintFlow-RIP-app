#pragma once
#include <cstdint>

struct MultiInkDotStrategy {
    int     minInkThreshold   = 8;
    int     smallDotThreshold = 104;
    int     medDotThreshold   = 168;

    bool    enablePromotion   = false;

    uint8_t floorRangeCMY     = 24;
    uint8_t floorMaxCMY       = 2;
    uint8_t floorRangeK       = 12;
    uint8_t floorMaxK         = 0;

    bool    enableDotSwap     = false;
};
