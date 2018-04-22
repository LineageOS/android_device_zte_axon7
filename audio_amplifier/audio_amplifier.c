/*
 * Copyright (C) 2013-2016, The CyanogenMod Project
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


#include <time.h>
#include <system/audio.h>
#include <platform.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <dlfcn.h>
#include <pthread.h>
#include <sys/ioctl.h>

#define LOG_TAG "axon7-tfa98xx"
#include <log/log.h>

#include <hardware/audio_amplifier.h>

//tfa9890_Stereo_SpeakerOn
//tfa9890_Set_SpeakerNeeded
//tfa9890_Stereo_SpeakerOff
//tfa9890_set_device
//exTfa98xx_calibration
//tfa9890_set_mode

typedef struct tfa9890_state {
    int in_stream;
    int in_call;
    int amp_device;
    int amp_mode;
    int calibration_done;
} tfa9890_state_t;

typedef struct tfa9890_device {
    amplifier_device_t amp_dev;
    void *lib_ptr;
    int (*speaker_needed)(int);
    void (*set_mode)(int);
    void (*speaker_on)(void);
    void (*speaker_off)(void);
    void (*set_device)(int);
    int (*calibration)(int);
    // Hold amp_update whenever calling amp_on or amp_off, or changing in_stream
    pthread_mutex_t amp_update;
    tfa9890_state_t state;
} tfa9890_device_t;

static void tfa9890_dump_state(tfa9890_device_t *t, char *label) {
    ALOGD("tfa9890[%s]:\n\t"
                "in_stream: %d\n\t"
                "in_call: %d\n\t"
                "amp_device: %d\n\t"
                "amp_mode: %d\n\t"
                "calibration_done: %d\n\t",
                label, t->state.in_stream, t->state.in_call, t->state.amp_device,
                t->state.amp_mode, t->state.calibration_done
    );
}

static void tfa9890_update_amp(tfa9890_device_t *tfa9890, int in_stream, int in_call,
                               int amp_device, int amp_mode) {
    bool reinitialize = false;

    if (in_call != tfa9890->state.in_call) {
        tfa9890->state.in_call = in_call;
        reinitialize = true;
    }

    // Only re-initialize when we're not in a call and the stream state changes
    if (!in_call && (!!in_stream != !!tfa9890->state.in_stream)) {
        reinitialize = true;
    }
    tfa9890->state.in_stream = in_stream;

    if (amp_device != tfa9890->state.amp_device) {
        // We can't change devices on the fly
        tfa9890->state.amp_device = amp_device;
        reinitialize = true;
    }

    if (amp_mode != tfa9890->state.amp_mode) {
        tfa9890->state.amp_mode = amp_mode;
        if (!reinitialize)
            tfa9890->set_mode(amp_mode);
    }

    if (!!(in_stream | in_call)) {
        if (reinitialize) {
            tfa9890->speaker_needed(0);
            tfa9890->speaker_off();
            tfa9890->set_device(tfa9890->state.amp_device);
            tfa9890->set_mode(tfa9890->state.amp_mode);
        }
        tfa9890->speaker_needed(1);
        tfa9890->speaker_on();
    } else {
        if (!tfa9890->state.calibration_done) {
            int ret = tfa9890->calibration(0);
            if (ret) {
                ALOGE("exTfa98xx_calibration: %d\n", ret);
            } else {
                tfa9890->state.calibration_done = 1;
            }
        }
        tfa9890->speaker_needed(0);
        tfa9890->speaker_off();
    }
}

static tfa9890_device_t *tfa9890_dev = NULL;

static int amp_dev_close(hw_device_t *device) {
    tfa9890_device_t *tfa9890 = (tfa9890_device_t*) device;
    if (tfa9890) {
        dlclose(tfa9890->lib_ptr);
        pthread_mutex_destroy(&tfa9890->amp_update);
        free(tfa9890);
    }
    return 0;
}

static int tfa9890_set_output_devices(struct amplifier_device *device, uint32_t devices) {
    ALOGD("%s: %u\n", __func__, devices);
    tfa9890_device_t *tfa9890 = (tfa9890_device_t*) device;
    tfa9890_dump_state(tfa9890, "before");
    switch (devices) {
        case SND_DEVICE_OUT_SPEAKER:
        case SND_DEVICE_OUT_SPEAKER_REVERSE:
        case SND_DEVICE_OUT_VOICE_SPEAKER:
            pthread_mutex_lock(&tfa9890->amp_update);
            tfa9890_update_amp(tfa9890, tfa9890->state.in_stream, tfa9890->state.in_call,
                               0, tfa9890->state.amp_mode);
            pthread_mutex_unlock(&tfa9890->amp_update);
            break;
        case SND_DEVICE_OUT_HANDSET:
        case SND_DEVICE_OUT_VOICE_HANDSET:
            pthread_mutex_lock(&tfa9890->amp_update);
            tfa9890_update_amp(tfa9890, tfa9890->state.in_stream, tfa9890->state.in_call,
                               1, tfa9890->state.amp_mode);
            pthread_mutex_unlock(&tfa9890->amp_update);
            break;
        default:
            ALOGE("%s: Unhandled output device: %d\n", __func__, devices);
            pthread_mutex_lock(&tfa9890->amp_update);
            tfa9890_update_amp(tfa9890, tfa9890->state.in_stream, tfa9890->state.in_call,
                               0, tfa9890->state.amp_mode);
            pthread_mutex_unlock(&tfa9890->amp_update);
            break;
    }
    tfa9890_dump_state(tfa9890, "after");
    return 0;
}

static int amp_enable_output_devices(struct amplifier_device *device, uint32_t devices, bool enable) {
    ALOGD("%s: %u %d\n", __func__, devices, enable);
    tfa9890_device_t *tfa9890 = (tfa9890_device_t*) device;
    tfa9890_dump_state(tfa9890, "before");
    switch (devices) {
        case SND_DEVICE_OUT_SPEAKER:
        case SND_DEVICE_OUT_SPEAKER_REVERSE:
            pthread_mutex_lock(&tfa9890->amp_update);
            tfa9890_update_amp(tfa9890, tfa9890->state.in_stream, tfa9890->state.in_call,
                               tfa9890->state.amp_device, tfa9890->state.amp_mode);
            pthread_mutex_unlock(&tfa9890->amp_update);
            break;
        case SND_DEVICE_OUT_VOICE_SPEAKER:
        case SND_DEVICE_OUT_HANDSET:
        case SND_DEVICE_OUT_VOICE_HANDSET:
            pthread_mutex_lock(&tfa9890->amp_update);
            tfa9890_update_amp(tfa9890, tfa9890->state.in_stream, enable,
                               tfa9890->state.amp_device, tfa9890->state.amp_mode);
            pthread_mutex_unlock(&tfa9890->amp_update);
            break;
        default:
            ALOGE("%s: Unandled device %d\n", __func__, devices);
            pthread_mutex_lock(&tfa9890->amp_update);
            tfa9890_update_amp(tfa9890, tfa9890->state.in_stream, tfa9890->state.in_call,
                               tfa9890->state.amp_device, tfa9890->state.amp_mode);
            pthread_mutex_unlock(&tfa9890->amp_update);
            break;
    }
    tfa9890_dump_state(tfa9890, "after");
    return 0;
}

static int tfa9890_set_mode(struct amplifier_device *device, audio_mode_t mode) {
    ALOGD("%s: %u\n", __func__, mode);
    tfa9890_device_t *tfa9890 = (tfa9890_device_t*) device;
    tfa9890_dump_state(tfa9890, "before");
    // Stock used 2 and 1 and 0, and all 3 seem to be the same... no clue.  Draken's log showed 2 for music
    switch (mode) {
        case AUDIO_MODE_NORMAL:
        case AUDIO_MODE_RINGTONE:
            pthread_mutex_lock(&tfa9890->amp_update);
            tfa9890_update_amp(tfa9890, tfa9890->state.in_stream, tfa9890->state.in_call,
                               tfa9890->state.amp_device, 2);
            pthread_mutex_unlock(&tfa9890->amp_update);
            break;
        case AUDIO_MODE_IN_CALL:
        case AUDIO_MODE_IN_COMMUNICATION:
            pthread_mutex_lock(&tfa9890->amp_update);
            tfa9890_update_amp(tfa9890, tfa9890->state.in_stream, tfa9890->state.in_call,
                               tfa9890->state.amp_device, 1);
            pthread_mutex_unlock(&tfa9890->amp_update);
            break;
        default:
            ALOGE("%s: Unhandled Mode: %d\n", __func__, mode);
            pthread_mutex_lock(&tfa9890->amp_update);
            tfa9890_update_amp(tfa9890, tfa9890->state.in_stream, tfa9890->state.in_call,
                               tfa9890->state.amp_device, 0);
            pthread_mutex_unlock(&tfa9890->amp_update);
            break;
    }
    tfa9890_dump_state(tfa9890, "after");
    return 0;
}

static int tfa9890_output_stream_start(struct amplifier_device *device, struct audio_stream_out *stream, bool offload) {
    ALOGD("%s: %p %d\n", __func__, stream, offload);
    tfa9890_device_t *tfa9890 = (tfa9890_device_t*) device;
    tfa9890_dump_state(tfa9890, "before");
    pthread_mutex_lock(&tfa9890->amp_update);
    tfa9890_update_amp(tfa9890, tfa9890->state.in_stream + 1, tfa9890->state.in_call,
                       tfa9890->state.amp_device, tfa9890->state.amp_mode);
    pthread_mutex_unlock(&tfa9890->amp_update);
    tfa9890_dump_state(tfa9890, "after");
    return 0;
}

static int tfa9890_output_stream_standby(struct amplifier_device *device, struct audio_stream_out *stream) {
    ALOGD("%s: %p\n", __func__, stream);
    tfa9890_device_t *tfa9890 = (tfa9890_device_t*) device;
    tfa9890_dump_state(tfa9890, "before");
    pthread_mutex_lock(&tfa9890->amp_update);
    tfa9890_update_amp(tfa9890, tfa9890->state.in_stream - 1, tfa9890->state.in_call,
                       tfa9890->state.amp_device, tfa9890->state.amp_mode);
    pthread_mutex_unlock(&tfa9890->amp_update);
    tfa9890_dump_state(tfa9890, "after");
    return 0;
}

int tfa9890_set_parameters(struct amplifier_device *device, struct str_parms *parms) {
    ALOGD("%s: %p\n", __func__, parms);
    tfa9890_device_t *tfa9890 = (tfa9890_device_t*) device;
    // todo, dump parms.  Might be more stuff we need to do here
    return 0;
}

static int amp_module_open(const hw_module_t *module,
                           __attribute__((unused)) const char *name, hw_device_t **device)
{
    if (tfa9890_dev) {
        ALOGE("%s:%d: Unable to open second instance of TFA9890 amplifier\n",
              __func__, __LINE__);
        return -EBUSY;
    }

    tfa9890_dev = calloc(1, sizeof(tfa9890_device_t));
    if (!tfa9890_dev) {
        ALOGE("%s:%d: Unable to allocate memory for amplifier device\n",
              __func__, __LINE__);
        return -ENOMEM;
    }

    tfa9890_dev->amp_dev.common.tag = HARDWARE_DEVICE_TAG;
    tfa9890_dev->amp_dev.common.module = (hw_module_t *) module;
    tfa9890_dev->amp_dev.common.version = HARDWARE_DEVICE_API_VERSION(1, 0);
    tfa9890_dev->amp_dev.common.close = amp_dev_close;
    pthread_mutex_init(&tfa9890_dev->amp_update, NULL);

    tfa9890_dev->amp_dev.enable_output_devices = amp_enable_output_devices;
    tfa9890_dev->amp_dev.set_output_devices = &tfa9890_set_output_devices;
    tfa9890_dev->amp_dev.set_mode = &tfa9890_set_mode;
    tfa9890_dev->amp_dev.output_stream_start = &tfa9890_output_stream_start;
    tfa9890_dev->amp_dev.output_stream_standby = &tfa9890_output_stream_standby;
    tfa9890_dev->amp_dev.set_parameters = &tfa9890_set_parameters;

    tfa9890_dev->lib_ptr = dlopen("libtfa9890.so", RTLD_NOW);
    if (!tfa9890_dev->lib_ptr) {
        ALOGE("%s:%d: Unable to open libtfa9890.so: %s",
              __func__, __LINE__, dlerror());
        pthread_mutex_destroy(&tfa9890_dev->amp_update);
        free(tfa9890_dev);
        return -ENODEV;
    }

    *(void **)&tfa9890_dev->speaker_needed = dlsym(tfa9890_dev->lib_ptr, "tfa9890_Set_SpeakerNeeded");
    *(void **)&tfa9890_dev->speaker_on = dlsym(tfa9890_dev->lib_ptr, "tfa9890_Stereo_SpeakerOn");
    *(void **)&tfa9890_dev->speaker_off = dlsym(tfa9890_dev->lib_ptr, "tfa9890_Stereo_SpeakerOff");
    *(void **)&tfa9890_dev->set_mode = dlsym(tfa9890_dev->lib_ptr, "tfa9890_set_mode");
    *(void **)&tfa9890_dev->set_device = dlsym(tfa9890_dev->lib_ptr, "tfa9890_set_device");
    *(void **)&tfa9890_dev->calibration = dlsym(tfa9890_dev->lib_ptr, "exTfa98xx_calibration");

    if (!tfa9890_dev->speaker_needed || !tfa9890_dev->speaker_on || !tfa9890_dev->speaker_off ||
        !tfa9890_dev->set_mode || !tfa9890_dev->set_device || !tfa9890_dev->calibration) {
        ALOGE("%s:%d: Unable to find required symbols", __func__, __LINE__);
        dlclose(tfa9890_dev->lib_ptr);
        pthread_mutex_destroy(&tfa9890_dev->amp_update);
        free(tfa9890_dev);
        return -ENODEV;
    }

    *device = (hw_device_t *) tfa9890_dev;

    return 0;
}

static struct hw_module_methods_t hal_module_methods = {
    .open = amp_module_open,
};

amplifier_module_t HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = AMPLIFIER_MODULE_API_VERSION_0_1,
        .hal_api_version = HARDWARE_HAL_API_VERSION,
        .id = AMPLIFIER_HARDWARE_MODULE_ID,
        .name = "Axon7 amplifier HAL",
        .author = "The CyanogenMod Open Source Project",
        .methods = &hal_module_methods,
    },
};
