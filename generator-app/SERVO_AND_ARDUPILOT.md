# RC Servo Control & Ardupilot Integration

This document covers RC servo PWM capabilities in VESC firmware and options for integrating the generator system with Ardupilot autopilot.

## Table of Contents
- [RC Servo Control](#rc-servo-control)
- [Ardupilot Integration Options](#ardupilot-integration-options)
- [DroneCAN Integration (Recommended)](#dronecan-integration-recommended)
- [UART Serial Integration](#uart-serial-integration)
- [Implementation Examples](#implementation-examples)

---

## RC Servo Control

### Overview
VESC firmware includes a PWM servo driver (`driver/pwm_servo.c/h`) that generates standard RC servo control signals. This is **perfect for automatic throttle/carburetor control** on ICE generators.

### Technical Specifications
- **Pulse Width Range**: 1000-2000 μs (standard RC servo protocol)
- **Update Rate**: 50 Hz (20ms period)
- **Resolution**: Sub-microsecond precision
- **Output Pin**: HW_ICU pin (conflicts with PPM input when enabled)
- **Control Range**: 0.0 to 1.0 (maps to 1000-2000μs)

### Configuration Constants
```c
// From conf_general.h
#define SERVO_OUT_PULSE_MIN_US  1000    // Minimum pulse width (μs)
#define SERVO_OUT_PULSE_MAX_US  2000    // Maximum pulse width (μs)
#define SERVO_OUT_RATE_HZ       50      // Update rate (Hz)
```

### API Functions

```c
// Initialize servo output
void pwm_servo_init_servo(void);

// Set servo position (0.0 = min, 1.0 = max)
// Maps to 1000-2000μs pulse width
void pwm_servo_set_servo_out(float output);

// Stop servo output
void pwm_servo_stop(void);

// Check if servo is running
bool pwm_servo_is_running(void);
```

### Hardware Connection

**VESC Pin**: HW_ICU (Input Capture Unit pin)
- Same pin normally used for PPM receiver input
- **Cannot use PPM input and servo output simultaneously**
- For generator application: Servo output has priority (no remote control needed)

**Servo Connection**:
```
VESC HW_ICU Pin → Servo Signal (white/yellow)
VESC GND        → Servo Ground (black/brown)
Servo +5V       → External 5V supply (red/orange)
```

**Important**: Power the servo from external 5V supply, not from VESC. Most servos draw too much current for VESC's 5V regulator.

### Usage in Generator Application

```c
#include "driver/pwm_servo.h"

// In app_custom_start()
void app_custom_start(void) {
    // Enable servo output (disables PPM input)
    // Note: This is typically set in app configuration
    
    // Initialize servo driver
    pwm_servo_init_servo();
    
    // Set initial throttle position (e.g., idle = 10%)
    pwm_servo_set_servo_out(0.10);
    
    // Start generator control thread
    // ...
}

// In generator control loop
static THD_FUNCTION(gen_thread, arg) {
    while (!chThdShouldTerminateX()) {
        // Read battery voltage
        float v_in = mc_interface_get_input_voltage(false);
        
        // Simple throttle control based on battery voltage
        float throttle = 0.10; // Idle
        
        if (v_in < 48.0) {
            // Battery low, increase throttle
            throttle = 0.50; // 50% throttle
        } else if (v_in < 52.0) {
            // Medium charge, moderate throttle
            throttle = 0.30; // 30% throttle
        } else {
            // Battery full, minimum throttle
            throttle = 0.10; // Idle
        }
        
        // Apply throttle
        pwm_servo_set_servo_out(throttle);
        
        // Control regenerative current...
        // ...
        
        chThdSleepMilliseconds(100); // 10 Hz throttle update
    }
}

// In app_custom_stop()
void app_custom_stop(void) {
    // Return throttle to idle before stopping
    pwm_servo_set_servo_out(0.10);
    chThdSleepMilliseconds(500); // Wait for engine to idle down
    
    // Stop servo output
    pwm_servo_stop();
}
```

### Terminal Commands for Servo Testing

```c
// Add to app_generator.c for manual servo testing
static void terminal_servo_test(int argc, const char **argv) {
    if (argc == 2) {
        float position = -1.0;
        sscanf(argv[1], "%f", &position);
        
        if (position >= 0.0 && position <= 1.0) {
            pwm_servo_set_servo_out(position);
            commands_printf("Servo set to %.2f (%.0f us)", 
                position, 1000 + position * 1000);
        } else {
            commands_printf("Error: Position must be 0.0 to 1.0");
        }
    } else {
        commands_printf("Usage: servo_test <position>");
        commands_printf("  position: 0.0 to 1.0 (0.0=1000us, 1.0=2000us)");
    }
}

// Register in app_custom_start():
terminal_register_command_callback(
    "servo_test",
    "Test servo output position",
    "[position]",
    terminal_servo_test);
```

---

## Ardupilot Integration Options

### Why Integrate with Ardupilot?

Integrating the generator with Ardupilot provides:
- **Battery monitoring** from flight controller
- **Automatic generator start/stop** based on mission requirements
- **Telemetry** of generator status (RPM, current, power)
- **Remote control** via MAVLink commands
- **Failsafe coordination** with flight systems
- **Mission planning** with power budget awareness

### Available Communication Protocols

| Protocol | Bandwidth | Reliability | Ardupilot Support | Complexity | Recommended For |
|----------|-----------|-------------|-------------------|------------|-----------------|
| DroneCAN | High | Excellent | Native | Low | **Primary choice** |
| UART + VESC Protocol | High | Good | Via custom driver | Medium | Development/testing |
| CAN (Comm Bridge) | Medium | Good | Via custom | High | Special cases |
| USB Serial | High | Good (when connected) | Via companion | Low | Ground testing |

**Note**: PPM input is **NOT available** as the HW_ICU pin will be used for servo output to control the carburetor.

---

## DroneCAN Integration (Recommended)

### Overview
DroneCAN (formerly UAVCAN) is the **recommended** method for integrating VESC with Ardupilot. It provides:
- Native support in both VESC firmware and Ardupilot
- Standardized ESC protocol
- Bidirectional communication
- Network topology (multiple devices on one bus)
- Built-in health monitoring

### VESC DroneCAN Support

VESC implements `uavcan.equipment.esc` messages:
- **Status messages**: RPM, voltage, current, temperature
- **Command messages**: RPM setpoint, raw command
- **Configuration**: ESC index, mode selection

### Ardupilot Configuration

#### 1. Enable DroneCAN
```
# Ardupilot parameters (via Mission Planner/QGC)
CAN_P1_DRIVER = 1        # First CAN driver = UAVCAN
CAN_D1_PROTOCOL = 1      # DroneCAN protocol
```

#### 2. VESC Configuration
```
# In VESC Tool: App Settings -> General
CAN Mode: UAVCAN
VESC ID: 1 (or unique ID on CAN bus)
CAN Baud Rate: 500k or 1M (match Ardupilot setting)
```

#### 3. Hardware Connection
```
Ardupilot CAN_H → VESC CAN_H
Ardupilot CAN_L → VESC CAN_L
Ardupilot GND   → VESC GND
```

Add 120Ω termination resistors at both ends of the bus if not already present.

### DroneCAN Message Handling for Generator

The standard DroneCAN ESC implementation sends motor status (RPM, current, voltage). For a generator application, you can:

1. **Use standard status messages** "as-is"
   - RPM = generator RPM
   - Current = regenerative current being produced
   - Voltage = battery voltage
   - Temperature = motor/controller temperature

2. **Monitor from Ardupilot**
   - Ardupilot receives UAVCAN ESC status
   - Custom Lua script can monitor generator health
   - Trigger alerts if generator fails

3. **Control from Ardupilot** (future enhancement)
   - Send RPM commands to adjust generator load
   - Ardupilot can request more/less power
   - Emergency shutdown via CAN command

### Example: Reading Generator Status in Ardupilot

Ardupilot Lua script to monitor generator:
```lua
-- generator_monitor.lua
-- Place in APM/scripts/ folder

local ESC_INDEX = 1  -- VESC CAN ID

function update()
    -- Get ESC telemetry
    local rpm = esc_telem:get_rpm(ESC_INDEX)
    local voltage = esc_telem:get_voltage(ESC_INDEX)
    local current = esc_telem:get_current(ESC_INDEX)
    local temp = esc_telem:get_temperature(ESC_INDEX)
    
    if rpm then
        -- Check generator health
        if rpm < 500 then
            gcs:send_text(2, "WARNING: Generator stopped!")
        end
        
        -- Calculate power
        local power = voltage * current
        
        -- Log to messages
        gcs:send_text(6, string.format("Generator: %.0fW %.1fV %.1fA", 
            power, voltage, current))
    end
    
    return update, 1000  -- Run every 1 second
end

return update()
```

### Custom DroneCAN Messages (Advanced)

For more sophisticated integration, you can add custom DroneCAN messages:
- Generator configuration commands
- Battery SOC requests
- Auto-start/stop commands
- Fuel level (if sensor installed)

This requires modifying both VESC and Ardupilot code.

---

## UART Serial Integration

### Overview
VESC's UART interface provides full access to the VESC protocol commands. Ardupilot can communicate via a spare serial port.

### VESC UART Ports

VESC firmware supports multiple UART ports:
- **UART_PORT_COMM_HEADER**: Primary UART (7/8 pin header on VESC)
- **UART_PORT_BUILTIN**: Built-in Bluetooth module (if present)
- **UART_PORT_EXTRA**: Third UART (hardware dependent)

### Configuration

#### VESC Side
```
# In VESC Tool: App Settings -> General
UART Baudrate: 115200 (or higher)
Permanent UART: Enabled (if using builtin port)
```

#### Ardupilot Side
```
# Use a spare serial port (e.g., SERIAL4)
SERIAL4_PROTOCOL = 28    # Scripting (for custom protocol)
SERIAL4_BAUD = 115200
```

### VESC Protocol Overview

VESC uses a packet-based protocol with:
- Start byte: 0x02 (short packet) or 0x03 (long packet)
- Length field
- Payload (commands and data)
- CRC16 checksum
- Stop byte: 0x03

Full protocol: `comm/packet.c`, `comm/commands.c`

### Available Commands

Useful commands for generator monitoring:
- `COMM_GET_VALUES` (4): Get all motor/controller values
- `COMM_GET_VALUES_SELECTIVE` (50): Get specific values (bandwidth efficient)
- `COMM_SET_CURRENT` (6): Set motor current (for load control)
- `COMM_SET_DUTY` (5): Set duty cycle
- `COMM_ALIVE` (30): Keepalive/timeout reset

### Example: Ardupilot Lua Script for UART

```lua
-- generator_uart.lua
-- Communicate with VESC generator via UART

local uart = serial:find_serial(0)  -- SERIAL4 = index 0 for scripting
if not uart then
    gcs:send_text(0, "Generator UART not found!")
    return
end

uart:begin(115200)
uart:set_flow_control(0)

-- VESC packet parser state
local rx_buffer = {}
local rx_state = 0

-- Send VESC command: COMM_ALIVE (keep connection alive)
function send_alive()
    local packet = {0x02, 0x01, 30, 0x00, 0x03}  -- Short packet, COMM_ALIVE
    -- TODO: Add CRC calculation
    uart:write(packet)
end

-- Send VESC command: COMM_GET_VALUES (request status)
function send_get_values()
    local packet = {0x02, 0x01, 4, 0x00, 0x03}  -- COMM_GET_VALUES
    -- TODO: Add CRC calculation
    uart:write(packet)
end

function update()
    -- Read UART data
    local n = uart:available()
    if n > 0 then
        -- Read and parse VESC packets
        -- TODO: Implement packet parser
    end
    
    -- Request generator status every second
    send_get_values()
    
    return update, 1000
end

return update()
```

**Note**: This is a simplified example. Full implementation requires:
- CRC16 calculation
- Packet parser
- Binary data unpacking
- Error handling

### Pros and Cons

**Pros:**
- Full access to all VESC commands
- High bandwidth
- Simple hardware (just 3 wires: TX, RX, GND)

**Cons:**
- Requires custom Ardupilot script
- Complex protocol parsing
- No standardization (breaks if VESC protocol changes)
- More development effort than DroneCAN

---

## Implementation Examples

### Example 1: Generator with DroneCAN Telemetry

**Hardware:**
- VESC 410 running generator app
- RC servo connected to HW_ICU pin for throttle control
- CAN bus connection to Pixhawk autopilot

**Configuration:**
```c
// app_generator.c modifications

#include "driver/pwm_servo.h"

// CAN status is automatically sent by VESC DroneCAN implementation
// Just ensure standard mc_interface values are up to date

static THD_FUNCTION(gen_thread, arg) {
    while (!chThdShouldTerminateX()) {
        // Get battery voltage from ADC
        float v_in = mc_interface_get_input_voltage(false);
        
        // Automatic throttle control
        float throttle;
        if (v_in < 48.0) {
            throttle = 0.60;  // High throttle when battery low
        } else if (v_in < 52.0) {
            throttle = 0.35;  // Medium throttle
        } else if (v_in < 54.0) {
            throttle = 0.15;  // Low throttle
        } else {
            throttle = 0.10;  // Idle when battery full
        }
        pwm_servo_set_servo_out(throttle);
        
        // Proportional current control
        float rpm = mc_interface_get_rpm();
        float erpm_diff = rpm - gen_erpm;
        
        if (erpm_diff > 0) {
            float current = (erpm_diff / gen_erpm) * gen_current;
            if (current > gen_current) current = gen_current;
            if (current < 0) current = 0;
            
            mc_interface_set_current(-current);  // Negative = regen
            timeout_reset();
        } else {
            mc_interface_set_current(0);
        }
        
        // DroneCAN status is sent automatically by VESC firmware
        // mc_interface values (RPM, current, voltage) are broadcast
        
        chThdSleepMilliseconds(1000 / gen_update_rate_hz);
    }
}
```

**Ardupilot Side:**
```
# Enable DroneCAN
CAN_P1_DRIVER = 1
CAN_D1_PROTOCOL = 1

# Install generator_monitor.lua script
# Monitors ESC_INDEX=1 for generator status
```

**Result:**
- Automatic throttle control based on battery voltage
- Generator status visible in Mission Planner / QGroundControl
- Alerts if generator RPM drops
- Telemetry logged to flight log

---

### Example 2: Ground Testing with USB Serial

**Use Case:** Development and testing without CAN bus

**Hardware:**
- VESC connected to PC via USB
- VESC Tool for monitoring

**Testing Procedure:**
```bash
# In VESC Tool Terminal:

# Start generator
gen_config 30000 20.0 0.8 100

# Test servo output manually
servo_test 0.10   # Idle
servo_test 0.30   # Medium
servo_test 0.60   # High
servo_test 0.10   # Back to idle

# Monitor status
gen_status

# Example output:
# Generator Status:
# Target: 30000 ERPM
# Current RPM: 31500 ERPM
# Max Current: 20.00 A
# Actual Current: 10.50 A
# Throttle: 0.30 (1300 us)
# Battery: 51.2 V
```

---

### Example 3: Custom CAN Commands from Ardupilot

**Advanced:** Send custom CAN commands to control generator directly

**VESC Side:** Add custom CAN packet handler
```c
// In app_generator.c

// Custom CAN command IDs (use unused IDs)
#define CAN_PACKET_GEN_SET_TARGET_RPM   0x50
#define CAN_PACKET_GEN_SET_MAX_CURRENT  0x51

void custom_can_handler(uint32_t id, uint8_t *data, uint8_t len) {
    uint8_t cmd = (id >> 8) & 0xFF;
    
    switch (cmd) {
        case CAN_PACKET_GEN_SET_TARGET_RPM: {
            int32_t rpm = (data[0] << 24) | (data[1] << 16) | 
                          (data[2] << 8) | data[3];
            gen_erpm = rpm;
            break;
        }
        case CAN_PACKET_GEN_SET_MAX_CURRENT: {
            int32_t current_scaled = (data[0] << 24) | (data[1] << 16) | 
                                      (data[2] << 8) | data[3];
            gen_current = current_scaled / 1000.0;
            break;
        }
    }
}

// Register handler in app_custom_start()
comm_can_set_custom_rx_handler(custom_can_handler);
```

**Ardupilot Side:** Send CAN commands from Lua script
```lua
-- Send custom CAN command to generator
function set_generator_rpm(rpm)
    local id = 0x5001  -- CMD=0x50, VESC_ID=1
    local data = {}
    data[1] = (rpm >> 24) & 0xFF
    data[2] = (rpm >> 16) & 0xFF
    data[3] = (rpm >> 8) & 0xFF
    data[4] = rpm & 0xFF
    
    CAN:write_frame(id, data, 4)
end

-- Call from mission or based on battery level
set_generator_rpm(35000)  -- Increase generator speed
```

---

## Safety Considerations

### Throttle Control
- **Always** return throttle to idle (0.10) before shutting down
- Implement watchdog timeout to idle throttle if VESC loses communication
- Test servo endpoints carefully - don't over-travel carburetor
- Consider mechanical throttle stop to prevent runaway

### CAN Bus
- Use proper termination resistors (120Ω at each end)
- Keep CAN wiring twisted pair, short as possible
- Test communication before flight
- Implement heartbeat monitoring in both directions

### Emergency Shutdown
- VESC timeout will stop regenerative load (engine will speed up!)
- Consider adding kill switch circuit for engine ignition
- Ardupilot failsafe should handle generator failure gracefully

### Example: Watchdog in Generator App
```c
static THD_FUNCTION(gen_thread, arg) {
    systime_t last_heartbeat = chVTGetSystemTime();
    
    while (!chThdShouldTerminateX()) {
        // Check for Ardupilot heartbeat (via CAN or UART)
        if (chVTTimeElapsedSinceX(last_heartbeat) > MS2ST(2000)) {
            // No communication for 2 seconds - go to safe state
            pwm_servo_set_servo_out(0.10);  // Idle throttle
            mc_interface_set_current(0);     // No load
            // Keep running in safe state until communication restored
        }
        
        // Normal operation...
        // (update last_heartbeat when valid command received)
    }
}
```

---

## Summary and Recommendations

### For ICE Generator + Ardupilot Integration

**Recommended Configuration:**
1. **Throttle Control**: RC servo output on HW_ICU pin
2. **Communication**: DroneCAN on CAN bus
3. **Monitoring**: Ardupilot Lua script reading ESC telemetry
4. **Failsafe**: Watchdog returns throttle to idle if communication lost

**Why This Configuration:**
- ✅ No PPM conflicts (servo output uses HW_ICU)
- ✅ Native Ardupilot support for DroneCAN
- ✅ Standardized protocol (no custom parsing)
- ✅ Telemetry in ground station
- ✅ Simple hardware (CAN-H, CAN-L, GND)
- ✅ Reliable and field-proven

**Development Path:**
1. **Phase 1**: Get generator app working standalone (servo + load control)
2. **Phase 2**: Add DroneCAN, test status messages
3. **Phase 3**: Ardupilot Lua script for monitoring
4. **Phase 4**: Advanced features (auto-start, mission integration)

### Next Steps
- Implement servo control in app_generator.c
- Test throttle control with actual servo
- Configure VESC for DroneCAN mode
- Connect to Ardupilot and verify telemetry
- Create Ardupilot monitoring script
- Flight test with safety pilot

---

## References

- [DroneCAN Website](https://dronecan.github.io)
- [Ardupilot DroneCAN Setup](https://ardupilot.org/copter/docs/common-uavcan-setup-advanced.html)
- [VESC CAN Documentation](../documentation/comm_can.md)
- [VESC Protocol](https://github.com/vedderb/bldc/blob/master/comm/commands.c)
- [Control Schemes](ICE_CONTROL_SCHEMES.md)

Created: March 2026
