/***************************************************************************
 *            SPRE22_plot_replay.cpp
 *
 *  Copyright 2026 Luca Geretti
 *
 *  Replays SPRE22 validated reach drawing inputs from a binary snapshot.
 ***************************************************************************/

#include "ariadne.hpp"
#include "drawing/drawing_command_line_interface.hpp"
#include "utility/stopwatch.hpp"
#include "spre22_plot_capture.hpp"

int main(int argc, const char* argv[]) {
    if (not Ariadne::DrawingCommandLineInterface().acquire(argc, argv)) return -1;
    using namespace Ariadne;
    const RealVariable x("x"), y("y");
    const SPRE22Capture::CapturedReach reach(SPRE22_CAPTURE_FILE_PATH);
    LOGGING_PRINTLN("Plotting...");
    Stopwatch<Milliseconds> plotting_sw;
    plot("SPRE22_replay", {-1000<=x<=200, -450<=y<=0},
         Colour(1.0,0.75,0.5), reach);
    plotting_sw.click();
    LOGGING_PRINTLN("File SPRE22_replay.png written.");
    LOGGING_PRINTLN("Plotting done in " << plotting_sw.elapsed_seconds() << " seconds.");
    return 0;
}
