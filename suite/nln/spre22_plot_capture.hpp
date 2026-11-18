/***** SPRE22 plot-capture experiment (arch branch only) *********************
 * Copyright 2026 Luca Geretti
 *
 * Captures the validated constrained-image inputs to the drawing layer,
 * not raster output or approximating bounding boxes. The generated header
 * can be compiled into SPRE22_plot_replay without running the evolver.
 ***************************************************************************/

#ifndef ARIADNE_ARCH_SPRE22_PLOT_CAPTURE_HPP
#define ARIADNE_ARCH_SPRE22_PLOT_CAPTURE_HPP

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

#include "ariadne.hpp"
#include "algebra/sweeper.hpp"
#include "function/taylor_function.hpp"
#include "function/taylor_model.hpp"
#include "hybrid/hybrid_graphics.hpp"
#include "rendering/rendering_interface.hpp"

namespace Ariadne::SPRE22Capture {

inline void write_double(std::ostream& out, double value) {
    if (!std::isfinite(value)) {
        throw std::runtime_error("Non-finite Taylor coefficient or bound cannot be captured");
    }
    out << std::hexfloat << value << std::defaultfloat;
}

inline void write_float(std::ostream& out, const FloatDP& value) {
    out << "FloatDP(ExactDouble(";
    write_double(out, value.get_d());
    out << "), dp)";
}

inline void write_dyadic(std::ostream& out, const Dyadic& value) {
    if (!value.is_finite()) {
        throw std::runtime_error("Non-finite parameter-domain endpoint");
    }
    out << "Dyadic(Integer(String(\"";
    out << value.mantissa();
    out << "\")), " << value.denominator_exponent() << "u)";
}

inline void write_domain(std::ostream& out, const ExactBoxType& domain) {
    out << "ExactBoxType{";
    for (SizeType i = 0; i != domain.size(); ++i) {
        if (i != 0u) out << ", ";
        out << "ExactIntervalType(";
        write_dyadic(out, domain[i].lower_bound());
        out << ", ";
        write_dyadic(out, domain[i].upper_bound());
        out << ")";
    }
    out << "}";
}

inline void write_sweeper(std::ostream& out, const Sweeper<FloatDP>& sweeper) {
    const auto& impl = sweeper.reference();
    if (const auto* p = dynamic_cast<const ThresholdSweeper<FloatDP>*>(&impl)) {
        out << "ThresholdSweeper<FloatDP>(dp, ";
        write_float(out, p->sweep_threshold());
        out << ")";
    } else if (const auto* p = dynamic_cast<const GradedSweeper<FloatDP>*>(&impl)) {
        out << "GradedSweeper<FloatDP>(dp, " << p->degree() << "u)";
    } else if (const auto* p = dynamic_cast<const GradedThresholdSweeper<FloatDP>*>(&impl)) {
        out << "GradedThresholdSweeper<FloatDP>(dp, " << p->degree() << "u, ";
        write_float(out, p->sweep_threshold());
        out << ")";
    } else if (const auto* p = dynamic_cast<const RelativeThresholdSweeper<FloatDP>*>(&impl)) {
        out << "RelativeThresholdSweeper<FloatDP>(dp, ";
        write_float(out, p->relative_sweep_threshold());
        out << ")";
    } else if (dynamic_cast<const TrivialSweeper<FloatDP>*>(&impl)) {
        out << "TrivialSweeper<FloatDP>(dp)";
    } else if (dynamic_cast<const NullSweeper<FloatDP>*>(&impl)) {
        out << "NullSweeper<FloatDP>(dp)";
    } else {
        throw std::runtime_error("Unsupported Taylor sweeper: refusing a non-faithful snapshot");
    }
}

inline void write_model(std::ostream& out, const ValidatedTaylorModelDP& model,
                        const char* variable_name) {
    const auto argument_size = model.argument_size();
    out << "        {\n";
    out << "            Expansion<MultiIndex, FloatDP> expansion(" << argument_size << "u, dp);\n";
    for (auto iter = model.begin(); iter != model.end(); ++iter) {
        const auto index = iter->index();
        out << "            expansion.append(MultiIndex({";
        for (SizeType k = 0; k != argument_size; ++k) {
            if (k != 0u) out << ", ";
            out << index[k] << "u";
        }
        out << "}), ";
        write_float(out, iter->coefficient());
        out << ");\n";
    }
    out << "            " << variable_name << " = ValidatedTaylorModelDP(expansion, FloatDPError(ExactDouble(";
    write_double(out, model.error().raw().get_d());
    out << "), dp), ";
    write_sweeper(out, model.sweeper());
    out << ");\n";
    out << "        }\n";
}

inline void write_bounds(std::ostream& out, const FloatDPBounds& bounds) {
    out << "FloatDPBounds(";
    write_float(out, bounds.lower_raw());
    out << ", ";
    write_float(out, bounds.upper_raw());
    out << ")";
}

inline void write_enclosure(std::ostream& out, const HybridEnclosure& enclosure,
                           const Variables2d& axes, SizeType index) {
    const auto& set = enclosure.continuous_set();
    const auto function = set.state_time_auxiliary_function();
    const auto* taylor = dynamic_cast<const ValidatedVectorMultivariateTaylorFunctionModelDP*>(
        &function.reference());
    if (!taylor) {
        throw std::runtime_error("Non-Taylor drawing function: cannot capture without changing geometry");
    }
    const auto projection = Ariadne::projection(enclosure.state_time_auxiliary_space(), axes);
    out << "        // Original reach enclosure " << index << "\n";
    out << "        {\n";
    out << "        const auto domain = ";
    write_domain(out, set.domain());
    out << ";\n";
    out << "        Vector<ValidatedTaylorModelDP> models(" << taylor->size()
        << "u, ValidatedTaylorModelDP(" << taylor->argument_size()
        << "u, TrivialSweeper<FloatDP>(dp)));\n";
    for (SizeType i = 0; i != taylor->size(); ++i) {
        std::string target = "models[" + std::to_string(i) + "u]";
        write_model(out, taylor->model(i), target.c_str());
    }
    out << "        const ValidatedVectorMultivariateTaylorFunctionModelDP mapping(domain, models);\n";
    out << "        List<ValidatedConstraint> constraints;\n";
    for (const auto& constraint : set.constraint_models()) {
        const auto* scalar = dynamic_cast<const ValidatedScalarMultivariateTaylorFunctionModelDP*>(
            &constraint.function().reference());
        if (!scalar) {
            throw std::runtime_error("Non-Taylor constraint: cannot capture without changing geometry");
        }
        out << "        ValidatedTaylorModelDP scalar_model(" << scalar->argument_size()
            << "u, TrivialSweeper<FloatDP>(dp));\n";
        write_model(out, scalar->model(), "scalar_model");
        out << "        constraints.append(ValidatedConstraint(";
        write_bounds(out, constraint.lower_bound());
        out << ", ValidatedScalarMultivariateTaylorFunctionModelDP(domain, scalar_model), ";
        write_bounds(out, constraint.upper_bound());
        out << "));\n";
    }
    out << "        DrawerManager::instance().drawer().draw(canvas, Projection2d("
        << projection.n << "u, " << projection.i << "u, " << projection.j
        << "u), ValidatedConstrainedImageSet(domain, mapping, constraints));\n";
    out << "        }\n";
}

inline SizeType environment_size(const char* name, SizeType fallback) {
    const char* text = std::getenv(name);
    if (!text || !*text) return fallback;
    const std::string value(text);
    std::size_t parsed = 0;
    unsigned long long result = std::stoull(value, &parsed);
    if (parsed != value.size() || result > std::numeric_limits<SizeType>::max())
        throw std::invalid_argument(std::string("Invalid value for ") + name);
    return static_cast<SizeType>(result);
}

// The header is written before any plotting. If generation is interrupted,
// the .partial file and the last reported enclosure index remain available.
inline Void write_header(const char* filename, const ListSet<HybridEnclosure>& reach,
                         const Variables2d& axes) {
    const SizeType start = environment_size("SPRE22_PLOT_FIRST", 0u);
    const SizeType count = environment_size("SPRE22_PLOT_COUNT", reach.size());
    if (start > reach.size()) {
        throw std::out_of_range("SPRE22_PLOT_FIRST exceeds reach size");
    }
    const SizeType end = start + std::min(count, reach.size() - start);

    const std::string temporary = std::string(filename) + ".partial";
    std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("Cannot create " + temporary);
    out << "// Generated by SPRE22: reach enclosures [" << start << ", " << end << ")\n";
    out << "// This captures the inputs to DrawerManager, not previously computed pixels.\n";
    out << "#ifndef ARIADNE_SPRE22_PLOT_DATA_GENERATED_HPP\n";
    out << "#define ARIADNE_SPRE22_PLOT_DATA_GENERATED_HPP\n";
    out << "#include \"ariadne.hpp\"\n";
    out << "#include \"algebra/sweeper.hpp\"\n";
    out << "#include \"drawing/drawer_manager.hpp\"\n";
    out << "#include \"drawing/geometry_draw.hpp\"\n";
    out << "#include \"function/taylor_function.hpp\"\n";
    out << "#include \"hybrid/hybrid_graphics.hpp\"\n";
    out << "namespace Ariadne {\n";
    out << "class SPRE22CapturedReach final : public HybridDrawableInterface {\n";
    out << "public:\n";
    out << "    Void draw(CanvasInterface& canvas, const Set<DiscreteLocation>&, const Variables2d&) const override {\n";
    for (SizeType i = start; i != end; ++i) {
        try {
            write_enclosure(out, reach[i], axes, i);
        } catch (const std::exception& e) {
            throw std::runtime_error("Cannot capture reach enclosure " + std::to_string(i)
                                     + ": " + e.what());
        }
        if ((i - start + 1u) % 256u == 0u) {
            out.flush();
            if (!out) throw std::runtime_error("Writing snapshot failed");
            std::cerr << "SPRE22 snapshot: " << (i - start + 1u)
                      << "/" << (end - start) << " enclosures\n";
        }
    }
    out << "    }\n";
    out << "};\n";
    out << "} // namespace Ariadne\n";
    out << "#endif\n";
    out.close();
    if (!out) throw std::runtime_error("Finalizing snapshot failed");
    std::filesystem::rename(temporary, filename);
}

} // namespace Ariadne::SPRE22Capture

#endif // ARIADNE_ARCH_SPRE22_PLOT_CAPTURE_HPP
