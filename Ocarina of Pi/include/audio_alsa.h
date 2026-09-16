#ifndef AUDIO_ALSA_H
#define AUDIO_ALSA_H

#include <alsa/asoundlib.h>
#include <cstdint>
#include <string>

class AudioAlsa {
public:
    struct Config {
        std::string device;
        unsigned int sample_rate;
        int channels;
        snd_pcm_format_t format;
        unsigned int buffer_frames;
        unsigned int period_frames;
        bool software_resample;
    };

    explicit AudioAlsa(const Config& config);
    ~AudioAlsa();

    bool initialize();
    void shutdown();

    bool write(const uint16_t* samples, unsigned int frame_count);
    bool writeInterleaved(const void* samples, unsigned int frame_count);

    bool start();
    void drain();
    void pause();
    void resume();

    unsigned int getBufferSizeFrames() const { return config_.buffer_frames; }
    unsigned int getPeriodSizeFrames() const { return config_.period_frames; }
    unsigned int getSampleRate() const { return config_.sample_rate; }
    int getChannels() const { return config_.channels; }

    double getLatencySeconds() const;

private:
    bool openPcm();
    bool configurePcm();
    bool preparePcm();
    int calculateResampleRatio(unsigned int src_rate, unsigned int dst_rate);

    Config config_;
    snd_pcm_t* pcm_handle_;
    snd_pcm_hw_params_t* hw_params_;
    snd_pcm_sw_params_t* sw_params_;
    bool running_;
};

#endif
