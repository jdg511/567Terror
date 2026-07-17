// ============================================================================
//  AudioFilePlayer.h — simple in-memory audio file player.
//  Files are loaded on the message thread into a mono buffer; the audio
//  thread reads with linear interpolation (sample-rate converted) under a
//  try-lock so loading never blocks audio.
// ============================================================================
#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

class AudioFilePlayer
{
public:
    void prepare (double sampleRateToUse) noexcept
    {
        hostRate = sampleRateToUse;
    }

    // message thread only; returns false if the file couldn't be read
    bool load (const juce::File& file, juce::AudioFormatManager& formatManager)
    {
        std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
        if (reader == nullptr || reader->lengthInSamples <= 0)
            return false;

        // cap at 10 minutes to bound memory
        const auto len = (int) juce::jmin (reader->lengthInSamples,
                                           (juce::int64) (reader->sampleRate * 600.0));

        juce::AudioBuffer<float> temp ((int) reader->numChannels, len);
        reader->read (&temp, 0, len, 0, true, true);

        juce::AudioBuffer<float> monoBuf (1, len);
        monoBuf.clear();
        const float scale = 1.0f / (float) juce::jmax (1, (int) reader->numChannels);
        for (int ch = 0; ch < (int) reader->numChannels; ++ch)
            monoBuf.addFrom (0, 0, temp, ch, 0, len, scale);

        {
            const juce::SpinLock::ScopedLockType sl (lock);
            buffer   = std::move (monoBuf);
            fileRate = reader->sampleRate;
            position = 0.0;
        }
        filePath = file.getFullPathName();
        return true;
    }

    void restart() noexcept
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        position = 0.0;
    }

    bool hasFile() const noexcept    { return filePath.isNotEmpty(); }
    juce::String getFilePath() const { return filePath; }

    // audio thread: render `num` samples into dest (replacing). Returns true
    // if the player reached the end of a non-looping file during this block.
    bool process (float* dest, int num, bool playing, bool loop) noexcept
    {
        juce::FloatVectorOperations::clear (dest, num);
        if (! playing)
            return false;

        const juce::SpinLock::ScopedTryLockType tl (lock);
        if (! tl.isLocked() || buffer.getNumSamples() < 2 || hostRate <= 0.0)
            return false;

        const float* src = buffer.getReadPointer (0);
        const int    n   = buffer.getNumSamples();
        const double step = fileRate / hostRate;
        bool finished = false;

        for (int i = 0; i < num; ++i)
        {
            if (position >= (double) (n - 1))
            {
                if (loop)
                {
                    position = 0.0;
                }
                else
                {
                    finished = true;
                    break;
                }
            }
            const int    idx  = (int) position;
            const float  frac = (float) (position - (double) idx);
            dest[i] = src[idx] + frac * (src[idx + 1] - src[idx]);
            position += step;
        }
        return finished;
    }

private:
    juce::SpinLock lock;
    juce::AudioBuffer<float> buffer;
    juce::String filePath;
    double fileRate = 44100.0, hostRate = 48000.0;
    double position = 0.0;
};
