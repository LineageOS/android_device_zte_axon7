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
#include <cutils/properties.h>
#include <cutils/str_parms.h>
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
#include <semaphore.h>
#include <sys/ioctl.h>

#define LOG_TAG "axon7-tfa98xx"
#include <log/log.h>

#include <hardware/audio_amplifier.h>

#define MUSIC 0
#define VOICE 1

typedef struct tfa9890_state {
    int streams;
    int in_call;
    int device;
    int mode;
    int reversed;
    int enabled;
    int calibration_done;
} tfa9890_state_t;

typedef struct tfa9890_device {
    amplifier_device_t amp_dev;
    void *lib_ptr;
    int (*ex_speaker_on)(int mode);
    void (*ex_speaker_off)(void);
    int (*ex_calibration)(int);
    int *device_to_amp_device;
    void (*set_device)(int);
    int (*calibration)(int);
    void (*set_mode)(int);
    int (*speaker_needed)(int);
    void (*stereo_speaker_on)(void);
    void (*stereo_speaker_off)(void);
    int (*stereo_speaker_needed)(int,int);
    void (*speaker_on_bypass)(void);
    void (*speaker_off_bypass)(void);
    void (*set_amp_state)(void *device, tfa9890_state_t rstat, int reinitialize);
    // Hold for all the things
    pthread_mutex_t amp_update;
    tfa9890_state_t state;
    sem_t trigger_update;
    sem_t trigger_exit;
    pthread_t amp_thread;
} tfa9890_device_t;

#define tfa9890_dump_state(t, n, label) do {\
    ALOGD("tfa9890[%s-%s]:\n\t"\
          "enabled: %d(%d)\n\t"\
          "device: %d(%d)\n\t"\
          "streams: %d(%d)\n\t"\
          "in_call: %d(%d)\n\t"\
          "mode: %d(%d)\n\t"\
          "reversed: %d(%d)\n",\
          __func__, label,\
          t.enabled, n.enabled,\
          t.device, n.device,\
          t.streams, n.streams,\
          t.in_call, n.in_call,\
          t.mode, n.mode,\
          t.reversed, n.reversed\
    );\
} while (0)

#define LEFT_HANDLE 0
#define RIGHT_HANDLE 1

#define LEFT_IN 0
#define RIGHT_IN 1

void reverse(void *device, int reversed) {
    tfa9890_device_t *tfa9890 = (tfa9890_device_t*) device;
    if(reversed) {
        ALOGD("The function, it does nothing.");
    }
}

// The new way to power on and use the amp.  This should be the correct option
void tfa9890_set_amp_state(void *device, tfa9890_state_t rstate, int reinitialize) {
    tfa9890_device_t *tfa9890 = (tfa9890_device_t*) device;
    if (reinitialize) {
        tfa9890->ex_speaker_off();
        tfa9890->set_device(rstate.device);
        reverse(tfa9890, tfa9890->state.reversed);
    }
    if(rstate.enabled && (rstate.streams || rstate.in_call)) {
        int again = 1;
        while (again && tfa9890->ex_speaker_on(rstate.mode)){
            pthread_mutex_unlock(&tfa9890->amp_update);
            usleep(5*1000);
            ALOGD("%s: Failed turning amp on %d times", __func__, again);
            again = (sem_trywait(&tfa9890->trigger_update) == -1) ? again + 1 : 0;
            pthread_mutex_lock(&tfa9890->amp_update);
        }
        if (!again) {
            ALOGD("%s: Amp state was changed, triggering refresh", __func__);
            sem_post(&tfa9890->trigger_update);
        }
    } else {
        tfa9890->ex_speaker_off();
    }
}

// Almost the same way we did it on the pre-o amp-hal.  The only change is in how we
// handle turning the amp on when in a call, which didn't work properly on N anyways
void tfa9890_set_amp_state_legacy(void *device, tfa9890_state_t rstate, int reinitialize) {
    tfa9890_device_t *tfa9890 = (tfa9890_device_t*) device;
    if (reinitialize) {
        tfa9890->speaker_needed(0);
        tfa9890->stereo_speaker_off();
        tfa9890->set_device(rstate.device);
        reverse(tfa9890, tfa9890->state.reversed);
    }
    if(rstate.enabled && (rstate.streams || rstate.in_call)) {
        int again = 1;
        if (rstate.in_call) {
            // Wait for a clock to become available from the modem
            tfa9890->speaker_needed(1);
            while (again && tfa9890->ex_speaker_on(rstate.mode)){
                pthread_mutex_unlock(&tfa9890->amp_update);
                usleep(1*1000);
                ALOGD("%s: Failed turning amp on %d times", __func__, again);
                again = (sem_trywait(&tfa9890->trigger_update) == -1) ? again + 1 : 0;
                pthread_mutex_lock(&tfa9890->amp_update);
            }
            if (!again) {
                ALOGD("%s: Amp state was changed, triggering refresh", __func__);
                sem_post(&tfa9890->trigger_update);
            }
        } else {
            tfa9890->set_mode(rstate.mode);
            tfa9890->speaker_needed(1);
            tfa9890->stereo_speaker_on();
        }
    } else {
        /*
        if (!tfa9890->state.calibration_done) {
            int ret = tfa9890->ex_calibration(0);
            if (ret) {
                if (ret != -1) {
                    ALOGE("exTfa98xx_calibration: %d\n", ret);
                } else {
                    tfa9890->state.calibration_done = 1;
                }
            } else {
                tfa9890->state.calibration_done = 1;
            }
        }*/
        tfa9890->speaker_needed(0);
        tfa9890->stereo_speaker_off();
    }
}

// The 'original' amp hal method that kinda worked.
void tfa9890_set_amp_state_bypass(void *device, tfa9890_state_t rstate, __attribute__((unused)) int reinit) {
    tfa9890_device_t *tfa9890 = (tfa9890_device_t*) device;
    if (rstate.enabled) {
        if (rstate.in_call) {
            tfa9890->stereo_speaker_needed(1, 0);
        } else {
            tfa9890->stereo_speaker_needed(1, 1);
        }
        tfa9890->speaker_on_bypass();
    } else {
        tfa9890->stereo_speaker_needed(0, 0);
        tfa9890->speaker_off_bypass();
    }
}

#define AMP_CONTROL_NORMAL 0
#define AMP_CONTROL_LEGACY 1
#define AMP_CONTROL_BYPASS 2

void tfa9890_switch_amp_control_mode(void *device) {
    tfa9890_device_t *tfa9890 = (tfa9890_device_t*) device;
    int32_t amp_control_mode = property_get_int32("persist.axon7.amp_hal_control", 0);
    switch(amp_control_mode) {
        default:
        case AMP_CONTROL_NORMAL:
            if (tfa9890->set_amp_state != &tfa9890_set_amp_state) {
                ALOGD("Using normal amp control");
                tfa9890->set_amp_state = &tfa9890_set_amp_state;
            }
            break;
        case AMP_CONTROL_LEGACY:
            if (tfa9890->set_amp_state != &tfa9890_set_amp_state_legacy) {
                ALOGD("Using legacy amp control");
                tfa9890->set_amp_state = &tfa9890_set_amp_state_legacy;
            }
            break;
        case AMP_CONTROL_BYPASS:
            if (tfa9890->set_amp_state != &tfa9890_set_amp_state_bypass) {
                ALOGD("Using bypass amp control");
                tfa9890->set_amp_state = &tfa9890_set_amp_state_bypass;
            }
            break;
    }
}

void *tfa9890_update_amp(void *device) {
    tfa9890_device_t *tfa9890 = (tfa9890_device_t*) device;
    tfa9890_state_t rstate = {0,0,0,0,0,0,0,};
    bool reinitialize = true;
    do {
        sem_wait(&tfa9890->trigger_update);
        pthread_mutex_lock(&tfa9890->amp_update);

        // Drain any other pending updates
        while(sem_trywait(&tfa9890->trigger_update) != -1);

        tfa9890_dump_state(tfa9890->state, rstate, "new(current)");


        if (tfa9890->state.device != rstate.device) {
            reinitialize = true;
        }
        rstate.device = tfa9890->state.device;
        rstate.enabled = tfa9890->state.enabled;
        rstate.streams = tfa9890->state.streams;
        rstate.in_call = tfa9890->state.in_call;
        rstate.mode = tfa9890->state.mode;
        if (tfa9890->state.reversed != rstate.reversed) {
            reinitialize = true;
        }
        rstate.reversed = tfa9890->state.reversed;

        tfa9890->set_amp_state(device, rstate, reinitialize);
        if (!rstate.enabled) {
            tfa9890_switch_amp_control_mode(tfa9890);
        }
        reinitialize = false;
        pthread_mutex_unlock(&tfa9890->amp_update);
    } while(sem_trywait(&tfa9890->trigger_exit) == -1);
    return NULL;
}

static tfa9890_device_t *tfa9890_dev = NULL;

static int tfa9890_dev_close(hw_device_t *device) {
    tfa9890_device_t *tfa9890 = (tfa9890_device_t*) device;
    if (tfa9890) {
        sem_post(&tfa9890->trigger_exit);
        sem_post(&tfa9890->trigger_update);
        pthread_join(tfa9890->amp_thread, NULL);
        dlclose(tfa9890->lib_ptr);
        pthread_mutex_destroy(&tfa9890->amp_update);
        sem_destroy(&tfa9890->trigger_update);
        sem_destroy(&tfa9890->trigger_exit);
        free(tfa9890->device_to_amp_device);
        free(tfa9890);
    }
    return 0;
}

#define DEFAULT 0
#define STEREO_SPEAKER 1
#define MONO_SPEAKER 2

const int device_to_amp_device_default[] = {
    [SND_DEVICE_OUT_HANDSET]                        = MONO_SPEAKER,
    [SND_DEVICE_OUT_SPEAKER]                        = STEREO_SPEAKER,
    [SND_DEVICE_OUT_SPEAKER_REVERSE]                = STEREO_SPEAKER,
    [SND_DEVICE_OUT_SPEAKER_VBAT]                   = STEREO_SPEAKER,
    [SND_DEVICE_OUT_SPEAKER_AND_HEADPHONES]         = STEREO_SPEAKER,
    [SND_DEVICE_OUT_SPEAKER_AND_LINE]               = STEREO_SPEAKER,
    [SND_DEVICE_OUT_VOICE_HANDSET]                  = MONO_SPEAKER,
    // Example: setprop persist.axon7.amp_hal_device_map "16,2;17,2;18,2;19,2"
    //     sets the 4 annotated use cases below to MONO_SPEAKER
    [SND_DEVICE_OUT_VOICE_SPEAKER]                  = STEREO_SPEAKER, //16
    [SND_DEVICE_OUT_VOICE_SPEAKER_VBAT]             = STEREO_SPEAKER, //17
    [SND_DEVICE_OUT_VOICE_SPEAKER_2]                = STEREO_SPEAKER, //18
    [SND_DEVICE_OUT_VOICE_SPEAKER_2_VBAT]           = STEREO_SPEAKER, //19
    [SND_DEVICE_OUT_SPEAKER_AND_HDMI]               = STEREO_SPEAKER,
    [SND_DEVICE_OUT_SPEAKER_AND_DISPLAY_PORT]       = STEREO_SPEAKER,
    [SND_DEVICE_OUT_SPEAKER_AND_BT_A2DP]            = STEREO_SPEAKER,
    [SND_DEVICE_OUT_SPEAKER_AND_BT_SCO]             = STEREO_SPEAKER,
    [SND_DEVICE_OUT_SPEAKER_AND_BT_SCO_WB]          = STEREO_SPEAKER,
    [SND_DEVICE_OUT_SPEAKER_AND_USB_HEADSET]        = STEREO_SPEAKER,
    [SND_DEVICE_OUT_SPEAKER_AND_ANC_HEADSET]        = STEREO_SPEAKER,
    [SND_DEVICE_OUT_SPEAKER_AND_ANC_FB_HEADSET]     = STEREO_SPEAKER,
    [SND_DEVICE_OUT_SPEAKER_PROTECTED]              = STEREO_SPEAKER,
    [SND_DEVICE_OUT_VOICE_SPEAKER_PROTECTED]        = STEREO_SPEAKER,
    [SND_DEVICE_OUT_VOICE_SPEAKER_2_PROTECTED]      = STEREO_SPEAKER,
    [SND_DEVICE_OUT_SPEAKER_PROTECTED_VBAT]         = STEREO_SPEAKER,
    [SND_DEVICE_OUT_VOICE_SPEAKER_PROTECTED_VBAT]   = STEREO_SPEAKER,
    [SND_DEVICE_OUT_VOICE_SPEAKER_2_PROTECTED_VBAT] = STEREO_SPEAKER,
    [SND_DEVICE_OUT_SPEAKER_WSA]                    = STEREO_SPEAKER,
    [SND_DEVICE_OUT_VOICE_SPEAKER_WSA]              = MONO_SPEAKER,
    [SND_DEVICE_OUT_VOICE_SPEAKER_2_WSA]            = MONO_SPEAKER,
    [SND_DEVICE_OUT_SPEAKER_PROTECTED_RAS]          = STEREO_SPEAKER,
    [SND_DEVICE_OUT_SPEAKER_PROTECTED_VBAT_RAS]     = STEREO_SPEAKER,
    [SND_DEVICE_OUT_VOICE_SPEAKER_AND_VOICE_HEADPHONES] = MONO_SPEAKER,
    [SND_DEVICE_OUT_VOICE_SPEAKER_AND_VOICE_ANC_HEADSET] = MONO_SPEAKER,
    [SND_DEVICE_OUT_VOICE_SPEAKER_AND_VOICE_ANC_FB_HEADSET] = MONO_SPEAKER,
};
#define LAST SND_DEVICE_OUT_VOICE_SPEAKER_AND_VOICE_ANC_FB_HEADSET

static void tfa9890_populate_device_map(tfa9890_device_t *tfa9890) {
    char buffer[PROPERTY_VALUE_MAX];
    size_t len = sizeof(device_to_amp_device_default);

    int *dev_map = malloc(len);
    memcpy(dev_map, device_to_amp_device_default, len);
    tfa9890->device_to_amp_device = dev_map;

    if (property_get("persist.axon7.amp_hal_device_map", buffer, "")) {
        // Gross hacks, don't judge this.
        char *end, *ptr = buffer;
        while (*ptr != '\0') {
            int adevice = 0, mode = 0;
            if (*ptr != ';') {
                adevice = strtol(ptr, &end, 10);
                if (*end != ',') {
                    ALOGE("Error parsing adevice.  Remaining string was %s", ptr);
                    return;
                }
                ptr = end + 1;
                mode = strtol(ptr, &end, 10);
                if (*end != ';' && *end != '\0') {
                    ALOGE("Error parsing mode.  Remaining string was %s", ptr);
                    return;
                }
                // Should sanity check here.  #YOLO
                ALOGD("Overriding device(%d): mode(%d->%d)\n", adevice, dev_map[adevice], mode);
                dev_map[adevice] = mode;
            } else {
                end = ptr + 1;
            }
            ptr = end;
        }
    }
}

static int tfa9890_set_output_devices(struct amplifier_device *device, uint32_t devices) {
    tfa9890_device_t *tfa9890 = (tfa9890_device_t*) device;

    if (devices == SND_DEVICE_NONE)
        return 0;

    pthread_mutex_lock(&tfa9890->amp_update);
    ALOGD("%s: %u\n", __func__, devices);

    if (devices > LAST) {
        ALOGD("%s: Device out of range, mapping needs updated: %d\n", __func__, devices);
        ALOGD("%s: Routing as STEREO_SPEAKER\n", __func__);
        devices = SND_DEVICE_OUT_SPEAKER;
    }

    int out_device = tfa9890->device_to_amp_device[devices];

    if (out_device == DEFAULT) {
        ALOGD("%s: Device not routed %d\n", __func__, devices);
        ALOGD("%s: Routing as STEREO_SPEAKER\n", __func__);
        out_device = STEREO_SPEAKER;
    }

    tfa9890->state.device = out_device - 1;
    tfa9890->state.reversed = (devices == SND_DEVICE_OUT_SPEAKER_REVERSE);
    sem_post(&tfa9890->trigger_update);

    pthread_mutex_unlock(&tfa9890->amp_update);
    return 0;
}

static int tfa9890_enable_output_devices(struct amplifier_device *device, uint32_t devices, bool enable) {
    tfa9890_device_t *tfa9890 = (tfa9890_device_t*) device;
    pthread_mutex_lock(&tfa9890->amp_update);
    ALOGD("%s: %u %d\n", __func__, devices, enable);
    switch (devices) {
        default:
            ALOGE("%s: Unhandled audio device: %d\n", __func__, devices);
        case SND_DEVICE_OUT_SPEAKER:
        case SND_DEVICE_OUT_SPEAKER_REVERSE:
            tfa9890->state.in_call = 0;
            break;
        case SND_DEVICE_OUT_VOICE_SPEAKER:
            tfa9890->state.in_call = enable;
            break;
        case SND_DEVICE_OUT_HANDSET:
        case SND_DEVICE_OUT_VOICE_HANDSET:
            tfa9890->state.in_call = enable;
            break;
    }
    // 'enabled' should probably be tracked based on devices
    tfa9890->state.enabled = enable;
    sem_post(&tfa9890->trigger_update);
    pthread_mutex_unlock(&tfa9890->amp_update);
    return 0;
}

static int tfa9890_set_mode(struct amplifier_device *device, audio_mode_t mode) {
    tfa9890_device_t *tfa9890 = (tfa9890_device_t*) device;
    pthread_mutex_lock(&tfa9890->amp_update);
    ALOGD("%s: %u\n", __func__, mode);
    switch (mode) {
        default:
            ALOGE("%s: Unhandled audio mode: %d\n", __func__, mode);
        case AUDIO_MODE_NORMAL:
        case AUDIO_MODE_RINGTONE:
            tfa9890->state.mode = MUSIC;
            break;
        case AUDIO_MODE_IN_CALL:
        case AUDIO_MODE_IN_COMMUNICATION:
            tfa9890->state.mode = VOICE;
            break;
    }
    sem_post(&tfa9890->trigger_update);
    pthread_mutex_unlock(&tfa9890->amp_update);
    return 0;
}

static int tfa9890_output_stream_start(struct amplifier_device *device, struct audio_stream_out *stream, bool offload) {
    tfa9890_device_t *tfa9890 = (tfa9890_device_t*) device;
    pthread_mutex_lock(&tfa9890->amp_update);
    ALOGD("%s: %p %d\n", __func__, stream, offload);
    tfa9890->state.streams++;
    sem_post(&tfa9890->trigger_update);
    pthread_mutex_unlock(&tfa9890->amp_update);
    return 0;
}

static int tfa9890_output_stream_standby(struct amplifier_device *device, struct audio_stream_out *stream) {
    tfa9890_device_t *tfa9890 = (tfa9890_device_t*) device;
    pthread_mutex_lock(&tfa9890->amp_update);
    ALOGD("%s: %p\n", __func__, stream);
    tfa9890->state.streams--;
    sem_post(&tfa9890->trigger_update);
    pthread_mutex_unlock(&tfa9890->amp_update);
    return 0;
}

int tfa9890_set_parameters(struct amplifier_device *device, struct str_parms *parms) {
    tfa9890_device_t *tfa9890 = (tfa9890_device_t*) device;
    // todo, dump parms.  Might be more stuff we need to do here
    pthread_mutex_lock(&tfa9890->amp_update);
    ALOGD("%s: %p\n", __func__, parms);
    str_parms_dump(parms);
    pthread_mutex_unlock(&tfa9890->amp_update);
    return 0;
}

static int tfa9890_module_open(const hw_module_t *module,
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
    tfa9890_dev->amp_dev.common.close = tfa9890_dev_close;

    tfa9890_dev->amp_dev.enable_output_devices = &tfa9890_enable_output_devices;
    tfa9890_dev->amp_dev.set_output_devices = &tfa9890_set_output_devices;
    tfa9890_dev->amp_dev.set_mode = &tfa9890_set_mode;
    tfa9890_dev->amp_dev.output_stream_start = &tfa9890_output_stream_start;
    tfa9890_dev->amp_dev.output_stream_standby = &tfa9890_output_stream_standby;
    tfa9890_dev->amp_dev.set_parameters = &tfa9890_set_parameters;

    tfa9890_dev->state.mode = MUSIC;

    tfa9890_dev->lib_ptr = dlopen("libtfa9890.so", RTLD_NOW);
    if (!tfa9890_dev->lib_ptr) {
        ALOGE("%s:%d: Unable to open libtfa9890.so: %s",
              __func__, __LINE__, dlerror());
        free(tfa9890_dev);
        return -ENODEV;
    }

    // Functions normally exported, or at least I can find reference to outside our hal.
    *(void **)&tfa9890_dev->ex_speaker_on = dlsym(tfa9890_dev->lib_ptr, "exTfa98xx_speakeron");
    *(void **)&tfa9890_dev->ex_speaker_off = dlsym(tfa9890_dev->lib_ptr, "exTfa98xx_speakeroff");
    *(void **)&tfa9890_dev->ex_calibration = dlsym(tfa9890_dev->lib_ptr, "exTfa98xx_calibration");
    if (!tfa9890_dev->ex_speaker_on || !tfa9890_dev->ex_speaker_off ||
        !tfa9890_dev->ex_calibration) {
        ALOGE("%s:%d: Unable to find required symbols", __func__, __LINE__);
        dlclose(tfa9890_dev->lib_ptr);
        free(tfa9890_dev);
        return -ENODEV;
    }


    // Internal functions we aren't supposed to need to use directly
    *(void **)&tfa9890_dev->speaker_needed = dlsym(tfa9890_dev->lib_ptr, "tfa9890_Set_SpeakerNeeded");
    *(void **)&tfa9890_dev->stereo_speaker_on = dlsym(tfa9890_dev->lib_ptr, "tfa9890_Stereo_SpeakerOn");
    *(void **)&tfa9890_dev->stereo_speaker_off = dlsym(tfa9890_dev->lib_ptr, "tfa9890_Stereo_SpeakerOff");
    *(void **)&tfa9890_dev->stereo_speaker_needed = dlsym(tfa9890_dev->lib_ptr, "tfa9890_Set_Stereo_SpeakerNeeded");
    *(void **)&tfa9890_dev->speaker_on_bypass = dlsym(tfa9890_dev->lib_ptr, "tfa9890_Stereo_SpeakerOn_bypass");
    *(void **)&tfa9890_dev->speaker_off_bypass = dlsym(tfa9890_dev->lib_ptr, "tfa9890_Stereo_SpeakerOff_bypass");
    *(void **)&tfa9890_dev->set_mode = dlsym(tfa9890_dev->lib_ptr, "tfa9890_set_mode");
    *(void **)&tfa9890_dev->set_device = dlsym(tfa9890_dev->lib_ptr, "tfa9890_set_device");
    if (!tfa9890_dev->speaker_needed || !tfa9890_dev->stereo_speaker_on ||
        !tfa9890_dev->stereo_speaker_off || !tfa9890_dev->set_mode || !tfa9890_dev->set_device ||
        !tfa9890_dev->stereo_speaker_needed || !tfa9890_dev->speaker_on_bypass ||
        !tfa9890_dev->speaker_off_bypass) {
        ALOGE("%s:%d: Unable to find required symbols", __func__, __LINE__);
        dlclose(tfa9890_dev->lib_ptr);
        free(tfa9890_dev);
        return -ENODEV;
    }

    tfa9890_switch_amp_control_mode(tfa9890_dev);
    tfa9890_populate_device_map(tfa9890_dev);

    sem_init(&tfa9890_dev->trigger_update, 0, 0);
    sem_init(&tfa9890_dev->trigger_exit, 0, 0);
    pthread_mutex_init(&tfa9890_dev->amp_update, NULL);
    pthread_create(&tfa9890_dev->amp_thread, NULL, tfa9890_update_amp, tfa9890_dev);
    *device = (hw_device_t *) tfa9890_dev;

    return 0;
}

static struct hw_module_methods_t hal_module_methods = {
    .open = tfa9890_module_open,
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
