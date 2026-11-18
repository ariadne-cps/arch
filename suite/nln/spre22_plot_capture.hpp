/***************************************************************************
 *            spre22_plot_capture.hpp
 *
 *  Copyright 2026 Luca Geretti
 *
 *  SPRE22 validated drawing-input snapshot: versioned, little-endian binary.
 *  Keeps mathematical Taylor models and constraints, not rendered polygons.
 *  A replay process reads bounded batches without recompiling snapshot data.
 ***************************************************************************/

#ifndef ARIADNE_ARCH_SPRE22_PLOT_CAPTURE_HPP
#define ARIADNE_ARCH_SPRE22_PLOT_CAPTURE_HPP

#include <algorithm>
#include <bit>
#include <cmath>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "ariadne.hpp"
#include "algebra/sweeper.hpp"
#include "drawing/drawer_manager.hpp"
#include "drawing/geometry_draw.hpp"
#include "function/taylor_function.hpp"
#include "function/taylor_model.hpp"
#include "hybrid/hybrid_graphics.hpp"
#include "rendering/recording_canvas.hpp"
#include "threading/workload.hpp"

namespace Ariadne::SPRE22Capture {

inline constexpr char magic[8] = {'S', 'P', 'R', 'P', 'L', 'T', 'B', '1'};
inline constexpr std::uint32_t format_version = 1u;
inline constexpr std::uint32_t max_dimension = 4096u;
inline constexpr std::uint32_t max_terms = 10000000u;
inline constexpr std::uint32_t max_constraints = 100000u;
inline constexpr std::uint64_t max_record_bytes = 1024ull * 1024ull * 1024ull;

inline void write_uint(std::ostream& out, std::uint64_t value, unsigned bytes) {
    for (unsigned k = 0; k != bytes; ++k) {
        out.put(static_cast<char>(value & 255u));
        value >>= 8u;
    }
    if (value || !out) throw std::runtime_error("SPRE22 binary output failed or integer overflow");
}
inline std::uint64_t read_uint(std::istream& in, unsigned bytes) {
    std::uint64_t value = 0u;
    for (unsigned k = 0; k != bytes; ++k) {
        const int c = in.get();
        if (c == std::char_traits<char>::eof())
            throw std::runtime_error("Truncated SPRE22 binary snapshot");
        value |= static_cast<std::uint64_t>(static_cast<unsigned char>(c)) << (8u * k);
    }
    return value;
}
inline void write_count(std::ostream& out, std::uint64_t value) {
    if (value > std::numeric_limits<std::uint32_t>::max())
        throw std::runtime_error("SPRE22 snapshot count exceeds uint32");
    write_uint(out, value, 4u);
}
inline std::uint32_t read_count(std::istream& in, std::uint32_t maximum, const char* name) {
    const auto result = read_uint(in, 4u);
    if (result > maximum) throw std::runtime_error(std::string("Invalid SPRE22 ") + name);
    return static_cast<std::uint32_t>(result);
}

// Binary64 is stored as an explicit little-endian bit pattern, not host bytes.
// Finite coefficients/errors/domain bounds are exact binary64 round-trips.
inline void write_f64(std::ostream& out, double value, const std::string& context,
                      bool allow_infinity = false) {
    static_assert(sizeof(double) == sizeof(std::uint64_t));
    if (std::isnan(value) || (!allow_infinity && !std::isfinite(value)))
        throw std::runtime_error("Invalid non-finite " + context);
    write_uint(out, std::bit_cast<std::uint64_t>(value), 8u);
}
inline double read_f64(std::istream& in, const std::string& context,
                       bool allow_infinity = false) {
    const double result = std::bit_cast<double>(read_uint(in, 8u));
    if (std::isnan(result) || (!allow_infinity && !std::isfinite(result)))
        throw std::runtime_error("Invalid non-finite " + context);
    return result;
}
inline FloatDP read_float(std::istream& in, const std::string& context,
                          bool allow_infinity = false) {
    const double value = read_f64(in, context, allow_infinity);
    if (std::isinf(value))
        return FloatDP::inf(std::signbit(value) ? Sign::NEGATIVE : Sign::POSITIVE, dp);
    return FloatDP(ExactDouble(value), dp);
}

inline void write_domain(std::ostream& out, const ExactBoxType& domain) {
    write_count(out, domain.size());
    for (SizeType i = 0; i != domain.size(); ++i) {
        write_f64(out, domain[i].lower_bound().get_d(), "domain lower");
        write_f64(out, domain[i].upper_bound().get_d(), "domain upper");
    }
}
inline ExactBoxType read_domain(std::istream& in) {
    const auto dimension = read_count(in, max_dimension, "domain dimension");
    ExactBoxType domain(dimension);
    for (std::uint32_t i = 0; i != dimension; ++i) {
        const auto lower = read_float(in, "domain lower");
        const auto upper = read_float(in, "domain upper");
        domain[i] = ExactIntervalType(lower, upper);
    }
    return domain;
}

// Sweeper type and parameters are preserved. Unsupported concrete policies
// fail explicitly, rather than silently changing the numerical semantics.
inline void write_sweeper(std::ostream& out, const Sweeper<FloatDP>& sweeper) {
    const auto& impl = sweeper.reference();
    if (const auto* p = dynamic_cast<const ThresholdSweeper<FloatDP>*>(&impl)) {
        write_uint(out, 1u, 1u);
        write_f64(out, p->sweep_threshold().get_d(), "threshold sweeper");
    } else if (const auto* p = dynamic_cast<const GradedSweeper<FloatDP>*>(&impl)) {
        write_uint(out, 2u, 1u);
        write_count(out, p->degree());
    } else if (const auto* p = dynamic_cast<const GradedThresholdSweeper<FloatDP>*>(&impl)) {
        write_uint(out, 3u, 1u);
        write_count(out, p->degree());
        write_f64(out, p->sweep_threshold().get_d(), "graded threshold sweeper");
    } else if (const auto* p = dynamic_cast<const RelativeThresholdSweeper<FloatDP>*>(&impl)) {
        write_uint(out, 4u, 1u);
        write_f64(out, p->relative_sweep_threshold().get_d(), "relative threshold sweeper");
    } else if (dynamic_cast<const TrivialSweeper<FloatDP>*>(&impl)) {
        write_uint(out, 5u, 1u);
    } else {
        throw std::runtime_error("Unsupported Taylor sweeper: refusing a non-faithful snapshot");
    }
}
inline Sweeper<FloatDP> read_sweeper(std::istream& in) {
    switch (read_uint(in, 1u)) {
        case 1u:
            return Sweeper<FloatDP>(new ThresholdSweeper<FloatDP>(
                dp, read_float(in, "threshold sweeper")));
        case 2u:
            return Sweeper<FloatDP>(new GradedSweeper<FloatDP>(
                dp, read_count(in, std::numeric_limits<std::uint32_t>::max(), "sweeper degree")));
        case 3u: {
            const auto degree = read_count(in, std::numeric_limits<std::uint32_t>::max(),
                                           "graded threshold degree");
            return Sweeper<FloatDP>(new GradedThresholdSweeper<FloatDP>(
                dp, degree, read_float(in, "graded threshold sweeper")));
        }
        case 4u:
            return Sweeper<FloatDP>(new RelativeThresholdSweeper<FloatDP>(
                dp, read_float(in, "relative threshold sweeper")));
        case 5u:
            return Sweeper<FloatDP>(new TrivialSweeper<FloatDP>(dp));
        default:
            throw std::runtime_error("Unknown SPRE22 sweeper type");
    }
}

inline void write_model(std::ostream& out, const ValidatedTaylorModelDP& model) {
    write_count(out, model.argument_size());
    write_sweeper(out, model.sweeper());
    write_f64(out, model.error().raw().get_d(), "Taylor uniform error");
    write_count(out, model.expansion().size());
    for (auto iter = model.begin(); iter != model.end(); ++iter) {
        const auto index = iter->index();
        for (SizeType k = 0; k != model.argument_size(); ++k)
            write_count(out, index[k]);
        write_f64(out, iter->coefficient().get_d(), "Taylor coefficient");
    }
}
inline ValidatedTaylorModelDP read_model(std::istream& in, SizeType dimension) {
    const auto arguments = read_count(in, max_dimension, "Taylor dimension");
    if (arguments != dimension)
        throw std::runtime_error("SPRE22 Taylor model dimension mismatch");
    const auto sweeper = read_sweeper(in);
    const auto error = read_float(in, "Taylor uniform error");
    const auto terms = read_count(in, max_terms, "Taylor term count");
    Expansion<MultiIndex, FloatDP> expansion(arguments, dp);
    expansion.reserve(terms);
    for (std::uint32_t j = 0; j != terms; ++j) {
        MultiIndex index(arguments);
        for (std::uint32_t k = 0; k != arguments; ++k)
            index[k] = read_count(in, std::numeric_limits<std::uint32_t>::max(), "Taylor exponent");
        expansion.append(index, read_float(in, "Taylor coefficient"));
    }
    return ValidatedTaylorModelDP(expansion, FloatDPError(error), sweeper);
}
inline void write_bounds(std::ostream& out, const FloatDPBounds& bounds) {
    write_f64(out, bounds.lower_raw().get_d(), "constraint lower bound", true);
    write_f64(out, bounds.upper_raw().get_d(), "constraint upper bound", true);
}
inline FloatDPBounds read_bounds(std::istream& in) {
    const auto lower = read_float(in, "constraint lower bound", true);
    const auto upper = read_float(in, "constraint upper bound", true);
    return FloatDPBounds(lower, upper);
}

struct DrawItem {
    SizeType original_index;
    Projection2d projection;
    ValidatedConstrainedImageSet set;
    // A version-1 writer bug added a redundant time component when t was
    // already a state variable. Preserve old snapshots without re-evolution.
    bool legacy_extra_time;
};

inline void write_record_with_mapping(
    std::ostream& out, const HybridEnclosure& enclosure,
    const Variables2d& axes, SizeType index,
    const ValidatedVectorMultivariateFunctionPatch& function) {
    const auto& continuous = enclosure.continuous_set();
    const auto* taylor =
        dynamic_cast<const ValidatedVectorMultivariateTaylorFunctionModelDP*>(&function.reference());
    if (!taylor)
        throw std::runtime_error("Non-Taylor mapping: cannot capture without changing geometry");

    const auto projection = Ariadne::projection(enclosure.state_time_auxiliary_space(), axes);
    const std::streampos start = out.tellp();
    if (start == std::streampos(-1)) throw std::runtime_error("Cannot seek snapshot output");
    write_uint(out, 0u, 8u); // Payload size, patched after writing.
    const std::streampos payload_start = out.tellp();
    write_uint(out, static_cast<std::uint64_t>(index), 8u);
    write_count(out, projection.n);
    write_count(out, projection.i);
    write_count(out, projection.j);
    write_domain(out, continuous.domain());
    write_count(out, taylor->size());
    for (SizeType i = 0; i != taylor->size(); ++i)
        write_model(out, taylor->model(i));
    write_count(out, continuous.number_of_constraints());
    for (const auto& constraint : continuous.constraint_models()) {
        const auto constraint_function = constraint.function();
        const auto* scalar = dynamic_cast<
            const ValidatedScalarMultivariateTaylorFunctionModelDP*>(
                &constraint_function.reference());
        if (!scalar)
            throw std::runtime_error("Non-Taylor constraint: cannot capture without changing geometry");
        write_bounds(out, constraint.lower_bound());
        write_model(out, scalar->model());
        write_bounds(out, constraint.upper_bound());
    }
    const std::streampos end = out.tellp();
    const std::streamoff payload_size = end - payload_start;
    if (payload_size <= 0 || static_cast<std::uint64_t>(payload_size) > max_record_bytes)
        throw std::runtime_error("SPRE22 record size out of bounds");
    out.seekp(start);
    write_uint(out, static_cast<std::uint64_t>(payload_size), 8u);
    out.seekp(end);
    if (!out) throw std::runtime_error("Cannot finalize SPRE22 record");
}

// Use the labelled function, exactly as HybridEnclosure::draw does. Calling
// continuous_set().state_time_auxiliary_function() loses the state labels and
// appends an extra time component when the model already has a state named t.
inline void write_record(std::ostream& out, const HybridEnclosure& enclosure,
                         const Variables2d& axes, SizeType index) {
    const auto function = enclosure.state_time_auxiliary_function();
    write_record_with_mapping(out, enclosure, axes, index, function);
}

inline DrawItem read_record(std::istream& in, std::uint64_t expected_index) {
    const std::uint64_t payload_size = read_uint(in, 8u);
    if (payload_size < 32u || payload_size > max_record_bytes)
        throw std::runtime_error("Invalid SPRE22 record size");
    const std::streampos start = in.tellg();
    const auto original = read_uint(in, 8u);
    if (original != expected_index)
        throw std::runtime_error("Unexpected SPRE22 enclosure index");
    const auto n = read_count(in, max_dimension, "projection dimension");
    const auto i = read_count(in, max_dimension, "projection x");
    const auto j = read_count(in, max_dimension, "projection y");
    if (i >= n || j >= n || i == j)
        throw std::runtime_error("Invalid SPRE22 projection");
    const auto domain = read_domain(in);
    const auto nmodels = read_count(in, max_dimension, "mapping components");
    // Compatibility with existing version-1 captures. The first writer
    // mistakenly used the unlabelled continuous Enclosure, duplicating the
    // time output if t was already in the state space. The x/y indices and
    // constraints are unchanged; retain the extra unprojected component.
    if (nmodels != n && !(n < max_dimension && nmodels == n + 1u))
        throw std::runtime_error("SPRE22 mapping/projection dimension mismatch");
    const auto dimensions = domain.size();
    Vector<ValidatedTaylorModelDP> models(nmodels, ValidatedTaylorModelDP(
        dimensions, TrivialSweeper<FloatDP>(dp)));
    for (std::uint32_t k = 0; k != nmodels; ++k)
        models[k] = read_model(in, dimensions);
    const ValidatedVectorMultivariateTaylorFunctionModelDP mapping(domain, models);
    const auto count = read_count(in, max_constraints, "constraint count");
    List<ValidatedConstraint> constraints;
    for (std::uint32_t k = 0; k != count; ++k) {
        const auto lower = read_bounds(in);
        const auto model = read_model(in, dimensions);
        const auto upper = read_bounds(in);
        constraints.append(ValidatedConstraint(
            lower, ValidatedScalarMultivariateTaylorFunctionModelDP(domain, model), upper));
    }
    if (in.tellg() - start != static_cast<std::streamoff>(payload_size))
        throw std::runtime_error("SPRE22 record length mismatch");
    return DrawItem{static_cast<SizeType>(original), Projection2d(n, i, j),
                    ValidatedConstrainedImageSet(domain, mapping, constraints), nmodels != n};
}

// Skip a validated record without materialising its Taylor models.
// Record payload includes the 8-byte original index already read here.
inline void skip_record(std::istream& in, std::uint64_t expected_index) {
    const auto payload_size = read_uint(in, 8u);
    if (payload_size < 32u || payload_size > max_record_bytes)
        throw std::runtime_error("Invalid SPRE22 record size while skipping");
    const auto original_index = read_uint(in, 8u);
    if (original_index != expected_index)
        throw std::runtime_error("Unexpected SPRE22 index while skipping");
    in.seekg(static_cast<std::streamoff>(payload_size - 8u), std::ios::cur);
    if (!in) throw std::runtime_error("Failed to skip SPRE22 record");
}

struct CaptureHeader {
    std::uint64_t first;
    std::uint64_t count;
};

inline void write_header(std::ostream& out, CaptureHeader header) {
    out.write(magic, sizeof(magic));
    write_uint(out, format_version, 4u);
    write_uint(out, header.first, 8u);
    write_uint(out, header.count, 8u);
    if (!out) throw std::runtime_error("Cannot write SPRE22 snapshot header");
}
inline CaptureHeader read_header(std::istream& in) {
    char stored_magic[8]{};
    in.read(stored_magic, sizeof(stored_magic));
    if (!in || !std::equal(std::begin(magic), std::end(magic), std::begin(stored_magic)))
        throw std::runtime_error("Not a versioned SPRE22 binary snapshot");
    if (read_uint(in, 4u) != format_version)
        throw std::runtime_error("Unsupported SPRE22 snapshot version");
    const auto first = read_uint(in, 8u);
    const auto count = read_uint(in, 8u);
    if (first > std::numeric_limits<SizeType>::max() ||
        count > std::numeric_limits<SizeType>::max() - first)
        throw std::runtime_error("SPRE22 snapshot range overflow");
    return CaptureHeader{first, count};
}

inline SizeType environment_size(const char* name, SizeType fallback) {
    const char* text = std::getenv(name);
    if (!text || !*text) return fallback;
    const std::string value(text);
    std::size_t parsed = 0u;
    const unsigned long long result = std::stoull(value, &parsed);
    if (parsed != value.size() || result > std::numeric_limits<SizeType>::max())
        throw std::invalid_argument(std::string("Invalid value for ") + name);
    return static_cast<SizeType>(result);
}

// Write to .partial and atomically rename only after the entire capture
// succeeds. No rendering or expensive Taylor drawing is performed here.
inline Void write_binary(const char* filename, const ListSet<HybridEnclosure>& reach,
                         const Variables2d& axes) {
    const SizeType first = environment_size("SPRE22_PLOT_FIRST", 0u);
    const SizeType requested = environment_size("SPRE22_PLOT_COUNT", reach.size());
    if (first > reach.size()) throw std::out_of_range("SPRE22_PLOT_FIRST exceeds reach size");
    const SizeType count = std::min(requested, reach.size() - first);
    const auto temporary = std::string(filename) + ".partial";
    std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("Cannot create " + temporary);
    write_header(out, CaptureHeader{first, count});

    for (SizeType local = 0; local != count; ++local) {
        const SizeType index = first + local;
        try {
            write_record(out, reach[index], axes, index);
        } catch (const std::exception& e) {
            throw std::runtime_error("Cannot capture reach enclosure " +
                                     std::to_string(index) + ": " + e.what());
        }
        if ((local + 1u) % 256u == 0u || local + 1u == count) {
            out.flush();
            if (!out) throw std::runtime_error("Writing SPRE22 snapshot failed");
            std::cerr << "SPRE22 capture: " << (local + 1u) << "/"
                      << count << " enclosures" << std::endl;
        }
    }
    out.close();
    if (!out) throw std::runtime_error("Finalizing SPRE22 snapshot failed");
    std::filesystem::rename(temporary, filename);
}

// Replay can be invoked by Figure without opening a giant source file.
// Enclosures are reconstructed one batch at a time; only their drawings
// are computed in parallel and are then replayed in original order.
class CapturedReach final : public HybridDrawableInterface {
  public:
    explicit CapturedReach(std::string path) : _path(std::move(path)) {}

    Void draw(CanvasInterface& canvas, const Set<DiscreteLocation>&,
              const Variables2d&) const override {
        std::ifstream in(_path, std::ios::binary);
        if (!in) throw std::runtime_error("Cannot open SPRE22 snapshot: " + _path);
        const CaptureHeader header = read_header(in);
        const auto final_index = header.first + header.count;
        const SizeType first = environment_size("SPRE22_REPLAY_FIRST",
                                                static_cast<SizeType>(header.first));
        if (first < header.first || first > final_index)
            throw std::out_of_range("SPRE22_REPLAY_FIRST is outside the snapshot");
        const SizeType available = static_cast<SizeType>(final_index - first);
        const SizeType requested = environment_size("SPRE22_REPLAY_COUNT", available);
        const SizeType selected = std::min(requested, available);
        const SizeType batch_size = environment_size("SPRE22_REPLAY_BATCH_SIZE", 256u);
        if (batch_size == 0u || batch_size > 256u)
            throw std::out_of_range("SPRE22_REPLAY_BATCH_SIZE must be in [1,256]");
        const SizeType trace_option = environment_size("SPRE22_REPLAY_TRACE", 0u);
        if (trace_option > 1u)
            throw std::out_of_range("SPRE22_REPLAY_TRACE must be 0 or 1");
        const bool trace = trace_option != 0u;
        std::mutex trace_mutex;

        // Index directly through length-prefixed records instead of
        // reconstructing all the enclosures preceding the selected range.
        for (std::uint64_t index = header.first; index < first; ++index)
            skip_record(in, index);
        if (first != header.first)
            std::cerr << "SPRE22 replay: seeking to enclosure " << first << std::endl;

        bool legacy_warning_reported = false;
        for (SizeType offset = 0; offset < selected; offset += batch_size) {
            const SizeType count = std::min(batch_size, selected - offset);
            std::vector<DrawItem> items;
            items.reserve(count);
            for (SizeType index = 0; index != count; ++index) {
                const auto absolute = static_cast<std::uint64_t>(first) + offset + index;
                try {
                    items.push_back(read_record(in, absolute));
                    if (items.back().legacy_extra_time && !legacy_warning_reported) {
                        std::cerr << "SPRE22 replay: legacy snapshot contains an extra "
                                     "unprojected time component; preserving the "
                                     "original x/y projection" << std::endl;
                        legacy_warning_reported = true;
                    }
                } catch (const std::exception& e) {
                    throw std::runtime_error("Cannot read enclosure " +
                                             std::to_string(absolute) + ": " + e.what());
                }
            }
            std::cerr << "SPRE22 replay: drawing [" << (first + offset)
                      << "," << (first + offset + count) << ")" << std::endl;
            std::vector<RecordingCanvas> recordings(count);
            StaticWorkload<SizeType> workload([&](const SizeType& index) {
                const auto absolute = items[index].original_index;
                const auto began = std::chrono::steady_clock::now();
                if (trace) {
                    std::lock_guard<std::mutex> guard(trace_mutex);
                    std::cerr << "SPRE22 draw BEGIN " << absolute << std::endl;
                }
                DrawerManager::instance().drawer().draw(
                    recordings[index], items[index].projection, items[index].set);
                if (trace) {
                    const auto elapsed = std::chrono::duration<double>(
                        std::chrono::steady_clock::now() - began).count();
                    std::lock_guard<std::mutex> guard(trace_mutex);
                    std::cerr << "SPRE22 draw END " << absolute
                              << " (" << elapsed << " s)" << std::endl;
                }
            });
            for (SizeType index = 0; index != count; ++index) workload.append(index);
            workload.process();
            std::cerr << "SPRE22 replay: replaying [" << (first + offset)
                      << "," << (first + offset + count) << ")" << std::endl;
            for (const auto& recording : recordings) recording.replay(canvas);
        }
        // Only expect EOF when replaying a range reaching the end of file.
        if (static_cast<std::uint64_t>(first) + selected == final_index) {
            if (in.peek() != std::char_traits<char>::eof())
                throw std::runtime_error("Trailing data after SPRE22 snapshot");
            if (in.bad()) throw std::runtime_error("I/O failure reading SPRE22 snapshot");
        }
    }

  private:
    std::string _path;
};

} // namespace Ariadne::SPRE22Capture

#endif // ARIADNE_ARCH_SPRE22_PLOT_CAPTURE_HPP
