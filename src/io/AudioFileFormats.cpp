#include "AudioFileFormats.h"
#include "Mp3Encoder.h"
#include "VideoAudio.h"
#include "../audio/BandlimitedInterpolator.h"
#include "../i18n/Translations.h"
#include <algorithm>

namespace silente::io
{
juce::String extensionFor(FileFormat f)
{
    switch (f)
    {
        case FileFormat::aiff: return ".aif";
        case FileFormat::flac: return ".flac";
        case FileFormat::mp3: return ".mp3";
        case FileFormat::m4a: return ".m4a";
        case FileFormat::wav: break;
    }
    return ".wav";
}

juce::String displayName(FileFormat f)
{
    switch (f)
    {
        case FileFormat::aiff: return "AIFF";
        case FileFormat::flac: return "FLAC";
        case FileFormat::mp3: return "MP3 (MPEG-1 Layer 3)";
        case FileFormat::m4a: return "M4A (AAC)";
        case FileFormat::wav: break;
    }
    return "Wave (WAV)";
}

juce::String readableWildcard(juce::AudioFormatManager& formats)
{
    return formats.getWildcardForAllFormats() + ";*.m4a;*.mp3;*.aac;*.caf;" + videoWildcard();
}

std::vector<double> supportedRates(FileFormat f)
{
    if (f == FileFormat::mp3 || f == FileFormat::m4a) return { 8000.0, 11025.0, 12000.0, 16000.0, 22050.0, 24000.0, 32000.0, 44100.0, 48000.0 };
    return { 8000.0, 11025.0, 16000.0, 22050.0, 32000.0, 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 };
}

std::vector<int> supportedBitDepths(FileFormat f)
{
    if (f == FileFormat::flac || f == FileFormat::aiff) return { 16, 24 }; // JUCE's AIFF writer has no 32-bit float
    if (f == FileFormat::wav) return { 16, 24, 32 };
    return {};
}

std::vector<int> supportedBitRates(FileFormat f, double rate)
{
    if (f == FileFormat::mp3) // LAME: MPEG-1 (32/44.1/48 kHz), MPEG-2 (16/22.05/24), MPEG-2.5 (8/11.025/12)
    {
        if (rate >= 31999.0) return { 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320 };
        if (rate >= 15999.0) return { 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160 };
        return { 8, 16, 24, 32, 40, 48, 56, 64 };
    }
    if (f == FileFormat::m4a)
    {
        // afconvert AAC-LC stereo, measured on macOS: below `low` it silently lowers the sample rate
        // of the file, above `high` it refuses. Only bit rates that keep the requested rate are offered.
        std::vector<int> all { 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320 };
        const int high = rate >= 44099.0 ? 320 : rate >= 31999.0 ? 192 : rate >= 22049.0 ? 128 : rate >= 15999.0 ? 96 : rate >= 11024.0 ? 64 : 48;
        const int low = rate >= 44099.0 ? 96 : rate >= 31999.0 ? 56 : rate >= 22049.0 ? 48 : rate >= 15999.0 ? 40 : rate >= 11024.0 ? 32 : 16;
        all.erase(std::remove_if(all.begin(), all.end(), [low, high](int b) { return b < low || b > high; }), all.end());
        return all;
    }
    return {};
}

juce::Result validate(const EncodeSettings& s)
{
    const auto rates = supportedRates(s.format);
    if (std::none_of(rates.begin(), rates.end(), [&s](double r) { return std::abs(r - s.sampleRate) < 0.5; }))
    {
        if (s.format == FileFormat::mp3) return juce::Result::fail(i18n::tr("MP3 admite frecuencias de hasta 48 kHz (44,1 o 48 kHz recomendadas)."));
        if (s.format == FileFormat::m4a) return juce::Result::fail(i18n::tr("M4A/AAC admite frecuencias de hasta 48 kHz."));
        return juce::Result::fail(i18n::tr("Frecuencia de exportación no válida."));
    }
    if (s.format == FileFormat::mp3 || s.format == FileFormat::m4a)
    {
        const auto rates2 = supportedBitRates(s.format, s.sampleRate);
        if (std::find(rates2.begin(), rates2.end(), s.kilobitsPerSecond) == rates2.end())
            return juce::Result::fail(i18n::tr("Tasa de bits no admitida para este formato y frecuencia."));
        return juce::Result::ok();
    }
    const auto depths = supportedBitDepths(s.format);
    if (std::find(depths.begin(), depths.end(), s.bitDepth) == depths.end())
        return juce::Result::fail(s.format == FileFormat::flac ? i18n::tr("FLAC admite 16 o 24 bits.") : s.format == FileFormat::aiff ? i18n::tr("AIFF admite 16 o 24 bits.") : i18n::tr("La profundidad debe ser 16, 24 o 32 bits."));
    return juce::Result::ok();
}

EncodedFileWriter::EncodedFileWriter() = default;
EncodedFileWriter::~EncodedFileWriter() = default;

juce::Result EncodedFileWriter::open(const juce::File& file, const EncodeSettings& s, int channelCount)
{
    if (auto r = validate(s); r.failed()) return r;
    target = file; settings = s; channels = juce::jlimit(1, 2, channelCount);
    if (target.getParentDirectory().createDirectory().failed()) return juce::Result::fail(i18n::tr("No se puede escribir en la carpeta de destino."));
    // M4A is encoded afterwards from a 24-bit WAV; everything else is written directly.
    const auto intermediate = settings.format == FileFormat::m4a ? target.withFileExtension(".wav") : target;
    temporary = std::make_unique<juce::TemporaryFile>(intermediate);
    if (settings.format == FileFormat::mp3)
    {
        mp3Stream = temporary->getFile().createOutputStream();
        if (! mp3Stream || ! mp3Stream->openedOk()) return juce::Result::fail(i18n::tr("No se puede escribir en la carpeta de destino."));
        mp3 = std::make_unique<Mp3Encoder>();
        return mp3->open(settings.sampleRate, channels, settings.kilobitsPerSecond);
    }
    std::unique_ptr<juce::OutputStream> stream(temporary->getFile().createOutputStream());
    if (! stream) return juce::Result::fail(i18n::tr("No se puede escribir en la carpeta de destino."));
    std::unique_ptr<juce::AudioFormat> format;
    int bits = settings.bitDepth;
    if (settings.format == FileFormat::aiff) format = std::make_unique<juce::AiffAudioFormat>();
    else if (settings.format == FileFormat::flac) format = std::make_unique<juce::FlacAudioFormat>();
    else { format = std::make_unique<juce::WavAudioFormat>(); if (settings.format == FileFormat::m4a) bits = 24; }
    auto options = juce::AudioFormatWriterOptions().withSampleRate(settings.sampleRate).withNumChannels(channels).withBitsPerSample(bits);
    if (bits == 32) options = options.withSampleFormat(juce::AudioFormatWriterOptions::SampleFormat::floatingPoint);
    writer = format->createWriterFor(stream, options);
    if (! writer) return juce::Result::fail(i18n::tr("No se pudo crear el archivo de audio."));
    return juce::Result::ok();
}

bool EncodedFileWriter::write(const juce::AudioBuffer<float>& block, int start, int count)
{
    if (count <= 0) return true;
    if (mp3)
        return mp3->encode(block.getReadPointer(0, start), block.getNumChannels() > 1 ? block.getReadPointer(1, start) : nullptr, count, *mp3Stream);
    if (! writer) return false;
    if (channels == block.getNumChannels()) return writer->writeFromAudioSampleBuffer(block, start, count);
    const float* source[2] { block.getReadPointer(0, start), block.getReadPointer(juce::jmin(1, block.getNumChannels() - 1), start) };
    return writer->writeFromFloatArrays(source, channels, count);
}

juce::Result EncodedFileWriter::finish()
{
    bool ok = true;
    if (mp3) { ok = mp3->finish(*mp3Stream); mp3Stream->flush(); mp3Stream.reset(); mp3.reset(); }
    writer.reset();
    if (! ok || ! temporary) return juce::Result::fail(i18n::tr("La escritura del archivo falló. Comprueba el espacio libre."));
    if (settings.format == FileFormat::m4a)
    {
        // macOS's own AAC encoder (Core Audio afconvert): AAC-LC in an M4A container.
        juce::ChildProcess converter;
        const juce::StringArray command { "/usr/bin/afconvert", "-f", "m4af", "-d", "aac", "-b", juce::String(settings.kilobitsPerSecond * 1000),
                                          temporary->getFile().getFullPathName(), target.getFullPathName() };
        target.deleteFile();
        if (! converter.start(command) || ! converter.waitForProcessToFinish(600000) || converter.getExitCode() != 0 || ! target.existsAsFile())
            return juce::Result::fail(i18n::tr("macOS no pudo codificar el archivo M4A/AAC."));
        temporary.reset();
        return juce::Result::ok();
    }
    if (! temporary->overwriteTargetFileWithTemporary()) return juce::Result::fail(i18n::tr("No se pudo finalizar el archivo de audio."));
    temporary.reset();
    return juce::Result::ok();
}

juce::Result convertFile(juce::AudioFormatManager& formats, const juce::File& input, const juce::File& output,
                         const EncodeSettings& settings, const std::function<bool(double)>& progress)
{
    if (auto r = validate(settings); r.failed()) return r;
    if (input == output) return juce::Result::fail(i18n::tr("El archivo de destino no puede ser el de origen."));
    juce::File source = input;
    std::unique_ptr<juce::TemporaryFile> extracted;
    if (isVideoFile(input))
    {
        extracted = std::make_unique<juce::TemporaryFile>(".m4a");
        if (auto r = extractAudioTrack(input, extracted->getFile()); r.failed()) return r;
        source = extracted->getFile();
    }
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(source));
    if (! reader) return juce::Result::fail(i18n::tr("No se puede leer el audio: ") + input.getFileName());
    if (reader->lengthInSamples <= 0 || reader->sampleRate <= 0.0) return juce::Result::fail(i18n::tr("El archivo de audio está vacío o no es válido."));
    if (reader->lengthInSamples > 100000000) return juce::Result::fail(i18n::tr("Este archivo supera el límite de 100 millones de muestras por clip. Divide el archivo antes de importarlo."));
    const int channels = juce::jlimit(1, 2, (int) reader->numChannels);
    juce::AudioBuffer<float> audio(channels, (int) reader->lengthInSamples);
    if (! reader->read(&audio, 0, audio.getNumSamples(), 0, true, channels > 1)) return juce::Result::fail(i18n::tr("No se puede leer el audio: ") + input.getFileName());
    EncodedFileWriter writer;
    if (auto r = writer.open(output, settings, channels); r.failed()) return r;
    const double ratio = reader->sampleRate / settings.sampleRate;
    const bool resample = std::abs(ratio - 1.0) > 1.0e-9;
    const auto& sinc = audio::BandlimitedInterpolator::instance();
    const double scale = audio::BandlimitedInterpolator::scaleFor(reader->sampleRate, settings.sampleRate);
    const auto total = resample ? (juce::int64) std::floor(audio.getNumSamples() / ratio) : (juce::int64) audio.getNumSamples();
    juce::AudioBuffer<float> block(channels, 4096);
    for (juce::int64 done = 0; done < total; done += block.getNumSamples())
    {
        const int count = (int) juce::jmin<juce::int64>(block.getNumSamples(), total - done);
        if (resample)
        {
            for (int ch = 0; ch < channels; ++ch)
                for (int i = 0; i < count; ++i)
                    block.setSample(ch, i, sinc.read(audio.getReadPointer(ch), audio.getNumSamples(), (double) (done + i) * ratio, scale));
            if (! writer.write(block, 0, count)) return juce::Result::fail(i18n::tr("La escritura del archivo falló. Comprueba el espacio libre."));
        }
        else if (! writer.write(audio, (int) done, count)) return juce::Result::fail(i18n::tr("La escritura del archivo falló. Comprueba el espacio libre."));
        if (progress && ! progress((double) done / (double) total)) return juce::Result::fail(i18n::tr("Conversión cancelada."));
    }
    return writer.finish();
}
}
