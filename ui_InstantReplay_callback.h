#include "ObsChoice.h"
#pragma once
#include "MatchPanel.h"
#include "ClipPanel.h"
#include "UpdatePanel.h"
#include <QTabWidget>
#include <QScrollArea>
#include <QtCore/QVariant>
#include <QtGui/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QLineEdit>

QT_BEGIN_NAMESPACE

class Ui_replayCallout
{
public:
	QGridLayout *replayLayout;
    MatchPanel *matches;
    ClipPanel *library;
    UpdatePanel *updates;

	QLabel *enableLabel;
	QCheckBox *enableCheck;

	QLabel *waitLabel;
	QSpinBox *waitSpinBox;
	QLabel *pauseLabel;
	QSpinBox *pauseSpinBox;
	QLabel *replayLabel;
	QSpinBox *replaySpinBox;

	QLabel *sceneLabel;
	ObsChoice *sceneLineEdit;
	QLabel *sourceLabel;
	QLineEdit *sourceLineEdit;

	QLabel *muteDLabel;
	QCheckBox *muteDCheck;
	QLabel *muteALabel;
	QCheckBox *muteACheck;
    ObsChoice *desktopMuteSource;
    ObsChoice *micMuteSource;

	QLabel *enableBackupLabel;
	QCheckBox *enableBackupCheck;
	QLabel *filePathBackupLabel;
	QLineEdit *filePathBackupEdit;

	QLabel *replaySourceNameLabel;
	ObsChoice *replaySourceNameEdit;

	QDialogButtonBox *closeButton;
	QLineEdit *replayFolderEdit;
	QPushButton *replayBrowse, *backupBrowse;
	QLabel *statusLabel;
    QCheckBox *autoSwitchCheck;
    QPushButton *playButton;
    QSpinBox *clipSeconds[3];

	void setupUi(QDialog *replayCallout)
	{
		if(replayCallout->objectName().isEmpty())
			replayCallout->setObjectName(QStringLiteral("InstantReplay"));
		replayCallout->resize(300, 300);
		auto *outer = new QVBoxLayout(replayCallout);
        auto *tabs = new QTabWidget(replayCallout); outer->addWidget(tabs);
        auto *general = new QWidget;
        auto *scroll = new QScrollArea; scroll->setWidgetResizable(true); scroll->setWidget(general);
        tabs->addTab(scroll, "Replay settings");
        auto *hotkeys = new QWidget;
        auto *hotkeyLayout = new QVBoxLayout(hotkeys);
        auto *description = new QLabel("Save the latest part of the replay buffer. Assign keys in OBS Settings > Hotkeys under Instant Replay XT: Save clip preset 1, 2 and 3.");
        description->setWordWrap(true); hotkeyLayout->addWidget(description);
        const int defaults[] = {15, 30, 60};
        for (int i = 0; i < 3; ++i) {
            auto *row = new QHBoxLayout;
            row->addWidget(new QLabel(QString("Clip preset %1").arg(i + 1)));
            clipSeconds[i] = new QSpinBox;
            clipSeconds[i]->setObjectName(QString("clipSeconds%1").arg(i + 1));
            clipSeconds[i]->setRange(1, 3600); clipSeconds[i]->setSuffix(" seconds");
            clipSeconds[i]->setValue(defaults[i]); row->addWidget(clipSeconds[i]);
            hotkeyLayout->addLayout(row);
        }
        auto *note = new QLabel("Fast saving keeps the original video quality and audio tracks. Clips start at a clean keyframe, so they may run longer by up to one keyframe interval. A 1–2 second recording keyframe interval keeps lengths closer.\n\nSet OBS Replay Buffer's maximum replay time at least as high as your longest preset, and allow it to collect enough video. If less history is available, the clip will be shorter.\n\nThe original Save replay hotkey still saves the full buffer. Presets use the same replay folder, active match backup folder and automatic scene-switch setting.");
        note->setWordWrap(true); hotkeyLayout->addWidget(note); hotkeyLayout->addStretch();
        tabs->addTab(hotkeys, "Clip hotkeys");
        matches = new MatchPanel;
        tabs->addTab(matches, "Matches / Playlist");
        library = new ClipPanel;
        tabs->addTab(library, "Clip library");
        auto *saveRow = new QHBoxLayout;
        auto *saveNote = new QLabel("Changes save to a separate config file as you edit.", replayCallout);
        saveNote->setObjectName("settingsSaveStatus"); saveNote->setWordWrap(true); saveRow->addWidget(saveNote, 1);
        auto *saveSettings = new QPushButton("Save settings", replayCallout);
        saveSettings->setObjectName("saveSettingsButton"); saveRow->addWidget(saveSettings);
        auto *exportConfig = new QPushButton("Export config…", replayCallout);
        exportConfig->setObjectName("exportConfigButton"); saveRow->addWidget(exportConfig);
        auto *importConfig = new QPushButton("Import config…", replayCallout);
        importConfig->setObjectName("importConfigButton"); saveRow->addWidget(importConfig);
        exportConfig->setToolTip("Export this collection's plugin settings and match folders. Clips, OBS scenes and hotkeys are not included.");
        importConfig->setToolTip("Replace this collection's plugin settings from an exported config. Review paths and scene/source names after transferring computers.");
        outer->addLayout(saveRow);
        updates = new UpdatePanel;
        tabs->addTab(updates, "Updates");
        replayCallout->resize(800, 720);
        replayLayout = new QGridLayout(general);
		replayLayout->setObjectName(QStringLiteral("replayLayout"));

		enableLabel = new QLabel(replayCallout);
		enableLabel->setObjectName(QStringLiteral("enableLabel"));
		replayLayout->addWidget(enableLabel, 0, 1, 1, 1);

		enableCheck = new QCheckBox(replayCallout);
		enableCheck->setObjectName(QStringLiteral("enableCheck"));
		enableCheck->setChecked(false);
		replayLayout->addWidget(enableCheck, 0, 2, 1, 1);

		waitLabel = new QLabel(replayCallout);
		waitLabel->setObjectName(QStringLiteral("waitLabel"));
		replayLayout->addWidget(waitLabel, 1, 1, 1, 1);

		waitSpinBox = new QSpinBox(replayCallout);
		waitSpinBox->setObjectName(QStringLiteral("waitSpinBox"));
		waitSpinBox->setMinimum(0);
		waitSpinBox->setMaximum(60);
		waitSpinBox->setValue(10);
		replayLayout->addWidget(waitSpinBox, 1, 2, 1, 1);

		pauseLabel = new QLabel(replayCallout);
		pauseLabel->setObjectName(QStringLiteral("pauseLabel"));
		replayLayout->addWidget(pauseLabel, 2, 1, 1, 1);

		pauseSpinBox = new QSpinBox(replayCallout);
		pauseSpinBox->setObjectName(QStringLiteral("pauseSpinBox"));
		pauseSpinBox->setMinimum(0);
		pauseSpinBox->setMaximum(60000);
		pauseSpinBox->setValue(2300);
		replayLayout->addWidget(pauseSpinBox, 2, 2, 1, 1);

		replayLabel = new QLabel(replayCallout);
		replayLabel->setObjectName(QStringLiteral("replayLabel"));
		replayLayout->addWidget(replayLabel, 3, 1, 1, 1);

		replaySpinBox = new QSpinBox(replayCallout);
		replaySpinBox->setObjectName(QStringLiteral("replaySpinBox"));
		replaySpinBox->setMinimum(0);
		replaySpinBox->setMaximum(600);
		replaySpinBox->setValue(19);
		replayLayout->addWidget(replaySpinBox, 3, 2, 1, 1);

		sceneLabel = new QLabel(replayCallout);
		sceneLabel->setObjectName(QStringLiteral("sceneLabel"));
		replayLayout->addWidget(sceneLabel, 4, 1, 1, 1);

		sceneLineEdit = new ObsChoice(replayCallout);
		sceneLineEdit->setObjectName(QStringLiteral("sceneLineEdit"));
		sceneLineEdit->setText("Replay Scene");
		replayLayout->addWidget(sceneLineEdit, 4, 2, 1, 2);

		muteDLabel = new QLabel(replayCallout);
		muteDLabel->setObjectName(QStringLiteral("muteDLabel"));
		replayLayout->addWidget(muteDLabel, 5, 1, 1, 1);

		muteDCheck = new QCheckBox(replayCallout);
		muteDCheck->setObjectName(QStringLiteral("muteDCheck"));
		muteDCheck->setChecked(true);
		replayLayout->addWidget(muteDCheck, 5, 2);
        desktopMuteSource = new ObsChoice(replayCallout);
        desktopMuteSource->setObjectName("desktopMuteSource");
        desktopMuteSource->setAccessibleName("Desktop audio source to mute");
        desktopMuteSource->emptyLabel = "Automatic: OBS desktop audio channels";
        replayLayout->addWidget(desktopMuteSource, 5, 3);

		muteALabel = new QLabel(replayCallout);
		muteALabel->setObjectName(QStringLiteral("muteALabel"));
		replayLayout->addWidget(muteALabel, 6, 1, 1, 1);

		muteACheck = new QCheckBox(replayCallout);
		muteACheck->setObjectName(QStringLiteral("muteACheck"));
		muteACheck->setChecked(false);
		replayLayout->addWidget(muteACheck, 6, 2);
        micMuteSource = new ObsChoice(replayCallout);
        micMuteSource->setObjectName("micMuteSource");
        micMuteSource->setAccessibleName("Microphone audio source to mute");
        micMuteSource->emptyLabel = "Automatic: OBS mic/aux channels";
        replayLayout->addWidget(micMuteSource, 6, 3);

		enableBackupLabel = new QLabel(replayCallout);
		enableBackupLabel->setObjectName(QStringLiteral("enableBackupLabel"));
		replayLayout->addWidget(enableBackupLabel, 7, 1, 1, 1);

		enableBackupCheck = new QCheckBox(replayCallout);
		enableBackupCheck->setObjectName(QStringLiteral("enableBackupCheck"));
		enableBackupCheck->setChecked(false);
		replayLayout->addWidget(enableBackupCheck, 7, 2, 1, 2);

		filePathBackupLabel = new QLabel(replayCallout);
		filePathBackupLabel->setObjectName(QStringLiteral("filePathBackupLabel"));
		replayLayout->addWidget(filePathBackupLabel, 8, 1, 1, 1);

		filePathBackupEdit = new QLineEdit(replayCallout);
		filePathBackupEdit->setObjectName(QStringLiteral("filePathBackupEdit"));
		filePathBackupEdit->setPlaceholderText("Choose a backup folder");
		replayLayout->addWidget(filePathBackupEdit, 8, 2);
		backupBrowse = new QPushButton("Browse…", replayCallout);
		replayLayout->addWidget(backupBrowse, 8, 3);

		replaySourceNameLabel = new QLabel(replayCallout);
		replaySourceNameLabel->setObjectName(QStringLiteral("replaySourceNameLabel"));
		replayLayout->addWidget(replaySourceNameLabel, 9, 1, 1, 1);

		replaySourceNameEdit = new ObsChoice(replayCallout);
		replaySourceNameEdit->setObjectName(QStringLiteral("replaySourceNameEdit"));
		replaySourceNameEdit->setText("Instant Replay Media Source");
		replayLayout->addWidget(replaySourceNameEdit, 9, 2, 1, 2);


		replayLayout->addWidget(new QLabel("Replay folder", replayCallout), 10, 1);
        replayFolderEdit = new QLineEdit(replayCallout);
        replayFolderEdit->setPlaceholderText("Blank: use OBS recording folder");
        replayLayout->addWidget(replayFolderEdit, 10, 2);
        replayBrowse = new QPushButton("Browse…", replayCallout);
        replayLayout->addWidget(replayBrowse, 10, 3);
        auto *hint = new QLabel("Main file: InstantReplay + video extension (replaced each save).\nBackups: InstantReplay_1, InstantReplay_2, …\nStart OBS Replay Buffer before using the Save replay hotkey.", replayCallout);
        hint->setWordWrap(true);
        replayLayout->addWidget(hint, 11, 1, 1, 3);
        statusLabel = new QLabel("Ready. Save a clip, then use Play latest replay or enter the replay scene.", replayCallout);
        statusLabel->setWordWrap(true);
        statusLabel->setTextFormat(Qt::PlainText);
        statusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        replayLayout->addWidget(statusLabel, 12, 1, 1, 3);
        replayCallout->setMinimumWidth(600);
        autoSwitchCheck = new QCheckBox("Automatically switch to replay scene after saving", replayCallout);
        autoSwitchCheck->setObjectName("autoSwitchCheck");
        autoSwitchCheck->setChecked(false);
        replayLayout->addWidget(autoSwitchCheck, 13, 1, 1, 3);
        playButton = new QPushButton("Play latest replay", replayCallout);
        replayLayout->addWidget(playButton, 14, 1, 1, 2);
        closeButton = new QDialogButtonBox(replayCallout);
		closeButton->setObjectName(QStringLiteral("buttonBox"));
		closeButton->setStandardButtons(QDialogButtonBox::Close);
		replayLayout->addWidget(closeButton, 14, 3, 1, 1);

		retranslateUi(replayCallout);

		QMetaObject::connectSlotsByName(replayCallout);
	}

	void retranslateUi(QDialog *replayCallout)
	{
		replayCallout->setWindowTitle("Instant Replay XT 2.0.21");
		enableLabel->setText(QApplication::translate("replayCallout", "Enable", Q_NULLPTR));
		waitLabel->setText(QApplication::translate("replayCallout", "WritePause", Q_NULLPTR));
		pauseLabel->setText(QApplication::translate("replayCallout", "TransitionPause", Q_NULLPTR));
		replayLabel->setText(QApplication::translate("replayCallout", "ReplayLength", Q_NULLPTR));
		sceneLabel->setText(QApplication::translate("replayCallout", "ReplayScene", Q_NULLPTR));
		muteDLabel->setText(QApplication::translate("replayCallout", "MuteDesktop", Q_NULLPTR));
		muteALabel->setText(QApplication::translate("replayCallout", "MuteAux", Q_NULLPTR));
		enableBackupLabel->setText(QApplication::translate("replayCallout", "EnableBackup", Q_NULLPTR));
		filePathBackupLabel->setText(QApplication::translate("replayCallout", "BackupFilePath", Q_NULLPTR));
		replaySourceNameLabel->setText(QApplication::translate("replayCallout", "ReplaySourceName", Q_NULLPTR));
	}
};

namespace Ui {
	class replayCallout : public Ui_replayCallout {};
} // namespace Ui

QT_END_NAMESPACE
