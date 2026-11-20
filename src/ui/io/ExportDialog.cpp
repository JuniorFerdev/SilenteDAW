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
    for (auto* c : std::initializer_list<juce::Component*> { &format, &name, &chooseFolder, &all, &none, &selectedOnly, &exportButton, &rangeBox, &perRange, &conflictBox, &folderLabel, &nameLabel, &rangeLabel, &conflictLabel, &status })
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
    name.setComponentID("export-name"); rangeBox.setComponentID("export-range"); perRange.setComponentID("export-per-range"); conflictBox.setComponentID("export-conflict");
    rangeLabel.setText(i18n::tr("Rango de exportación"), juce::dontSendNotification);
    conflictLabel.setText(i18n::tr("Si el archivo ya existe"), juce::dontSendNotification);
    for (auto* l : { &rangeLabel, &conflictLabel }) l->setColour(juce::Label::textColourId, muted);
    rangeBox.addItem(i18n::tr("Proyecto completo"), 1);
    rangeBox.addItem(i18n::tr("Entre localizadores izquierdo y derecho"), 2);
    rangeBox.addItem(i18n::tr("Marcadores de ciclo"), 3);
    rangeBox.setSelectedId(1, juce::dontSendNotification);
    rangeBox.onChange = [this] { updateRangeControls(); };
    perRange.setButtonText(i18n::tr("Un archivo por rango"));
    perRange.setToggleState(true, juce::dontSendNotification);
    conflictBox.addItem(i18n::tr("Preguntar"), 1);
    conflictBox.addItem(i18n::tr("Crear nombre único"), 2);
    conflictBox.addItem(i18n::tr("Sobrescribir"), 3);
    conflictBox.setSelectedId(1, juce::dontSendNotification);
    updateRangeControls();
    exportButton.setButtonText(i18n::tr("Exportar audio"));
    exportButton.onClick = [this] { startExport(); };
    status.setColour(juce::Label::textColourId, muted);
    status.setText(i18n::tr("Cada canal marcado se exporta en su propio archivo: «Nombre - Pista»."), juce::dontSendNotification);
    setSize(820, 520);
}

void ExportDialog::setFolder(const juce::File& f)
{
    folder = f; folderLabel.setText(folder.getFullPathName(), juce::dontSendNotification);
}

void ExportDialog::updateRangeControls()
{
    perRange.setVisible(rangeBox.getSelectedId() == 3);
    resized();
}

io::ExportJob ExportDialog::buildJob() const
{
    io::ExportJob job;
    job.folder = folder; job.baseName = name.getText().trim().isEmpty() ? engine.projectName : name.getText().trim();
    job.settings = format.settings(); job.mix = checked[0];
    if (rangeBox.getSelectedId() == 2) { job.useRange = true; job.rangeStart = engine.loopStart.load(); job.rangeEnd = engine.loopEnd.load(); }
    if (rangeBox.getSelectedId() == 3)
    {
        job.ranges = io::cycleMarkerRanges(engine);
        if (! perRange.getToggleState() && ! job.ranges.empty())
        {
            // One file from the first cycle's start to the last cycle's end.
            io::ExportRange all { {}, job.ranges.front().start, job.ranges.front().end };
            for (const auto& r : job.ranges) { all.start = juce::jmin(all.start, r.start); all.end = juce::jmax(all.end, r.end); }
            job.ranges = { all };
        }
    }
    for (size_t n = 1; n < channels.size(); ++n) if (checked[n]) job.trackIds.add(channels[n].first);
    job.conflict = conflictBox.getSelectedId() == 2 ? io::NameConflict::uniqueName : conflictBox.getSelectedId() == 3 ? io::NameConflict::overwrite : io::NameConflict::ask;
    return job;
}

void ExportDialog::startExport()
{
    auto job = buildJob();
    if (auto r = io::validate(job.settings); r.failed()) { status.setText(r.getErrorMessage(), juce::dontSendNotification); return; }
    if (! job.mix && job.trackIds.isEmpty()) { status.setText(i18n::tr("Selecciona al menos un canal para exportar."), juce::dontSendNotification); return; }
    if (rangeBox.getSelectedId() == 2 && ! (job.rangeEnd > job.rangeStart)) { status.setText(i18n::tr("El rango de localizadores no es válido."), juce::dontSendNotification); return; }
    if (rangeBox.getSelectedId() == 3 && job.ranges.empty()) { status.setText(i18n::tr("La pista de marcadores activa no tiene rangos de ciclo."), juce::dontSendNotification); return; }
    if (engine.recording.load()) { status.setText(i18n::tr("Detén la grabación antes de exportar."), juce::dontSendNotification); return; }
    if (job.conflict != io::NameConflict::ask) { run(job); return; }
    int existing = 0;
    for (const auto& f : io::plannedTargets(engine, job)) if (f.exists()) ++existing;
    if (existing == 0) { job.conflict = io::NameConflict::overwrite; run(job); return; }
    juce::AlertWindow::showAsync(juce::MessageBoxOptions().withIconType(juce::MessageBoxIconType::QuestionIcon)
                                     .withTitle(i18n::tr("El archivo ya existe"))
                                     .withMessage(juce::String(existing) + i18n::tr(" archivo(s) de destino ya existen en la carpeta."))
                                     .withButton(i18n::tr("Crear nombre único")).withButton(i18n::tr("Sobrescribir")).withButton(i18n::tr("Cancelar"))
                                     .withAssociatedComponent(this),
                                 [safe = juce::Component::SafePointer<ExportDialog>(this), job](int choice) mutable
    {
        if (! safe || choice == 2) return;
        job.conflict = choice == 0 ? io::NameConflict::uniqueName : io::NameConflict::overwrite;
        safe->run(job);
    });
}

void ExportDialog::run(io::ExportJob job)
{
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
    line = r.removeFromTop(28); rangeLabel.setBounds(line.removeFromLeft(170)); rangeBox.setBounds(line); r.removeFromTop(6);
    perRange.setBounds(rangeBox.getSelectedId() == 3 ? r.removeFromTop(26) : juce::Rectangle<int>()); r.removeFromTop(6);
    line = r.removeFromTop(28); conflictLabel.setBounds(line.removeFromLeft(170)); conflictBox.setBounds(line);
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
