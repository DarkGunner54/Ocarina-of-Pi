#include "audio_alsa.h"
#include <cstdio>
#include <cstring>
#include <iostream>
#include <cassert>
#include <unistd.h>

AudioAlsa::AudioAlsa(const Config& config)
    : config_(config)
    , pcm_handle_(nullptr)
    , hw_params_(nullptr)
    , sw_params_(nullptr)
    , running_(false)
{
}

AudioAlsa::~AudioAlsa() {
    shutdown();
}

bool AudioAlsa::initialize() {
    if (!openPcm()) {
        return false;
    }
    if (!configurePcm()) {
        return false;
    }
    if (!preparePcm()) {
        return false;
    }
    return true;
}

void AudioAlsa::shutdown() {
    running_ = false;

    if (sw_params_) {
        snd_pcm_sw_params_free(sw_params_);
        sw_params_ = nullptr;
    }
    if (hw_params_) {
        snd_pcm_hw_params_free(hw_params_);
        hw_params_ = nullptr;
    }

    if (pcm_handle_) {
        snd_pcm_close(pcm_handle_);
        pcm_handle_ = nullptr;
    }
}

bool AudioAlsa::openPcm() {
    int err = snd_pcm_open(&pcm_handle_, config_.device.c_str(), SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) {
        std::cerr << "[AudioAlsa] Cannot open PCM device " << config_.device
                  << ": " << snd_strerror(err) << std::endl;
        return false;
    }
    std::cout << "[AudioAlsa] Opened PCM device: " << config_.device << std::endl;
    return true;
}

bool AudioAlsa::configurePcm() {
    int err;

    err = snd_pcm_hw_params_malloc(&hw_params_);
    if (err < 0) {
        std::cerr << "[AudioAlsa] Cannot allocate hardware parameter structure" << std::endl;
        return false;
    }

    err = snd_pcm_hw_params_any(pcm_handle_, hw_params_);
    if (err < 0) {
        std::cerr << "[AudioAlsa] Cannot get initial parameters" << std::endl;
        return false;
    }

    err = snd_pcm_hw_params_set_access(pcm_handle_, hw_params_, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err < 0) {
        std::cerr << "[AudioAlsa] Cannot set interleaved access" << std::endl;
        return false;
    }

    err = snd_pcm_hw_params_set_format(pcm_handle_, hw_params_, config_.format);
    if (err < 0) {
        std::cerr << "[AudioAlsa] Cannot set format: " << snd_strerror(err) << std::endl;
        return false;
    }

    unsigned int channels = config_.channels;
    err = snd_pcm_hw_params_set_channels(pcm_handle_, hw_params_, channels);
    if (err < 0) {
        std::cerr << "[AudioAlsa] Cannot set channels" << std::endl;
        return false;
    }

    unsigned int rate = config_.sample_rate;
    err = snd_pcm_hw_params_set_rate_near(pcm_handle_, hw_params_, &rate, 0);
    if (err < 0) {
        std::cerr << "[AudioAlsa] Cannot set sample rate" << std::endl;
        return false;
    }
    if (rate != config_.sample_rate && config_.software_resample) {
        std::cout << "[AudioAlsa] Hardware rate " << rate << " != target " << config_.sample_rate
                  << " - will use software resampling" << std::endl;
    }

    snd_pcm_uframes_t period = config_.period_frames;
    err = snd_pcm_hw_params_set_period_size_near(pcm_handle_, hw_params_, &period, 0);
    if (err < 0) {
        std::cerr << "[AudioAlsa] Cannot set period size" << std::endl;
        return false;
    }

    snd_pcm_uframes_t buffer = config_.buffer_frames;
    err = snd_pcm_hw_params_set_buffer_size_near(pcm_handle_, hw_params_, &buffer);
    if (err < 0) {
        std::cerr << "[AudioAlsa] Cannot set buffer size" << std::endl;
        return false;
    }

    err = snd_pcm_hw_params(pcm_handle_, hw_params_);
    if (err < 0) {
        std::cerr << "[AudioAlsa] Cannot apply hardware parameters: " << snd_strerror(err) << std::endl;
        return false;
    }

    err = snd_pcm_sw_params_malloc(&sw_params_);
    if (err < 0) {
        std::cerr << "[AudioAlsa] Cannot allocate software parameters" << std::endl;
        return false;
    }

    err = snd_pcm_sw_params_current(pcm_handle_, sw_params_);
    if (err < 0) {
        std::cerr << "[AudioAlsa] Cannot get current software parameters" << std::endl;
        return false;
    }

    err = snd_pcm_sw_params_set_start_threshold(pcm_handle_, sw_params_, period);
    if (err < 0) {
        std::cerr << "[AudioAlsa] Cannot set start threshold" << std::endl;
        return false;
    }

    err = snd_pcm_sw_params_set_avail_min(pcm_handle_, sw_params_, period);
    if (err < 0) {
        std::cerr << "[AudioAlsa] Cannot set avail min" << std::endl;
        return false;
    }

    err = snd_pcm_sw_params(pcm_handle_, sw_params_);
    if (err < 0) {
        std::cerr << "[AudioAlsa] Cannot apply software parameters" << std::endl;
        return false;
    }

    return true;
}

bool AudioAlsa::preparePcm() {
    int err = snd_pcm_prepare(pcm_handle_);
    if (err < 0) {
        std::cerr << "[AudioAlsa] Cannot prepare PCM: " << snd_strerror(err) << std::endl;
        return false;
    }
    return true;
}

bool AudioAlsa::start() {
    running_ = true;
    return true;
}

bool AudioAlsa::write(const uint16_t* samples, unsigned int frame_count) {
    if (!running_ || !pcm_handle_) return false;

    snd_pcm_uframes_t frames = frame_count;
    long result = snd_pcm_writei(pcm_handle_, samples, frames);

    if (result < 0) {
        if (result == -EPIPE) {
            snd_pcm_prepare(pcm_handle_);
            result = snd_pcm_writei(pcm_handle_, samples, frames);
        } else {
            return false;
        }
    }

    if (static_cast<unsigned long>(result) != frame_count) {
        return false;
    }
    return true;
}

bool AudioAlsa::writeInterleaved(const void* samples, unsigned int frame_count) {
    if (!running_ || !pcm_handle_) return false;

    snd_pcm_uframes_t frames = frame_count;
    long result = snd_pcm_writei(pcm_handle_, samples, frames);

    if (result < 0) {
        if (result == -EPIPE) {
            snd_pcm_prepare(pcm_handle_);
            result = snd_pcm_writei(pcm_handle_, samples, frames);
        } else {
            return false;
        }
    }

    if (static_cast<unsigned long>(result) != frame_count) {
        return false;
    }
    return true;
}

void AudioAlsa::drain() {
    if (pcm_handle_) {
        snd_pcm_drain(pcm_handle_);
    }
}

void AudioAlsa::pause() {
    if (pcm_handle_) {
        snd_pcm_pause(pcm_handle_, 1);
    }
    running_ = false;
}

void AudioAlsa::resume() {
    if (pcm_handle_) {
        snd_pcm_pause(pcm_handle_, 0);
    }
    running_ = true;
}

double AudioAlsa::getLatencySeconds() const {
    if (!pcm_handle_) return 0.0;
    snd_pcm_sframes_t delay_frames;
    if (snd_pcm_avail_update(pcm_handle_) < 0) return 0.0;
    if (snd_pcm_delay(pcm_handle_, &delay_frames) < 0) return 0.0;
    return static_cast<double>(delay_frames) / config_.sample_rate;
}
