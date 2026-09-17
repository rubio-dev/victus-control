#ifndef VICTUS_GAUGES_HPP
#define VICTUS_GAUGES_HPP

#include <cairo.h>

#include <vector>

// Analog telemetry dials drawn with cairo.

// A fan rotor. `angle` is the current rotation in radians and `duty` is the
// speed as a fraction of the fan's maximum (0-1), which sets how brightly the
// blades are lit. `spinning` is false when the fan has stopped, so a stalled
// fan reads differently from one turning slowly.
void draw_fan_rotor(cairo_t *cr, int width, int height, double angle,
                    double duty, bool spinning);

// A thermometer. `celsius` fills the column against `max_celsius`, and the
// mercury shifts from cool to amber to red as it climbs, matching the colour
// thresholds used by the digital readouts.
void draw_thermometer(cairo_t *cr, int width, int height, double celsius,
                      double max_celsius, bool valid);

// A rolling plot of the last few minutes, in two stacked bands sharing one time
// axis: temperatures on top, fan speeds below. Separate bands rather than one
// mixed scale, so neither series has to be squashed to fit the other.
//
// Each vector is oldest-sample-first and they are drawn right-aligned, so a
// half-filled history grows in from the right instead of stretching. A negative
// value marks a sample with no reading (a runtime-suspended GPU, say) and
// breaks the line there rather than drawing through zero.
void draw_history_plot(cairo_t *cr, int width, int height,
                       const std::vector<double> &cpu_c,
                       const std::vector<double> &gpu_c,
                       const std::vector<double> &fan1_rpm,
                       const std::vector<double> &fan2_rpm,
                       size_t capacity, double max_rpm, int span_minutes);

// A row of `steps` segments with the first `level` of them lit, so the Better
// Auto level can be read without parsing a sentence. Lit segments warm up as
// they approach the top of the range, matching the temperature colours.
void draw_level_meter(cairo_t *cr, int width, int height, int level, int steps);

#endif // VICTUS_GAUGES_HPP
