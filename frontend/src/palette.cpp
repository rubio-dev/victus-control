#include <cstdio>

#include "palette.hpp"

namespace {

constexpr VictusColor rgb(int r, int g, int b)
{
	return VictusColor{r / 255.0, g / 255.0, b / 255.0};
}

// A light palette built around soft mauve and rose. Structure stays near-white
// so the colour that does appear carries meaning: mauve for anything active,
// and the temperature run kept deliberately separate - amber at 70C, rose-red
// at 85C - so heat never blends into the decoration.
const VictusPalette kPalette = {
	rgb(247, 244, 250),  // bg
	rgb(255, 255, 255),  // surface
	rgb(250, 246, 253),  // surface_alt
	rgb(230, 220, 240),  // border
	rgb(185, 140, 217),  // accent
	rgb(224, 205, 240),  // accent_dim
	rgb( 91,  58, 118),  // accent_fg
	rgb( 58,  49,  66),  // text
	rgb(125, 114, 137),  // text_dim
	rgb(208, 138,  41),  // warn
	rgb(212,  99, 127),  // hot
	rgb( 92, 185, 143),  // ok
	rgb(239, 231, 245),  // trough

	rgb(253, 250, 255),  // plot_panel
	rgb(236, 226, 243),  // plot_grid
	rgb(168, 110, 205),  // series_cpu
	rgb(224, 135, 187),  // series_gpu
	rgb( 86, 196, 167),  // series_fan1
	rgb(143, 166, 239),  // series_fan2
	rgb(214, 205, 224),  // rotor_idle
};

} // namespace

const VictusPalette &victus_palette()
{
	return kPalette;
}

std::string victus_hex(const VictusColor &colour)
{
	char buffer[8];
	snprintf(buffer, sizeof(buffer), "#%02x%02x%02x",
	         static_cast<int>(colour.r * 255.0 + 0.5),
	         static_cast<int>(colour.g * 255.0 + 0.5),
	         static_cast<int>(colour.b * 255.0 + 0.5));
	return buffer;
}
