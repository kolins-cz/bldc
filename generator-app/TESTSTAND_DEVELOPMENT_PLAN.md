# Generator Application Test Stand Development Plan

This document outlines the development plan for validating the generator application using a dual-motor test stand before deployment on an actual ICE generator.

## Table of Contents
- [Test Stand Overview](#test-stand-overview)
- [Hardware Setup](#hardware-setup)
- [Safety Considerations](#safety-considerations)
- [Development Phases](#development-phases)
- [Test Procedures](#test-procedures)
- [Data Collection](#data-collection)
- [Troubleshooting Guide](#troubleshooting-guide)
- [Success Criteria](#success-criteria)

---

## Test Stand Overview

### Purpose
Validate generator application functionality in a controlled environment before deploying on ICE generator system.

### Configuration
```
┌─────────────┐    Mechanical    ┌─────────────┐
│   Motor 1   │◄──── Shaft ─────►│   Motor 2   │
│  (Prime     │     Coupling     │ (Generator) │
│   Mover)    │                  │             │
└──────┬──────┘                  └──────┬──────┘
       │                                │
       │                                │
┌──────▼──────┐                  ┌──────▼──────┐
│   VESC 1    │                  │   VESC 2    │
│  STOCK FW   │                  │  GENERATOR  │
│ (DUTY/RPM)  │                  │   APP FW    │
└──────┬──────┘                  └──────┬──────┘
       │                                │
       └────────────┬───────────────────┘
                    │
            ┌───────▼────────┐
            │  Power Supply  │
            │   or Battery   │
            └────────────────┘
```

### Advantages of Test Stand
- **Controlled environment**: No fuel, exhaust, noise, vibration
- **Repeatable tests**: Exact same conditions every time
- **Safety**: Easy emergency stop, no engine hazards
- **Rapid iteration**: Instant startup, no warm-up time
- **Precise control**: Stock VESC provides exact RPM simulation
- **Measurement**: Easy to add sensors, oscilloscope probes
- **Debugging**: Full VESC Tool access on both controllers

---

## Hardware Setup

### Bill of Materials

| Component | Quantity | Purpose | Notes |
|-----------|----------|---------|-------|
| BLDC Motor | 2 | Prime mover + Generator | Same model as final application |
| VESC 410 | 2 | Motor controllers | One stock, one with generator FW |
| Shaft Coupling | 1 | Mechanical connection | Rigid or flexible coupling |
| Motor Mounts | 2 | Secure motors to bench | Vibration-resistant |
| Power Supply | 1 | 48V bench supply | Or Li-ion battery pack |
| Load Resistor | 1 | Alternative passive load | 50-100W, for initial tests |
| Hall Sensor Board | 1 | RPM measurement | Optional, for validation |
| Current Clamp | 1 | Current measurement | Oscilloscope or multimeter |
| USB Cables | 2 | VESC Tool connection | For both VESCs |
| CAN Cable | 1 | VESC-to-VESC comms | Optional for later phases |

### Electrical Connections

#### VESC 1 (Prime Mover - Stock Firmware)
```
Power Supply +48V → VESC1 VIN+
Power Supply GND  → VESC1 GND
VESC1 Phase A/B/C → Motor1 Phase A/B/C
VESC1 Hall 1/2/3  → Motor1 Hall 1/2/3
USB               → PC (VESC Tool)
```

#### VESC 2 (Generator - Custom Firmware)
```
Power Supply +48V → VESC2 VIN+
Power Supply GND  → VESC2 GND
VESC2 Phase A/B/C → Motor2 Phase A/B/C
VESC2 Hall 1/2/3  → Motor2 Hall 1/2/3
USB               → PC (VESC Tool)
(Optional) CAN-H/L → VESC1 CAN-H/L
```

#### Optional: Servo for Future Testing
```
VESC2 HW_ICU Pin → RC Servo Signal (for throttle simulation)
External 5V      → Servo Power
GND              → Servo Ground
```

### Mechanical Assembly

1. **Motor Mounting**
   - Mount both motors on bench with ~50-100mm spacing
   - Ensure shafts are **perfectly aligned** (use laser alignment tool if available)
   - Secure mounts - motors will generate significant torque
   - Leave access to motor connections

2. **Shaft Coupling**
   - **Rigid coupling**: Better for high-speed testing, requires perfect alignment
   - **Flexible coupling**: Forgiving of minor misalignment, recommended for initial tests
   - Use coupling rated for motor torque (check motor specs)
   - Ensure proper shaft insertion depth and secure set screws

3. **Safety Enclosure**
   - Protect rotating shaft with guard
   - Emergency stop button easily accessible
   - Warning labels on moving parts
   - Ventilation for motor cooling

### Power Supply Considerations

**Bench Power Supply:**
- Voltage: 48V (or match target battery voltage)
- Current: 30A minimum (for 1kW+ testing)
- Over-current protection
- Adjustable voltage (45-54V) to simulate battery discharge

**Battery Pack (Alternative):**
- 13S Li-ion (46.8V - 54.6V)
- BMS with balancing
- Capacity: 5Ah minimum for extended tests
- Built-in protection

**Important**: Generator will push current BACK into supply
- Bench supply must accept reverse current (most lab supplies do)
- Battery is ideal as it naturally accepts charging
- Add dump resistor as safety backup

---

## Safety Considerations

### Mechanical Hazards
- ⚠️ **Rotating shaft**: Can catch clothing, tools, fingers
- ⚠️ **High speed**: Motors may spin >10,000 RPM
- ⚠️ **Coupling failure**: Could launch projectiles
- ⚠️ **Vibration**: Improper alignment causes excessive vibration

**Mitigations:**
- Install shaft guard before ANY powered tests
- Secure all tools and loose items away from test stand
- Start with low RPM (<1000) to check alignment
- Never touch motors while powered
- Tether motors to bench (safety cable)

### Electrical Hazards
- ⚠️ **48V DC**: Can cause shock, especially with wet hands
- ⚠️ **High current**: Potential for arcing, fire
- ⚠️ **Regenerative current**: Generator pushes current back

**Mitigations:**
- Power OFF when making connections
- Use insulated terminals
- Check polarity before power-on
- Keep multimeter nearby to verify voltage
- Use proper wire gauge (14 AWG minimum for 30A)
- Install fuse or circuit breaker

### Software/Control Hazards
- ⚠️ **Unexpected motor start**: VESC may start motor on power-up
- ⚠️ **Runaway**: Motor accelerates uncontrolled
- ⚠️ **Firmware bugs**: Generator app may behave unexpectedly

**Mitigations:**
- **ALWAYS** disable motor output in VESC Tool before testing
- Test prime mover VESC alone first (disconnect generator motor)
- Keep hand on emergency stop / power switch
- Start with very conservative parameters (low current, low RPM)
- Monitor VESC Tool RT data continuously

### Emergency Procedures

**Emergency Stop Sequence:**
1. Press STOP button in VESC Tool (or spacebar)
2. If that fails: Cut power (switch or pull plug)
3. Wait for motors to spin down completely (10-30 seconds)
4. Do NOT touch until fully stopped

**Test Abort Criteria:**
- Excessive vibration or noise
- Smoke or burning smell
- VESC overheat warning (>80°C)
- Unexpected behavior (wrong direction, acceleration)
- Coupling slippage
- Any unusual observation

---

## Development Phases

### Phase 0: Baseline Setup and Validation
**Goal**: Ensure test stand hardware works correctly before generator testing

**Duration**: 1-2 hours

**Tasks**:
1. Assemble mechanical test stand
2. Connect VESC 1 (stock firmware) to Motor 1
3. Run motor detection wizard in VESC Tool
4. Test motor in duty cycle mode (no load)
5. Verify motor spins smoothly in both directions
6. Test motor in RPM mode (no load)
7. Verify RPM control accuracy

**Success Criteria**:
- Motor detection completes successfully
- Motor spins smoothly without vibration
- RPM control holds setpoint within ±5%
- No overheating, errors, or faults

**Deliverables**:
- Motor detection results saved
- VESC configuration backed up
- Test log with observations

---

### Phase 1: Passive Load Testing
**Goal**: Validate basic generator app function with simple resistive load

**Duration**: 2-3 hours

**Prerequisites**: Phase 0 complete

**Setup**:
- VESC 2 (generator firmware) connected to Motor 2
- Motors mechanically coupled
- Power resistor (50-100W) connected across VESC 2 battery terminals
- Both VESCs connected to same power supply

**Tasks**:

**1.1 Motor Detection (Generator Side)**
```bash
# In VESC Tool connected to VESC 2:
1. Motor Settings → FOC → General → Run Detection
2. Apply settings
3. Write Configuration
```

**1.2 Configure Generator App**
```bash
# In VESC Tool Terminal (VESC 2):
gen_config 10000 5.0 0.8 100
# Target: 10,000 ERPM (low to start)
# Max current: 5A (conservative)
# Start ratio: 80%
# Update rate: 100 Hz
```

**1.3 Manual Prime Mover Test**
```bash
# In VESC Tool (VESC 1):
1. Set Current to 0A
2. Slowly increase Duty Cycle: 0.05 → 0.10 → 0.15
3. Observe VESC 2 status
```

**Expected Behavior**:
- VESC 1 motor spins up smoothly
- VESC 2 motor follows (mechanically coupled)
- When RPM > 10,000 ERPM, generator app engages
- Regenerative current flows into power resistor
- VESC 1 load increases (harder to turn)

**Measurements**:
- RPM on both VESCs (should match)
- Current from VESC 1 (motor current)
- Current to VESC 2 (regenerative current)
- Power dissipated in resistor
- MOSFET temperature on both VESCs

**Success Criteria**:
- Generator app engages at correct RPM threshold
- Regenerative current limited to configured maximum (5A)
- No faults or errors
- Temperature remains <70°C
- Power balance: P_motor ≈ P_regen + P_losses

**Deliverables**:
- Test log with measurements
- Screenshots of VESC Tool RT data
- Thermal images (if available)

---

### Phase 2: Battery Charging Test
**Goal**: Validate generator charging actual battery (or bench supply)

**Duration**: 2-3 hours

**Prerequisites**: Phase 1 complete

**Setup**:
- Replace resistor with battery pack (or use bench supply)
- Same mechanical setup as Phase 1

**Tasks**:

**2.1 Low-Power Initial Test**
```bash
# VESC 2 Terminal:
gen_config 10000 5.0 0.8 100

# VESC 1: Slowly ramp duty cycle to reach 10,000 ERPM
```

**Expected**: Generator charges battery at ~5A max

**2.2 Increase Current**
```bash
# Gradually increase max current
gen_config 10000 10.0 0.8 100  # 10A
gen_config 10000 15.0 0.8 100  # 15A
gen_config 10000 20.0 0.8 100  # 20A
```

**Monitor**:
- Battery voltage rise
- Current stability
- MOSFET temperature
- Motor temperature
- Any oscillations or instability

**2.3 RPM Sweep**
```bash
# Test different RPM setpoints
gen_config 5000 10.0 0.8 100   # 5k ERPM
gen_config 15000 10.0 0.8 100  # 15k ERPM
gen_config 20000 10.0 0.8 100  # 20k ERPM
gen_config 30000 10.0 0.8 100  # 30k ERPM (target for ICE)
```

**Success Criteria**:
- Stable charging at all current levels up to 20A
- Battery voltage increases (or bench supply accepts current)
- No oscillations or hunting
- Temperature <75°C at 20A continuous
- Proportional control algorithm works across RPM range

**Deliverables**:
- Power curves: Current vs RPM
- Efficiency measurements
- Temperature profiles
- Battery charging curves

---

### Phase 3: Dynamic Response Testing
**Goal**: Validate generator response to RPM changes (simulates throttle changes)

**Duration**: 2-3 hours

**Prerequisites**: Phase 2 complete

**Tasks**:

**3.1 Step Response**
```bash
# VESC 2 configured for 30,000 ERPM, 20A max
gen_config 30000 20.0 0.8 100

# VESC 1: Apply RPM steps
0 ERPM → 30,000 ERPM (step up)
30,000 ERPM → 35,000 ERPM (step up)
35,000 ERPM → 30,000 ERPM (step down)
30,000 ERPM → 25,000 ERPM (step down below threshold)
```

**Measure**:
- Time to reach target current
- Overshoot/undershoot
- Settling time
- Current ripple

**3.2 Ramp Response**
```bash
# VESC 1: Slowly ramp RPM
25,000 → 35,000 ERPM over 10 seconds
35,000 → 25,000 ERPM over 10 seconds
```

**Measure**:
- Current tracking during ramp
- Smoothness of response
- Lag between RPM change and current change

**3.3 Oscillation Testing**
```bash
# VESC 1: Create RPM oscillation
30,000 ±2000 ERPM at 1 Hz
30,000 ±2000 ERPM at 5 Hz
```

**Observe**:
- Generator current response
- Stability at different frequencies
- Potential resonances

**Success Criteria**:
- Fast response to RPM changes (<100ms)
- No excessive overshoot (<10%)
- Stable under oscillating prime mover
- Current never exceeds configured limit

**Deliverables**:
- Step response plots
- Frequency response data
- Stability margins

---

### Phase 4: Edge Cases and Fault Handling
**Goal**: Test abnormal conditions and verify safe behavior

**Duration**: 2-3 hours

**Prerequisites**: Phase 3 complete

**Tasks**:

**4.1 Overspeed Test**
```bash
# Generator configured for 30k ERPM
gen_config 30000 20.0 0.8 100

# VESC 1: Increase RPM beyond target
30,000 → 40,000 → 50,000 ERPM
```

**Expected**: Current increases proportionally but stays within limits

**4.2 Undervoltage/Overvoltage**
```bash
# Bench supply: Adjust voltage
45V → 40V (undervoltage - battery depleted simulation)
50V → 55V → 58V (overvoltage - battery full simulation)
```

**Expected**: 
- Undervoltage: Current capability limited by voltage
- Overvoltage: Generator should still regulate current correctly

**4.3 Sudden Load Change**
```bash
# While running at steady state:
1. Disconnect/reconnect battery (use physical switch)
2. Observe VESC behavior and recovery
```

**Expected**: Graceful handling, no damage

**4.4 Timeout Test**
```bash
# While running, close VESC Tool or disconnect USB
# Wait >timeout period (default 1 second)
```

**Expected**: VESC releases motor (current → 0A)

**4.5 Direction Reversal**
```bash
# VESC 1: Spin motor backward
# (Should not happen on real generator, but test anyway)
```

**Expected**: Generator app ignores or handles gracefully

**4.6 Very Low RPM**
```bash
gen_config 30000 20.0 0.8 100
# VESC 1: Run at 1000, 2000, 5000 ERPM (below threshold)
```

**Expected**: Generator app stays inactive, no regen current

**Success Criteria**:
- No unsafe behavior in any test
- No VESC faults or reboots
- Graceful degradation under extreme conditions
- Timeout functions correctly
- No damage to hardware

**Deliverables**:
- Fault log with all edge cases tested
- Behavior documentation
- Any firmware bugs discovered

---

### Phase 5: Constant Power Mode (Optional Enhancement)
**Goal**: Implement and validate constant power control algorithm

**Duration**: 4-6 hours

**Prerequisites**: Phase 4 complete

**Background**: Proportional current mode (current implementation) causes power to vary with voltage. Constant power mode provides more consistent energy delivery.

**Tasks**:

**5.1 Implement Constant Power Algorithm**
```c
// Modify app_generator.c
// Instead of: mc_interface_set_current(current)
// Calculate: current = target_power / voltage
float v_in = mc_interface_get_input_voltage(false);
float current = gen_power / v_in;  // P = V × I
if (current > gen_current) current = gen_current;
mc_interface_set_current(-current);
```

**5.2 Test Power Regulation**
```bash
# Terminal: Configure for constant power
gen_config 30000 500.0 0.8 100  # 500W target power
```

**Test matrix**:
| Voltage | Target Power | Expected Current |
|---------|--------------|------------------|
| 45V | 500W | 11.1A |
| 48V | 500W | 10.4A |
| 50V | 500W | 10.0A |
| 52V | 500W | 9.6A |
| 54V | 500W | 9.3A |

**Measure**: Actual current vs expected

**5.3 Compare Modes**
```bash
# Run same test with proportional current mode
# Run same test with constant power mode
# Compare efficiency, stability, usability
```

**Success Criteria**:
- Power remains constant as voltage varies
- Current adjusts inversely with voltage
- Smooth transition as voltage changes
- At least equal efficiency to proportional mode

**Deliverables**:
- Modified source code
- Comparison report: proportional vs constant power
- Recommendation for ICE generator use

---

### Phase 6: Automatic Throttle Simulation
**Goal**: Test servo output for automatic throttle control

**Duration**: 2-3 hours

**Prerequisites**: Phase 2 complete, RC servo available

**Setup**:
- Connect RC servo to VESC 2 HW_ICU pin
- Servo simulates carburetor throttle
- Mechanical linkage to VESC 1 duty cycle (manual simulation) OR
- Programmatic control: servo position → affects VESC 1 RPM

**Tasks**:

**6.1 Servo Output Test**
```c
// Add servo test command to app_generator.c
terminal_register_command_callback(
    "servo_test",
    "Test servo output",
    "[position]",
    terminal_servo_test);
```

```bash
# Terminal test commands
servo_test 0.0   # Min position (1000 μs)
servo_test 0.5   # Mid position (1500 μs)
servo_test 1.0   # Max position (2000 μs)
```

**Verify**: Servo moves smoothly through full range

**6.2 Voltage-Based Throttle Control**
```c
// Modify gen_thread() to control throttle based on battery voltage
float v_in = mc_interface_get_input_voltage(false);
float throttle;

if (v_in < 48.0) {
    throttle = 0.60;  // High throttle when battery low
} else if (v_in < 52.0) {
    throttle = 0.30;  // Medium
} else {
    throttle = 0.10;  // Idle when full
}

pwm_servo_set_servo_out(throttle);
```

**6.3 Closed-Loop Test**
```bash
# Manual simulation:
# 1. Servo moves to high position (0.60)
# 2. Manually increase VESC 1 duty to simulate throttle response
# 3. Battery voltage rises
# 4. Servo should automatically reduce to lower position
# 5. Manually decrease VESC 1 duty
# 6. Verify control loop stability
```

**Success Criteria**:
- Servo responds correctly to voltage changes
- Smooth servo motion (no jitter)
- Control loop is stable (no oscillation)
- Servo returns to idle when voltage is high

**Deliverables**:
- Servo control code
- Test videos showing automatic throttle response
- Tuning parameters for ICE application

---

### Phase 7: Endurance and Reliability Testing
**Goal**: Validate long-term operation and thermal management

**Duration**: 8+ hours (can run unattended with monitoring)

**Prerequisites**: All previous phases complete

**Tasks**:

**7.1 Continuous Operation Test**
```bash
# Configure for moderate power (not max)
gen_config 30000 15.0 0.8 100  # 15A = ~720W at 48V

# VESC 1: Hold steady RPM at 30,000 ERPM
# Let run for 4-8 hours
```

**Monitor** (log every 1 minute):
- MOSFET temperature (both VESCs)
- Motor temperature (both motors)
- Battery voltage
- Current (actual vs commanded)
- Any faults or warnings

**7.2 Thermal Cycling**
```bash
# Cycle power on/off
5 minutes ON (max power) → 5 minutes OFF → repeat 10x
```

**Observe**: Thermal expansion, connection integrity

**7.3 Varying Load Profile**
```bash
# Simulate real-world usage pattern
10 min @ 500W
5 min @ 1000W
2 min @ 1500W
10 min @ 200W
5 min @ 0W (idle)
Repeat
```

**7.4 Long-Term Data Collection**
```python
# Python script to log VESC data via USB
import time
from vesc import VESC

vesc = VESC('/dev/ttyACM0')

while True:
    data = vesc.get_values()
    print(f"{time.time()}, {data.rpm}, {data.current}, "
          f"{data.voltage}, {data.temp_mosfet}, {data.temp_motor}")
    time.sleep(1)
```

**Success Criteria**:
- No faults during 8+ hour run
- Temperature stabilizes below 70°C
- Current remains stable within ±2%
- No degradation over time
- No connection failures

**Deliverables**:
- Long-term data logs
- Thermal profiles
- Reliability assessment report
- Identified weaknesses or concerns

---

### Phase 8: Integration Testing (CAN Bus)
**Goal**: Test VESC-to-VESC communication and coordinated control

**Duration**: 3-4 hours

**Prerequisites**: Phase 3 complete

**Setup**:
- Connect CAN-H and CAN-L between VESC 1 and VESC 2
- Both VESCs on same CAN network
- Configure CAN IDs (VESC 1 = ID 0, VESC 2 = ID 1)

**Tasks**:

**8.1 CAN Forwarding**
```bash
# VESC Tool connected to VESC 1
# Should be able to see and configure VESC 2 via CAN forwarding
```

**8.2 Status Monitoring**
```c
// Add to app_generator.c
// Read prime mover VESC status via CAN
void gen_thread() {
    can_status_msg *msg = comm_can_get_status_msg_id(0);
    if (msg) {
        float prime_mover_rpm = msg->rpm;
        float prime_mover_current = msg->current;
        // Use for advanced control or monitoring
    }
}
```

**8.3 Coordinated Control**
```c
// Example: Generator requests more/less power from prime mover
if (battery_voltage < 48.0) {
    // Request higher RPM from prime mover
    comm_can_set_rpm(0, 35000);  // ID 0, 35k ERPM
}
```

**Success Criteria**:
- CAN communication established
- VESC Tool can access both VESCs
- Status messages received correctly
- Coordinated control functions (if implemented)

**Deliverables**:
- CAN communication code
- Network topology documentation
- Protocol documentation

---

## Test Procedures

### Standard Operating Procedure: Test Stand Startup

1. **Visual Inspection**
   - Check all connections tight
   - Verify shaft coupling secure
   - Ensure no tools or debris near motors
   - Confirm safety guard in place

2. **Pre-Power Checks**
   - Set bench supply to OFF
   - Verify voltage setting (48V)
   - Current limit set appropriately (30A)
   - VESC Tool open and connected

3. **Power-Up Sequence**
   - Enable bench supply (no load current should be <0.1A)
   - Verify battery voltage on both VESCs
   - Check for any fault LEDs
   - Open VESC Tool RT data view

4. **Pre-Run Verification**
   - Disable motor output in VESC Tool
   - Configure test parameters
   - Review test procedure
   - Identify emergency stop method

5. **Start Test**
   - Enable motor output
   - Slowly increase prime mover duty/RPM
   - Monitor all parameters
   - Document observations

### Standard Operating Procedure: Test Stand Shutdown

1. **Controlled Stop**
   - Slowly reduce prime mover duty to 0
   - Wait for motors to stop spinning
   - Disable motor output in VESC Tool

2. **Data Collection**
   - Save VESC Tool logs
   - Take photos/thermal images
   - Record final observations
   - Note any anomalies

3. **Power-Down**
   - Disable bench supply
   - Disconnect USB cables
   - Allow motors to cool (10+ minutes)

4. **Post-Test Inspection**
   - Check for loose connections
   - Verify no damage or wear
   - Look for burnt components
   - Document condition

---

## Data Collection

### Required Measurements

**Real-Time (during test):**
- RPM (both motors) - should match
- Current motor (VESC 1)
- Current regen (VESC 2)
- Battery voltage
- MOSFET temp (both VESCs)
- Motor temp (both motors)
- Duty cycle (VESC 1)

**Calculated:**
- Power input (V × I_motor)
- Power output (V × I_regen)
- Efficiency (P_out / P_in × 100%)
- Temperature rise rate (°C/min)

**Logged (1 Hz minimum):**
- All real-time measurements
- Timestamp
- Test phase/condition
- Any events or changes

### Data Format

CSV file with headers:
```csv
timestamp,rpm_motor,rpm_gen,current_motor,current_regen,voltage,temp_mosfet1,temp_mosfet2,temp_motor1,temp_motor2,duty,power_in,power_out,efficiency
```

### Analysis

For each test:
1. Plot RPM vs Time
2. Plot Current vs Time  
3. Plot Power vs Time
4. Plot Temperature vs Time
5. Calculate average efficiency
6. Identify any oscillations or instability
7. Compare to predicted behavior

---

## Troubleshooting Guide

### Problem: Motors won't spin

**Possible Causes:**
- Motor detection not run
- Phase wires incorrect
- Hall sensors wrong
- Motor disabled in VESC Tool
- Overcurrent fault

**Solutions:**
1. Re-run motor detection wizard
2. Verify phase wire connections (swap any two if wrong direction)
3. Check hall sensor wiring and polarity
4. Enable motor output
5. Check fault codes, reset VESC

---

### Problem: Excessive vibration

**Possible Causes:**
- Shaft misalignment
- Loose coupling
- Motor mount loose
- Unbalanced rotor
- Resonance frequency

**Solutions:**
1. Re-align shafts (use laser alignment tool)
2. Tighten coupling set screws
3. Secure motor mounts
4. Reduce RPM to avoid resonance
5. Add damping (rubber mounts)

---

### Problem: Generator not engaging

**Possible Causes:**
- RPM below threshold (gen_erpm)
- Generator app not running
- Start ratio too high
- Timeout expired

**Solutions:**
1. Check actual RPM: `gen_status`
2. Verify generator app loaded: check VESC Tool app config
3. Lower start ratio: `gen_config 30000 20.0 0.5 100`
4. Ensure VESC Tool not sending commands (timeout)

---

### Problem: Current oscillation/hunting

**Possible Causes:**
- Control loop too aggressive
- Update rate too high
- Mechanical resonance
- Electrical resonance (LC circuit)
- Proportional gain too high

**Solutions:**
1. Reduce update rate: `gen_config 30000 20.0 0.8 50`
2. Lower max current temporarily
3. Add damping (small load resistor in parallel)
4. Check for loose connections
5. Modify control algorithm (add derivative term)

---

### Problem: Overheating

**Possible Causes:**
- Excessive current
- Poor cooling airflow
- Continuous high power
- High ambient temperature
- Motor/VESC undersized

**Solutions:**
1. Reduce current: `gen_config 30000 10.0 0.8 100`
2. Add cooling fan
3. Reduce duty cycle (run intermittently)
4. Improve ventilation
5. Use larger VESC or motor

---

### Problem: Direction goes wrong way

**Possible Causes:**
- Phase wires swapped
- Motor detection reversed
- FOC observer wrong

**Solutions:**
1. Swap any two phase wires
2. In VESC Tool: Motor Settings → FOC → Invert Direction
3. Re-run motor detection
4. Check hall sensor polarity

---

## Success Criteria

### Phase-by-Phase Acceptance

Each phase must meet its success criteria before proceeding to next phase.

### Overall Project Success

System is ready for ICE generator deployment when:

✅ **Functionality**
- Generator app engages at correct RPM
- Current control accurate within ±5%
- Proportional control algorithm works correctly
- Timeout and safety features function
- Terminal commands work reliably

✅ **Performance**
- Efficiency >85% (motor to regen)
- Stable operation from 10,000 to 50,000 ERPM
- Current regulation up to 20A
- Temperature <70°C continuous at rated power

✅ **Reliability**
- 8+ hours continuous operation without faults
- No degradation over time
- Survives all edge case tests
- Safe behavior under all failure modes

✅ **Documentation**
- All test data collected and analyzed
- Configuration parameters documented
- Known issues and limitations identified
- Tuning guide written for ICE application

✅ **Optional Features** (if implemented)
- Constant power mode validated
- Servo throttle control working
- CAN communication functional

### Final Validation Test

**24-Hour Continuous Run:**
- Configure for typical ICE generator profile
- Vary load every 30 minutes (simulate real usage)
- Log all data continuously
- Zero faults or manual interventions required
- All parameters within specifications

If successful: **Ready for ICE generator integration**

---

## Next Steps After Test Stand Validation

1. **Prepare ICE Generator Hardware**
   - Install VESC 2 (generator firmware)
   - Install throttle servo
   - Connect to engine via clutch or direct drive

2. **ICE-Specific Calibration**
   - Determine actual ERPM at engine idle
   - Determine ERPM at target operating speed
   - Tune throttle servo endpoints
   - Configure voltage thresholds for throttle control

3. **Integration Testing**
   - Start engine manually
   - Verify generator engages at correct RPM
   - Test automatic throttle control
   - Validate charging under load

4. **Field Testing**
   - Run generator under real load (tools, appliances)
   - Test start/stop cycles
   - Measure fuel efficiency
   - Assess noise and vibration

5. **Documentation Updates**
   - Record any differences from test stand
   - Update tuning parameters
   - Create operator manual
   - Document maintenance procedures

---

## Appendix: Quick Reference

### Important Terminal Commands

```bash
# Configure generator
gen_config <erpm> <current> <start_ratio> <rate_hz>

# Check status
gen_status

# Test servo (if implemented)
servo_test <0.0-1.0>
```

### VESC Tool RT Data Fields

**Monitor These:**
- RPM (ERPM / pole_pairs)
- Current In (battery current)
- Current Motor (motor current)
- Duty Cycle
- Voltage In (battery voltage)
- Temp MOSFET
- Temp Motor
- Fault Code

### Typical Parameters for 48V System

```bash
# Conservative (testing)
gen_config 30000 10.0 0.8 100

# Moderate (normal operation)
gen_config 30000 20.0 0.8 100

# Aggressive (max power)
gen_config 30000 30.0 0.8 100
```

### Power Calculations

```
Power (W) = Voltage (V) × Current (A)
Torque (Nm) = (Current × Kt) / pole_pairs
RPM = ERPM / pole_pairs
Mechanical Power (W) = Torque × RPM × 2π / 60
```

### Temperature Limits

- MOSFET: 85°C max, 70°C recommended continuous
- Motor: 100°C max, 80°C recommended continuous
- PCB: 70°C max

---

**Document Version**: 1.0  
**Created**: March 2026  
**For**: VESC Generator Application Development  
**Test Stand**: Dual BLDC Motor Configuration
