/***************************************************************************
 *            SPRE22_plot_capture_smoke.cpp
 *
 *  Copyright 2026 Luca Geretti
 *
 *  Small synthetic fixture to compile generated replay code without evolution.
 ***************************************************************************/

#include "spre22_plot_capture.hpp"

int main(int argc, const char* argv[]) {
    if (argc != 2) return 2;
    using namespace Ariadne;
    const RealVariable x("x"), y("y");
    const EnclosureConfiguration configuration(
        TaylorFunctionFactory(ThresholdSweeper<FloatDP>(dp, 1e-8)));
    HybridEnclosure enclosure(
        HybridExactBox(DiscreteLocation(1), RealSpace({x, y}),
                       ExactBoxType({{-1, 1}, {-2, 2}})),
        configuration);
    const auto coordinates = EffectiveVectorMultivariateFunction::identity(2u);
    const DiscreteEvent invariant("invariant");
    const DiscreteEvent activation("activation");
    const DiscreteEvent bounded("bounded");
    enclosure.new_invariant(invariant, coordinates[0]);
    enclosure.new_activation(activation, coordinates[1]);
    enclosure.new_state_constraint(bounded, ValidatedConstraint(-1, coordinates[0], 1));
    const ListSet<HybridEnclosure> reach(enclosure);
    SPRE22Capture::write_binary(argv[1], reach, Variables2d(x, y));
    std::ifstream in(argv[1], std::ios::binary);
    if (!in) return 3;
    const auto header = SPRE22Capture::read_header(in);
    if (header.first != 0u || header.count != 1u) return 4;
    const auto item = SPRE22Capture::read_record(in, 0u);
    if (item.original_index != 0u || item.projection.n != 3u ||
        item.set.domain().size() != 2u || item.set.constraints().size() != 3u)
        return 5;
    if (in.peek() != std::char_traits<char>::eof()) return 6;
    return 0;
}
