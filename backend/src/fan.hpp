#pragma once

#include <array>
#include <cstddef>
#include <string>

void fan_mode_trigger(const std::string mode);
std::string set_fan_mode(const std::string &value);
std::string get_fan_mode();

std::string get_fan_speed(const std::string &fan_num);
std::string get_fan_max_speed(const std::string &fan_num);
std::string set_fan_speed(const std::string &fan_num, const std::string &speed, bool trigger_mode = true, bool update_cache = true);
std::string get_cpu_temperature();
std::string get_gpu_temperature();
// "DISABLED" while VICTUS_NO_FAN_CONTROL=1 is set for the service,
// "SUPPORTED" when the driver exposes fan1_target and fan2_target for this
// board, otherwise "UNSUPPORTED". Boards whose BIOS refuses software fan
// control never get those files, so MANUAL and Better Auto (which drives the
// fans through the same targets) cannot work there; only AUTO and MAX can.
// The UI uses this to offer just the profiles that do something.
std::string get_fan_target_support();
// The rule behind get_fan_target_support(), split out so it can be tested
// against a scratch directory: "SUPPORTED" when both target files exist.
std::string fan_target_support_in(const std::string &hwmon_dir);

// True when VICTUS_NO_FAN_CONTROL=1 is set: the backend then leaves the fans
// to the firmware and every fan-changing command is refused.
bool fan_control_disabled();
// The rule behind fan_control_disabled(), split out so it can be tested.
bool fan_control_disabled_by(const char *value);

// Puts the driver back into AUTO so the firmware curve runs the fans, after
// stopping Better Auto and any MANUAL/MAX watchdog. Used at start-up whenever
// the backend is not going to run the fans itself, so nothing left behind by
// a previous run (a pinned manual target, MAX) stays in effect.
std::string restore_firmware_fan_control();

// --- Better Auto decision rule ---------------------------------------------
//
// A single 2 s sample is not evidence of heat. The CPU package sensor swings
// from 40 to 85 C in milliseconds when one core takes a turbo burst: real
// silicon temperature, but a die hotspot with almost no energy behind it, which
// the heatsink never feels. Deciding on one such sample sent the fans to
// maximum at 3 % CPU load, and since the level walks back down one step per
// apply (~13 s), a 2 s spike cost about 90 s of full-speed noise.
//
// So the rule decides on the median of the last few samples, climbs at most a
// couple of steps at a time, and lets load pre-spin the fans without letting it
// reach the top of the curve on its own. Genuinely sustained heat still
// bypasses all of that through the emergency path below.

// How many temperature samples the decision looks at. Three is enough to throw
// away a one-sample spike while still reacting within two ticks.
inline constexpr std::size_t kBetterAutoTempWindow = 3;
// Consecutive samples at or above the emergency temperature that mean the heat
// is real and the fans should go straight to maximum.
inline constexpr int kBetterAutoEmergencySamples = 2;
inline constexpr double kBetterAutoEmergencyTempC = 90.0;
// Ceiling on the level that CPU/GPU load may ask for by itself. Load leads
// temperature, so it is allowed to pre-spin the fans, but a busy-and-cool
// machine must never run them flat out.
inline constexpr int kBetterAutoMaxUsageLevel = 6;
// Most steps the level may climb in one decision.
inline constexpr int kBetterAutoMaxRisePerTick = 2;
// How far a reading must fall below the threshold it climbed through before the
// level is given up, in degrees C and in percentage points of load. Keeps a
// machine idling on a boundary from swinging the fans every few seconds.
inline constexpr double kBetterAutoHysteresisC = 2.0;
inline constexpr double kBetterAutoHysteresisPct = 5.0;

// The rolling state the rule carries between decisions.
struct BetterAutoFilter {
	std::array<double, kBetterAutoTempWindow> samples{};
	std::size_t count = 0;    // samples collected so far, saturating at the window
	std::size_t next = 0;     // next slot to overwrite
	int hot_streak = 0;       // consecutive samples at or above the emergency temperature
	double smoothed_c = 0.0;  // median behind the last decision, for the log line
	double usage_pct = 0.0;   // load the last decision saw
	bool usage_driven = false;// true when load, not temperature, asked for the level
};

// One tick of sensor input, already reduced to the hottest temperature and the
// busiest of CPU/GPU. Either half may be missing when a sensor is unavailable.
struct BetterAutoReading {
	bool have_temp = false;
	double hottest_c = 0.0;
	bool have_usage = false;
	double usage_pct = 0.0;
};

// The rule itself: the fan level (1..kBetterAutoSteps) for this tick, given the
// previous level. Pure apart from the filter it updates, so it can be tested
// without hardware.
int better_auto_next_level(BetterAutoFilter &filter, const BetterAutoReading &reading,
                           int previous_level);
// Number of fan levels the rule works in; the top one is maximum RPM.
int better_auto_level_count();

// What Better Auto is doing right now, for the UI to show instead of leaving
// the user staring at a greyed-out slider: "<level> <steps> <TEMP|LOAD> <value>"
// (e.g. "3 8 TEMP 55"), or "INACTIVE" when the control loop is not running.
std::string get_better_auto_status();

std::string ensure_better_auto_mode();
void shutdown_fan_controller();
