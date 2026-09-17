#ifndef FAN_HPP
#define FAN_HPP

#include <gtk/gtk.h>
#include <atomic>
#include <mutex>
#include <memory>
#include <string>
#include <vector>
#include "socket.hpp"

class VictusFanControl
{
public:
	GtkWidget *fan_page;

	VictusFanControl(std::shared_ptr<VictusSocketClient> client);

	GtkWidget *get_page();

private:
    // Cooling profile: four linked toggles rather than a dropdown, so every
    // option is visible and one click away instead of hidden behind a popup.
    struct ModeButton {
        GtkWidget *button = nullptr;
        std::string mode;
        VictusFanControl *owner = nullptr;
    };
    GtkWidget *mode_bar = nullptr;
    std::vector<std::unique_ptr<ModeButton>> mode_buttons;
    void add_mode_button(const char *label, const char *mode);
    void connect_mode_buttons();
    void select_mode_button(const std::string &mode);

    GtkWidget *speed_slider;
    GtkWidget *slider_label;

    // Labels for displaying current state
	GtkWidget *state_label;

	// Analog dials, drawn in gauges.cpp, with the digital value under each.
	GtkWidget *fan1_gauge;
	GtkWidget *fan2_gauge;
	GtkWidget *cpu_gauge;
	GtkWidget *gpu_gauge;
	GtkWidget *manual_speed_box;   // hidden outright on boards without support

	// Rolling telemetry, so the card shows a trend and not just an instant:
	// one sample per refresh tick, oldest first, newest at the right edge.
	// Five minutes is long enough to see a compile or a match, short enough
	// that a spike is still visible rather than a single pixel.
	static constexpr size_t kHistoryCapacity = 150;   // x 2 s tick = 5 minutes
	static constexpr int kHistorySpanMinutes = 5;
	GtkWidget *history_plot = nullptr;
	std::vector<double> history_cpu_c;
	std::vector<double> history_gpu_c;
	std::vector<double> history_fan1_rpm;
	std::vector<double> history_fan2_rpm;
	// A negative sample means "no reading", which the plot draws as a gap.
	void push_history(double cpu_c, double gpu_c, double fan1, double fan2);

	// Last mode the UI was told about, so the status line can name it without
	// a GET_FAN_MODE round trip on every refresh tick.
	std::string last_known_mode = "AUTO";

	// Better Auto's decision, drawn as a lit-segment meter beside the wording.
	GtkWidget *level_meter = nullptr;
	int better_auto_level = 0;
	int better_auto_steps = 8;

	// Numeric state the dials draw from, kept alongside the label strings.
	double fan1_rpm = 0.0;
	double fan2_rpm = 0.0;
	double cpu_celsius = 0.0;
	double gpu_celsius = 0.0;
	bool cpu_valid = false;
	bool gpu_valid = false;

	// Rotor angles advance from the measured RPM, so the blades visibly track
	// how hard the fans are actually working.
	double fan1_angle = 0.0;
	double fan2_angle = 0.0;
	guint gauge_tick_id = 0;
	gint64 gauge_last_frame_us = 0;
	GtkWidget *fan1_speed_label;
	GtkWidget *fan2_speed_label;
	GtkWidget *cpu_temp_label;
	GtkWidget *gpu_temp_label;

	void update_fan_speeds();
	void update_ui_from_system_state();
    void set_fan_rpm(int level);

    // Signal handlers
	static void on_mode_button_toggled(GtkToggleButton *button, gpointer data);
	static void on_speed_slider_changed(GtkRange *range, gpointer data);
	static gboolean on_gauge_tick(gpointer data);
	static void draw_fan1(GtkDrawingArea *area, cairo_t *cr, int w, int h, gpointer data);
	static void draw_fan2(GtkDrawingArea *area, cairo_t *cr, int w, int h, gpointer data);
	static void draw_cpu(GtkDrawingArea *area, cairo_t *cr, int w, int h, gpointer data);
	static void draw_gpu(GtkDrawingArea *area, cairo_t *cr, int w, int h, gpointer data);
	static void draw_history(GtkDrawingArea *area, cairo_t *cr, int w, int h, gpointer data);
	static void draw_level(GtkDrawingArea *area, cairo_t *cr, int w, int h, gpointer data);

	// False when the driver exposes no fan*_target for this board, which
	// means MANUAL speed cannot work no matter what the slider is set to.
	bool fan_targets_supported = true;

	std::shared_ptr<VictusSocketClient> socket_client;
    // Fan 2 cannot be written until kFanApplyGap after fan 1, and the socket
    // client serialises every call, so the wait cannot happen inline without
    // freezing the UI. The deferred write used to be *cancelled* by any newer
    // request, which is why dragging the slider left fan 2 stuck at an old
    // speed while fan 1 followed every step. Now a single writer runs and
    // always sends the newest value instead.
    std::mutex manual_mutex;
    int pending_fan2_rpm = 0;
    std::atomic<bool> fan2_writer_active{false};
    // Set while a periodic refresh worker is running so a slow backend call
    // (GET_GPU_TEMP -> `timeout 3 nvidia-smi`) can't pile up overlapping workers.
    std::atomic<bool> refresh_in_flight{false};
};

#endif // FAN_HPP
