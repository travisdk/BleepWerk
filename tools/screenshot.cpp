// Copyright (C) 2026 Henrik Juhl. Part of BleepForge, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

// Renders the interface of a fresh instance to a PNG, for checking the layout and for the
// README: BleepForge_Screenshot <output.png> [scale] [velocity|ratchet] [foley] [range=<octaves>] [note=<bottom note>]

#include "MainView.h"
#include "Theme.h"

int main (int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "usage: BleepForge_Screenshot <output.png> [scale] [velocity|ratchet] [foley] [range=<octaves>] [note=<bottom note>]\n";
        return 1;
    }

    const juce::ScopedJuceInitialiser_GUI gui;
    const std::unique_ptr<juce::AudioProcessor> plugin (createPluginFilter());
    auto& processor = dynamic_cast<BleepForgeProcessor&> (*plugin);
    bleepforge::theme::LookAndFeel lookAndFeel;
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
    const float scale = argc > 2 ? juce::String (argv[2]).getFloatValue() : 1.0f;
    for (int i = 3; i < argc; ++i)
    {
        const juce::String option (argv[i]);
        if (option == "velocity")
            processor.editMode = BleepForgeProcessor::EditMode::Velocity;
        else if (option == "ratchet")
            processor.editMode = BleepForgeProcessor::EditMode::Ratchet;
        else if (option == "foley")
            processor.apvts.getParameter ("engine")->setValueNotifyingHost (1.0f);
        else if (option.startsWith ("range="))
        {
            auto* range = processor.apvts.getParameter ("seqRange");
            range->setValueNotifyingHost (range->convertTo0to1 (option.fromFirstOccurrenceOf ("=", false, false).getFloatValue()));
        }
        else if (option.startsWith ("note="))
            processor.bottomNote = option.fromFirstOccurrenceOf ("=", false, false).getIntValue();
    }

    auto view = std::make_unique<MainView> (processor);
    const auto image = view->createComponentSnapshot (view->getLocalBounds(), true, scale);
    view.reset();
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
