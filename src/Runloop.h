#pragma once

#include <iostream>

#include <opencv2/opencv.hpp>

#include "Options.h"
#include "Capture.h"
#include "FPS.h"
#include "Eyes.h"
#include "Hands.h"
#include "Brain.h"
#include "DebugOverlay.h"

class Runloop
{
public:
    Runloop(int argc, char* argv[]) : // throws std::exception
        m_options   {argc, argv},
        m_brain     {m_eyes, m_hands}
    {}

    void Run();
private:
    ::Options m_options;
    ::Capture m_capture;
    ::FPS<100> m_fps;
    ::Eyes m_eyes;
    ::Hands m_hands;
    ::Brain m_brain;
    ::DebugOverlay m_overlay;

    void DrawWorldInfo(cv::Mat &image) const;
    void ShowDebugInfo(cv::Mat &image);
    void DrawOverlay();
    void ConfigureEyes();
    void ConfigureHands();
    void ConfigureBrain();
};
