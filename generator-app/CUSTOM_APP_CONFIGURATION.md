# Custom App Configuration in VESC

## How Custom Apps Work with VESC Tool GUI

### 1. App Selection ✅
In VESC Tool GUI:
- Navigate to **App Settings** → **General** → **App to Use**
- Select **"Custom"** from dropdown
- This activates your `app_generator.c` code

### 2. Configuration Parameters ❌ (Limited)
**Problem**: Custom apps have **NO dedicated GUI configuration section** by default!

Looking at `datatypes.h`, the `app_configuration` structure has specific configs for each app:
```c
typedef struct {
    app_use app_to_use;           // Dropdown selection
    
    ppm_config app_ppm_conf;      // PPM has GUI config ✅
    adc_config app_adc_conf;      // ADC has GUI config ✅
    chuk_config app_chuk_conf;    // Nunchuk has GUI config ✅
    pas_config app_pas_conf;      // PAS has GUI config ✅
    
    // NO app_custom_conf! ❌
} app_configuration;
```

---

## Solutions for Configuring Custom Apps

### Option 1: Compile-Time Configuration (Current)
Edit `#define` values in `applications/app_generator.c`:
```c
#define GEN_ERPM         2000.0
#define GEN_CURRENT        20.0
#define GEN_START          0.90
```
Then recompile and reflash firmware.

**Pros**: 
- Simple
- No extra code needed

**Cons**: 
- Requires rebuild for every parameter change
- No runtime adjustment

---

### Option 2: Terminal Commands (Recommended) ⭐

Add interactive configuration via VESC Tool Terminal.

#### Implementation Example

Add to `app_generator.c`:

```c
#include "terminal.h"
#include "commands.h"

// Change from #define to variables
static float gen_erpm = 2000.0;
static float gen_current = 20.0;
static float gen_start = 0.90;

static void terminal_set_gen_params(int argc, const char **argv) {
    if (argc == 4) {
        sscanf(argv[1], "%f", &gen_erpm);
        sscanf(argv[2], "%f", &gen_current);
        sscanf(argv[3], "%f", &gen_start);
        
        commands_printf("Generator config updated:");
        commands_printf("  Target ERPM: %.1f", (double)gen_erpm);
        commands_printf("  Max Current: %.1f A", (double)gen_current);
        commands_printf("  Start Ratio: %.2f", (double)gen_start);
    } else {
        commands_printf("Usage: gen_config <erpm> <current> <start>");
        commands_printf("Current settings:");
        commands_printf("  Target ERPM: %.1f", (double)gen_erpm);
        commands_printf("  Max Current: %.1f A", (double)gen_current);
        commands_printf("  Start Ratio: %.2f", (double)gen_start);
    }
}

static void terminal_get_gen_status(int argc, const char **argv) {
    (void)argc; (void)argv;
    
    float rpm_now = mc_interface_get_rpm();
    commands_printf("Generator Status:");
    commands_printf("  Current RPM: %.1f", (double)rpm_now);
    commands_printf("  Target ERPM: %.1f", (double)gen_erpm);
    commands_printf("  Active: %s", fabsf(rpm_now) > gen_start * gen_erpm ? "YES" : "NO");
}

void app_custom_start(void) {
    // Register terminal commands
    terminal_register_command_callback(
        "gen_config",
        "Configure generator parameters",
        "<erpm> <current> <start>",
        terminal_set_gen_params);
        
    terminal_register_command_callback(
        "gen_status",
        "Show generator status",
        "",
        terminal_get_gen_status);
    
    stop_now = false;
    chThdCreateStatic(gen_thread_wa, sizeof(gen_thread_wa), NORMALPRIO, gen_thread, NULL);
}

void app_custom_stop(void) {
    terminal_unregister_callback(terminal_set_gen_params);
    terminal_unregister_callback(terminal_get_gen_status);
    
    stop_now = true;
    while (is_running) {
        chThdSleepMilliseconds(1);
    }
}
```

#### Usage in VESC Tool Terminal

**Set parameters**:
```
gen_config 3000 15 0.85
```

**View current settings**:
```
gen_config
```

**Check generator status**:
```
gen_status
```

**Pros**: 
- Live tuning without reflash
- Easy to test different parameters
- Can add status monitoring commands

**Cons**: 
- Settings lost on reboot (unless saved to flash)
- Need to type commands

---

### Option 3: Reuse Existing App Config (Clever Hack)

Repurpose unused app configuration parameters.

```c
void app_custom_configure(app_configuration *conf) {
    // Steal PPM config values for generator settings
    // (when PPM app is not in use)
    gen_erpm = conf->app_ppm_conf.pulse_center * 10.0;  // Reuse PPM center
    gen_current = conf->app_ppm_conf.pulse_end;          // Reuse PPM end
    gen_start = conf->app_ppm_conf.hyst;                 // Reuse hysteresis
    
    commands_printf("Generator configured from PPM settings:");
    commands_printf("  ERPM: %.1f", (double)gen_erpm);
    commands_printf("  Current: %.1f A", (double)gen_current);
}
```

Configure via **PPM settings in GUI**, but interpret values for generator!

**Pros**: 
- Uses existing GUI
- Settings persist across reboots
- No VESC Tool modification needed

**Cons**: 
- Confusing (GUI shows PPM labels)
- Hacky approach
- Limited number of parameters

---

### Option 4: Persistent Storage with EEPROM

Combine terminal commands with persistent storage.

```c
#include "eeprom.h"

#define EEPROM_ADDR_GEN_ERPM    1000
#define EEPROM_ADDR_GEN_CURRENT 1004
#define EEPROM_ADDR_GEN_START   1008

static void load_config_from_eeprom(void) {
    uint32_t erpm_raw, current_raw, start_raw;
    
    if (eeprom_read_var(&erpm_raw, EEPROM_ADDR_GEN_ERPM) &&
        eeprom_read_var(&current_raw, EEPROM_ADDR_GEN_CURRENT) &&
        eeprom_read_var(&start_raw, EEPROM_ADDR_GEN_START)) {
        
        gen_erpm = *(float*)&erpm_raw;
        gen_current = *(float*)&current_raw;
        gen_start = *(float*)&start_raw;
        
        commands_printf("Generator config loaded from EEPROM");
    }
}

static void save_config_to_eeprom(void) {
    eeprom_store_var(*(uint32_t*)&gen_erpm, EEPROM_ADDR_GEN_ERPM);
    eeprom_store_var(*(uint32_t*)&gen_current, EEPROM_ADDR_GEN_CURRENT);
    eeprom_store_var(*(uint32_t*)&gen_start, EEPROM_ADDR_GEN_START);
    
    commands_printf("Generator config saved to EEPROM");
}

static void terminal_gen_save(int argc, const char **argv) {
    (void)argc; (void)argv;
    save_config_to_eeprom();
}

void app_custom_start(void) {
    load_config_from_eeprom();  // Load saved settings on start
    
    terminal_register_command_callback(
        "gen_config",
        "Configure generator parameters",
        "<erpm> <current> <start>",
        terminal_set_gen_params);
        
    terminal_register_command_callback(
        "gen_save",
        "Save generator config to EEPROM",
        "",
        terminal_gen_save);
    
    // ... rest of code
}
```

**Usage**:
```
gen_config 3000 15 0.85
gen_save
```

**Pros**: 
- Settings persist across reboots
- Live tuning capability
- No firmware rebuild needed

**Cons**: 
- More complex code
- Need to manage EEPROM addresses

---

### Option 5: LispBM Scripting (Modern Approach) 🚀

Use LispBM instead of C for fully configurable apps with GUI integration.

LispBM apps can:
- Have dedicated GUI configuration pages in VESC Tool
- Store settings persistently automatically
- Update without firmware reflash
- Access all VESC functions

**Example**: See `lispBM/` directory in firmware source

**Pros**: 
- Best GUI integration
- No firmware rebuild ever
- Full VESC Tool integration
- Script can be updated remotely

**Cons**: 
- Different programming paradigm (Lisp)
- May have performance limitations for high-frequency control

---

### Option 6: Modify VESC Tool (Advanced)

Add custom app configuration to both firmware AND VESC Tool source code.

#### Required Steps:

1. **Modify datatypes.h**:
   ```c
   typedef struct {
       float gen_erpm;
       float gen_current;
       float gen_start;
       float gen_update_rate_hz;
   } generator_config;
   
   // In app_configuration struct:
   generator_config app_generator_conf;
   ```

2. **Update confgenerator.c** (autogenerated from VESC Tool)

3. **Modify VESC Tool QML UI** to add configuration page

4. **Rebuild firmware AND VESC Tool**

**Pros**: 
- Professional solution
- Full GUI integration like standard apps
- Settings persist properly

**Cons**: 
- Very complex implementation
- Must maintain VESC Tool fork
- Requires knowledge of Qt/QML
- Need to keep in sync with upstream updates

---

## Recommendation for Generator App

### Best Approach: **Option 2 + Option 4**

Implement **Terminal Commands with EEPROM Persistence**:

**Benefits**:
1. ✅ Live tuning during testing
2. ✅ Settings persist across reboots
3. ✅ No VESC Tool modification needed
4. ✅ Easy to implement
5. ✅ Professional enough for production use

**Implementation Priority**:
1. Start with terminal commands (quick testing)
2. Add EEPROM persistence (production use)
3. Add status/monitoring commands (diagnostics)

### Future Enhancement: LispBM

For ultimate flexibility, consider migrating to a LispBM implementation that can be configured directly in VESC Tool without any firmware modifications.

---

## Comparison Table

| Feature | Compile-Time | Terminal | Reuse Config | EEPROM | LispBM | Modify Tool |
|---------|-------------|----------|--------------|---------|---------|-------------|
| **Live Tuning** | ❌ | ✅ | ✅ | ✅ | ✅ | ✅ |
| **Persistence** | ✅ | ❌ | ✅ | ✅ | ✅ | ✅ |
| **GUI Integration** | ❌ | ❌ | 😐 | ❌ | ✅ | ✅ |
| **Implementation Effort** | Low | Low | Low | Medium | Medium | Very High |
| **Maintenance** | Easy | Easy | Easy | Medium | Medium | Complex |
| **Professional** | ❌ | 😐 | ❌ | ✅ | ✅ | ✅ |
| **Flexibility** | Low | High | Low | High | Very High | Very High |

---

## Example: Complete Terminal + EEPROM Implementation

See `app_custom_template.c` for pattern to follow. Key points:

1. **Include necessary headers**:
   ```c
   #include "terminal.h"
   #include "commands.h"
   #include "eeprom.h"
   ```

2. **Use static variables instead of #defines**

3. **Register commands in `app_custom_start()`**

4. **Unregister commands in `app_custom_stop()`**

5. **Load from EEPROM on startup**

6. **Provide save command for user**

---

*Document created: March 3, 2026*
