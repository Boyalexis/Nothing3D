#pragma once
#include <QImage>
#include <algorithm>

// GPU readback checks: reject a blank clear, a missing vertex or swapped colors.
inline bool hasTriangle(const QImage& image)
{
    if (image.width() < 100 || image.height() < 100) return false;
    const int side = std::min(image.width(), image.height());
    auto sample = [&](float x, float y) {
        return image.pixelColor(int(float(image.width())/2 + x*float(side)/2),
                                int(float(image.height())/2 + y*float(side)/2));
    };
    const auto top = sample(0, -0.45f);
    const auto left = sample(-0.4f, 0.35f);
    const auto right = sample(0.4f, 0.35f);
    const auto center = sample(0, 0);
    const auto background = image.pixelColor(5, 5);
    return top.red() > top.green()+60 && top.red() > top.blue()+60
        && left.blue() > left.red()+60 && left.blue() > left.green()+60
        && right.green() > right.red()+60 && right.green() > right.blue()+60
        && center.red() > 30 && center.green() > 30 && center.blue() > 30
        && background.red() < 40 && background.green() < 50 && background.blue() < 70;
}
