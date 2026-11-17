// Copyright (C) 2026 Henrik Juhl. Part of BleepWerk, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// File and parameter helpers for the presets. (From SubForge, same author and licence.)
namespace bleepwerk::files
{

// The user's files with the given extension in a folder, sorted by name.
inline juce::Array<juce::File> listSorted (const juce::File& folder, const char* extension)
{
    auto files = folder.findChildFiles (juce::File::findFiles, false, juce::String ("*") + extension);
    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b)
               { return a.getFileNameWithoutExtension().compareNatural (b.getFileNameWithoutExtension()) < 0; });
    return files;
}

inline juce::File fileFor (const juce::File& folder, const juce::String& name, const char* extension)
{
    return folder.getChildFile (juce::File::createLegalFileName (name.trim()) + extension);
}

// A file that does not exist yet: the name as it is, else "name 2", "name 3" and so on, so an
// earlier file is never replaced.
inline juce::File uniqueFileFor (const juce::File& folder, const juce::String& name, const char* extension)
{
    auto file = fileFor (folder, name, extension);
    for (int n = 2; file.exists(); ++n)
        file = fileFor (folder, name + " " + juce::String (n), extension);
    return file;
}

// JUCE's XML parser goes one level deeper on the stack for each nested element, so a damaged or
// hostile file nested tens of thousands deep crashes the host, and a huge one stalls it. Real
// projects and presets are a few kB with about sixty elements; far beyond that is refused.
constexpr size_t kMaxXmlBytes = 1 << 20;
constexpr int kMaxXmlElements = 1000; // each element starts with '<', so this also limits the nesting

inline bool isPlausibleXml (const void* data, size_t numBytes)
{
    const auto* text = static_cast<const char*> (data);
    return numBytes <= kMaxXmlBytes && std::count (text, text + numBytes, '<') <= kMaxXmlElements;
}

// juce::parseXML for preset files, or nullptr if the file is not plausible.
inline std::unique_ptr<juce::XmlElement> parseXmlFile (const juce::File& file)
{
    juce::MemoryBlock data;
    if (file.getSize() > (juce::int64) kMaxXmlBytes || ! file.loadFileAsData (data) || ! isPlausibleXml (data.getData(), data.getSize()))
        return nullptr;
    return juce::parseXML (juce::String::createStringFromData (data.getData(), (int) data.getSize()));
}

// Per-user preferences, not per project (the colour theme): ~/.config/<name>/<name>.settings on
// Linux. (From SubForge.)
inline juce::PropertiesFile::Options settingsOptions (const juce::String& name)
{
    juce::PropertiesFile::Options options;
    options.applicationName = name;
   #if JUCE_LINUX
    options.folderName = ".config/" + name; // JUCE puts Linux settings straight under ~ otherwise
   #else
    options.folderName = name;
   #endif
    options.filenameSuffix = "settings";
    options.osxLibrarySubFolder = "Application Support";
    return options;
}

// The plug-in was called BleepForge before its release. These are the folders it then used
// (the only place the old name is kept), copied once to the new name so the user's own presets,
// patterns and theme follow.
inline const char* legacyName() { return "BleepForge"; }
inline juce::File documentsFolder (const juce::String& name)
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile (name);
}
inline juce::File settingsFile (const juce::String& name) { return settingsOptions (name).getDefaultFile(); }

// Copies a folder under the old name to the new one if the new one does not exist yet; never
// touches the old one or overwrites anything. Message thread only (file I/O).
inline void migrateLegacyUserData (const juce::String& newName)
{
    const auto oldDocs = documentsFolder (legacyName()), newDocs = documentsFolder (newName);
    if (oldDocs.isDirectory() && ! newDocs.exists())
        oldDocs.copyDirectoryTo (newDocs);

    const auto oldSettings = settingsFile (legacyName()), newSettings = settingsFile (newName);
    const auto oldFolder = oldSettings.getParentDirectory(), newFolder = newSettings.getParentDirectory();
    if (oldSettings.existsAsFile() && ! newFolder.exists() && oldFolder.copyDirectoryTo (newFolder))
        newFolder.getChildFile (oldSettings.getFileName()).moveFileTo (newSettings);
}

// Sets a parameter as one host-visible gesture.
inline void setParameterValue (juce::RangedAudioParameter& param, float value)
{
    param.beginChangeGesture();
    param.setValueNotifyingHost (param.convertTo0to1 (value));
    param.endChangeGesture();
}

// Shows a folder in the file manager. On Linux, JUCE's File::startAsProcess() hands the path to
// a shell, which runs "$(...)" in a folder name; here xdg-open gets it as an argument.
inline bool openFolder (const juce::File& folder)
{
   #if JUCE_LINUX
    juce::ChildProcess opener; // xdg-open keeps running after this goes
    return folder.isDirectory() && opener.start (juce::StringArray { "xdg-open", folder.getFullPathName() }, 0);
   #else
    return folder.startAsProcess();
   #endif
}

// JUCE's Linux File::moveToTrash() skips the .trashinfo file, so file managers
// can't show or restore the file. This follows the freedesktop.org trash spec.
inline bool moveToTrash (const juce::File& file)
{
   #if JUCE_LINUX
    const auto dataHome = juce::SystemStats::getEnvironmentVariable ("XDG_DATA_HOME", {});
    const auto trash = (dataHome.isNotEmpty() ? juce::File (dataHome)
                                              : juce::File::getSpecialLocation (juce::File::userHomeDirectory)
                                                    .getChildFile (".local/share"))
                           .getChildFile ("Trash");
    const auto files = trash.getChildFile ("files"), info = trash.getChildFile ("info");

    if (files.createDirectory() && info.createDirectory())
    {
        const auto target = files.getNonexistentChildFile (file.getFileNameWithoutExtension(), file.getFileExtension(), false);
        const auto infoFile = info.getChildFile (target.getFileName() + ".trashinfo");
        auto segments = juce::StringArray::fromTokens (file.getFullPathName(), "/", {});
        for (auto& segment : segments)
            segment = juce::URL::addEscapeChars (segment, false); // escape the parts, keep the slashes
        const auto infoText = "[Trash Info]\nPath=" + segments.joinIntoString ("/")
                              + "\nDeletionDate=" + juce::Time::getCurrentTime().formatted ("%Y-%m-%dT%H:%M:%S") + "\n";

        if (infoFile.replaceWithText (infoText))
        {
            if (file.moveFileTo (target))
                return true;
            infoFile.deleteFile();
        }
    }
   #endif
    return file.moveToTrash();
}

} // namespace bleepwerk::files
