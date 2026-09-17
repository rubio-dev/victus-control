#ifndef VICTUS_PALETTE_HPP
#define VICTUS_PALETTE_HPP

#include <string>

// One palette, two consumers: the GTK stylesheet and the cairo gauges. They
// used to carry their own copies of the same hex values, so any retheme had to
// be done twice and drifted. Everything visual comes from here now.

struct VictusColor {
	double r;
	double g;
	double b;
};

struct VictusPalette {
	VictusColor bg;           // window behind the cards
	VictusColor surface;      // the cards themselves
	VictusColor surface_alt;  // panels inset into a card
	VictusColor border;
	VictusColor accent;       // fills, active states, fan blades
	VictusColor accent_dim;   // softer accent for rails and hairlines
	VictusColor accent_fg;    // text sitting on an accent fill
	VictusColor text;
	VictusColor text_dim;
	VictusColor warn;         // 70 C and up
	VictusColor hot;          // 85 C and up
	VictusColor ok;
	VictusColor trough;       // slider and progress channels

	VictusColor plot_panel;
	VictusColor plot_grid;
	VictusColor series_cpu;
	VictusColor series_gpu;
	VictusColor series_fan1;
	VictusColor series_fan2;
	VictusColor rotor_idle;   // blades of a fan that is not turning
};

const VictusPalette &victus_palette();

// "#rrggbb", for building the stylesheet's colour definitions.
std::string victus_hex(const VictusColor &colour);

// cairo_set_source_rgb(cr, C(x)) — keeps the call sites readable.
#define VICTUS_RGB(c) (c).r, (c).g, (c).b

#endif // VICTUS_PALETTE_HPP
