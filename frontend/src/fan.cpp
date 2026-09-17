#include "fan.hpp"
#include "gauges.hpp"
#include "icons.hpp"
#include "palette.hpp"
#include "socket.hpp"
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <memory>
#include <sstream>
#include <vector>

namespace {
// Carries the strings produced by the off-thread refresh back to the GTK main
// thread, where the label writes must happen.
struct FanLabelUpdate {
    VictusFanControl *self;
    std::string fan1;
    std::string fan2;
    std::string cpu;
    std::string gpu;
    std::string better_auto;   // "<level> <steps> <TEMP|LOAD> <value>" or "INACTIVE"
};
} // namespace

// Constants for manual fan control
const int MIN_RPM = 2000;
// Measured on the hardware in MAX: both fans top out at ~5100. The driver
// reports fanN_max as 0 on this board, so nothing can read it back at runtime.
const int FAN1_MAX_RPM = 5100;
const int FAN2_MAX_RPM = 5100;
const int RPM_STEPS = 8;

namespace {

// Temperature readouts change colour as they climb, so a hot machine is
// obvious without reading the number.
void apply_temperature_class(GtkWidget *label, const std::string &value)
{
    gtk_widget_remove_css_class(label, "warn");
    gtk_widget_remove_css_class(label, "hot");

    try {
        int degrees = std::stoi(value);
        if (degrees >= 85)
            gtk_widget_add_css_class(label, "hot");
        else if (degrees >= 70)
            gtk_widget_add_css_class(label, "warn");
    } catch (...) {
        // "idle" or "N/A": leave it in the default accent colour.
    }
}

// One telemetry dial: caption, the analog gauge, then the digital value under
// it, so the shape gives the impression and the number gives the detail.
GtkWidget *make_gauge_tile(const char *caption, const char *unit,
                           VictusIcon icon, int gauge_width,
                           int gauge_height, GtkDrawingAreaDrawFunc draw_func,
                           gpointer draw_data, GtkWidget **gauge_out,
                           GtkWidget **value_out)
{
    GtkWidget *tile = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_hexpand(tile, TRUE);
    gtk_widget_set_halign(tile, GTK_ALIGN_CENTER);

    GtkWidget *caption_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *caption_icon = victus_icon_new(icon, 14, victus_palette().text_dim);
    GtkWidget *caption_label = gtk_label_new(caption);
    gtk_widget_add_css_class(caption_label, "field-label");
    gtk_box_append(GTK_BOX(caption_row), caption_icon);
    gtk_box_append(GTK_BOX(caption_row), caption_label);
    gtk_widget_set_halign(caption_row, GTK_ALIGN_CENTER);

    GtkWidget *gauge = gtk_drawing_area_new();
    gtk_widget_set_size_request(gauge, gauge_width, gauge_height);
    gtk_widget_set_halign(gauge, GTK_ALIGN_CENTER);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(gauge), draw_func,
                                   draw_data, nullptr);

    GtkWidget *value_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_set_halign(value_row, GTK_ALIGN_CENTER);
    GtkWidget *value = gtk_label_new("--");
    gtk_widget_add_css_class(value, "readout");
    GtkWidget *unit_label = gtk_label_new(unit);
    gtk_widget_add_css_class(unit_label, "readout-unit");
    gtk_widget_set_valign(unit_label, GTK_ALIGN_END);
    gtk_widget_set_margin_bottom(unit_label, 4);
    gtk_box_append(GTK_BOX(value_row), value);
    gtk_box_append(GTK_BOX(value_row), unit_label);

    gtk_box_append(GTK_BOX(tile), caption_row);
    gtk_box_append(GTK_BOX(tile), gauge);
    gtk_box_append(GTK_BOX(tile), value_row);

    *gauge_out = gauge;
    *value_out = value;
    return tile;
}

// Turn the backend's "3 8 TEMP 55" into something worth reading, plus the
// numbers behind it. Better Auto decides a level every couple of seconds and
// used to say nothing about it, so the card showed a greyed-out slider and left
// the user guessing.
struct BetterAutoStatus {
    bool active = false;
    int level = 0;
    int steps = 8;
    std::string text;
};

BetterAutoStatus describe_better_auto(const std::string &status, const std::string &mode)
{
    BetterAutoStatus out;
    std::istringstream iss(status);
    std::string driver;
    int value = 0;

    if (!(iss >> out.level >> out.steps >> driver >> value) || out.level <= 0 ||
        out.steps <= 0) {
        out.level = 0;
        out.text = "Current State: " + mode;
        return out;
    }

    std::string reason = (driver == "LOAD")
        ? "following load, " + std::to_string(value) + "%"
        : "following temperature, " + std::to_string(value) + "\u00b0C";

    out.active = true;
    out.text = "Better Auto \u2014 level " + std::to_string(out.level) + " of " +
               std::to_string(out.steps) + " \u00b7 " + reason;
    return out;
}

} // namespace

VictusFanControl::VictusFanControl(std::shared_ptr<VictusSocketClient> client) : socket_client(client)
{
    fan_page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_set_margin_top(fan_page, 20);
    gtk_widget_set_margin_bottom(fan_page, 20);
    gtk_widget_set_margin_start(fan_page, 20);
    gtk_widget_set_margin_end(fan_page, 20);

    // --- Header ---
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *header_icon = victus_icon_new(VictusIcon::Fan, 18, victus_palette().accent_fg);
    GtkWidget *header_label = gtk_label_new("COOLING");
    gtk_widget_add_css_class(header_label, "section-title");

    gtk_box_append(GTK_BOX(header), header_icon);
    gtk_box_append(GTK_BOX(header), header_label);
    gtk_box_append(GTK_BOX(fan_page), header);

    // --- Cooling profile: one row of linked toggles ---
    mode_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(mode_bar, "linked");
    gtk_widget_add_css_class(mode_bar, "mode-bar");
    gtk_widget_set_halign(mode_bar, GTK_ALIGN_FILL);

    add_mode_button("AUTO", "AUTO");
    add_mode_button("BETTER AUTO", "BETTER_AUTO");
    add_mode_button("MANUAL", "MANUAL");
    add_mode_button("MAX", "MAX");

    gtk_box_append(GTK_BOX(fan_page), mode_bar);

    // --- Analog dials ---
    GtkWidget *dial_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 26);
    gtk_widget_set_halign(dial_row, GTK_ALIGN_CENTER);

    gtk_box_append(GTK_BOX(dial_row),
        make_gauge_tile("FAN 1", "RPM", VictusIcon::Fan, 104, 104,
                        draw_fan1, this, &fan1_gauge, &fan1_speed_label));
    gtk_box_append(GTK_BOX(dial_row),
        make_gauge_tile("FAN 2", "RPM", VictusIcon::Fan, 104, 104,
                        draw_fan2, this, &fan2_gauge, &fan2_speed_label));
    gtk_box_append(GTK_BOX(dial_row),
        make_gauge_tile("CPU", "\u00b0C", VictusIcon::Cpu, 46, 104,
                        draw_cpu, this, &cpu_gauge, &cpu_temp_label));
    gtk_box_append(GTK_BOX(dial_row),
        make_gauge_tile("GPU", "\u00b0C", VictusIcon::Gpu, 46, 104,
                        draw_gpu, this, &gpu_gauge, &gpu_temp_label));

    gtk_box_append(GTK_BOX(fan_page), dial_row);

    // --- Manual speed, under the dials it drives, and only in MANUAL ---
    manual_speed_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_top(manual_speed_box, 4);
    slider_label = gtk_label_new("MANUAL SPEED");
    gtk_widget_add_css_class(slider_label, "field-label");
    gtk_box_append(GTK_BOX(manual_speed_box), slider_label);

    speed_slider = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 1, RPM_STEPS, 1);
    gtk_scale_set_draw_value(GTK_SCALE(speed_slider), TRUE);
    // Above the trough the value sits on top of the label next to it, so put it
    // at the end of the rail instead.
    gtk_scale_set_value_pos(GTK_SCALE(speed_slider), GTK_POS_RIGHT);
    // "4" says nothing on its own; show what the level actually asks the fans for.
    gtk_scale_set_format_value_func(
        GTK_SCALE(speed_slider),
        +[](GtkScale *, double value, gpointer) -> char * {
            int level = std::clamp(static_cast<int>(value), 1, RPM_STEPS);
            double step = static_cast<double>(FAN1_MAX_RPM - MIN_RPM) / (RPM_STEPS - 1);
            int rpm = static_cast<int>(std::round(MIN_RPM + (level - 1) * step));
            return g_strdup_printf("%d \u00b7 %d RPM", level, rpm);
        },
        nullptr, nullptr);
    gtk_widget_set_hexpand(speed_slider, TRUE);
    g_signal_connect(speed_slider, "value-changed", G_CALLBACK(on_speed_slider_changed), this);
    gtk_box_append(GTK_BOX(manual_speed_box), speed_slider);
    gtk_box_append(GTK_BOX(fan_page), manual_speed_box);

    // --- What the controller is doing ---
    GtkWidget *status_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_halign(status_row, GTK_ALIGN_CENTER);

    level_meter = gtk_drawing_area_new();
    gtk_widget_set_size_request(level_meter, 104, 12);
    gtk_widget_set_valign(level_meter, GTK_ALIGN_CENTER);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(level_meter), draw_level, this, nullptr);
    gtk_box_append(GTK_BOX(status_row), level_meter);

    state_label = gtk_label_new("Current State: N/A");
    gtk_widget_add_css_class(state_label, "status-line");
    gtk_box_append(GTK_BOX(status_row), state_label);
    gtk_box_append(GTK_BOX(fan_page), status_row);

    // --- Rolling history ---
    history_plot = gtk_drawing_area_new();
    gtk_widget_set_size_request(history_plot, -1, 128);
    gtk_widget_set_hexpand(history_plot, TRUE);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(history_plot), draw_history,
                                   this, nullptr);
    gtk_box_append(GTK_BOX(fan_page), history_plot);

    // Boards whose firmware refuses software fan control never expose
    // fan*_target, so neither manual speed nor Better Auto (which steers the
    // fans through the same targets) can work there: both are removed rather
    // than shown greyed out. With VICTUS_NO_FAN_CONTROL=1 the backend leaves
    // the fans to the firmware altogether, so the profile selector is locked
    // and the card is telemetry only.
    std::string support;
    {
        auto reply = socket_client->send_command_async(GET_FAN_TARGET_SUPPORT);
        support = reply.get();
    }
    fan_targets_supported = (support == "SUPPORTED");
    const bool fan_control_disabled = (support == "DISABLED");

    if (!fan_targets_supported) {
        gtk_widget_set_visible(manual_speed_box, FALSE);
        gtk_widget_set_visible(status_row, FALSE);
        for (const auto &entry : mode_buttons) {
            if (entry->mode == "MANUAL" || entry->mode == "BETTER_AUTO")
                gtk_widget_set_visible(entry->button, FALSE);
        }
        if (fan_control_disabled)
            gtk_widget_set_sensitive(mode_bar, FALSE);

        const char *text = fan_control_disabled
            ? "Fan control is switched off for this machine "
              "(VICTUS_NO_FAN_CONTROL=1), so the firmware runs the fans. "
              "Speeds and temperatures are still shown."
            : "This board's firmware does not accept fan speed targets, so "
              "manual speed and Better Auto are unavailable. AUTO and MAX "
              "still work.";
        GtkWidget *notice = gtk_label_new(text);
        gtk_label_set_wrap(GTK_LABEL(notice), TRUE);
        gtk_widget_add_css_class(notice, "notice");
        gtk_box_append(GTK_BOX(fan_page), notice);
    }

    // Rotors turn from the measured RPM, so the dials track the real fans.
    gauge_last_frame_us = g_get_monotonic_time();
    gauge_tick_id = g_timeout_add(33, on_gauge_tick, this);

    // select_mode_button() blocks each toggle's handler while it syncs, so
    // reflecting the current mode here cannot re-send it to the backend.
    update_ui_from_system_state();
    connect_mode_buttons();

    update_fan_speeds();

    // Set up a timer to periodically update fan speeds and temps
    g_timeout_add_seconds(2, [](gpointer data) -> gboolean {
        static_cast<VictusFanControl*>(data)->update_fan_speeds();
        return G_SOURCE_CONTINUE;
    }, this);
}

GtkWidget* VictusFanControl::get_page()
{
    return fan_page;
}

void VictusFanControl::update_ui_from_system_state()
{
    auto response = socket_client->send_command_async(GET_FAN_MODE);
    std::string fan_mode = response.get();

    if (fan_mode.find("ERROR") != std::string::npos) {
        fan_mode = "AUTO"; // Default to AUTO on error
        std::cerr << "Failed to get fan mode, defaulting to AUTO." << std::endl;
    }

    last_known_mode = fan_mode;
    gtk_label_set_text(GTK_LABEL(state_label), ("Current State: " + fan_mode).c_str());

    // The slider only means anything in MANUAL, so it is shown there and
    // nowhere else rather than sitting greyed out under every other profile.
    gtk_widget_set_visible(manual_speed_box, fan_targets_supported && fan_mode == "MANUAL");

    if (fan_mode == "MANUAL") {
        select_mode_button("MANUAL");
        gtk_widget_set_sensitive(speed_slider, TRUE);
        gtk_widget_set_sensitive(slider_label, TRUE);
    } else if (fan_mode == "BETTER_AUTO") {
        select_mode_button("BETTER_AUTO");
        gtk_widget_set_sensitive(speed_slider, FALSE);
        gtk_widget_set_sensitive(slider_label, FALSE);
    } else if (fan_mode == "MAX") {
        select_mode_button("MAX");
        gtk_widget_set_sensitive(speed_slider, FALSE);
        gtk_widget_set_sensitive(slider_label, FALSE);
    } else { // AUTO
        select_mode_button("AUTO");
        gtk_widget_set_sensitive(speed_slider, FALSE);
        gtk_widget_set_sensitive(slider_label, FALSE);
    }
}

void VictusFanControl::update_fan_speeds()
{
    // The socket client serialises every call on one connection, and
    // GET_GPU_TEMP makes the backend run `timeout 3 nvidia-smi` when the dGPU is
    // awake. Doing the blocking .get()s on the GTK main thread (this is called
    // from a 2 s g_timeout) freezes the UI for up to ~3 s per tick. So run the
    // round-trips on a worker thread and marshal the label writes back to the
    // main thread with g_idle_add. Skip if a prior refresh is still running so
    // slow ticks don't pile up overlapping workers.
    bool expected = false;
    if (!refresh_in_flight.compare_exchange_strong(expected, true)) {
        return;
    }

    std::thread([this]() {
        auto response1 = socket_client->send_command_async(GET_FAN_SPEED, "1");
        auto response2 = socket_client->send_command_async(GET_FAN_SPEED, "2");
        auto response_temp = socket_client->send_command_async(GET_CPU_TEMP);
        auto response_gpu_temp = socket_client->send_command_async(GET_GPU_TEMP);
        auto response_better_auto = socket_client->send_command_async(GET_BETTER_AUTO_STATUS);

        std::string fan1_speed = response1.get();
        if (fan1_speed.find("ERROR") != std::string::npos) fan1_speed = "N/A";

        std::string fan2_speed = response2.get();
        if (fan2_speed.find("ERROR") != std::string::npos) fan2_speed = "N/A";

        std::string cpu_temp = response_temp.get();
        if (cpu_temp.find("ERROR") != std::string::npos) cpu_temp = "N/A";

        // GPU: "IDLE" means the dGPU is runtime-suspended (no reading, not an error).
        std::string gpu_temp = response_gpu_temp.get();
        std::string gpu_temp_text;
        if (gpu_temp == "IDLE") {
            gpu_temp_text = "idle";
        } else if (gpu_temp.find("ERROR") != std::string::npos) {
            gpu_temp_text = "N/A";
        } else {
            gpu_temp_text = gpu_temp;
        }

        // The tiles carry their own captions and units, so only the value goes here.
        std::string better_auto = response_better_auto.get();
        if (better_auto.find("ERROR") != std::string::npos)
            better_auto = "INACTIVE";

        auto *payload = new FanLabelUpdate{
            this, fan1_speed, fan2_speed, cpu_temp, gpu_temp_text, better_auto};

        g_idle_add(
            +[](gpointer data) -> gboolean {
                std::unique_ptr<FanLabelUpdate> u(static_cast<FanLabelUpdate *>(data));
                VictusFanControl *self = u->self;
                gtk_label_set_text(GTK_LABEL(self->fan1_speed_label), u->fan1.c_str());
                gtk_label_set_text(GTK_LABEL(self->fan2_speed_label), u->fan2.c_str());
                gtk_label_set_text(GTK_LABEL(self->cpu_temp_label), u->cpu.c_str());
                gtk_label_set_text(GTK_LABEL(self->gpu_temp_label), u->gpu.c_str());
                apply_temperature_class(self->cpu_temp_label, u->cpu);
                apply_temperature_class(self->gpu_temp_label, u->gpu);

                // Keep the dials' numeric state in step with the labels.
                auto to_number = [](const std::string &text, double *out) {
                    try { *out = std::stod(text); return true; }
                    catch (...) { *out = 0.0; return false; }
                };
                to_number(u->fan1, &self->fan1_rpm);
                to_number(u->fan2, &self->fan2_rpm);
                self->cpu_valid = to_number(u->cpu, &self->cpu_celsius);
                self->gpu_valid = to_number(u->gpu, &self->gpu_celsius);

                BetterAutoStatus status =
                    describe_better_auto(u->better_auto, self->last_known_mode);
                gtk_label_set_text(GTK_LABEL(self->state_label), status.text.c_str());
                self->better_auto_level = status.level;
                self->better_auto_steps = status.steps;
                if (self->level_meter) {
                    // An empty row of segments under AUTO or MAX says nothing.
                    gtk_widget_set_visible(self->level_meter, status.active);
                    gtk_widget_queue_draw(self->level_meter);
                }

                self->push_history(self->cpu_valid ? self->cpu_celsius : -1.0,
                                   self->gpu_valid ? self->gpu_celsius : -1.0,
                                   self->fan1_rpm, self->fan2_rpm);
                if (self->history_plot)
                    gtk_widget_queue_draw(self->history_plot);

                gtk_widget_queue_draw(self->cpu_gauge);
                gtk_widget_queue_draw(self->gpu_gauge);
                gtk_widget_queue_draw(self->fan1_gauge);
                gtk_widget_queue_draw(self->fan2_gauge);
                self->refresh_in_flight.store(false);
                return G_SOURCE_REMOVE;
            },
            payload);
    }).detach();
}

void VictusFanControl::set_fan_rpm(int level)
{
    if (level < 1 || level > RPM_STEPS) return;

    auto compute_rpm = [](int lvl, int max_rpm) {
        if (RPM_STEPS <= 1) {
            return max_rpm;
        }
        double step = static_cast<double>(max_rpm - MIN_RPM) / static_cast<double>(RPM_STEPS - 1);
        double value = static_cast<double>(MIN_RPM) + static_cast<double>(lvl - 1) * step;
        int rpm = static_cast<int>(std::round(value));
        rpm = std::clamp(rpm, MIN_RPM, max_rpm);
        return rpm;
    };

    const std::string fan1_rpm_str = std::to_string(compute_rpm(level, FAN1_MAX_RPM));
    {
        std::lock_guard<std::mutex> lock(manual_mutex);
        pending_fan2_rpm = compute_rpm(level, FAN2_MAX_RPM);
    }

    // Fan 1 goes out straight away.
    std::thread([this, fan1_rpm_str]() {
        auto result = socket_client->send_command_async(SET_FAN_SPEED, "1 " + fan1_rpm_str).get();
        if (result != "OK")
            std::cerr << "Failed to set fan 1 speed: " << result << std::endl;
    }).detach();

    // Fan 2 is handled by one writer that waits out the gap and then sends
    // whatever the latest request asked for, so moving the slider again
    // supersedes the pending value instead of cancelling it.
    bool expected = false;
    if (!fan2_writer_active.compare_exchange_strong(expected, true))
        return;

    std::thread([this]() {
        int last_sent = -1;
        for (;;) {
            std::this_thread::sleep_for(std::chrono::seconds(10));

            int target = 0;
            {
                std::lock_guard<std::mutex> lock(manual_mutex);
                target = pending_fan2_rpm;
            }

            if (target != last_sent) {
                auto result = socket_client
                                  ->send_command_async(SET_FAN_SPEED,
                                                       "2 " + std::to_string(target))
                                  .get();
                if (result != "OK")
                    std::cerr << "Failed to set fan 2 speed: " << result << std::endl;
                last_sent = target;
                continue;  // it may have moved again while that was in flight
            }

            // Nothing new to send. Stand down, then look once more: a request
            // that landed while we were deciding would have found the writer
            // still marked active and started no replacement.
            fan2_writer_active.store(false, std::memory_order_release);
            {
                std::lock_guard<std::mutex> lock(manual_mutex);
                if (pending_fan2_rpm == last_sent)
                    return;
            }
            bool expected_restart = false;
            if (!fan2_writer_active.compare_exchange_strong(expected_restart, true))
                return;  // someone else picked it up
        }
    }).detach();
}

void VictusFanControl::add_mode_button(const char *label, const char *mode)
{
    auto entry = std::make_unique<ModeButton>();
    entry->mode = mode;
    entry->owner = this;
    entry->button = gtk_toggle_button_new_with_label(label);
    gtk_widget_add_css_class(entry->button, "mode-button");
    gtk_widget_set_hexpand(entry->button, TRUE);

    // One group, so selecting a profile releases the previous one for us.
    if (!mode_buttons.empty())
        gtk_toggle_button_set_group(GTK_TOGGLE_BUTTON(entry->button),
                                    GTK_TOGGLE_BUTTON(mode_buttons.front()->button));

    // The handler is deliberately not connected here. Grouping toggles makes
    // GTK activate the first member, and a "toggled" arriving during
    // construction would send the backend a profile the user never picked --
    // which is how opening the window used to knock the machine out of Better
    // Auto. connect_mode_buttons() wires them up once the UI is in sync.
    gtk_box_append(GTK_BOX(mode_bar), entry->button);
    mode_buttons.push_back(std::move(entry));
}

void VictusFanControl::connect_mode_buttons()
{
    for (const auto &entry : mode_buttons)
        g_signal_connect(entry->button, "toggled", G_CALLBACK(on_mode_button_toggled),
                         entry.get());
}

// Reflect a mode the backend reported, without bouncing it back as a command.
void VictusFanControl::select_mode_button(const std::string &mode)
{
    // Only ever activate the wanted button: the group releases the previous one
    // by itself, and asking GTK to deactivate the group's only active member
    // gets reverted -- firing "toggled" as if the user had clicked it. Blocking
    // every handler, not just the target's, covers that reversal too.
    for (const auto &entry : mode_buttons)
        g_signal_handlers_block_by_func(entry->button,
                                        (gpointer)on_mode_button_toggled, entry.get());

    for (const auto &entry : mode_buttons) {
        if (entry->mode == mode)
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(entry->button), TRUE);
    }

    for (const auto &entry : mode_buttons)
        g_signal_handlers_unblock_by_func(entry->button,
                                          (gpointer)on_mode_button_toggled, entry.get());
}

void VictusFanControl::on_mode_button_toggled(GtkToggleButton *button, gpointer data)
{
    ModeButton *entry = static_cast<ModeButton *>(data);
    // Grouped toggles fire twice per change: once for the button being released
    // and once for the one taking over. Only the latter is a request.
    if (!gtk_toggle_button_get_active(button))
        return;

    VictusFanControl *self = entry->owner;
    const std::string mode_str = entry->mode;

    auto result = self->socket_client->send_command_async(SET_FAN_MODE, mode_str).get();

    if (result == "OK") {
        if (mode_str == "MANUAL") {
            int level = static_cast<int>(gtk_range_get_value(GTK_RANGE(self->speed_slider)));
            self->set_fan_rpm(level);
        } else if (mode_str == "BETTER_AUTO") {
            gtk_widget_set_sensitive(self->speed_slider, FALSE);
            gtk_widget_set_sensitive(self->slider_label, FALSE);
        }
    } else {
        std::cerr << "Failed to set fan mode: " << result << std::endl;
    }

    self->update_ui_from_system_state();
}

void VictusFanControl::on_speed_slider_changed(GtkRange *range, gpointer data)
{
    VictusFanControl *self = static_cast<VictusFanControl*>(data);
    // The slider only commands the fans while MANUAL is the selected profile.
    if (self->last_known_mode != "MANUAL") {
        return;
    }

    int level = static_cast<int>(gtk_range_get_value(range));
    self->set_fan_rpm(level);
}

// --- Analog dials -----------------------------------------------------------

gboolean VictusFanControl::on_gauge_tick(gpointer data)
{
    VictusFanControl *self = static_cast<VictusFanControl *>(data);

    gint64 now_us = g_get_monotonic_time();
    double elapsed = (now_us - self->gauge_last_frame_us) / 1000000.0;
    self->gauge_last_frame_us = now_us;

    // Real fans turn far too fast to render honestly (3000 RPM is 50 rev/s), so
    // the drawn rotation is scaled down while staying proportional to the
    // measured speed.
    const double kVisualRevPerRpmSecond = 1.0 / 1200.0;
    self->fan1_angle += elapsed * self->fan1_rpm * kVisualRevPerRpmSecond * 2.0 * M_PI;
    self->fan2_angle += elapsed * self->fan2_rpm * kVisualRevPerRpmSecond * 2.0 * M_PI;
    self->fan1_angle = std::fmod(self->fan1_angle, 2.0 * M_PI);
    self->fan2_angle = std::fmod(self->fan2_angle, 2.0 * M_PI);

    // Only the rotors animate; the thermometers redraw when a reading lands.
    if (self->fan1_rpm > 0.0)
        gtk_widget_queue_draw(self->fan1_gauge);
    if (self->fan2_rpm > 0.0)
        gtk_widget_queue_draw(self->fan2_gauge);

    return G_SOURCE_CONTINUE;
}

void VictusFanControl::draw_fan1(GtkDrawingArea *, cairo_t *cr, int w, int h, gpointer data)
{
    VictusFanControl *self = static_cast<VictusFanControl *>(data);
    draw_fan_rotor(cr, w, h, self->fan1_angle, self->fan1_rpm / FAN1_MAX_RPM,
                   self->fan1_rpm > 0.0);
}

void VictusFanControl::draw_fan2(GtkDrawingArea *, cairo_t *cr, int w, int h, gpointer data)
{
    VictusFanControl *self = static_cast<VictusFanControl *>(data);
    draw_fan_rotor(cr, w, h, self->fan2_angle, self->fan2_rpm / FAN2_MAX_RPM,
                   self->fan2_rpm > 0.0);
}

void VictusFanControl::draw_cpu(GtkDrawingArea *, cairo_t *cr, int w, int h, gpointer data)
{
    VictusFanControl *self = static_cast<VictusFanControl *>(data);
    draw_thermometer(cr, w, h, self->cpu_celsius, 100.0, self->cpu_valid);
}

void VictusFanControl::draw_gpu(GtkDrawingArea *, cairo_t *cr, int w, int h, gpointer data)
{
    VictusFanControl *self = static_cast<VictusFanControl *>(data);
    draw_thermometer(cr, w, h, self->gpu_celsius, 100.0, self->gpu_valid);
}

void VictusFanControl::draw_history(GtkDrawingArea *, cairo_t *cr, int w, int h, gpointer data)
{
    VictusFanControl *self = static_cast<VictusFanControl *>(data);
    // Both fans share the plot's RPM band, so scale it to the faster of the two
    // ceilings rather than letting fan 2 clip against fan 1's maximum.
    const double max_rpm = std::max(FAN1_MAX_RPM, FAN2_MAX_RPM);
    draw_history_plot(cr, w, h, self->history_cpu_c, self->history_gpu_c,
                      self->history_fan1_rpm, self->history_fan2_rpm,
                      kHistoryCapacity, max_rpm, kHistorySpanMinutes);
}

void VictusFanControl::push_history(double cpu_c, double gpu_c, double fan1, double fan2)
{
    auto push = [](std::vector<double> &series, double value) {
        if (series.size() == kHistoryCapacity)
            series.erase(series.begin());
        series.push_back(value);
    };

    push(history_cpu_c, cpu_c);
    push(history_gpu_c, gpu_c);
    push(history_fan1_rpm, fan1);
    push(history_fan2_rpm, fan2);
}

void VictusFanControl::draw_level(GtkDrawingArea *, cairo_t *cr, int w, int h, gpointer data)
{
    VictusFanControl *self = static_cast<VictusFanControl *>(data);
    draw_level_meter(cr, w, h, self->better_auto_level, self->better_auto_steps);
}
