#ifndef VICTUS_ICONS_HPP
#define VICTUS_ICONS_HPP

#include <gtk/gtk.h>

#include "palette.hpp"

// The app used whatever symbolic icons the desktop happened to ship, which
// meant a weather glyph standing in for a fan and a different look on every
// distro. These are drawn here instead, in the same hand as the gauges, so the
// set is consistent and cannot go missing.
enum class VictusIcon {
	Fan,       // three swept blades
	Cpu,       // chip with pins
	Gpu,       // board with a blower
	Keyboard,
	Menu,
};

// A drawing area that paints `icon` at `size` px in `colour`. The widget owns
// its own copy of the colour, so palette changes only need a redraw.
GtkWidget *victus_icon_new(VictusIcon icon, int size, const VictusColor &colour);

// Paint one directly, for callers that already have a context.
void victus_icon_draw(cairo_t *cr, VictusIcon icon, double size,
                      const VictusColor &colour);

#endif // VICTUS_ICONS_HPP
