#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

#include <unistd.h>

#include "fan.hpp"

namespace {

bool expect(bool condition, const char *message) {
  if (condition)
    return true;

  std::cerr << "FAILED: " << message << std::endl;
  return false;
}

// A scratch directory standing in for /sys/devices/platform/hp-wmi/hwmon/hwmonN.
class FakeHwmon {
public:
  FakeHwmon() {
    char pattern[] = "/tmp/victus-fan-test-XXXXXX";
    char *dir = mkdtemp(pattern);
    path_ = dir ? dir : "";
  }

  ~FakeHwmon() {
    if (path_.empty())
      return;
    for (const char *name : {"fan1_target", "fan2_target", "pwm1_enable"})
      unlink((path_ + "/" + name).c_str());
    rmdir(path_.c_str());
  }

  const std::string &path() const { return path_; }

  void touch(const char *name) const {
    std::ofstream file(path_ + "/" + name);
    file << "0\n";
  }

private:
  std::string path_;
};

// One Better Auto tick with both sensors present.
int step(BetterAutoFilter &filter, double temp_c, double usage_pct, int previous_level) {
  BetterAutoReading reading;
  reading.have_temp = true;
  reading.hottest_c = temp_c;
  reading.have_usage = true;
  reading.usage_pct = usage_pct;
  return better_auto_next_level(filter, reading, previous_level);
}

} // namespace

int main() {
  bool ok = true;

  // --- VICTUS_NO_FAN_CONTROL parsing -------------------------------------
  ok &= expect(!fan_control_disabled_by(nullptr),
               "an unset VICTUS_NO_FAN_CONTROL leaves fan control enabled");
  ok &= expect(!fan_control_disabled_by(""),
               "an empty VICTUS_NO_FAN_CONTROL leaves fan control enabled");
  ok &= expect(!fan_control_disabled_by("0"),
               "VICTUS_NO_FAN_CONTROL=0 leaves fan control enabled");
  ok &= expect(fan_control_disabled_by("1"),
               "VICTUS_NO_FAN_CONTROL=1 disables fan control");

  // --- fan target probe ----------------------------------------------------
  ok &= expect(fan_target_support_in("") == "UNSUPPORTED",
               "no hwmon directory means no fan targets");
  ok &= expect(fan_target_support_in("/nonexistent/victus-hwmon") == "UNSUPPORTED",
               "a missing hwmon directory means no fan targets");

  {
    FakeHwmon hwmon;
    ok &= expect(!hwmon.path().empty(), "scratch hwmon directory is created");
    hwmon.touch("pwm1_enable");
    ok &= expect(fan_target_support_in(hwmon.path()) == "UNSUPPORTED",
                 "pwm1_enable alone does not make targets supported");

    hwmon.touch("fan1_target");
    ok &= expect(fan_target_support_in(hwmon.path()) == "UNSUPPORTED",
                 "only one fan target file is not full support");

    hwmon.touch("fan2_target");
    ok &= expect(fan_target_support_in(hwmon.path()) == "SUPPORTED",
                 "both fan target files present means targets are supported");
  }

  // --- Better Auto: a one-sample spike must not move the fans --------------
  {
    // Replay of the real journal: idle at 37 C and 1-12 % load, one 2 s sample
    // reading 83 C from a turbo burst. That sample used to mean level 8.
    BetterAutoFilter filter;
    int level = 1;
    int peak = 1;
    const double temps[] = {37.0, 37.0, 83.0, 49.0, 38.0, 38.0};
    const double loads[] = {1.0, 4.0, 3.0, 5.0, 12.0, 2.0};
    for (int i = 0; i < 6; i++) {
      level = step(filter, temps[i], loads[i], level);
      peak = std::max(peak, level);
    }
    ok &= expect(peak <= 2, "a single hot sample at idle barely moves the level");
    ok &= expect(level == 1, "the level returns to silent once the spike passes");
  }

  // --- Better Auto: sustained heat still reaches maximum -------------------
  {
    BetterAutoFilter filter;
    int level = 1;
    int first = step(filter, 85.0, 60.0, level);
    ok &= expect(first <= 1 + kBetterAutoMaxRisePerTick,
                 "the first hot sample climbs by at most the rise limit");

    level = first;
    for (int i = 0; i < 5; i++)
      level = step(filter, 85.0, 60.0, level);
    ok &= expect(level == better_auto_level_count(),
                 "heat that persists still takes the fans to maximum");
  }

  // --- Better Auto: real heat bypasses the smoothing ----------------------
  {
    BetterAutoFilter filter;
    int level = step(filter, 91.0, 30.0, 1);
    ok &= expect(level < better_auto_level_count(),
                 "one sample above the emergency temperature is still a transient");
    level = step(filter, 91.0, 30.0, level);
    ok &= expect(level == better_auto_level_count(),
                 "two samples in a row above the emergency temperature go straight to maximum");
  }

  {
    BetterAutoFilter filter;
    int level = step(filter, 95.0, 30.0, 1);
    level = step(filter, 40.0, 3.0, level);
    level = step(filter, 40.0, 3.0, level);
    ok &= expect(level < better_auto_level_count(),
                 "a lone spike above the emergency temperature does not arm the emergency path");
  }

  // --- Better Auto: load alone never runs the fans flat out ---------------
  {
    BetterAutoFilter filter;
    int level = 1;
    for (int i = 0; i < 10; i++)
      level = step(filter, 44.0, 100.0, level);
    ok &= expect(level <= kBetterAutoMaxUsageLevel,
                 "a busy but cool machine stays under the load ceiling");
  }

  // --- Better Auto: cooling walks down one step at a time -----------------
  {
    BetterAutoFilter filter;
    int level = 8;
    for (int i = 0; i < 3; i++)
      level = step(filter, 35.0, 1.0, level);
    ok &= expect(level == 5, "the level drops one step per sample, not all at once");
  }

  // --- Better Auto: no flapping on a threshold boundary -------------------
  {
    // Replay of the journal right after the fix went in: the machine idled at
    // 44-47 C, either side of the 45 C boundary, and the level bounced 2-1-2
    // in 35 seconds, swinging the fans about 540 RPM each way.
    BetterAutoFilter filter;
    int level = 1;
    const double temps[] = {47.0, 44.0, 45.0, 44.0, 46.0};
    int levels[5];
    for (int i = 0; i < 5; i++) {
      level = step(filter, temps[i], 3.0, level);
      levels[i] = level;
    }
    bool steady = true;
    for (int i = 1; i < 5; i++)
      steady &= (levels[i] == levels[0]);
    ok &= expect(steady, "a reading sitting on a threshold holds one level");

    // It must still let go once the machine is genuinely cooler.
    for (int i = 0; i < 4; i++)
      level = step(filter, 39.0, 2.0, level);
    ok &= expect(level == 1, "hysteresis gives way to a real drop in temperature");
  }

  // --- Better Auto: no sensors means hold, not guess ----------------------
  {
    BetterAutoFilter filter;
    BetterAutoReading blind;
    ok &= expect(better_auto_next_level(filter, blind, 4) == 4,
                 "with no usable sensor the level holds where it is");
  }

  return ok ? 0 : 1;
}
