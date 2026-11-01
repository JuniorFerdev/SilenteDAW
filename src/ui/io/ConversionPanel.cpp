#include "ConversionPanel.h"
#include "BackgroundJob.h"
#include "../../i18n/Translations.h"
#include "../../io/VideoAudio.h"
#include "../theme/Drawing.h"

namespace silente::ui
{
using namespace theme;

ConversionPanel::ConversionPanel(juce::AudioFormatManager& f) : formats(f)
{
    setLookAndFeel(&look);
    for (auto* c : std::initializer_list<juce::Component*> { &format, &addButton, &addVideo, &removeButton, &clearButton, &chooseFolder, &convertButton, &sameFolder, &folderLabel, &status })
        addAndMakeVisible(c);
    addButton.setButtonText(i18n::tr("Añadir archivos…"));
    addVideo.setButtonText(i18n::tr("Audio de video…"));
    removeButton.setButtonText(i18n::tr("Quitar"));
    clearButton.setButtonText(i18n::tr("Vaciar"));
    chooseFolder.setButtonText(i18n::tr("Ruta…"));
    convertButton.setButtonText(i18n::tr("Convertir"));
    sameFolder.setButtonText(i18n::tr("Guardar junto al original"));
    sameFolder.setToggleState(true, juce::dontSendNotification);
    folder = juce::File::getSpecialLocation(juce::File::userMusicDirectory).getChildFile("Silente/Convertidos");
    folderLabel.setText(folder.getFullPathName(), juce::dontSendNotification);
    folderLabel.setColour(juce::Label::textColourId, muted);
    status.setColour(juce::Label::textColourId, muted);
    status.setText(i18n::tr("Añade audio (WAV, AIFF, FLAC, MP3, M4A, OGG…) o video (MP4, MOV, M4V) y elige el formato de salida."), juce::dontSendNotification);
    addButton.onClick = [this] { addFiles(false); };
    addVideo.onClick = [this] { addFiles(true); };
    removeButton.onClick = [this] { if (juce::isPositiveAndBelow(selectedRow, (int) items.size())) items.erase(items.begin() + selectedRow); selectedRow = -1; repaint(); };
    clearButton.onClick = [this] { items.clear(); selectedRow = -1; repaint(); };
    chooseFolder.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser>(i18n::tr("Carpeta de destino"), folder, "", true);
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [safe = juce::Component::SafePointer<ConversionPanel>(this)](const juce::FileChooser& fc)
        {
            if (! safe || fc.getResult() == juce::File()) return;
            safe->folder = fc.getResult(); safe->sameFolder.setToggleState(false, juce::dontSendNotification);
            safe->folderLabel.setText(safe->folder.getFullPathName(), juce::dontSendNotification);
        });
    };
    convertButton.onClick = [this] { startConversion(); };
    setSize(860, 540);
}

void ConversionPanel::addFiles(bool videoOnly)
{
    const auto wildcard = videoOnly ? io::videoWildcard() : io::readableWildcard(formats);
    chooser = std::make_unique<juce::FileChooser>(videoOnly ? i18n::tr("Extraer audio de video") : i18n::tr("Archivos para convertir"),
                                                  juce::File::getSpecialLocation(juce::File::userHomeDirectory), wildcard, true);
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectMultipleItems,
                         [safe = juce::Component::SafePointer<ConversionPanel>(this)](const juce::FileChooser& fc)
    {
        if (! safe) return;
        for (const auto& f : fc.getResults()) safe->items.push_back({ f, i18n::tr("Pendiente"), false });
        safe->repaint();
    });
}

void ConversionPanel::startConversion()
{
    const auto settings = format.settings();
    if (auto r = io::validate(settings); r.failed()) { status.setText(r.getErrorMessage(), juce::dontSendNotification); return; }
    if (items.empty()) { status.setText(i18n::tr("Añade al menos un archivo."), juce::dontSendNotification); return; }
    std::vector<std::pair<juce::File, juce::File>> work;
    for (const auto& item : items)
    {
        const auto dir = sameFolder.getToggleState() ? item.file.getParentDirectory() : folder;
        auto target = dir.getChildFile(item.file.getFileNameWithoutExtension() + io::extensionFor(settings.format));
        if (target == item.file || target.exists()) target = dir.getNonexistentChildFile(item.file.getFileNameWithoutExtension() + " (convertido)", io::extensionFor(settings.format), false);
        work.push_back({ item.file, target });
    }
    auto results = std::make_shared<std::vector<juce::String>>(work.size());
    BackgroundJob::start(i18n::tr("Convirtiendo…"), [this, work, settings, results](const std::function<bool(double)>& progress)
    {
        for (size_t n = 0; n < work.size(); ++n)
        {
            auto r = io::convertFile(formats, work[n].first, work[n].second, settings, [&](double p) { return progress(((double) n + p) / (double) work.size()); });
            (*results)[n] = r.wasOk() ? juce::String::fromUTF8("✓ ") + work[n].second.getFileName() : r.getErrorMessage();
            if (! progress(((double) n + 1.0) / (double) work.size())) return juce::Result::fail("cancel");
        }
        return juce::Result::ok();
    },
    [safe = juce::Component::SafePointer<ConversionPanel>(this), results](const juce::Result& r)
    {
        if (! safe) return;
        int converted = 0;
        for (size_t n = 0; n < results->size() && n < safe->items.size(); ++n)
            if ((*results)[n].isNotEmpty()) { safe->items[n].state = (*results)[n]; safe->items[n].ok = (*results)[n].startsWith(juce::String::fromUTF8("✓")); converted += safe->items[n].ok ? 1 : 0; }
        safe->status.setText(r.failed() && r.getErrorMessage() == "cancel" ? i18n::tr("Conversión cancelada.") : juce::String(converted) + i18n::tr(" archivo(s) convertido(s)."), juce::dontSendNotification);
        safe->repaint();
    });
}

void ConversionPanel::resized()
{
    auto r = getLocalBounds().reduced(16);
    auto footer = r.removeFromBottom(34);
    convertButton.setBounds(footer.removeFromRight(160)); footer.removeFromRight(10); status.setBounds(footer);
    r.removeFromBottom(10);
    auto right = r.removeFromRight(360);
    r.removeFromRight(16);
    auto buttons = r.removeFromBottom(28);
    addButton.setBounds(buttons.removeFromLeft(140)); buttons.removeFromLeft(6); addVideo.setBounds(buttons.removeFromLeft(130)); buttons.removeFromLeft(6);
    removeButton.setBounds(buttons.removeFromLeft(70)); buttons.removeFromLeft(6); clearButton.setBounds(buttons.removeFromLeft(70));
    r.removeFromBottom(6);
    listArea = r.withTrimmedTop(26);
    right.removeFromTop(26);
    format.setBounds(right.removeFromTop(110)); right.removeFromTop(14);
    sameFolder.setBounds(right.removeFromTop(26)); right.removeFromTop(6);
    auto line = right.removeFromTop(28); chooseFolder.setBounds(line.removeFromLeft(90)); line.removeFromLeft(8); folderLabel.setBounds(line);
}

void ConversionPanel::paint(juce::Graphics& g)
{
    g.fillAll(panel);
    text(g, i18n::tr("Archivos"), { listArea.getX(), listArea.getY() - 24, 300, 20 }, 12, ink);
    text(g, i18n::tr("Formato de salida"), { listArea.getRight() + 16, 16, 300, 20 }, 12, ink);
    g.setColour(bg); g.fillRoundedRectangle(listArea.toFloat(), 4.0f);
    if (items.empty()) text(g, i18n::tr("Sin archivos. Usa «Añadir archivos…» o «Audio de video…»."), listArea, 11, muted, juce::Justification::centred);
    int y = listArea.getY() + 4;
    for (size_t n = 0; n < items.size() && y + rowHeight <= listArea.getBottom(); ++n, y += rowHeight)
    {
        if ((int) n == selectedRow) { g.setColour(accent.withAlpha(.18f)); g.fillRect(listArea.getX() + 2, y, listArea.getWidth() - 4, rowHeight); }
        const bool video = io::isVideoFile(items[n].file);
        text(g, (video ? juce::String::fromUTF8("▶ ") : juce::String()) + items[n].file.getFileName(), { listArea.getX() + 10, y, listArea.getWidth() / 2, rowHeight }, 11, ink);
        text(g, items[n].state, { listArea.getX() + listArea.getWidth() / 2, y, listArea.getWidth() / 2 - 10, rowHeight }, 10, items[n].ok ? mint : muted, juce::Justification::centredRight);
    }
}

void ConversionPanel::mouseDown(const juce::MouseEvent& e)
{
    if (! listArea.contains(e.getPosition())) return;
    selectedRow = (e.y - listArea.getY() - 4) / rowHeight;
    repaint(listArea);
}
}
