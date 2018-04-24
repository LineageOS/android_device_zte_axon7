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
#include <semaphore.h>
#include <sys/ioctl.h>

#define LOG_TAG "axon7-tfa98xx"
#include <log/log.h>

#include <hardware/audio_amplifier.h>

#define AUDIO_NORMAL 0
#define AUDIO_VOICE 1
#define AUDIO_MUSIC 2

typedef struct tfa9890_state {
    int streams;
    int in_call;
    int device;
    int mode;
    int amp_mode;
    int reversed;
    int enabled;
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
    int (*speaker_on_call)(int mode);
    void (*speaker_off_call)(void);
    void (*reverse)(int);
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
          "amp_mode: %d(%d)\n\t"\
          "reversed: %d(%d)\n",\
          __func__, label,\
          t.enabled, n.enabled,\
          t.device, n.device,\
          t.streams, n.streams,\
          t.in_call, n.in_call,\
          t.mode, n.mode,\
          t.amp_mode, t.amp_mode,\
          t.reversed, n.reversed\
    );\
} while (0)

void *tfa9890_update_amp(void *device) {
    tfa9890_device_t *tfa9890 = (tfa9890_device_t*) device;
    tfa9890_state_t rstate = {0};
    do {
        bool reinitialize = false;
        sem_wait(&tfa9890->trigger_update);
        pthread_mutex_lock(&tfa9890->amp_update);

        tfa9890_dump_state(tfa9890->state, rstate, "new(current)");

        if (tfa9890->state.enabled != rstate.enabled) {
            reinitialize = true;
        }
        rstate.enabled = tfa9890->state.enabled;

        if (tfa9890->state.device != rstate.device) {
            reinitialize = true;
        }
        rstate.device = tfa9890->state.device;

        if (!!tfa9890->state.streams != !!rstate.streams) {
            reinitialize = true;
        }
        rstate.streams = tfa9890->state.streams;

        if (tfa9890->state.in_call != rstate.in_call) {
            reinitialize = true;
        }
        rstate.in_call = tfa9890->state.in_call;

        rstate.mode = tfa9890->state.mode;

        if (tfa9890->state.amp_mode != rstate.amp_mode) {
            if (!reinitialize) {
                tfa9890->set_mode(tfa9890->state.amp_mode);
            }
        }
        rstate.amp_mode = tfa9890->state.amp_mode;

        if (tfa9890->state.reversed != rstate.reversed) {
            if (!reinitialize) {
                tfa9890->reverse(tfa9890->state.reversed);
            }
        }
        rstate.reversed = tfa9890->state.reversed;

        if(rstate.enabled && (rstate.streams || rstate.in_call)) {
            if (reinitialize) {
                tfa9890->speaker_needed(0);
                tfa9890->speaker_off();
                tfa9890->set_device(rstate.device);
                tfa9890->set_mode(rstate.amp_mode);
                //tfa9890->reverse(rstate.reversed);
            }
            if (rstate.in_call) {
                pthread_mutex_unlock(&tfa9890->amp_update);
                // Bump this number if call audio ever fails.
                usleep(40*1000);
                pthread_mutex_lock(&tfa9890->amp_update);
            }
            tfa9890->speaker_needed(1);
// Revisit this uglyness when stereo audio is sorted out.  It works better for in-call
// audio, but my sound is currently bunk.
//            if (rstate.in_call) {
//                bool again = true;
//                while (tfa9890->speaker_on_call(rstate.mode) && again){
//                    pthread_mutex_unlock(&tfa9890->amp_update);
//
//                    again = (sem_trywait(&tfa9890->trigger_update) == -1);
//                    pthread_mutex_lock(&tfa9890->amp_update);
//                }
//                if (!again) {
//                    sem_post(&tfa9890->trigger_update);
//                } else {
//                    tfa9890->reverse(1);
//                }
//            } else {
                tfa9890->speaker_on();
//            }
        } else {
            tfa9890->speaker_needed(0);
            tfa9890->speaker_off();
        }
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
        free(tfa9890);
    }
    return 0;
}

static int tfa9890_set_output_devices(struct amplifier_device *device, uint32_t devices) {
    ALOGD("%s: %u\n", __func__, devices);
    tfa9890_device_t *tfa9890 = (tfa9890_device_t*) device;
    pthread_mutex_lock(&tfa9890->amp_update);
    switch (devices) {
        default:
            ALOGE("%s: Unhandled output device: %d\n", __func__, devices);
        case SND_DEVICE_OUT_SPEAKER:
        case SND_DEVICE_OUT_SPEAKER_REVERSE:
        case SND_DEVICE_OUT_VOICE_SPEAKER:
            tfa9890->state.device = 0;
            break;
        case SND_DEVICE_OUT_HANDSET:
        case SND_DEVICE_OUT_VOICE_HANDSET:
            tfa9890->state.device = 1;
            break;
    }
    tfa9890->state.reversed = (devices == SND_DEVICE_OUT_SPEAKER_REVERSE);
    sem_post(&tfa9890->trigger_update);
    pthread_mutex_unlock(&tfa9890->amp_update);
    return 0;
}

static int tfa9890_enable_output_devices(struct amplifier_device *device, uint32_t devices, bool enable) {
    ALOGD("%s: %u %d\n", __func__, devices, enable);
    tfa9890_device_t *tfa9890 = (tfa9890_device_t*) device;
    pthread_mutex_lock(&tfa9890->amp_update);
    switch (devices) {
        default:
            ALOGE("%s: Unhandled audio device: %d\n", __func__, devices);
        case SND_DEVICE_OUT_SPEAKER:
        case SND_DEVICE_OUT_SPEAKER_REVERSE:
        case SND_DEVICE_OUT_VOICE_SPEAKER:
            tfa9890->state.in_call = 0;
            tfa9890->state.mode = 0;
            tfa9890->state.amp_mode = AUDIO_MUSIC;
            break;
        case SND_DEVICE_OUT_HANDSET:
        case SND_DEVICE_OUT_VOICE_HANDSET:
            tfa9890->state.in_call = enable;
            tfa9890->state.mode = 1;
            tfa9890->state.amp_mode = AUDIO_VOICE;
            break;
    }
    // 'enabled' should probably be tracked based on devices
    tfa9890->state.enabled = enable;
    sem_post(&tfa9890->trigger_update);
    pthread_mutex_unlock(&tfa9890->amp_update);
    return 0;
}

static int tfa9890_set_mode(struct amplifier_device *device, audio_mode_t mode) {
    ALOGD("%s: %u\n", __func__, mode);
    tfa9890_device_t *tfa9890 = (tfa9890_device_t*) device;
    pthread_mutex_lock(&tfa9890->amp_update);
    // Stock used 2 and 1 and 0, and all 3 seem to be the same... no clue.  Draken's log showed 2 for music
    switch (mode) {
        default:
            ALOGE("%s: Unhandled audio mode: %d\n", __func__, mode);
        case AUDIO_MODE_NORMAL:
        case AUDIO_MODE_RINGTONE:
            tfa9890->state.amp_mode = AUDIO_MUSIC;
            break;
        case AUDIO_MODE_IN_CALL:
        case AUDIO_MODE_IN_COMMUNICATION:
            tfa9890->state.amp_mode = AUDIO_VOICE;
            break;
    }
    sem_post(&tfa9890->trigger_update);
    pthread_mutex_unlock(&tfa9890->amp_update);
    return 0;
}

static int tfa9890_output_stream_start(struct amplifier_device *device, struct audio_stream_out *stream, bool offload) {
    ALOGD("%s: %p %d\n", __func__, stream, offload);
    tfa9890_device_t *tfa9890 = (tfa9890_device_t*) device;
    pthread_mutex_lock(&tfa9890->amp_update);
    tfa9890->state.streams++;
    sem_post(&tfa9890->trigger_update);
    pthread_mutex_unlock(&tfa9890->amp_update);
    return 0;
}

static int tfa9890_output_stream_standby(struct amplifier_device *device, struct audio_stream_out *stream) {
    ALOGD("%s: %p\n", __func__, stream);
    tfa9890_device_t *tfa9890 = (tfa9890_device_t*) device;
    pthread_mutex_lock(&tfa9890->amp_update);
    tfa9890->state.streams--;
    sem_post(&tfa9890->trigger_update);
    pthread_mutex_unlock(&tfa9890->amp_update);
    return 0;
}

int tfa9890_set_parameters(struct amplifier_device *device, struct str_parms *parms) {
    ALOGD("%s: %p\n", __func__, parms);
    tfa9890_device_t *tfa9890 = (tfa9890_device_t*) device;
    // todo, dump parms.  Might be more stuff we need to do here
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

    tfa9890_dev->lib_ptr = dlopen("libtfa9890.so", RTLD_NOW);
    if (!tfa9890_dev->lib_ptr) {
        ALOGE("%s:%d: Unable to open libtfa9890.so: %s",
              __func__, __LINE__, dlerror());
        free(tfa9890_dev);
        return -ENODEV;
    }

    // Functions normally exported, or at least I can find reference to outside our hal.
    *(void **)&tfa9890_dev->speaker_on_call = dlsym(tfa9890_dev->lib_ptr, "exTfa98xx_speakeron");
    *(void **)&tfa9890_dev->speaker_off_call = dlsym(tfa9890_dev->lib_ptr, "exTfa98xx_speakeroff");
    *(void **)&tfa9890_dev->reverse = dlsym(tfa9890_dev->lib_ptr, "exTfa98xx_LR_Switch");
    *(void **)&tfa9890_dev->calibration = dlsym(tfa9890_dev->lib_ptr, "exTfa98xx_calibration");
    if (!tfa9890_dev->speaker_on_call || !tfa9890_dev->speaker_off_call ||
        !tfa9890_dev->reverse || !tfa9890_dev->calibration) {
        ALOGE("%s:%d: Unable to find required symbols", __func__, __LINE__);
        dlclose(tfa9890_dev->lib_ptr);
        free(tfa9890_dev);
        return -ENODEV;
    }


    // Uncertain if we're supposed to need to use these.  ZTE Made them?
    *(void **)&tfa9890_dev->speaker_on = dlsym(tfa9890_dev->lib_ptr, "tfa9890_Stereo_SpeakerOn");
    *(void **)&tfa9890_dev->speaker_off = dlsym(tfa9890_dev->lib_ptr, "tfa9890_Stereo_SpeakerOff");
    *(void **)&tfa9890_dev->speaker_needed = dlsym(tfa9890_dev->lib_ptr, "tfa9890_Set_SpeakerNeeded");
    if (!tfa9890_dev->speaker_on || !tfa9890_dev->speaker_off ||
        !tfa9890_dev->speaker_needed) {
        ALOGE("%s:%d: Unable to find required symbols", __func__, __LINE__);
        dlclose(tfa9890_dev->lib_ptr);
        free(tfa9890_dev);
        return -ENODEV;
    }

    // Internal functions we aren't supposed to need to use directly
    *(void **)&tfa9890_dev->set_mode = dlsym(tfa9890_dev->lib_ptr, "tfa9890_set_mode");
    *(void **)&tfa9890_dev->set_device = dlsym(tfa9890_dev->lib_ptr, "tfa9890_set_device");
    if (!tfa9890_dev->set_mode || !tfa9890_dev->set_device) {
        ALOGE("%s:%d: Unable to find required symbols", __func__, __LINE__);
        dlclose(tfa9890_dev->lib_ptr);
        free(tfa9890_dev);
        return -ENODEV;
    }

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
