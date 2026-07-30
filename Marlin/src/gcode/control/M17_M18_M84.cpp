/**
 * Marlin 3D Printer Firmware
 * Copyright (C) 2019 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 *
 * Based on Sprinter and grbl.
 * Copyright (C) 2011 Camiel Gubbels / Erik van der Zalm
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "../gcode.h"
#include "../../Marlin.h" // for stepper_inactive_time, disable_e_steppers
#include "../../module/stepper.h"
#include "../../core/utility.h"   // A350 + 5-axis: V9 fix - need axis_codes[] for J_AXIS char

#if BOTH(AUTO_BED_LEVELING_UBL, ULTRA_LCD)
  #include "../../feature/bedlevel/bedlevel.h"
#endif

/**
 * M17: Enable power on all stepper motors
 */
void GcodeSuite::M17() {
  enable_all_steppers();
}

/**
 * M18, M84: Disable stepper motors
 */
void GcodeSuite::M18_M84() {
  if (parser.seenval('S')) {
    stepper_inactive_time = parser.value_millis_from_seconds();
  }
  else {
    // A350 + 5-axis V9 fix: use LOOP_X_TO_E so that any axis (including the
    // J axis whose G-code char is axis_codes[J_AXIS] = 'A') is properly
    // recognised. Previously this hard-coded 'J' which would never match the
    // A character that users actually send in commands like M18 A10.
    bool all_axis = true;
    LOOP_X_TO_E(i) {
      if (parser.seenval(axis_codes[i])) { all_axis = false; break; }
    }
    if (all_axis) {
      planner.finish_and_disable();
    }
    else {
      planner.synchronize();
      if (parser.seen('X')) disable_X();
      if (parser.seen('Y')) disable_Y();
      if (parser.seen('Z')) disable_Z();
      if (parser.seen(axis_codes[B_AXIS])) disable_B();
      if (parser.seen(axis_codes[J_AXIS])) disable_J();   // A350 + 5-axis: J axis char is 'A'
      // Only disable on boards that have separate ENABLE_PINS or another method for disabling the driver
      #if (E0_ENABLE_PIN != X_ENABLE_PIN && E1_ENABLE_PIN != Y_ENABLE_PIN) || AXIS_DRIVER_TYPE_E0(TMC2660) || AXIS_DRIVER_TYPE_A1(TMC2660) || AXIS_DRIVER_TYPE_E2(TMC2660) || AXIS_DRIVER_TYPE_E3(TMC2660) || AXIS_DRIVER_TYPE_E4(TMC2660) || AXIS_DRIVER_TYPE_E5(TMC2660)
        if (parser.seen('E')) disable_e_steppers();
      #endif
    }

    #if HAS_LCD_MENU && ENABLED(AUTO_BED_LEVELING_UBL)
      if (ubl.lcd_map_control) {
        ubl.lcd_map_control = false;
        ui.defer_status_screen(false);
      }
    #endif
  }
}
