# Generator App Configuration & Control Guide

## Table of Contents
- [Quick Start](#quick-start)
- [Configuration via Terminal](#configuration-via-terminal)
- [How the Control Loop Works](#how-the-control-loop-works)
- [Parameter Tuning Guide](#parameter-tuning-guide)
- [Use Cases](#use-cases)
- [Safety & Limitations](#safety--limitations)

---

## Quick Start

### 1. Flash Firmware
1. Build firmware: `make fw_410` (or your target)
2. Flash `build/410/410.bin` to your VESC
3. In VESC Tool: **App Configuration** → **App to Use** → **Custom**
4. Write app configuration to VESC

### 2. Basic Configuration
Open VESC Tool Terminal and type:
```
gen_config
```

This shows current settings (defaults):
```
Generator Configuration:
  Target ERPM:    2000.0
  Max Current:    20.0 A
  Start Ratio:    0.90 (1800.0 ERPM)
  Update Rate:    1000 Hz
```

### 3. Test Operation
Manually spin the motor and monitor:
```
gen_status
```

Output example:
```
Generator Status:
  Current RPM:    1850.0 (forward)
  Target ERPM:    2000.0
  Start ERPM:     1800.0
  Active:         YES
  Gen Current:    10.5 A
  Running:        YES
```

---

## Configuration via Terminal

### Command: `gen_config`

**View current settings:**
```
gen_config
```

**Set all parameters:**
```
gen_config <erpm> <current> <start> <rate_hz>
```

**Example - Conservative Wind Generator:**
```
gen_config 3000 10 0.85 1000
```
- Targets 3000 ERPM for full generation
- Maximum 10A regenerative current
- Starts generating at 2550 ERPM (85% of target)
- Updates control loop 1000 times per second

**Example - Aggressive E-Bike Regen:**
```
gen_config 1500 30 0.80 1000
```
- Targets 1500 ERPM for full generation
- Maximum 30A regenerative current
- Starts at 1200 ERPM (80% of target)
- Fast engagement for braking feel

### Command: `gen_status`

**Monitor real-time operation:**
```
gen_status
```

Shows:
- Current motor RPM (forward/reverse)
- Target and start ERPM thresholds
- Whether generation is active
- Actual current being applied
- App running status

**Use during testing** to verify behavior before connecting loads!

---

## How the Control Loop Works

### Overview

The generator app implements **proportional current control** based on motor speed. It creates a **virtual resistance** that increases linearly with RPM, converting kinetic energy into electrical energy (charging battery).

### Control Algorithm

```
┌─────────────────────────────────────────────────────────┐
│                   GENERATOR CONTROL LOOP                │
│                    (Runs @ 1000 Hz)                     │
└─────────────────────────────────────────────────────────┘

  INPUT: Motor RPM (measured by VESC)
    ↓
  1. Get absolute RPM value
    ↓
  2. Normalize to target ERPM
     rpm_rel = |rpm_now| / gen_erpm
    ↓
  3. Calculate scaled position in range
     current = (rpm_rel - gen_start) / (1.0 - gen_start)
    ↓
  4. Clamp to valid range [0.0, 1.0+]
    ↓
  5. Scale to maximum current
     current = current * gen_current
    ↓
  6. Apply with correct polarity
     - Forward rotation → Negative current (regen)
     - Reverse rotation → Positive current (regen)
    ↓
  OUTPUT: Set motor current (charging battery)
```

### What It Regulates

**Primary Control Variable**: **Current** (not speed, not duty cycle)

The app uses `mc_interface_set_current()` which implements **current mode control**:
- **Positive current**: Motor accelerates (motoring mode)
- **Negative current**: Motor decelerates (regenerative braking)
- The VESC's FOC controller handles the actual current regulation

### What It Expects

#### 1. **External Prime Mover**
The control loop assumes something is spinning the motor:
- Wind turbine blades
- Bicycle wheel (e-bike regenerative braking)
- Hydraulic/steam turbine
- Hand crank
- Falling weight

**Important**: The app does NOT actively motor the shaft. It only applies **resistive load**.

#### 2. **Speed Range Above Start Threshold**
Generation only occurs when:
```
|RPM| >= gen_start * gen_erpm
```

Below this threshold:
- Current command = 0A
- Motor freewheels (coasts)
- No regeneration occurs

#### 3. **Energy Source with Sufficient Power**
The external prime mover must supply power to:
1. Overcome motor losses (copper, iron, mechanical)
2. Supply the regenerative current
3. Maintain desired RPM

**Power equation**:
```
P_required = P_losses + (V_battery * I_regen)
```

### Linear Current Ramp Characteristic

The control creates a **linear relationship** between RPM and current:

```
Current (A)
    ↑
    │
 20 │                    ┌─────────── (gen_current)
    │                   /│
 15 │                  / │
    │                 /  │
 10 │                /   │
    │               /    │
  5 │              /     │
    │             /      │
  0 ├────────────┴───────┼───────────→ RPM
    0         1800      2000        2500
              ↑          ↑
          gen_start   gen_erpm
```

**Characteristics**:
- **Below `gen_start * gen_erpm`**: Zero current (freewheeling)
- **At `gen_erpm`**: Full current (`gen_current`)
- **Above `gen_erpm`**: Current can exceed maximum (limited by VESC settings)

### Current Polarity Logic

The app applies current **opposite** to rotation direction:

```c
if (rpm_now < 0.0) {
    mc_interface_set_current(current);   // Positive current opposes reverse
} else {
    mc_interface_set_current(-current);  // Negative current opposes forward
}
```

**Why?** In VESC convention:
- **Motoring** (accelerating): Current same sign as rotation
- **Generating** (braking): Current opposite sign as rotation

This ensures the motor always generates (opposes motion) regardless of direction.

### Update Rate

**Default**: 1000 Hz (1 ms period)

**Why this frequency?**
- **Fast enough**: Responds quickly to RPM changes
- **Stable**: Doesn't overload motor controller's inner loops (~20 kHz)
- **Efficient**: Low CPU overhead (~0.1% per 1000 Hz)

**Can be adjusted**:
- **Higher (2000+ Hz)**: More responsive, slightly higher CPU load
- **Lower (500 Hz)**: More sluggish, lower CPU load
- **Not recommended**: < 100 Hz (too slow) or > 5000 Hz (unstable)

### Power Limiting

The app commands current, but **VESC limits** still apply:

1. **Motor current limits** (App Config → Motor Max/Min)
2. **Battery current limits** (App Config → Battery Max/Min Regen)
3. **Temperature limits** (automatic thermal throttling)
4. **Voltage limits** (overvoltage protection)

**Example**:
```
gen_config 2000 50 0.9 1000
```

Even though you set 50A, if battery max regen is set to 30A in VESC Tool, the actual current will be **clamped to 30A**.

---

## Parameter Tuning Guide

### Parameter: `gen_erpm` (Target ERPM)

**What it does**: RPM where maximum current is reached.

**Higher values** (3000+):
- ✅ Smoother engagement
- ✅ Less aggressive braking
- ✅ Better for high-inertia systems (wind turbines)
- ❌ Requires higher speed to generate meaningfully

**Lower values** (1000-2000):
- ✅ Engages at lower speeds
- ✅ More aggressive braking feel
- ✅ Better for e-bike regeneration
- ❌ Risk of oscillation without inertia

**Tuning tips**:
- Start with expected **operating speed**
- Wind turbine: Set to 80% of max safe RPM
- E-bike: Set to typical cruising speed's ERPM
- Test bed: Start conservative (3000+), reduce gradually

### Parameter: `gen_current` (Maximum Current)

**What it does**: Maximum regenerative current in Amperes.

**Higher values**:
- ✅ More power generation
- ✅ Stronger braking effect
- ✅ Faster battery charging
- ❌ Can stall weak prime movers
- ❌ Higher heat generation
- ❌ Risk of oscillation

**Lower values**:
- ✅ Gentle on mechanical system
- ✅ Safer for testing
- ✅ Less heat
- ❌ Less power captured

**Tuning tips**:
- **Start at 25% of motor continuous rating**
- Monitor motor/VESC temperature
- Check battery can accept the current
- Gradually increase while testing

**Warning**: Author's note from original patch:
> "If you set GEN_ERPM too low and/or GEN_CURRENT too high, and run the motor 
> without an inertial load, you might get rather violent oscillations"

### Parameter: `gen_start` (Start Ratio)

**What it does**: Fraction of `gen_erpm` where generation begins (0.0 to 1.0).

**Higher values** (0.95):
- ✅ Very narrow activation range
- ✅ Acts more like on/off switch
- ✅ Minimizes low-speed losses
- ❌ Abrupt engagement

**Lower values** (0.70-0.80):
- ✅ Smooth, gradual engagement
- ✅ Better for braking feel
- ✅ Wider operating range
- ❌ Generates at lower speeds (more losses)

**Typical values**:
- **Wind turbine**: 0.85-0.90 (wide range)
- **E-bike regen**: 0.80-0.85 (smooth braking)
- **Test bench**: 0.90-0.95 (narrow, predictable)

**Formula**:
```
Start RPM = gen_start * gen_erpm
```

Example: `gen_erpm=2000`, `gen_start=0.85` → Starts at **1700 ERPM**

### Parameter: `gen_update_rate_hz` (Control Frequency)

**What it does**: How often the control loop runs per second.

**Recommended**: 1000 Hz (default)

**Can adjust if needed**:
- **500 Hz**: Adequate for most applications
- **2000 Hz**: Very responsive, slightly more CPU
- **100 Hz**: Sluggish, not recommended

**Don't change unless**:
- Experiencing oscillations (try lowering to 500 Hz)
- Need ultra-fast response (try 2000 Hz)
- CPU usage concerns (lower to 500 Hz)

---

## Use Cases

### 1. Wind Turbine Generator

**Application**: Small wind turbine charging battery bank.

**Configuration**:
```
gen_config 3500 15 0.85 1000
```

**Rationale**:
- **3500 ERPM**: Typical wind turbine operating speed
- **15A**: Conservative for continuous operation
- **0.85**: Starts at 2975 ERPM, smooth engagement
- **1000 Hz**: Standard response rate

**Behavior**:
- **Below 2975 ERPM**: Freewheels (no load on turbine)
- **2975-3500 ERPM**: Current ramps from 0A to 15A
- **Above 3500 ERPM**: Full 15A regeneration (turbine slows down)

**Advantage**: Self-regulating! Wind speed increases → RPM rises → More current → More load → RPM stabilizes

### 2. E-Bike Regenerative Braking

**Application**: Electric bicycle with regenerative braking.

**Configuration**:
```
gen_config 1800 25 0.80 1000
```

**Rationale**:
- **1800 ERPM**: Low engagement (useful at slow speeds)
- **25A**: Moderate braking force
- **0.80**: Starts at 1440 ERPM, smooth feel
- **1000 Hz**: Fast response for rider input

**Behavior**:
- Coasting → Motor starts braking above 1440 ERPM
- Braking force increases smoothly with speed
- Natural feel similar to engine braking

**Note**: For stronger braking, increase `gen_current` to 35-40A.

### 3. Hydro/Micro-Turbine

**Application**: Water wheel or small hydro turbine.

**Configuration**:
```
gen_config 2500 30 0.90 1000
```

**Rationale**:
- **2500 ERPM**: Typical hydro turbine speed
- **30A**: High continuous power
- **0.90**: Narrow range (2250-2500 ERPM active)
- **1000 Hz**: Standard

**Behavior**:
- Water flow constant → RPM regulated around 2500 ERPM
- If flow increases → RPM rises → More load → RPM stabilizes
- Self-regulating within the 2250-2500 ERPM window

### 4. Test Bench / Dynamometer

**Application**: Motor testing, brake dyno measurement.

**Configuration**:
```
gen_config 5000 10 0.95 1000
```

**Rationale**:
- **5000 ERPM**: High threshold for safety
- **10A**: Low load for initial testing
- **0.95**: Very narrow range (4750-5000 ERPM)
- **1000 Hz**: Standard

**Use**:
- Spin motor with drill or other motor
- Monitor with `gen_status`
- Increase current gradually: `gen_config 5000 20 0.95 1000`

---

## Safety & Limitations

### ⚠️ Warning: Oscillation Risk

**From original author**:
> "If you set GEN_ERPM too low and/or GEN_CURRENT too high, and run the motor 
> without an inertial load, you might get rather violent oscillations."

**Why this happens**:

1. **RPM exceeds threshold** → Current applied → Motor slows
2. **RPM drops below threshold** → Current removed → Motor accelerates
3. **Cycle repeats rapidly** → Violent oscillation

**Prevention**:
- ✅ Always test with **inertial load attached**
- ✅ Start with **conservative current** (< 10A)
- ✅ Use **higher ERPM targets** initially
- ✅ Monitor with `gen_status` during testing

**If oscillation occurs**:
1. Immediately reduce `gen_current`: `gen_config 2000 5 0.90 1000`
2. Increase `gen_erpm`: `gen_config 3000 10 0.90 1000`
3. Increase `gen_start`: `gen_config 2000 10 0.95 1000`

### VESC Limits Take Precedence

The app respects all VESC configuration limits:

**Motor Configuration**:
- Motor current max/min
- Absolute max current
- Temperature limits

**App Configuration**:
- Battery current max (charging)
- Battery current regen max ← **Important for generators!**

**Example problem**:
```
Your setting:        gen_config 2000 50 0.90 1000  (50A command)
VESC battery limit:  Battery regen = 20A max
Actual behavior:     Only 20A flows (VESC limit wins)
```

**Solution**: Check and increase battery limits in VESC Tool if needed.

### Don't Motor Through Threshold

**Bad idea**:
> Using VESC Tool to actively motor the shaft to speeds where generation kicks in.

**Why?**:
- Motor app and generator app fight each other
- Undefined behavior
- Potential current spikes
- VESC may fault

**Correct approach**:
- Set App to **Custom** (generator app)
- Use **external prime mover only**
- Monitor with `gen_status`

### Temperature Monitoring

Generator operation produces heat:
- Motor: I²R losses in windings
- VESC: FET switching losses, regen current handling

**Monitor**:
```
gen_status
```
Plus watch VESC Tool realtime data:
- Motor temperature
- MOSFET temperature
- Battery current

**If overheating**:
1. Reduce `gen_current`
2. Improve cooling (airflow, heatsink)
3. Lower duty cycle (intermittent operation)

### No Persistence (Yet)

**Current limitation**: Settings are **not saved** to flash memory.

**Impact**:
- Settings lost on power cycle
- Must reconfigure after reboot

**Workaround**:
- Note your optimal settings
- Create startup script in LispBM (if available)
- Or: Modify default values in source and rebuild

**Future enhancement**: Add EEPROM persistence (see `CUSTOM_APP_CONFIGURATION.md`)

---

## Troubleshooting

### No Generation Occurring

**Check**:
1. Is app set to Custom? `App Configuration → App to Use → Custom`
2. Is RPM above threshold? Run `gen_status` to verify
3. Are battery limits too low? Check `Battery Max Regen` setting
4. Motor temperature fault? Check realtime data

### Weak/Insufficient Current

**Possible causes**:
1. `gen_current` set too low → Increase it
2. RPM below target → Check `gen_status`
3. VESC limits restricting → Check battery regen limits
4. Motor losses too high → Normal at low efficiency

### Oscillation/Vibration

**Immediate action**:
```
gen_config 3000 5 0.95 1000
```

**Then gradually tune**:
- Add more inertia to mechanical system
- Increase `gen_erpm`
- Decrease `gen_current`
- Increase `gen_start`

### Settings Don't Take Effect

**Solutions**:
- Ensure Custom app is active
- Check terminal for error messages
- Try stopping/starting app (switch to None, then back to Custom)
- Reboot VESC if needed

---

## Technical Details

### Code Location
- File: `applications/app_generator.c`
- Configured in: `conf_general.h` (`APP_CUSTOM_TO_USE`)

### Thread Priority
- Priority: `NORMALPRIO` (standard application priority)
- Stack: 1024 bytes
- Name: "App Generator"

### ChibiOS Integration
- Uses ChibiOS threading (`chThdCreateStatic`)
- Sleep/timing via `chThdSleep`
- Proper thread lifecycle management

### VESC API Dependencies
- `mc_interface_get_rpm()` - Read motor speed
- `mc_interface_set_current()` - Command motor current
- `timeout_reset()` - Prevent app timeout watchdog
- `terminal_register_command_callback()` - Terminal commands
- `commands_printf()` - Terminal output

---

## Future Enhancements

### Planned (Not Yet Implemented)

1. **EEPROM Persistence**
   - Save settings to flash memory
   - Auto-load on startup
   - Terminal command: `gen_save`

2. **Power Limiting Mode**
   - Regulate to constant power instead of current
   - Better for battery charging
   - Requires voltage measurement

3. **MPPT (Maximum Power Point Tracking)**
   - Automatically find optimal RPM
   - Maximize energy capture
   - For wind/hydro turbines

4. **Multi-Point Curve**
   - Define custom current vs. RPM curve
   - Non-linear characteristics
   - Per-application tuning

5. **GUI Configuration**
   - VESC Tool integration
   - Visual parameter adjustment
   - Real-time plotting

See `CUSTOM_APP_CONFIGURATION.md` for implementation approaches.

---

## Additional Resources

- **Original Forum Post**: 2018-09-22 by arvidb
- **Feasibility Analysis**: `generator-app/FEASIBILITY_ANALYSIS.md`
- **Custom App Config**: `generator-app/CUSTOM_APP_CONFIGURATION.md`
- **Source Code**: `applications/app_generator.c`

---

*Document created: March 3, 2026*  
*Generator App Version: Terminal Configuration (v2)*  
*Compatible with: VESC Firmware v7.00+*
