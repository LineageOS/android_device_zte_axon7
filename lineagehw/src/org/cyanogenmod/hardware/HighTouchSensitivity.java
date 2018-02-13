/*
 * Copyright (C) 2016 The CyanogenMod Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.lineageos.hardware;

import org.lineageos.internal.util.FileUtils;

/**
 * Glove mode / high touch sensitivity
 */
public class HighTouchSensitivity {
    private static final String FILE_MODE = "/proc/gloved_finger_switch";
    private static final String ENABLED = "1";
    private static final String DISABLED = "0";

    /**
     * Whether device supports high touch sensitivity.
     *
     * @return boolean Supported devices must return always true
     */
    public static boolean isSupported() {
        return FileUtils.isFileReadable(FILE_MODE) &&
                FileUtils.isFileWritable(FILE_MODE);
    }

    /** This method returns the current activation status of high touch sensitivity
     *
     * @return boolean Must be false if high touch sensitivity is not supported or not activated,
     * or the operation failed while reading the status; true in any other case.
     */
    public static boolean isEnabled() {
        return ENABLED.equals(FileUtils.readOneLine(FILE_MODE));
    }

    /**
     * This method allows to setup high touch sensitivity status.
     *
     * @param state The new high touch sensitivity status
     * @return boolean Must be false if high touch sensitivity is not supported or the operation
     * failed; true in any other case.
     */
    public static boolean setEnabled(boolean state) {
        return FileUtils.writeLine(FILE_MODE, state ? ENABLED : DISABLED);
    }
}
