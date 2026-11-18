/***************************************************************************
 *            SPRE22_plot_replay.cpp
 *
 *  Copyright 2026 Luca Geretti
 *
 *  This file is part of Ariadne and is distributed under GPL-3.0-or-later.
 ***************************************************************************/

#include "ariadne.hpp"
#include "drawing/drawing_command_line_interface.hpp"
#include "spre22_plot_data.generated.hpp"

int main(int argc, const char* argv[]) {
    if (not Ariadne::DrawingCommandLineInterface().acquire(argc, argv)) return -1;

    using namespace Ariadne;
    const RealVariable x("x"), y("y");
    const SPRE22CapturedReach reach;
    plot("SPRE22_replay", {-1000<=x<=200, -450<=y<=0},
         Colour(1.0,0.75,0.5), reach);
    return 0;
}
