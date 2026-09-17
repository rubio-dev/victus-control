#include <algorithm>
#include <cmath>

#include "icons.hpp"

namespace {

// Every icon is drawn inside a 16x16 box and scaled from there, so the line
// weights stay in proportion whatever size the caller asks for.
constexpr double kDesignSize = 16.0;

void stroke_setup(cairo_t *cr, const VictusColor &colour, double width) {
	cairo_set_source_rgb(cr, VICTUS_RGB(colour));
	cairo_set_line_width(cr, width);
	cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
	cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
}

void rounded(cairo_t *cr, double x, double y, double w, double h, double r) {
	cairo_new_sub_path(cr);
	cairo_arc(cr, x + w - r, y + r, r, -M_PI / 2, 0);
	cairo_arc(cr, x + w - r, y + h - r, r, 0, M_PI / 2);
	cairo_arc(cr, x + r, y + h - r, r, M_PI / 2, M_PI);
	cairo_arc(cr, x + r, y + r, r, M_PI, 1.5 * M_PI);
	cairo_close_path(cr);
}

// Three petal blades around a hub. Swept, tapering blades turn into an
// illegible knot at 14px; solid petals still read as a fan.
void draw_fan(cairo_t *cr, const VictusColor &colour) {
	const double cx = 8.0, cy = 8.0;
	cairo_set_source_rgb(cr, VICTUS_RGB(colour));

	for (int i = 0; i < 3; i++) {
		cairo_save(cr);
		cairo_translate(cr, cx, cy);
		cairo_rotate(cr, i * 2.0 * M_PI / 3.0 + 0.35);
		cairo_translate(cr, 0.0, -3.3);
		cairo_scale(cr, 1.0, 1.75);
		cairo_new_sub_path(cr);
		cairo_arc(cr, 0.0, 0.0, 2.0, 0, 2 * M_PI);
		cairo_restore(cr);
		cairo_fill(cr);
	}

	// Hub, punched out of the blades so the centre stays readable.
	cairo_set_source_rgb(cr, VICTUS_RGB(colour));
	cairo_arc(cr, cx, cy, 2.0, 0, 2 * M_PI);
	cairo_fill(cr);
	cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
	cairo_arc(cr, cx, cy, 1.0, 0, 2 * M_PI);
	cairo_fill(cr);
	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
}

void draw_cpu(cairo_t *cr, const VictusColor &colour) {
	stroke_setup(cr, colour, 1.4);

	rounded(cr, 3.5, 3.5, 9.0, 9.0, 1.6);
	cairo_stroke(cr);

	rounded(cr, 6.3, 6.3, 3.4, 3.4, 0.7);
	cairo_stroke(cr);

	// Pins on all four sides.
	for (int i = 0; i < 3; i++) {
		double at = 5.5 + i * 2.5;
		cairo_move_to(cr, at, 1.4);  cairo_line_to(cr, at, 3.4);
		cairo_move_to(cr, at, 12.6); cairo_line_to(cr, at, 14.6);
		cairo_move_to(cr, 1.4, at);  cairo_line_to(cr, 3.4, at);
		cairo_move_to(cr, 12.6, at); cairo_line_to(cr, 14.6, at);
	}
	cairo_stroke(cr);
}

void draw_gpu(cairo_t *cr, const VictusColor &colour) {
	stroke_setup(cr, colour, 1.4);

	rounded(cr, 1.6, 4.0, 12.8, 8.0, 1.4);
	cairo_stroke(cr);

	// The blower, so it does not read as just another chip.
	cairo_arc(cr, 6.0, 8.0, 2.3, 0, 2 * M_PI);
	cairo_stroke(cr);
	cairo_arc(cr, 6.0, 8.0, 0.7, 0, 2 * M_PI);
	cairo_fill(cr);

	// Board edge connector.
	cairo_move_to(cr, 10.4, 6.4); cairo_line_to(cr, 12.6, 6.4);
	cairo_move_to(cr, 10.4, 8.6); cairo_line_to(cr, 12.6, 8.6);
	cairo_stroke(cr);
}

void draw_keyboard(cairo_t *cr, const VictusColor &colour) {
	stroke_setup(cr, colour, 1.4);

	rounded(cr, 1.4, 4.2, 13.2, 7.6, 1.4);
	cairo_stroke(cr);

	for (int row = 0; row < 2; row++) {
		for (int col = 0; col < 4; col++) {
			double x = 3.4 + col * 2.4;
			double y = 6.4 + row * 2.2;
			cairo_rectangle(cr, x - 0.35, y - 0.35, 0.7, 0.7);
		}
	}
	cairo_fill(cr);

	// Space bar.
	cairo_move_to(cr, 5.4, 10.0);
	cairo_line_to(cr, 10.6, 10.0);
	cairo_stroke(cr);
}

void draw_menu(cairo_t *cr, const VictusColor &colour) {
	stroke_setup(cr, colour, 1.6);
	for (int i = 0; i < 3; i++) {
		double y = 4.5 + i * 3.5;
		cairo_move_to(cr, 3.0, y);
		cairo_line_to(cr, 13.0, y);
	}
	cairo_stroke(cr);
}

struct IconPayload {
	VictusIcon icon;
	VictusColor colour;
};

void on_icon_draw(GtkDrawingArea *, cairo_t *cr, int width, int height,
                  gpointer data) {
	const IconPayload *payload = static_cast<const IconPayload *>(data);
	victus_icon_draw(cr, payload->icon, std::min(width, height), payload->colour);
}

} // namespace

void victus_icon_draw(cairo_t *cr, VictusIcon icon, double size,
                      const VictusColor &colour) {
	cairo_save(cr);
	cairo_new_path(cr);
	cairo_scale(cr, size / kDesignSize, size / kDesignSize);

	switch (icon) {
	case VictusIcon::Fan:      draw_fan(cr, colour);      break;
	case VictusIcon::Cpu:      draw_cpu(cr, colour);      break;
	case VictusIcon::Gpu:      draw_gpu(cr, colour);      break;
	case VictusIcon::Keyboard: draw_keyboard(cr, colour); break;
	case VictusIcon::Menu:     draw_menu(cr, colour);     break;
	}

	cairo_restore(cr);
}

GtkWidget *victus_icon_new(VictusIcon icon, int size, const VictusColor &colour) {
	GtkWidget *area = gtk_drawing_area_new();
	gtk_widget_set_size_request(area, size, size);
	gtk_widget_set_valign(area, GTK_ALIGN_CENTER);

	IconPayload *payload = new IconPayload{icon, colour};
	gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(area), on_icon_draw, payload,
	                               +[](gpointer data) {
		                               delete static_cast<IconPayload *>(data);
	                               });
	return area;
}
