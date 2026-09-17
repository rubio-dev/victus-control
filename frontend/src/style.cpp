#include <gtk/gtk.h>

#include <string>

#include "palette.hpp"
#include "style.hpp"

namespace {

// A light palette: off-white structure, pastel accents, and temperature keeping
// the only loud colours in the app so that hue always means something. The
// values live in palette.cpp, shared with the cairo gauges; the definitions
// below are generated from them so the two can never drift apart.
std::string colour_definitions()
{
	const VictusPalette &p = victus_palette();
	std::string css;

	auto define = [&css](const char *name, const VictusColor &colour) {
		css += "@define-color " + std::string(name) + " " + victus_hex(colour) + ";\n";
	};

	define("victus_bg", p.bg);
	define("victus_surface", p.surface);
	define("victus_surface_alt", p.surface_alt);
	define("victus_border", p.border);
	define("victus_accent", p.accent);
	define("victus_accent_dim", p.accent_dim);
	define("victus_accent_fg", p.accent_fg);
	define("victus_text", p.text);
	define("victus_text_dim", p.text_dim);
	define("victus_warn", p.warn);
	define("victus_hot", p.hot);
	define("victus_ok", p.ok);
	define("victus_trough", p.trough);

	// GTK draws radios, focus rings and selections from its own accent colours,
	// which follow the desktop's accent (orange on Ubuntu) and would otherwise
	// clash with everything here. Point them at the app accent.
	css += "@define-color accent_color @victus_accent;\n";
	css += "@define-color accent_bg_color @victus_accent;\n";
	css += "@define-color accent_fg_color @victus_accent_fg;\n";
	css += "@define-color theme_selected_bg_color @victus_accent;\n";
	css += "@define-color theme_selected_fg_color @victus_accent_fg;\n";

	return css;
}

const char *kVictusCssBody = R"CSS(
window,
.victus-root {
  background-color: @victus_bg;
  color: @victus_text;
  font-family: "Inter", "Cantarell", "Segoe UI", sans-serif;
}

/* Title bar: a slim carbon strip with an accent hairline under it. */
headerbar {
  background: linear-gradient(180deg, @victus_surface 0%, @victus_surface_alt 100%);
  border-bottom: 1px solid @victus_border;
  min-height: 42px;
  padding: 0 6px;
}

headerbar label {
  font-weight: 700;
  letter-spacing: 2px;
  color: @victus_text;
}

/* Tabs read as angular console buttons, with the active one underlined. */
notebook > header {
  background-color: @victus_bg;
  border-bottom: 1px solid @victus_border;
  padding-top: 4px;
}

notebook > header > tabs > tab {
  background-color: transparent;
  border: none;
  border-bottom: 2px solid transparent;
  border-radius: 0;
  padding: 8px 22px;
  margin: 0 2px;
  min-height: 30px;
  color: @victus_text_dim;
  font-weight: 700;
  letter-spacing: 1.6px;
  transition: color 160ms ease, border-color 160ms ease, background-color 160ms ease;
}

notebook > header > tabs > tab:hover {
  color: @victus_text;
  background-color: alpha(@victus_accent, 0.10);
}

notebook > header > tabs > tab:checked {
  color: @victus_accent;
  border-bottom-color: @victus_accent;
  background-color: alpha(@victus_accent, 0.16);
}

notebook > stack,
scrolledwindow,
scrolledwindow > viewport {
  background-color: @victus_bg;
}

scrollbar {
  background-color: @victus_bg;
  border: none;
}

scrollbar slider {
  background-color: @victus_border;
  border-radius: 3px;
  min-width: 7px;
}

scrollbar slider:hover {
  background-color: @victus_accent_dim;
}

/* Grouped panels. */
.victus-card {
  background-color: @victus_surface;
  border: 1px solid @victus_border;
  border-radius: 10px;
  padding: 16px;
  box-shadow: 0 1px 3px alpha(@victus_text, 0.07);
}

.victus-card:hover {
  border-color: alpha(@victus_accent, 0.55);
}

/* Small uppercase heading that labels a panel. */
.section-title {
  font-size: 11px;
  font-weight: 800;
  letter-spacing: 2.4px;
  color: @victus_accent_fg;
}

.section-icon {
  color: @victus_accent;
  -gtk-icon-size: 16px;
}

.tile-icon {
  color: @victus_text_dim;
  -gtk-icon-size: 13px;
}

/* Explains a control that hardware will not honour. */
.notice {
  color: @victus_warn;
  font-size: 12px;
  background-color: alpha(@victus_warn, 0.12);
  border-left: 3px solid @victus_warn;
  border-radius: 2px;
  padding: 8px 10px;
}

.field-label {
  color: @victus_text_dim;
  font-size: 12px;
  font-weight: 600;
  letter-spacing: 1.1px;
}

/* Numeric telemetry: monospace so digits do not jitter as values change. */
.readout {
  font-family: "JetBrains Mono", "Fira Code", "DejaVu Sans Mono", monospace;
  font-size: 22px;
  font-weight: 700;
  color: @victus_accent_fg;
}

.readout.warn { color: @victus_warn; }
.readout.hot  { color: @victus_hot; }
.readout.ok   { color: @victus_ok; }

.readout-unit {
  font-family: "JetBrains Mono", "DejaVu Sans Mono", monospace;
  font-size: 11px;
  font-weight: 600;
  color: @victus_text_dim;
}

.status-line {
  color: @victus_text_dim;
  font-size: 12px;
  letter-spacing: 0.6px;
}

/* Buttons: squared off, hairline border, accent wash on hover. */
button {
  background: linear-gradient(180deg, @victus_surface_alt 0%, @victus_surface 100%);
  border: 1px solid @victus_border;
  border-radius: 3px;
  color: @victus_text;
  padding: 9px 16px;
  font-weight: 700;
  letter-spacing: 1.2px;
  transition: all 160ms ease;
}

button:hover {
  border-color: @victus_accent;
  color: @victus_accent;
  box-shadow: 0 1px 4px alpha(@victus_accent, 0.35);
}

button:active {
  background: @victus_accent_dim;
  color: @victus_accent_fg;
}

button:disabled {
  color: alpha(@victus_text_dim, 0.5);
  border-color: alpha(@victus_border, 0.6);
  box-shadow: none;
}

/* The one prominent action on a page. */
button.primary-action {
  background: linear-gradient(180deg, @victus_accent 0%, @victus_accent_dim 100%);
  border-color: @victus_accent;
  color: @victus_accent_fg;
  font-weight: 800;
}

button.primary-action:hover {
  box-shadow: 0 2px 8px alpha(@victus_accent, 0.45);
  color: @victus_accent_fg;
}

/* Power toggle reads as lit when the backlight is on. */
button.power-toggle {
  font-size: 13px;
  padding: 12px;
}

button.power-toggle.is-on {
  border-color: @victus_ok;
  color: @victus_ok;
  background: alpha(@victus_ok, 0.14);
}

button.power-toggle.is-off {
  border-color: @victus_border;
  color: @victus_text_dim;
}

/* Material-style backlight switch: pill track, circular knob, accent when on. */
switch {
  background-color: @victus_trough;
  border: 1px solid @victus_border;
  border-radius: 15px;
  min-width: 50px;
  min-height: 26px;
  padding: 2px;
  transition: background-color 200ms ease, border-color 200ms ease,
              box-shadow 200ms ease;
}

switch:checked {
  background-color: @victus_accent_dim;
  border-color: @victus_accent;
  box-shadow: 0 1px 3px alpha(@victus_text, 0.12);
}

switch > slider {
  background: @victus_surface;
  border: none;
  border-radius: 50%;
  min-width: 20px;
  min-height: 20px;
  margin: 0;
  transition: background 200ms ease;
}

switch:checked > slider {
  background: #ffffff;
}

switch:disabled {
  opacity: 0.45;
}

/* Style selector: radios that read as chips rather than form controls. */
checkbutton.style-radio {
  color: @victus_text_dim;
  font-weight: 700;
  letter-spacing: 0.8px;
  padding: 4px 6px;
  transition: color 160ms ease;
}

checkbutton.style-radio:hover {
  color: @victus_text;
}

checkbutton.style-radio:checked {
  color: @victus_accent;
}

checkbutton.style-radio > radio,
checkbutton.style-radio > check {
  background-color: @victus_surface;
  border: 1px solid @victus_border;
  min-width: 15px;
  min-height: 15px;
  transition: all 160ms ease;
}

checkbutton.style-radio:checked > radio,
checkbutton.style-radio:checked > check,
checkbutton.style-radio radio:checked,
checkbutton.style-radio check:checked {
  background-color: @victus_accent;
  background-image: none;
  border-color: @victus_accent;
  color: @victus_accent_fg;
}

/* Cooling profile: four linked toggles that read as one control. Every option
   is on screen, so choosing a profile is one click and no popup. */
.mode-bar {
  margin-top: 2px;
}

button.mode-button {
  background: @victus_surface;
  border: 1px solid @victus_border;
  color: @victus_text_dim;
  font-size: 11px;
  font-weight: 800;
  letter-spacing: 1.2px;
  padding: 10px 12px;
  box-shadow: none;
}

button.mode-button:hover {
  background: alpha(@victus_accent, 0.10);
  color: @victus_accent_fg;
  border-color: @victus_accent_dim;
  box-shadow: none;
}

button.mode-button:checked {
  background: linear-gradient(180deg, @victus_accent 0%, @victus_accent_dim 100%);
  border-color: @victus_accent;
  color: @victus_accent_fg;
}

button.mode-button:checked:hover {
  background: @victus_accent;
  color: @victus_accent_fg;
}

button.mode-button:disabled {
  background: @victus_surface_alt;
  color: alpha(@victus_text_dim, 0.6);
}

/* Sliders: thin dark rail, glowing accent fill, square-ish knob. */
scale {
  min-height: 26px;
}

scale trough {
  background-color: @victus_trough;
  border: 1px solid @victus_border;
  border-radius: 2px;
  min-height: 6px;
}

scale highlight {
  background: linear-gradient(90deg, @victus_accent_dim 0%, @victus_accent 100%);
  border-radius: 2px;
  box-shadow: none;
}

scale slider {
  background: @victus_surface;
  border: 2px solid @victus_accent;
  border-radius: 3px;
  min-width: 14px;
  min-height: 14px;
  margin: -6px;
  box-shadow: 0 1px 3px alpha(@victus_text, 0.18);
  transition: box-shadow 160ms ease;
}

scale slider:hover {
  box-shadow: 0 1px 6px alpha(@victus_accent, 0.55);
}

scale value {
  font-family: "JetBrains Mono", "DejaVu Sans Mono", monospace;
  font-weight: 700;
  color: @victus_accent_fg;
}

/* Dropdowns. */
dropdown > button,
combobox button.combo {
  background: @victus_surface_alt;
  border: 1px solid @victus_border;
  border-radius: 3px;
  color: @victus_text;
  letter-spacing: 1px;
}

dropdown > button:hover,
combobox button.combo:hover {
  border-color: @victus_accent;
  color: @victus_accent;
}

popover > contents,
dropdown popover > contents {
  background-color: @victus_surface;
  border: 1px solid @victus_border;
  border-radius: 8px;
  box-shadow: 0 2px 10px alpha(@victus_text, 0.12);
  color: @victus_text;
  padding: 4px;
}

popover listview > row:selected,
dropdown listview > row:selected {
  background-color: alpha(@victus_accent, 0.18);
  color: @victus_accent;
}

/* The drawn keyboard sits in its own recessed well. */
.keyboard-stage {
  background-color: @victus_surface_alt;
  border: 1px solid @victus_border;
  border-radius: 4px;
  padding: 10px;
}

separator {
  background-color: @victus_border;
  min-height: 1px;
  min-width: 1px;
}

dialog,
messagedialog,
aboutdialog,
.background {
  background-color: @victus_bg;
  color: @victus_text;
}

aboutdialog label,
dialog label {
  color: @victus_text;
}

tooltip {
  background-color: @victus_surface;
  border: 1px solid @victus_accent_dim;
  color: @victus_text;
}
)CSS";

} // namespace

void apply_victus_style() {
  // No colour-scheme is forced here on purpose. Asking GTK for a dark scheme
  // makes it load the system theme's dark variant, and themes that ship only a
  // gtk-dark.css (Yaru among them) fail that import and drop their whole base
  // stylesheet. The rules below paint every surface this app shows instead, so
  // it looks the same either way.

  GtkCssProvider *provider = gtk_css_provider_new();
  const std::string css = colour_definitions() + kVictusCssBody;
  gtk_css_provider_load_from_string(provider, css.c_str());

  gtk_style_context_add_provider_for_display(
      gdk_display_get_default(), GTK_STYLE_PROVIDER(provider),
      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  g_object_unref(provider);
}
