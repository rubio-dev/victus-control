#include <cstdio>
#include <algorithm>
#include <cmath>

#include "gauges.hpp"
#include "palette.hpp"

namespace {

// Shared with the stylesheet, through the same palette: cool below 70C, amber
// to 85C, red above.
void temperature_colour(double celsius, double *r, double *g, double *b) {
  const VictusPalette &p = victus_palette();
  const VictusColor &c = celsius >= 85.0   ? p.hot
                         : celsius >= 70.0 ? p.warn
                                           : p.series_cpu;
  *r = c.r; *g = c.g; *b = c.b;
}

void rounded_rect(cairo_t *cr, double x, double y, double w, double h,
                  double radius) {
  cairo_new_sub_path(cr);
  cairo_arc(cr, x + w - radius, y + radius,     radius, -M_PI / 2, 0);
  cairo_arc(cr, x + w - radius, y + h - radius, radius, 0, M_PI / 2);
  cairo_arc(cr, x + radius,     y + h - radius, radius, M_PI / 2, M_PI);
  cairo_arc(cr, x + radius,     y + radius,     radius, M_PI, 1.5 * M_PI);
  cairo_close_path(cr);
}

} // namespace

void draw_fan_rotor(cairo_t *cr, int width, int height, double angle,
                    double duty, bool spinning) {
  // cairo_arc() draws a line from the current point to where the arc starts,
  // so a caller that left a point behind (a label, say) would get a stray line
  // running into the housing. Start from a clean path.
  cairo_new_path(cr);

  const double cx = width / 2.0;
  const double cy = height / 2.0;
  const double outer = std::min(width, height) / 2.0 - 4.0;
  const double hub = outer * 0.20;

  duty = std::clamp(duty, 0.0, 1.0);

  const VictusPalette &pal = victus_palette();

  // Housing.
  cairo_set_source_rgb(cr, VICTUS_RGB(pal.surface_alt));
  cairo_arc(cr, cx, cy, outer, 0, 2 * M_PI);
  cairo_fill_preserve(cr);
  cairo_set_source_rgb(cr, VICTUS_RGB(pal.border));
  cairo_set_line_width(cr, 1.5);
  cairo_stroke(cr);

  // A faint ring that brightens with speed, so the dial reads even at a glance.
  if (spinning) {
    cairo_set_source_rgba(cr, VICTUS_RGB(pal.accent), 0.25 + 0.6 * duty);
    cairo_set_line_width(cr, 2.0);
    cairo_arc(cr, cx, cy, outer - 1.0, 0, 2 * M_PI);
    cairo_stroke(cr);
  }

  const int blade_count = 7;
  for (int i = 0; i < blade_count; i++) {
    double a = angle + i * (2 * M_PI / blade_count);

    cairo_new_path(cr);
    cairo_arc(cr, cx, cy, hub, a, a + 0.62);
    cairo_arc_negative(cr, cx, cy, outer * 0.92, a + 1.02, a + 0.30);
    cairo_close_path(cr);

    if (spinning) {
      // Deeper as the fan works harder, so a busy fan reads darker.
      cairo_set_source_rgba(cr, VICTUS_RGB(pal.accent), 0.45 + 0.5 * duty);
    } else {
      cairo_set_source_rgba(cr, VICTUS_RGB(pal.rotor_idle), 0.9);
    }
    cairo_fill(cr);
  }

  // Hub.
  cairo_set_source_rgb(cr, VICTUS_RGB(pal.surface));
  cairo_arc(cr, cx, cy, hub, 0, 2 * M_PI);
  cairo_fill_preserve(cr);
  cairo_set_source_rgba(cr, VICTUS_RGB(pal.accent), spinning ? 0.9 : 0.35);
  cairo_set_line_width(cr, 1.2);
  cairo_stroke(cr);
}

void draw_thermometer(cairo_t *cr, int width, int height, double celsius,
                      double max_celsius, bool valid) {
  cairo_new_path(cr);

  if (max_celsius <= 0.0)
    max_celsius = 100.0;

  const double bulb_radius = std::min(width * 0.34, height * 0.14);
  const double tube_width = bulb_radius * 1.15;
  const double cx = width / 2.0;
  const double bulb_cy = height - bulb_radius - 3.0;
  const double tube_top = 6.0;
  const double tube_bottom = bulb_cy;
  const double tube_height = tube_bottom - tube_top;

  const VictusPalette &pal = victus_palette();

  // Glass.
  cairo_set_source_rgb(cr, VICTUS_RGB(pal.surface_alt));
  rounded_rect(cr, cx - tube_width / 2.0, tube_top, tube_width, tube_height,
               tube_width / 2.0);
  cairo_fill(cr);
  cairo_arc(cr, cx, bulb_cy, bulb_radius, 0, 2 * M_PI);
  cairo_fill(cr);

  double r = 0.0, g = 0.0, b = 0.0;
  temperature_colour(celsius, &r, &g, &b);

  if (valid) {
    double fraction = std::clamp(celsius / max_celsius, 0.0, 1.0);
    double column = tube_height * fraction;

    // Mercury: bulb always filled, column rises with temperature.
    cairo_set_source_rgba(cr, r, g, b, 0.95);
    cairo_arc(cr, cx, bulb_cy, bulb_radius - 2.0, 0, 2 * M_PI);
    cairo_fill(cr);

    if (column > 1.0) {
      rounded_rect(cr, cx - (tube_width - 4.0) / 2.0, tube_bottom - column,
                   tube_width - 4.0, column, (tube_width - 4.0) / 2.0);
      cairo_fill(cr);
    }

    // A soft halo grows with heat, so a hot sensor draws the eye.
    cairo_set_source_rgba(cr, r, g, b, 0.08 + 0.18 * fraction);
    cairo_arc(cr, cx, bulb_cy, bulb_radius + 4.0, 0, 2 * M_PI);
    cairo_fill(cr);
  }

  // Graduations down the side of the tube.
  cairo_set_source_rgba(cr, VICTUS_RGB(pal.text_dim), 0.45);
  cairo_set_line_width(cr, 1.0);
  for (int i = 1; i < 5; i++) {
    double y = tube_top + tube_height * (i / 5.0);
    cairo_move_to(cr, cx + tube_width / 2.0 + 2.0, y);
    cairo_line_to(cr, cx + tube_width / 2.0 + 7.0, y);
  }
  cairo_stroke(cr);

  // Glass outline last so it sits over the mercury.
  cairo_set_source_rgb(cr, VICTUS_RGB(pal.border));
  cairo_set_line_width(cr, 1.4);
  rounded_rect(cr, cx - tube_width / 2.0, tube_top, tube_width, tube_height,
               tube_width / 2.0);
  cairo_stroke(cr);
  cairo_arc(cr, cx, bulb_cy, bulb_radius, 0, 2 * M_PI);
  cairo_stroke(cr);
}

namespace {

struct PlotBand {
  double top;
  double height;
  double min_value;
  double max_value;
};

// Map a sample to a y coordinate inside its band, clamped so an out-of-range
// reading rides the edge instead of escaping the plot.
double band_y(const PlotBand &band, double value) {
  double span = band.max_value - band.min_value;
  if (span <= 0.0)
    return band.top + band.height;
  double fraction = std::clamp((value - band.min_value) / span, 0.0, 1.0);
  return band.top + band.height * (1.0 - fraction);
}

void stroke_series(cairo_t *cr, const std::vector<double> &values,
                   size_t capacity, const PlotBand &band, double left,
                   double plot_width, double r, double g, double b) {
  if (values.size() < 2 || capacity < 2)
    return;

  const double dx = plot_width / static_cast<double>(capacity - 1);
  // Right-align: the newest sample always sits at the right edge.
  const double x0 = left + plot_width - dx * static_cast<double>(values.size() - 1);

  cairo_set_source_rgba(cr, r, g, b, 0.95);
  cairo_set_line_width(cr, 1.6);
  cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);

  bool pen_down = false;
  for (size_t i = 0; i < values.size(); i++) {
    if (values[i] < 0.0) { // no reading: break the line here
      pen_down = false;
      continue;
    }
    double x = x0 + dx * static_cast<double>(i);
    double y = band_y(band, values[i]);
    if (pen_down)
      cairo_line_to(cr, x, y);
    else
      cairo_move_to(cr, x, y);
    pen_down = true;
  }
  cairo_stroke(cr);
}

// Right-aligned against `x`, for the scale labels that live in the left gutter.
void plot_caption_right(cairo_t *cr, double x, double y, const char *text,
                        double alpha);

void plot_caption(cairo_t *cr, double x, double y, const char *text, double r,
                  double g, double b, double alpha) {
  cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL,
                         CAIRO_FONT_WEIGHT_NORMAL);
  cairo_set_font_size(cr, 9.0);
  cairo_set_source_rgba(cr, r, g, b, alpha);
  cairo_move_to(cr, x, y);
  cairo_show_text(cr, text);
}

void plot_caption_right(cairo_t *cr, double x, double y, const char *text,
                        double alpha) {
  cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL,
                         CAIRO_FONT_WEIGHT_NORMAL);
  cairo_set_font_size(cr, 9.0);
  cairo_text_extents_t extents;
  cairo_text_extents(cr, text, &extents);
  plot_caption(cr, x - extents.x_advance, y, text, VICTUS_RGB(victus_palette().text_dim), alpha);
}

// A colour swatch followed by its label, returning the x to continue from.
double legend_entry(cairo_t *cr, double x, double y, const char *label,
                    double r, double g, double b) {
  cairo_set_source_rgba(cr, r, g, b, 0.95);
  cairo_rectangle(cr, x, y - 4.0, 7.0, 2.0);
  cairo_fill(cr);

  plot_caption(cr, x + 11.0, y, label, VICTUS_RGB(victus_palette().text_dim), 0.95);

  cairo_text_extents_t extents;
  cairo_text_extents(cr, label, &extents);
  return x + 11.0 + extents.x_advance + 12.0;
}

} // namespace

void draw_history_plot(cairo_t *cr, int width, int height,
                       const std::vector<double> &cpu_c,
                       const std::vector<double> &gpu_c,
                       const std::vector<double> &fan1_rpm,
                       const std::vector<double> &fan2_rpm,
                       size_t capacity, double max_rpm, int span_minutes) {
  // The traces fill the whole width, so the scale labels get a gutter of their
  // own on the left rather than being drawn on top of the data.
  cairo_new_path(cr);

  const double gutter = 38.0;
  const double left = gutter;
  const double right_pad = 8.0;
  const double plot_width = width - left - right_pad;
  const double legend_height = 14.0;
  const double gap = 8.0;
  const double usable = height - legend_height - gap;
  const double band_height = (usable - gap) / 2.0;

  if (plot_width <= 4.0 || band_height <= 4.0)
    return;

  // Temperatures on top, fans below. 30-95 C covers everything this machine
  // does without wasting half the band on temperatures it never reaches.
  const PlotBand temp_band{2.0, band_height, 30.0, 95.0};
  const PlotBand rpm_band{2.0 + band_height + gap, band_height, 0.0,
                          max_rpm > 0.0 ? max_rpm : 6200.0};

  // Panel.
  const VictusPalette &pal = victus_palette();
  cairo_set_source_rgb(cr, VICTUS_RGB(pal.plot_panel));
  rounded_rect(cr, left - 4.0, 0.0, plot_width + 8.0, usable + 2.0, 6.0);
  cairo_fill_preserve(cr);
  cairo_set_source_rgb(cr, VICTUS_RGB(pal.border));
  cairo_set_line_width(cr, 1.0);
  cairo_stroke(cr);

  // A gridline across the middle of each band for a sense of scale.
  cairo_set_source_rgb(cr, VICTUS_RGB(pal.plot_grid));
  cairo_set_line_width(cr, 1.0);
  for (const PlotBand &band : {temp_band, rpm_band}) {
    double y = band.top + band.height / 2.0;
    cairo_move_to(cr, left, y);
    cairo_line_to(cr, left + plot_width, y);
  }
  cairo_stroke(cr);

  stroke_series(cr, fan1_rpm, capacity, rpm_band, left, plot_width, VICTUS_RGB(pal.series_fan1));
  stroke_series(cr, fan2_rpm, capacity, rpm_band, left, plot_width, VICTUS_RGB(pal.series_fan2));
  stroke_series(cr, cpu_c, capacity, temp_band, left, plot_width, VICTUS_RGB(pal.series_cpu));
  stroke_series(cr, gpu_c, capacity, temp_band, left, plot_width, VICTUS_RGB(pal.series_gpu));

  // Band scales, in the gutter.
  const double label_x = gutter - 8.0;
  char scale[24];
  plot_caption_right(cr, label_x, temp_band.top + 8.0, "95°C", 0.8);
  plot_caption_right(cr, label_x, temp_band.top + temp_band.height, "30°C", 0.8);
  snprintf(scale, sizeof(scale), "%d", static_cast<int>(rpm_band.max_value));
  plot_caption_right(cr, label_x, rpm_band.top + 8.0, scale, 0.8);
  plot_caption_right(cr, label_x, rpm_band.top + rpm_band.height, "0 RPM", 0.8);

  // Legend.
  double y = usable + gap + 8.0;
  double x = left - 4.0;
  x = legend_entry(cr, x, y, "CPU", VICTUS_RGB(pal.series_cpu));
  x = legend_entry(cr, x, y, "GPU", VICTUS_RGB(pal.series_gpu));
  x = legend_entry(cr, x, y, "FAN 1", VICTUS_RGB(pal.series_fan1));
  legend_entry(cr, x, y, "FAN 2", VICTUS_RGB(pal.series_fan2));

  char span[24];
  snprintf(span, sizeof(span), "last %d min", span_minutes);
  cairo_text_extents_t extents;
  cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL,
                         CAIRO_FONT_WEIGHT_NORMAL);
  cairo_set_font_size(cr, 9.0);
  cairo_text_extents(cr, span, &extents);
  plot_caption(cr, left + plot_width - extents.x_advance, y, span,
               VICTUS_RGB(pal.text_dim), 0.75);
}

void draw_level_meter(cairo_t *cr, int width, int height, int level, int steps) {
  cairo_new_path(cr);

  if (steps <= 0)
    return;

  const VictusPalette &pal = victus_palette();
  const double gap = 3.0;
  const double segment = (width - gap * (steps - 1)) / static_cast<double>(steps);
  if (segment <= 0.0)
    return;

  const double bar_height = std::min<double>(height, 10.0);
  const double top = (height - bar_height) / 2.0;

  for (int i = 0; i < steps; i++) {
    double x = i * (segment + gap);
    rounded_rect(cr, x, top, segment, bar_height, 2.0);

    if (i < level) {
      // The same run the thermometers use, so a high level looks hot here too.
      double fraction = static_cast<double>(i + 1) / static_cast<double>(steps);
      const VictusColor &colour = fraction > 0.87   ? pal.hot
                                  : fraction > 0.62 ? pal.warn
                                                    : pal.accent;
      cairo_set_source_rgb(cr, VICTUS_RGB(colour));
    } else {
      cairo_set_source_rgb(cr, VICTUS_RGB(pal.trough));
    }
    cairo_fill(cr);
  }
}
