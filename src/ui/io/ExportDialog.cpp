#include "ExportDialog.h"
#include "BackgroundJob.h"
#include "../../i18n/Translations.h"
#include "../../io/TrackExport.h"
#include "../theme/Drawing.h"

namespace silente::ui
{
using namespace theme;

ExportDialog::ExportDialog(AudioEngine& e, int selected) : engine(e), selectedTrack(selected)
{
    setLookAndFeel(&look);
    for (auto* c : std::initializer_list<juce::Component*> { &format, &name, &chooseFolder, &all, &none, &selectedOnly, &exportButton, &rangeOnly, &folderLabel, &nameLabel, &status })
        addAndMakeVisible(c);
    channels.push_back({ {}, i18n::tr("Stereo Out (mezcla)") });
    for (const auto& t : engine.tracks)
        if (t->hasAudioChannel()) channels.push_back({ t->id, t->name });
    checked.assign(channels.size(), false);
    checked[0] = true;
    folder = engine.projectFile.existsAsFile() ? engine.projectFile.getParentDirectory().getChildFile("Mixdown")
                                               : juce::File::getSpecialLocation(juce::File::userMusicDirectory).getChildFile("Silente/Mixdown");
    nameLabel.setText(i18n::tr("Nombre"), juce::dontSendNotification);
    name.setText(engine.projectName, juce::dontSendNotification);
    folderLabel.setColour(juce::Label::textColourId, ink);
    folderLabel.setText(folder.getFullPathName(), juce::dontSendNotification);
    chooseFolder.setButtonText(i18n::tr("Ruta…"));
    chooseFolder.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser>(i18n::tr("Carpeta de exportación"), folder, "", true);
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [safe = juce::Component::SafePointer<ExportDialog>(this)](const juce::FileChooser& fc)
        {
            if (! safe || fc.getResult() == juce::File()) return;
            safe->folder = fc.getResult(); safe->folderLabel.setText(safe->folder.getFullPathName(), juce::dontSendNotification);
        });
    };
    all.setButtonText(i18n::tr("Todos")); none.setButtonText(i18n::tr("Ninguno")); selectedOnly.setButtonText(i18n::tr("Pista seleccionada"));
    all.onClick = [this] { std::fill(checked.begin(), checked.end(), true); repaint(); };
    none.onClick = [this] { std::fill(checked.begin(), checked.end(), false); repaint(); };
    selectedOnly.onClick = [this]
    {
        std::fill(checked.begin(), checked.end(), false);
        if (juce::isPositiveAndBelow(selectedTrack, (int) engine.tracks.size()))
            for (size_t n = 1; n < channels.size(); ++n) if (channels[n].first == engine.tracks[(size_t) selectedTrack]->id) checked[n] = true;
        repaint();
    };
    rangeOnly.setButtonText(i18n::tr("Solo entre localizadores izquierdo y derecho"));
    exportButton.setButtonText(i18n::tr("Exportar audio"));
    exportButton.onClick = [this] { startExport(); };
    status.setColour(juce::Label::textColourId, muted);
    status.setText(i18n::tr("Cada canal marcado se exporta en su propio archivo: «Nombre - Pista»."), juce::dontSendNotification);
    setSize(820, 520);
}

void ExportDialog::startExport()
{
    io::ExportJob job;
    job.folder = folder; job.baseName = name.getText().trim().isEmpty() ? engine.projectName : name.getText().trim();
    job.settings = format.settings(); job.mix = checked[0];
    if (rangeOnly.getToggleState()) { job.useRange = true; job.rangeStart = engine.loopStart.load(); job.rangeEnd = engine.loopEnd.load(); }
    for (size_t n = 1; n < channels.size(); ++n) if (checked[n]) job.trackIds.add(channels[n].first);
    if (auto r = io::validate(job.settings); r.failed()) { status.setText(r.getErrorMessage(), juce::dontSendNotification); return; }
    if (! job.mix && job.trackIds.isEmpty()) { status.setText(i18n::tr("Selecciona al menos un canal para exportar."), juce::dontSendNotification); return; }
    if (engine.recording.load()) { status.setText(i18n::tr("Detén la grabación antes de exportar."), juce::dontSendNotification); return; }
    auto written = std::make_shared<juce::StringArray>();
    BackgroundJob::start(i18n::tr("Exportando audio…"), [this, job, written](const std::function<bool(double)>& progress)
    {
        return io::exportChannels(engine, job, progress, *written);
    },
    [safe = juce::Component::SafePointer<ExportDialog>(this), written](const juce::Result& r)
    {
        if (! safe) return;
        if (r.failed()) { safe->status.setText(r.getErrorMessage() == "cancel" ? i18n::tr("Exportación cancelada.") : r.getErrorMessage(), juce::dontSendNotification); return; }
        safe->status.setText(juce::String(written->size()) + i18n::tr(" archivo(s) exportado(s) en ") + safe->folder.getFullPathName(), juce::dontSendNotification);
        if (! written->isEmpty()) juce::File((*written)[0]).revealToUser();
    });
}

void ExportDialog::resized()
{
    auto r = getLocalBounds().reduced(16);
    auto footer = r.removeFromBottom(34);
    exportButton.setBounds(footer.removeFromRight(170));
    footer.removeFromRight(10);
    status.setBounds(footer);
    r.removeFromBottom(10);
    auto left = r.removeFromLeft(300);
    auto buttons = left.removeFromBottom(28);
    all.setBounds(buttons.removeFromLeft(70)); buttons.removeFromLeft(4); none.setBounds(buttons.removeFromLeft(80)); buttons.removeFromLeft(4); selectedOnly.setBounds(buttons);
    left.removeFromBottom(6);
    listArea = left.withTrimmedTop(26);
    r.removeFromLeft(20);
    r.removeFromTop(26);
    auto line = r.removeFromTop(28); nameLabel.setBounds(line.removeFromLeft(170)); name.setBounds(line); r.removeFromTop(6);
    line = r.removeFromTop(28); chooseFolder.setBounds(line.removeFromLeft(100)); line.removeFromLeft(8); folderLabel.setBounds(line); r.removeFromTop(28);
    format.setBounds(r.removeFromTop(110)); r.removeFromTop(8);
    rangeOnly.setBounds(r.removeFromTop(26));
}

void ExportDialog::paint(juce::Graphics& g)
{
    g.fillAll(panel);
    text(g, i18n::tr("Selección de canales"), { listArea.getX(), listArea.getY() - 24, listArea.getWidth(), 20 }, 12, ink);
    text(g, i18n::tr("Ubicación y formato de archivo"), { listArea.getRight() + 20, 16, 400, 20 }, 12, ink);
    g.setColour(bg); g.fillRoundedRectangle(listArea.toFloat(), 4.0f);
    int y = listArea.getY() + 4;
    for (size_t n = 0; n < channels.size() && y + rowHeight <= listArea.getBottom(); ++n, y += rowHeight)
    {
        g.setColour(raised); g.fillRoundedRectangle((float) listArea.getX() + 8.0f, (float) y + 5.0f, 14.0f, 14.0f, 3.0f);
        if (checked[n]) { g.setColour(accent); g.fillRoundedRectangle((float) listArea.getX() + 11.0f, (float) y + 8.0f, 8.0f, 8.0f, 2.0f); }
        juce::Colour colour = accent;
        if (n > 0) if (auto t = engine.findTrack(channels[n].first)) colour = t->colour;
        g.setColour(colour); g.fillRect(listArea.getX() + 30, y + 4, 4, 16);
        text(g, channels[n].second, { listArea.getX() + 40, y, listArea.getWidth() - 48, rowHeight }, 11, n == 0 ? ink : ink.withAlpha(.9f));
    }
}

void ExportDialog::mouseDown(const juce::MouseEvent& e)
{
    if (! listArea.contains(e.getPosition())) return;
    const int row = (e.y - listArea.getY() - 4) / rowHeight;
    if (juce::isPositiveAndBelow(row, (int) channels.size())) { checked[(size_t) row] = ! checked[(size_t) row]; repaint(listArea); }
}
}
