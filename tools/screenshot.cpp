// Copyright (C) 2026 Henrik Juhl. Part of BleepWerk, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

// Renders the interface of a fresh instance to a PNG, for checking the layout and for the
// README: BleepWerk_Screenshot <output.png> [scale] [werk] [seq] [held] [picker[=text]] [about]

#include "MainView.h"
#include "Theme.h"
#include "PresetPicker.h"

namespace
{
// A host that plays, so the sequencer runs and a key played is taken as the root
struct PlayingHost : juce::AudioPlayHead
{
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info;
        info.setPpqPosition (0.0);
        info.setIsPlaying (true);
        return info;
    }
};
} // namespace

int main (int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "usage: BleepWerk_Screenshot <output.png> [scale] [werk] [seq] [held] [picker[=text]] [about]\n";
        return 1;
    }

    const juce::ScopedJuceInitialiser_GUI gui;
    const std::unique_ptr<juce::AudioProcessor> plugin (createPluginFilter());
    auto& processor = dynamic_cast<BleepWerkProcessor&> (*plugin);
    bleepwerk::theme::LookAndFeel lookAndFeel;
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
    const float scale = argc > 2 ? juce::String (argv[2]).getFloatValue() : 1.0f;
    bool wantsAbout = false, wantsPicker = false;
    juce::String pickerText;
    PlayingHost host;
    for (int i = 3; i < argc; ++i)
    {
        const juce::String option (argv[i]);
        if (option == "werk")
            processor.apvts.getParameter ("engine")->setValueNotifyingHost (1.0f);
        else if (option.startsWith ("picker")) // the preset list, with the text after "picker=" typed in the search
            pickerText = option.fromFirstOccurrenceOf ("=", false, false), wantsPicker = true;
        else if (option == "about")
            wantsAbout = true;
        else if (option == "seq")
            processor.apvts.getParameter ("seqEnabled")->setValueNotifyingHost (1.0f);
        else if (option == "held") // G3 held while the sequencer plays
        {
            processor.setRateAndBufferSizeDetails (48000.0, 512);
            processor.prepareToPlay (48000.0, 512);
            processor.setPlayHead (&host);
            juce::AudioBuffer<float> buffer (2, 512);
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 67, 0.8f), 0);
            processor.processBlock (buffer, midi);
        }
    }

    if (wantsPicker)
    {
        PresetPicker picker (processor.getSoundPresets());
        if (pickerText.isNotEmpty())
            picker.setSearchText (pickerText);
        const auto image = picker.createComponentSnapshot (picker.getLocalBounds(), true, scale);
        const juce::File file = juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]);
        file.deleteFile();
        juce::FileOutputStream out (file);
        juce::PNGImageFormat().writeImageToStream (image, out);
        std::cout << "wrote " << file.getFullPathName() << "\n";
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
        return 0;
    }

    auto view = std::make_unique<MainView> (processor);
    view->setSize (MainView::kWidth, view->designHeight());
    if (wantsAbout)
        view->showAbout();
    const auto image = view->createComponentSnapshot (view->getLocalBounds(), true, scale);
    view.reset();
    processor.setPlayHead (nullptr);
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);

    const juce::File file = juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]);
    file.deleteFile();
    juce::FileOutputStream out (file);
    if (! out.openedOk() || ! juce::PNGImageFormat().writeImageToStream (image, out))
    {
        std::cerr << "could not write " << file.getFullPathName() << "\n";
        return 1;
    }
    std::cout << "wrote " << file.getFullPathName() << " (" << image.getWidth() << " x " << image.getHeight() << ")\n";
    return 0;
}
