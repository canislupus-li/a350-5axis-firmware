/*
 * Snapmaker2-Controller Firmware
 * Copyright (C) 2019-2020 Snapmaker [https://github.com/Snapmaker]
 *
 * This file is part of Snapmaker2-Controller
 * (see https://github.com/Snapmaker/Snapmaker2-Controller)
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
 */

#ifndef SNAPMAKER_ROTATE_H_
#define SNAPMAKER_ROTATE_H_

#include "module_base.h"
#include "can_host.h"

typedef enum {
  ROTATE_ONLINE = 0,
  ROTATE_OFFLINE = 1,
  ROTATE_UNUSABLE = 2,
}ROTATE_STATE_E;

/**
 * Rotary Module
 *
 * Abstraction of rotary module add-on.
 * Supports multiple instances (A-axis and B-axis) for 5-axis upgrade.
 */
class RotaryModule: public ModuleBase {

  public:
    /**
     * @param axis  AxisEnum this rotary module drives (A_AXIS or B_AXIS)
     */
    RotaryModule(AxisEnum axis): ModuleBase(MODULE_DEVICE_ID_ROTARY), axis_(axis) {
    }
    /**
     * Initialize rotary module.
     */
    ErrCode Init(MAC_t &mac, uint8_t mac_index);

    /**
     * Detection of module.
     */
    // void Detect();

    /**
     * Check if module is activated.
     */
    ROTATE_STATE_E status() {return status_;}
    void status(ROTATE_STATE_E s) {status_ = s;}

    /**
     * Get the axis this rotary module drives.
     */
    AxisEnum axis() { return axis_; }

  private:
    ROTATE_STATE_E status_ = ROTATE_OFFLINE;
    AxisEnum       axis_;
};

// B-axis rotary module (existing - backwards compatible)
extern RotaryModule rotaryModuleB;
// A-axis rotary module (new for 5-axis upgrade)
extern RotaryModule rotaryModuleA;

// Backwards-compat alias for existing code that references `rotaryModule`
#define rotaryModule rotaryModuleB
#endif
