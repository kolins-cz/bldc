# Generator Control Schemes for ICE Applications

## Application Profile

**Prime Mover:** Gasoline Internal Combustion Engine (ICE)  
**Battery:** Lithium-Ion (with BMS)  
**Use Case:** Range extender, backup power, mobile generator  
**Control Goal:** Efficient battery charging from engine-driven motor/generator

---

## Quick Recommendation

For ICE generators, you have **two practical options**:

1. **Proportional Current** ⭐ (current implementation) - Works great, start here
2. **Constant Power** - Better efficiency, worth upgrading to

Skip the complex stuff (MPPT, multi-stage charging, etc.) - not relevant for ICE applications.

---

## Table of Contents

1. [Proportional Current Control](#1-proportional-current-control-current-implementation)
2. [Constant Power Control](#2-constant-power-control-recommended-upgrade)
3. [Speed Governor Mode](#3-speed-governor-mode-optional)
4. [Quick Comparison](#quick-comparison)
5. [ICE-Specific Tuning](#ice-specific-tuning)

---

## 1. Proportional Current Control (Current Implementation)

### What It Does

Commands current proportional to motor RPM. As the ICE spins the motor faster, more regenerative current is applied, creating a load that increases linearly with speed.

### Algorithm

```c
// Current proportional to RPM
float rpm_now = mc_interface_get_rpm();
float rpm_rel = fabsf(rpm_now) / gen_erpm;

float current = rpm_rel - gen_start;
if (current < 0.0) current = 0.0;

current /= 1.00 - gen_start;
current *= gen_current;

mc_interface_set_current(-current);  // Negative = regen
```

### Behavior with ICE

```
Engine idle (1000 RPM) → 0A load → Engine runs light
Engine speeds up        → Current increases → Engine feels the load
At target RPM           → Full current → Engine works harder
```

**Self-regulating:** Engine power output determines final RPM and current.

### Configuration Example

```
gen_config 2500 25 0.85 1000
```

For typical ICE generator:
- **2500 ERPM**: Normal operating speed
- **25A**: Battery can handle this charging current
- **0.85**: Starts at 2125 ERPM (smooth engagement)
- **1000 Hz**: Standard update rate

### Pros for ICE Applications
- ✅ Simple and reliable
- ✅ Engine naturally finds its operating point
- ✅ Works with variable engine speed
- ✅ Easy to tune and understand
- ✅ Can't overload engine (linear relationship)

### Cons for ICE Applications
- ❌ Current varies with battery voltage (less efficient)
- ❌ Not optimized for fuel efficiency
- ❌ Power varies as battery charges (V×I changes with V)

### Tuning Tips

**Set `gen_erpm` to:**
- Your engine's normal operating speed after warm-up
- Usually 2000-3000 ERPM for small engines
- Monitor with `gen_status` while engine runs

**Set `gen_current` to:**
- Start conservative: 20-25A
- Check battery BMS limits
- Ensure engine has enough power
- Increase gradually if needed

**Set `gen_start` to:**
- 0.80-0.85 for smooth engagement
- Higher (0.90+) if engine stalls at low speeds
- Lower (0.75) for gradual loading

### When to Use

**Use proportional current if:**
- ✅ You want simple, reliable operation
- ✅ Engine speed varies (no governor)
- ✅ You're just getting started
- ✅ Battery BMS handles all charging logic

**Consider upgrading if:**
- Engine runs at constant speed (has governor)
- You want maximum fuel efficiency
- Battery charging takes a long time
- You want predictable power output

---

## 2. Constant Power Control (Recommended Upgrade)

### What It Does

Regulates to maintain constant electrical power (Watts) regardless of battery voltage. Adjusts current inversely with voltage to maintain `P = V × I`.

### Why Better for ICE

**Problem with current control:**
- Battery at 48V: 25A = 1200W
- Battery at 54V: 25A = 1350W
- Engine sees varying load!

**Solution with power control:**
- Battery at 48V: 25.0A = 1200W
- Battery at 54V: 22.2A = 1200W
- Engine sees constant 1200W load

### Algorithm

```c
static float target_power = 1200.0;  // Watts
static float min_rpm = 1500.0;

float rpm_now = mc_interface_get_rpm();
float voltage = mc_interface_get_input_voltage();

if (fabsf(rpm_now) < min_rpm) {
    mc_interface_set_current(0);
    return;
}

// P = V × I → I = P / V
float current = target_power / voltage;

// Safety limit
if (current > max_current) {
    current = max_current;
}

mc_interface_set_current(-current);
```

### Configuration Example

```
gen_power 1000 1800 30
```

Parameters:
- **1000W**: Target power (adjust to engine output)
- **1800 ERPM**: Minimum RPM to engage
- **30A**: Maximum current safety limit

### Pros for ICE Applications
- ✅ Constant engine load = better fuel efficiency
- ✅ Predictable power output
- ✅ Simpler power budget calculations
- ✅ Better for battery (consistent charge rate)
- ✅ Easier to match engine to electrical load

### Cons for ICE Applications
- ❌ Slightly more complex
- ❌ Requires voltage measurement (VESC has this)
- ❌ Must set power based on engine capability

### Tuning Tips

**Calculate target power:**
1. Check engine rated power (e.g., 2 HP = ~1500W)
2. Account for losses (motor ~85% efficient)
3. Set to 60-80% of available power
4. Example: 2 HP engine → 1500W × 0.85 × 0.7 = **900W**

**Set minimum RPM:**
- Just below normal idle speed
- Prevents loading during startup
- Typical: 1500-2000 ERPM

**Set maximum current:**
- Check battery BMS limit
- Safety margin (20% under max)
- Typical: 25-40A for small systems

### When to Use

**Use constant power if:**
- ✅ Engine runs at steady speed (has governor)
- ✅ You want maximum fuel efficiency
- ✅ Charging takes significant time
- ✅ You know engine power output

**Stick with current control if:**
- Engine speed varies wildly
- You prefer simpler control
- Current control is working fine

---

## 3. Speed Governor Mode (Optional)

### What It Does

Maintains constant motor/engine RPM by adjusting electrical load. Acts as an electronic governor in addition to (or instead of) the engine's mechanical governor.

### Why for ICE?

Most small engines have their own governor (mechanical or electronic). Adding VESC speed control can:
- Assist weak governors
- Provide finer RPM control
- Protect from overspeed
- Stabilize speed under varying battery voltage

### Algorithm

```c
// Use VESC's built-in speed controller
float target_rpm = 2500.0;
mc_interface_set_pid_speed(target_rpm);

// VESC automatically adjusts braking current to maintain RPM
```

### Configuration Example

```
gen_governor 2500 30
```

- **2500 RPM**: Target engine speed
- **30A**: Maximum current

### Behavior

```
Engine produces more power → RPM rises → VESC increases load → RPM stabilizes
Engine produces less power → RPM drops → VESC decreases load → RPM stabilizes
```

**Result:** Constant RPM regardless of engine throttle position (within limits).

### Pros for ICE Applications
- ✅ Very stable RPM
- ✅ Can override weak engine governor
- ✅ Protects from overspeed
- ✅ Good for noise-sensitive applications (constant speed = constant noise)

### Cons for ICE Applications
- ❌ Fights with engine's own governor (can be unstable)
- ❌ May prevent engine from load-following
- ❌ Requires PID tuning
- ❌ Usually unnecessary (engine has governor)

### When to Use

**Use governor mode if:**
- Engine has NO governor (rare)
- Engine governor is very weak/poor
- You need precise RPM control
- Constant frequency AC generation (future feature)

**Don't use if:**
- ❌ Engine has good mechanical governor (creates fighting)
- ❌ You want engine to respond to throttle changes
- ❌ Simpler control works fine

**Most ICE generators:** Skip this, use current or power mode instead.

---

## Quick Comparison

| Feature | Proportional Current | Constant Power | Speed Governor |
|---------|---------------------|----------------|----------------|
| **Complexity** | Low | Medium | Medium |
| **Fuel Efficiency** | Good | Excellent | Good |
| **Engine Load** | Varies with battery V | Constant | Varies |
| **Stability** | Excellent | Very Good | Good (if tuned) |
| **Setup Difficulty** | Easy | Easy | Medium (PID tuning) |
| **Best For** | General use | Fuel efficiency | Special cases |

### Recommendation Path

1. **Start with Proportional Current** (already implemented) ⭐
   - Test basic operation
   - Tune parameters (`gen_config`)
   - Monitor engine behavior

2. **Upgrade to Constant Power** (future enhancement)
   - Calculate engine power output
   - Set target power: `gen_power 1000 1800 30`
   - Measure fuel savings

3. **Only use Governor if needed** (rare)
   - Most engines have their own governor
   - Only for special applications

---

## ICE-Specific Tuning

### Starting from Scratch

**Step 1: Determine Engine Operating RPM**
1. Start engine with no electrical load
2. Let it warm up
3. Note stable RPM with `gen_status`
4. This is your `gen_erpm` target

**Step 2: Conservative Initial Settings**
```
gen_config 2500 15 0.90 1000
```
- Use your measured RPM for first parameter
- Start with low current (15A)
- High start ratio (0.90) for safety

**Step 3: Apply Load and Observe**
1. Monitor engine RPM with `gen_status`
2. Engine should slow down slightly under load
3. If engine bogs down/stalls: **Reduce current** or **increase gen_erpm**
4. If engine barely affected: **Increase current**

**Step 4: Optimize**
```
gen_config 2500 25 0.85 1000
```
- Increase current to match engine capability
- Lower start ratio for smoother engagement
- Monitor battery current and engine behavior

### Power-Based Tuning (Better)

**Step 1: Calculate Available Power**
```
Engine Power (HP) × 746 W/HP × Motor Efficiency × Load Factor = Target Power

Example: 2 HP × 746 × 0.85 × 0.75 = 950W
```

**Step 2: Configure**
```
gen_power 950 1800 30
```
- 950W target
- 1800 ERPM minimum (below warm idle)
- 30A safety limit

**Step 3: Verify**
- `gen_status` shows actual current
- Calculate power: P = V × I
- Should be close to target
- Engine should run smoothly

### Common Issues

**Engine stalls when load applied:**
- ✅ Reduce `gen_current` or `target_power`
- ✅ Increase `gen_erpm` or `min_rpm`
- ✅ Check engine hasn't seized/fuel issue
- ✅ Warm up engine fully first

**Engine races but low current:**
- ✅ Check battery BMS limits
- ✅ Verify VESC battery config (max regen current)
- ✅ Check VESC not in error state
- ✅ Try `gen_status` to see what's limiting

**Hunting/oscillation:**
- ✅ Lower `gen_current` slightly
- ✅ Increase `gen_start` ratio
- ✅ Check engine governor adjustment
- ✅ May be fighting engine governor (use constant power instead)

**Battery not charging:**
- ✅ Check BMS hasn't cut off charging
- ✅ Verify battery voltage is below max
- ✅ Check VESC app is set to "Custom"
- ✅ Monitor with `gen_status`

### Safety Considerations

**Thermal Management:**
- Monitor motor temperature (VESC Tool)
- Monitor VESC temperature
- ICE generates heat + motor generates heat = add cooling
- Reduce current if overheating

**Battery Protection:**
- VESC respects battery current limits
- BMS should provide protection
- Don't exceed battery C-rating
- Monitor battery temperature

**Engine Protection:**
- Don't exceed engine continuous rating
- Let engine warm up before loading
- Use proper break-in procedure for new engines
- Monitor engine temperature (separate from VESC)

**Electrical Protection:**
- Ensure good ground connection (ICE frame to battery)
- Check all connections tight (vibration!)
- Use proper gauge wire for current
- Fuse appropriately

---

## Example Configurations

### Small 2 HP Generator (Portable)

**Engine:** 2 HP gasoline, ~3000 RPM  
**Battery:** 48V 20Ah Li-ion  
**Goal:** Backup/portable power

**Proportional Current:**
```
gen_config 2800 20 0.85 1000
```

**Constant Power (better):**
```
gen_power 900 1800 25
```

**Expected behavior:**
- 900W continuous → ~18A at 48V
- Charges 20Ah battery in ~1 hour
- Engine runs comfortably at rated speed

### Medium 5 HP Generator (RV/Mobile)

**Engine:** 5 HP gasoline, ~2500 RPM  
**Battery:** 48V 50Ah Li-ion  
**Goal:** Mobile home/RV range extender

**Proportional Current:**
```
gen_config 2500 40 0.80 1000
```

**Constant Power (better):**
```
gen_power 2000 1500 50
```

**Expected behavior:**
- 2000W continuous → ~40A at 48V
- Charges 50Ah battery in ~1.25 hours
- Significant range extension

### Lawn Mower Conversion

**Engine:** 3 HP, mower engine  
**Battery:** 48V 30Ah Li-ion  
**Goal:** Hybrid mower, regen from engine

**Constant Power (recommended):**
```
gen_power 1200 2000 30
```

**Operation:**
- Engine drives blades AND charges battery
- ~25A charging current
- Extends runtime significantly

---

## What's NOT Needed for ICE

### ❌ MPPT (Maximum Power Point Tracking)
**Why not:** Engine provides consistent power, not variable like wind/solar. MPPT searches for optimal operating point - engine already knows its operating point.

**Skip it** unless you're doing something very unusual.

### ❌ Multi-Stage Battery Charging
**Why not:** Your Li-ion battery has a BMS that handles charging stages. VESC just provides current, BMS controls the charging profile.

**Skip it** - let the BMS do its job.

### ❌ Complex Governor Algorithms
**Why not:** Engine has its own governor (mechanical or electronic). Adding VESC governor usually creates control conflicts.

**Skip it** unless engine has NO governor (very rare).

### ❌ Duty Cycle Control
**Why not:** Current control is better for generators. Duty cycle doesn't give you predictable charging current.

**Skip it** - not recommended for any generator application.

---

## Implementation Recommendations

### Current Status (v2)
✅ **Proportional Current** with terminal configuration  
- Working great
- Easy to tune
- Good for getting started

### Suggested Enhancement
🔄 **Add Constant Power Mode**
- Better fuel efficiency
- More predictable operation
- Easy implementation (add mode flag)

**Implementation:**
```c
typedef enum {
    GEN_MODE_CURRENT,
    GEN_MODE_POWER
} gen_mode_t;

static gen_mode_t gen_mode = GEN_MODE_CURRENT;

// Terminal command to select mode:
// gen_mode current
// gen_mode power
```

### Not Needed for ICE
- ❌ MPPT
- ❌ Multi-stage charging
- ❌ Governor mode (usually)
- ❌ Duty cycle control

---

## Fuel Efficiency Tips

### Engine Optimization
1. **Run at optimal RPM** - Usually 60-80% of max rated RPM
2. **Use constant power mode** - Steady load = better efficiency
3. **Size appropriately** - Don't use 10 HP engine for 500W load
4. **Maintain engine** - Clean air filter, fresh fuel, proper oil

### Electrical Optimization
1. **Match load to engine** - Set power target to 60-80% of engine output
2. **Minimize idle time** - Don't run engine when battery full
3. **Use battery wisely** - Let battery supply peak loads, engine supplies average
4. **Monitor efficiency** - Track fuel consumption vs. energy stored

### System Integration
Consider adding:
- Automatic engine start/stop (when battery SOC low/high)
- Load sensing (only run engine when power needed)
- Smart battery management (prioritize generator when available)

---

## Summary

### For ICE Generator Applications:

**Best approach:**
1. Start with **Proportional Current** (already have it)
2. Upgrade to **Constant Power** (better efficiency)
3. Skip the complex renewables-focused stuff

**Your hardware: ICE + Li-ion + VESC**
- ✅ Current mode works great
- ✅ Power mode would be better
- ❌ Don't need MPPT, multi-stage, complex algorithms

**Next steps:**
1. Test current implementation with your engine
2. Tune parameters for smooth operation
3. Monitor fuel consumption
4. Consider constant power mode upgrade

---

*Document created: March 3, 2026*  
*Application: ICE Generator with Li-ion Battery*  
*Focus: Practical, efficiency-focused control schemes*
