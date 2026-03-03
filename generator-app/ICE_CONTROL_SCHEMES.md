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
5. [Throttle vs Load Control Integration](#throttle-vs-load-control-integration)
6. [ICE-Specific Tuning](#ice-specific-tuning)

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

## Throttle vs Load Control Integration

### The Two-Controller Problem

You have **two independent controllers** that must work together:

```
┌─────────────────────────────────────────────────────────┐
│                    SYSTEM DIAGRAM                       │
└─────────────────────────────────────────────────────────┘

Fuel/Air → [CARBURETOR] → [ICE] → [MOTOR/GEN] → [VESC] → Battery
              ↑                         ↑
          THROTTLE                 ELECTRICAL
          CONTROL                     LOAD
       (mechanical)               (electronic)
```

**The challenge:** These two controllers affect the same variable (RPM) but through different mechanisms!

### Control Strategies

There are several ways to coordinate these controllers:

---

### Strategy 1: Fixed Throttle + Variable Load ⭐ (Recommended)

**How it works:**
- Set carburetor throttle to **fixed position** (e.g., 50%)
- Let engine governor maintain RPM
- VESC adjusts electrical load based on battery needs
- Engine self-regulates to balance power

**Setup:**
1. Set throttle to fixed position (lever, cable lock, etc.)
2. Start engine, let warm up
3. Configure VESC for proportional current or constant power
4. Engine finds equilibrium point

**Configuration example:**
```
Throttle: Fixed at 60% (mechanical)
VESC: gen_config 2500 25 0.85 1000

Result: Engine runs ~2500 RPM, provides up to 25A charging
```

**Pros:**
- ✅ Simple - no coordination needed
- ✅ Reliable - one control loop
- ✅ Engine governor does what it's designed for
- ✅ VESC can't force engine into bad operating range
- ✅ Easy to set up and tune

**Cons:**
- ❌ Not optimized for efficiency at all loads
- ❌ Engine may idle too high when battery full
- ❌ Wastes fuel if electrical load is low
- ❌ No automatic start/stop

**Best for:**
- Simple setups
- Manual operation
- Backup/emergency generators
- Testing and development

**Behavior:**
```
High battery SOC → Low electrical load → Engine runs light at set RPM
Low battery SOC  → High electrical load → Engine works harder at set RPM
```

---

### Strategy 2: Manual Throttle + Adaptive Load ⭐ (User Control)

**How it works:**
- User controls throttle manually (like driving)
- VESC adapts electrical load to match available power
- User increases throttle → More RPM → VESC allows more current
- User decreases throttle → Less RPM → VESC reduces current

**Setup:**
1. Keep carburetor throttle manual (cable/lever)
2. Configure VESC with proportional current (RPM-based)
3. User adjusts throttle based on needs
4. VESC automatically scales load with RPM

**Configuration example:**
```
Throttle: Manual control by user
VESC: gen_config 3000 30 0.80 1000

Idle (1500 RPM):     0A load (below 2400 RPM threshold)
Half throttle (2500): ~10A load (partial range)
Full throttle (3500): 30A load (above target RPM)
```

**Pros:**
- ✅ User has full control
- ✅ Can optimize fuel use on the fly
- ✅ Can reduce noise when needed (lower throttle)
- ✅ Proportional current naturally matches
- ✅ Intuitive operation

**Cons:**
- ❌ Requires human operator
- ❌ Not suitable for automatic operation
- ❌ Operator must understand system
- ❌ Can be misused (full throttle, low load = waste)

**Best for:**
- Mobile applications (RV, vehicle)
- Operator-attended systems
- Variable power requirements
- Learning/tuning phase

---

### Strategy 3: Electronic Throttle Control (Advanced)

**How it works:**
- VESC controls throttle via servo/motor
- Feedback loop: Measure battery current/voltage
- Increase throttle when more power needed
- Decrease throttle when battery charged

**Hardware needed:**
- Servo motor on throttle linkage
- VESC servo output or separate servo controller
- Throttle position sensor (optional feedback)

**Implementation sketch:**
```c
// Pseudo-code for throttle control
float battery_voltage = mc_interface_get_input_voltage();
float battery_current = mc_interface_get_tot_current_in();

if (battery_voltage < 50.0) {
    // Battery needs charging, increase throttle
    throttle_position += 0.01;
} else if (battery_voltage > 54.0) {
    // Battery charged, reduce throttle
    throttle_position -= 0.01;
}

// Output to servo
pwm_servo_set_pos(throttle_position);
```

**Pros:**
- ✅ Fully automatic operation
- ✅ Can optimize fuel efficiency
- ✅ Enables automatic start/stop logic
- ✅ Best long-term efficiency
- ✅ Integrated system control

**Cons:**
- ❌ Complex implementation
- ❌ Requires additional hardware (servo)
- ❌ Safety critical (runaway throttle!)
- ❌ Needs careful tuning
- ❌ Must handle failures gracefully

**Best for:**
- Sophisticated installations
- Automatic range extenders
- Unattended operation
- Production systems (after development)

**Safety requirements:**
- Watchdog timer (return to idle if control fails)
- Manual throttle override capability
- RPM limiter (don't over-rev engine)
- Current limiter (don't overload engine)

---

### Strategy 4: Constant Speed (Governor Cooperation)

**How it works:**
- Set throttle to provide excess power
- Engine's mechanical governor limits max RPM
- VESC speed control adds electrical load to maintain target RPM
- System finds equilibrium between mechanical and electrical governors

**Setup:**
1. Set throttle high enough to exceed target RPM
2. Engine governor prevents over-rev
3. Configure VESC for speed control mode
4. VESC loads engine to maintain target RPM

**Configuration example:**
```
Throttle: Set to 80% (would give 3500 RPM unloaded)
VESC: gen_governor 2500 30

Result: Engine runs exactly 2500 RPM
- Governor provides upper limit (3500 RPM)
- VESC provides lower limit (2500 RPM via load)
- System stable at 2500 RPM
```

**Pros:**
- ✅ Very precise RPM control
- ✅ Two governors provide redundancy
- ✅ Constant speed regardless of load
- ✅ Good for AC generation (future)

**Cons:**
- ❌ Two control loops can interact poorly
- ❌ Requires careful tuning
- ❌ May hunt/oscillate if gains wrong
- ❌ Wastes fuel (always running at set RPM)
- ❌ Can fight each other

**Best for:**
- Constant frequency AC generation
- Applications requiring precise RPM
- Well-tuned production systems

**Tuning tips:**
- Use slow VESC PID gains (don't fight governor)
- Set governor RPM slightly higher than VESC target
- Monitor for hunting behavior
- May need to disable one governor

---

### Strategy 5: Hybrid - Manual Override with Auto Load

**How it works:**
- Normal: Fixed throttle, VESC auto-adjusts load
- Override: User can manually adjust throttle for special situations
- VESC always adapts load to current RPM

**Setup:**
1. Default throttle position (e.g., 50%)
2. Manual override available (cable/linkage)
3. VESC proportional current mode
4. Works at any throttle position

**Configuration example:**
```
Default: Throttle locked at 50%
VESC: gen_config 2500 25 0.85 1000

User can unlock and adjust throttle as needed
VESC automatically adapts load to RPM
```

**Pros:**
- ✅ Automatic normal operation
- ✅ Manual override when needed
- ✅ Simple and reliable
- ✅ Best of both worlds
- ✅ Fail-safe (can always go manual)

**Cons:**
- ❌ Requires mechanical override design
- ❌ User must understand system
- ❌ Still not fully automatic

**Best for:**
- General purpose generators
- Semi-automatic operation
- Systems requiring occasional intervention

---

## Recommended Approach for Different Applications

### Simple Backup Generator
**Use Strategy 1: Fixed Throttle + Variable Load**
```
Setup: Cable lock on carburetor at 60%
VESC: gen_config 2500 20 0.85 1000
Operation: Start engine, let it run, VESC handles charging
```

### RV/Mobile Range Extender
**Use Strategy 2: Manual Throttle + Adaptive Load**
```
Setup: Throttle cable to operator position
VESC: gen_config 3000 30 0.80 1000
Operation: User controls throttle, VESC adapts load
```

### Automatic Range Extender (Advanced)
**Use Strategy 3: Electronic Throttle Control**
```
Setup: Servo on throttle, feedback control
VESC: Custom app code for throttle management
Operation: Fully automatic based on battery SOC
```

### Portable AC Generator
**Use Strategy 4: Constant Speed**
```
Setup: Throttle at 75%, governor at 3000 RPM
VESC: gen_governor 2500 25
Operation: Maintains 2500 RPM for 50Hz AC output
```

---

## Practical Integration Examples

### Example 1: Lawn Mower Conversion (Fixed Throttle)

**Hardware:**
- Briggs & Stratton 3 HP engine
- Fixed throttle at 3/4 position
- Governor set for 3200 RPM max

**Configuration:**
```bash
gen_config 2800 25 0.85 1000
```

**Operation:**
1. Start mower engine normally
2. Engine idles at ~2800 RPM
3. VESC engages at 2380 RPM (85% of 2800)
4. Ramps up to 25A at 2800 RPM
5. Engine governor prevents over-rev
6. System self-balances

**User experience:**
- Pull start mower
- Battery starts charging automatically
- Can still use mower for cutting (mechanical PTO)
- Stop engine when battery full or done mowing

---

### Example 2: RV Generator (Manual Throttle)

**Hardware:**
- Honda clone 5 HP engine
- Manual throttle cable to dashboard
- RPM gauge visible to operator

**Configuration:**
```bash
gen_config 3000 40 0.80 1000
```

**Operation:**
1. Operator sets throttle based on needs:
   - Idle: Minimum noise, trickle charge
   - Half: Moderate charging
   - Full: Maximum charging (short periods)

2. VESC adapts automatically:
   - Below 2400 RPM: No load
   - 2400-3000 RPM: Proportional load
   - Above 3000: Full 40A

**User experience:**
- Turn throttle like volume control
- More throttle = faster charging + more noise
- Less throttle = quieter + slower charging
- Can optimize for situation

---

### Example 3: Automatic System (Electronic Throttle)

**Hardware:**
- 5 HP engine with governor
- RC servo on throttle linkage
- VESC servo output connected
- Battery voltage monitoring

**Custom app code addition:**
```c
// Add to generator app
float battery_voltage = mc_interface_get_input_voltage();
float target_rpm;

if (battery_voltage < 48.0) {
    // Battery low, full power
    target_rpm = 3000.0;
} else if (battery_voltage < 52.0) {
    // Battery medium, moderate power
    target_rpm = 2500.0;
} else {
    // Battery high, idle
    target_rpm = 1500.0;
}

// Set servo position based on target RPM
float throttle_pos = (target_rpm - 1000.0) / 2000.0;
pwm_servo_set_pos(throttle_pos);

// Still use electrical load control
gen_config as normal
```

**Operation:**
- Completely automatic
- Battery low → Throttle increases → More charging
- Battery full → Throttle decreases → Fuel savings
- Could even implement auto start/stop

---

## Carburetor Types & Considerations

### Fixed Jet Carburetor
Most common on small engines.

**Characteristics:**
- Air/fuel ratio changes with throttle position
- Richest at mid-throttle
- Leaner at extremes (idle and WOT)

**Implications:**
- Fixed throttle position = consistent mixture
- Best efficiency at specific throttle position
- Tune carburetor for your chosen operating point

**Recommendation:**
- Find throttle position with best efficiency
- Lock throttle there (Strategy 1)
- Let VESC vary load

### Governor-Controlled Carburetor
Engine speed feedback adjusts throttle.

**Characteristics:**
- Automatically maintains RPM under varying load
- Throttle opens when load increases
- Closes when load decreases

**Implications:**
- Works well with fixed electrical load
- Can fight with VESC speed control
- Best with Strategy 1 or 2

**Recommendation:**
- Use proportional current or constant power
- Don't use VESC speed governor
- Let mechanical governor do its job

### Manual/Butterfly Carburetor
User directly controls air/fuel.

**Characteristics:**
- Direct throttle control
- No automatic adjustment
- Requires careful operation

**Implications:**
- Must match throttle to load manually
- Can stall if overloaded
- Gives maximum user control

**Recommendation:**
- Use Strategy 2 (Manual Throttle)
- Operator learns optimal positions
- VESC proportional current adapts

---

## Common Integration Issues

### Problem: Engine Stalls Under Load

**Symptoms:**
- Engine runs fine no-load
- Bogs down when VESC applies current
- May stall completely

**Causes:**
1. Carburetor mixture too lean
2. Throttle position too low
3. VESC load too aggressive
4. Engine not warmed up

**Solutions:**
- ✅ Increase throttle position
- ✅ Reduce VESC current: `gen_config 2500 15 0.90 1000`
- ✅ Adjust carburetor mixture (richer)
- ✅ Always warm up engine first
- ✅ Increase `gen_start` ratio

### Problem: Engine Races/Hunting

**Symptoms:**
- RPM oscillates up and down
- Unstable speed
- Noise varies

**Causes:**
1. VESC and governor fighting
2. PID gains too aggressive
3. Carburetor hunting
4. Loose governor linkage

**Solutions:**
- ✅ Use current mode, not speed governor
- ✅ Check mechanical governor adjustment
- ✅ Reduce VESC update rate: `gen_config 2500 25 0.85 500`
- ✅ Check carburetor idle mixture
- ✅ Tighten all linkages

### Problem: Poor Fuel Efficiency

**Symptoms:**
- Excessive fuel consumption
- Engine running but little charging

**Causes:**
1. Throttle set too high for load
2. Constant power mode not used
3. Battery full but engine still running
4. Carburetor mixture wrong

**Solutions:**
- ✅ Use constant power mode: `gen_power 1000 1800 30`
- ✅ Reduce fixed throttle position
- ✅ Add auto-shutoff when battery full
- ✅ Tune carburetor for efficiency
- ✅ Monitor actual power vs fuel consumption

### Problem: Won't Generate at Idle

**Symptoms:**
- Engine idles fine
- Zero current from VESC
- Increasing throttle helps

**Causes:**
1. `gen_start` threshold too high
2. Idle RPM below threshold
3. Battery voltage limits hit
4. VESC in error state

**Solutions:**
- ✅ Lower threshold: `gen_config 2000 25 0.75 1000`
- ✅ Increase idle speed (carburetor adjustment)
- ✅ Check battery BMS not blocking
- ✅ Verify VESC app set to "Custom"
- ✅ Check `gen_status` for actual RPM

---

## Advanced: Closed-Loop Throttle Control

For those wanting to implement sophisticated automatic control:

### Feedback Control Loop

```
Battery SOC → [Controller] → Throttle Servo → Engine → Generator → Battery SOC
                 ↑                                                        ↓
                 └────────────────────────────────────────────────────────┘
```

### Control Algorithm

```c
// PID controller for throttle based on battery
static float integral = 0;
static float last_error = 0;

float target_voltage = 50.4;  // Target battery voltage
float voltage = mc_interface_get_input_voltage();

// Error signal
float error = target_voltage - voltage;

// PID calculation
float Kp = 0.05;
float Ki = 0.001;
float Kd = 0.01;

integral += error;
float derivative = error - last_error;
float throttle = 0.5 + (Kp * error) + (Ki * integral) + (Kd * derivative);

// Clamp throttle
if (throttle > 0.9) throttle = 0.9;
if (throttle < 0.1) throttle = 0.1;

// Output to servo
pwm_servo_set_pos(throttle);

last_error = error;
```

### Safety Considerations

**Must have:**
- Watchdog timer (revert to safe throttle if control freezes)
- Manual override (physical cable)
- RPM limiter (protect engine)
- Temperature monitoring (overheat protection)
- Failsafe defaults (idle on error)

**Recommended:**
- Throttle position feedback sensor
- Engine RPM monitoring
- Fuel level sensing
- Auto start/stop logic
- Remote monitoring capability

---

## Summary: Choosing Throttle Strategy

| Application | Recommended Strategy | Throttle Setup | VESC Mode |
|-------------|---------------------|----------------|-----------|
| Simple backup generator | Fixed Throttle | Lock at 60% | Current |
| RV/Mobile range extender | Manual Throttle | Cable to dash | Current |
| Lawn mower hybrid | Fixed Throttle | 3/4 position | Current |
| Portable AC generator | Constant Speed | 75% + governor | Governor |
| Automatic range extender | Electronic Control | Servo controlled | Power |
| Test bench | Manual Throttle | Hand control | Current |

**General guidance:**
1. **Start simple** - Fixed throttle + proportional current
2. **Test and tune** - Find optimal operating point
3. **Upgrade if needed** - Add automation only if necessary
4. **Safety first** - Always have manual override capability

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
