# Board 8BC8 — Victus by HP Gaming Laptop 15-fa1xxx

What it takes for the `hp-wmi` driver to recognise this machine. Two ways to
get there: a patch for mainline, and a DKMS package for running it today.

Nothing here is needed to use victus-control with the out-of-tree module the
main README installs. This is for driving the fans through the kernel's *own*
driver instead.

## The problem

`8BC8` is missing from the driver's board table. Its neighbours `8BC2`, `8BCA`
and `8BCD` are listed; this one never was, in mainline or in the out-of-tree
module. Without the entry the driver does not wire up fan control on this
machine at all.

The firmware makes it worse by lying. `HPWMI_GET_SYSTEM_DESIGN_DATA` byte 4
reads 0, meaning "no software fan support", which is untrue: the EC accepts
manual fan targets and both fans reach them exactly. Upstream never consults
that byte for fan control — it reads it only for the Omen thermal policy
version — so listing the board is genuinely all it takes.

Verified on the hardware: both fans reach a requested 4000 RPM, MAX reaches
5100 RPM, the automatic reset hands them back to the firmware curve, and the
low-power, balanced and performance profiles are accepted and change real fan
behaviour (2200/2000 RPM balanced against 2500/2400 performance).

## The kernel patch

`0001-platform-x86-hp-wmi-Add-Victus-15-fa1xxx-fan-and-the.patch` adds the
board to `hp_wmi_feature_boards[]`. Four lines, one table entry, against
`torvalds/master`. `checkpatch --strict` is clean.

## The DKMS package

For a kernel that already ships the Victus S support — 6.20 onwards — but not
this board. It builds the stock driver of your kernel series with the entry
added, and DKMS rebuilds it on each kernel update.

    cd dkms
    ./fetch-source.sh            # downloads hp-wmi.c for the running kernel, applies the entry
    sudo dkms remove -m hp-wmi-fan-and-backlight-control -v 0.0.2 --all
    sudo make install-dkms
    sudo modprobe -r hp_wmi && sudo modprobe hp_wmi

The driver source is not kept in this repository, the same way the out-of-tree
module is not: it is the kernel's own file. `fetch-source.sh` pulls it from
kernel.org and applies `8bc8.patch`, which is the whole difference.

The board table has been restructured at least once, so check where the entry
belongs before assuming the patch applies: in 7.2.5 it goes in
`victus_s_thermal_profile_boards[]`, a `dmi_system_id[]` carrying per-board
`driver_data`, while later trees use `hp_wmi_feature_boards[]` with
`*_board_params`.

`force_fan_control_support=1` is a parameter of the *out-of-tree* module and
does not exist here. It is not needed: upstream gates fan control on the board
table alone.

### Which thermal params

The entry uses `victus_s_thermal_params` — profile bytes `0x00`/`0x01`, no EC
readback — because those are the values the out-of-tree module sends on this
machine and they are known to work.

The neighbouring boards use `omen_v1_thermal_params` instead: bytes
`0x30`/`0x31` plus thermal profile readback from EC offset `0x59`. That may be
the better fit and would replace the driver's "Unknown EC layout" warning with
a working readback, but it sends different bytes to the EC and has not been
tested here.

## STOP: this is not a drop-in replacement for victus-control

The upstream driver exposes a *different* hwmon interface from the out-of-tree
one, and victus-control is written against the out-of-tree one.

    upstream:  HWMON_CHANNEL_INFO(fan, HWMON_F_INPUT, HWMON_F_INPUT)
               HWMON_CHANNEL_INFO(pwm, HWMON_PWM_ENABLE | HWMON_PWM_INPUT)

That is `fan1_input` and `fan2_input` only, read-only — every fan attribute
returns 0444. There is no `fan1_target`, no `fan2_target`, no `fan1_max`.
Manual speed goes through `pwm1_enable` (0 max / 1 manual / 2 auto, the same
encoding the backend already uses) plus `pwm1` as a 0-255 PWM value: one
channel for both fans, the second derived from a fixed delta.

Installing this without adapting the backend first would break
`set-fan-speed.sh`, make `fan_target_support_in()` report UNSUPPORTED — so the
UI would drop MANUAL and Better Auto from the profile selector — and send
`fan_max_for_index()` to its fallback. It also means giving up independent
per-fan RPM control.

So the migration is an interface change in victus-control, not a module swap.
The sensible shape is to detect which interface the loaded driver offers and
drive either one, which would also make the app work for anyone on the stock
driver.

One thing it would buy immediately: CoolerControl and friends need a `pwm1`
file to write to, which the out-of-tree module does not expose. Only the stock
driver does.

## Reverting

    cd dkms && sudo make uninstall-dkms
    sudo modprobe -r hp_wmi && sudo modprobe hp_wmi

The distribution's own module comes back. Without `8BC8` there is no fan
control, but the firmware curve runs the fans and nothing else is affected.
