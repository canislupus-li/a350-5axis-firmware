#include "rotary_module.h"
#include "src/inc/MarlinConfig.h"

// B-axis rotary module (existing)
RotaryModule rotaryModuleB(B_AXIS);
// A-axis rotary module (new for 5-axis upgrade)
// Note: A350 + 5-axis uses 'J' as the A-axis G-code character to avoid
// conflict with B_AXIS=3 (which is the rotary module firmware handshake value).
RotaryModule rotaryModuleA(J_AXIS);

/**
 * Initialize rotary module.
 *
 * Drives both A-axis (J) and B-axis rotary modules (if present).
 * Each instance handles its own AxisEnum (J_AXIS or B_AXIS) for Marlin internal
 * routing, but both send the same B_AXIS=3 protocol value to the rotary module
 * firmware (which doesn't distinguish A vs B).
 */
ErrCode RotaryModule::Init(MAC_t &mac, uint8_t mac_index) {
  CanExtCmd_t cmd;
  uint8_t     buffer[16];
  cmd.mac    = mac;
  cmd.data   = buffer;

  // Drive DIR pin HIGH for the axis this module owns
  // Note: A350 + 5-axis uses 'A' as the A-axis G-code character (compatible with A400).
  // Internally A-axis uses J_AXIS=4 enum value (to avoid conflict with B's rotary module handshake).
  // V10: B_DIR_PIN/J_DIR_PIN are kernel-set variables — actual port mapping comes from
  // DEFAULT_AXIS_TO_PORT in pins_GD32F1.h (B=P6, J=P2 per original design).
  if (axis_ == B_AXIS) {
    OUT_WRITE(E1_DIR_PIN, LOW);
    WRITE(B_DIR_PIN, HIGH);  // B module — kernel routes B to P6, B_DIR_PIN is PC12
  } else {
    // A axis (5-axis upgrade) — kernel routes J to P2, J_DIR_PIN is PC10
    // V8 fix: was previously using PE13 (J_STEP_PIN) which the rotary module does not monitor.
    // The module only checks its DIR pin, so we must use J_DIR_PIN for handshake.
    OUT_WRITE(E1_DIR_PIN, LOW);
    WRITE(J_DIR_PIN, HIGH);
  }
  vTaskDelay(pdMS_TO_TICKS(10));

  cmd.data[MODULE_EXT_CMD_INDEX_ID]   = MODULE_EXT_CMD_CONFIG_REQ;
  // Always send B_AXIS=3 to the rotary module - the hardware protocol is fixed
  // (rotary module firmware doesn't distinguish between A and B, it just responds to CONFIG_REQ).
  // The axis_ field (B_AXIS or J_AXIS) is used internally by Marlin to drive the correct stepper pins.
  cmd.data[MODULE_EXT_CMD_INDEX_DATA] = (uint8_t)B_AXIS;
  cmd.length = 2;
  if (canhost.SendExtCmdSync(cmd, 500) == E_SUCCESS) {
    if (cmd.data[MODULE_EXT_CMD_INDEX_DATA] == 1) {
      status(ROTATE_ONLINE);
      SERIAL_ECHOLN(axis_ == J_AXIS ? "Rotary module A detected." : "Rotary module B detected.");
    } else {
      status(ROTATE_UNUSABLE);
      SERIAL_ECHOLN(axis_ == J_AXIS ? "Rotary module A unusable." : "Rotary module B unusable.");
    }
  } else {
    status(ROTATE_OFFLINE);
    SERIAL_ECHOLN(axis_ == J_AXIS ? "Rotary module A not found." : "Rotary module B not found.");
  }

  // Drive DIR pin LOW for the axis this module owns
  if (axis_ == B_AXIS) {
    WRITE(B_DIR_PIN, LOW);
  } else {
    WRITE(J_DIR_PIN, LOW);  // V8 fix: was digitalWrite(PE13, LOW) (wrong pin)
  }
  return E_SUCCESS;
}
