#include "Mp3Encoder.h"
#include "../i18n/Translations.h"
#include <lame.h>

namespace silente::io
{
namespace
{
lame_global_flags* flags(void* p) { return static_cast<lame_global_flags*>(p); }
}

Mp3Encoder::~Mp3Encoder()
{
    if (lame != nullptr) lame_close(flags(lame));
}

bool Mp3Encoder::supportsRate(double rate)
{
    for (double r : { 8000.0, 11025.0, 12000.0, 16000.0, 22050.0, 24000.0, 32000.0, 44100.0, 48000.0 })
        if (std::abs(r - rate) < 0.5) return true;
    return false;
}

juce::Result Mp3Encoder::open(double sampleRate, int channelCount, int kilobitsPerSecond)
{
    if (! supportsRate(sampleRate)) return juce::Result::fail(i18n::tr("MP3 admite frecuencias de hasta 48 kHz (44,1 o 48 kHz recomendadas)."));
    channels = juce::jlimit(1, 2, channelCount);
    lame = lame_init();
    if (lame == nullptr) return juce::Result::fail(i18n::tr("No se pudo iniciar el codificador MP3."));
    auto* gfp = flags(lame);
    lame_set_in_samplerate(gfp, juce::roundToInt(sampleRate));
    lame_set_out_samplerate(gfp, juce::roundToInt(sampleRate));
    lame_set_num_channels(gfp, channels);
    lame_set_mode(gfp, channels == 1 ? MONO : JOINT_STEREO);
    lame_set_VBR(gfp, vbr_off);
    lame_set_brate(gfp, juce::jlimit(32, 320, kilobitsPerSecond));
    lame_set_quality(gfp, 2);
    if (lame_init_params(gfp) < 0) return juce::Result::fail(i18n::tr("Ajustes MP3 no válidos."));
    return juce::Result::ok();
}

bool Mp3Encoder::encode(const float* left, const float* right, int samples, juce::OutputStream& out)
{
    if (lame == nullptr || samples <= 0) return samples == 0;
    buffer.resize((size_t) (samples + samples / 4 + 7200));
    const int bytes = lame_encode_buffer_ieee_float(flags(lame), left, channels == 2 && right != nullptr ? right : left, samples,
                                                    buffer.data(), (int) buffer.size());
    return bytes >= 0 && (bytes == 0 || out.write(buffer.data(), (size_t) bytes));
}

bool Mp3Encoder::finish(juce::OutputStream& out)
{
    if (lame == nullptr) return false;
    buffer.resize(7200);
    const int bytes = lame_encode_flush(flags(lame), buffer.data(), (int) buffer.size());
    return bytes >= 0 && (bytes == 0 || out.write(buffer.data(), (size_t) bytes));
}
}
