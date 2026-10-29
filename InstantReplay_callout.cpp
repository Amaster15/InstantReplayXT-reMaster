#include "InstantReplay_callout.h"
#include "InstantReplay.h"
#include "ReplayMode.h"
#include "SettingsStore.h"
#include <obs-frontend-api.h>
#include <obs-module.h>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMessageBox>
#include <QtGui/QAction>

#include <QPointer>
#include <QCloseEvent>
#include <QHideEvent>
#include <QFileDialog>

QPointer<replayCallout> co;
static QPointer<QAction> settingsAction;
static void save_replay_callout(obs_data_t *, bool, void *);

bool get_enabled()
{
	return co->ui->enableCheck->isChecked();
}

bool get_auto_switch()
{
    return co->ui->autoSwitchCheck->isChecked();
}

int get_wait()
{
	return co->ui->waitSpinBox->value() * 1000;
}

int get_pause()
{
	return co->ui->pauseSpinBox->value();
}

int get_replay()
{
	return co->ui->replaySpinBox->value() * 1000;
}

std::string get_scene()
{
	return co->ui->sceneLineEdit->text().toStdString();
}

bool get_D_mute()
{
	return co->ui->muteDCheck->isChecked();
}

bool get_A_mute()
{
	return co->ui->muteACheck->isChecked();
}

bool get_backup_enabled()
{
	return co->ui->enableBackupCheck->isChecked();
}

std::string get_backup_path()
{
	return co->ui->matches->enabled->isChecked() ? co->ui->matches->activePath().toStdString() : co->ui->filePathBackupEdit->text().trimmed().toStdString();
}

std::string get_replay_source_name()
{
	return (co->ui->replaySourceNameEdit->text() + (co->ui->library->playbackMode->currentIndex() == 1 ? " (VLC live)" : "")).toStdString();
}


std::string get_replay_path()
{
    return co->ui->replayFolderEdit->text().trimmed().toStdString();
}

void replay_backup_saved() { if (co) co->ui->matches->backupSaved(); }

void replay_status(const std::string &message)
{
    blog(LOG_INFO, "[InstantReplayXT] %s", message.c_str());
    if (co) { co->ui->statusLabel->setText(QString::fromUtf8(message.c_str())); co->ui->library->status->setText(QString::fromUtf8(message.c_str())); }
}


replayCallout::replayCallout(QWidget *parent)
	: QDialog(parent),
	ui(new Ui_replayCallout)
{
	ui->setupUi(this);
    if (obs_current_module()) {
        ui->updates->modulePath = QString::fromUtf8(obs_get_module_binary_path(obs_current_module()));
#ifdef Q_OS_WIN
        char *helper = obs_module_file("updater/install-after-obs.ps1");
#else
        char *helper = obs_module_file("updater/install-after-obs.py");
#endif
        if (helper) ui->updates->helperPath = QString::fromUtf8(helper);
        bfree(helper);
    }
    if (obs_current_module()) {
        char *path = obs_module_config_path("settings.json");
        if (path) setProperty("irxtConfigPath", QString::fromUtf8(path));
        bfree(path);
    }
    ui->sceneLineEdit->options = [] { return obsNames(); };
    ui->replaySourceNameEdit->options = [this] { return replaySourceNames(ui->sceneLineEdit->text()); };
    ui->sceneLineEdit->refresh(); ui->replaySourceNameEdit->refresh();
    settingsSaveTimer = new QTimer(this);
    settingsSaveTimer->setSingleShot(true); settingsSaveTimer->setInterval(500);
    connect(settingsSaveTimer, &QTimer::timeout, this, &replayCallout::saveSettings);
    for (auto *field : findChildren<QLineEdit *>())
        connect(field, &QLineEdit::textChanged, this, &replayCallout::requestSettingsSave);
    for (auto *check : findChildren<QCheckBox *>())
        connect(check, &QCheckBox::toggled, this, &replayCallout::requestSettingsSave);
    for (auto *number : findChildren<QSpinBox *>())
        connect(number, qOverload<int>(&QSpinBox::valueChanged), this, &replayCallout::requestSettingsSave);
    for (auto *choice : {ui->sceneLineEdit, ui->replaySourceNameEdit, ui->matches->vlcName})
        connect(choice, qOverload<int>(&QComboBox::currentIndexChanged), this, &replayCallout::requestSettingsSave);
    connect(ui->sceneLineEdit, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] { ui->replaySourceNameEdit->refresh(); });
    connect(ui->library->speed, &QSlider::valueChanged, this, &replayCallout::requestSettingsSave);
    ui->matches->settingsChanged = [this] { requestSettingsSave(); };
    connect(findChild<QPushButton *>("saveSettingsButton"), &QPushButton::clicked, this, [this] {
        saveSettings();
    });
    ui->library->folders = [this] {
        QList<QPair<QString, QString>> result;
        result.append({"Backup folder", ui->filePathBackupEdit->text().trimmed()});
        for (int row = 0; row < ui->matches->folders->rowCount(); ++row)
            result.append({ui->matches->folders->item(row, 1)->text(), ui->matches->folders->item(row, 2)->text().trimmed()});
        return result;
    };
    ui->library->latest = [this] {
        auto current = replay_current_file();
        if (!current.empty()) return QString::fromStdString(current);
        auto *source = obs_get_source_by_name(get_replay_source_name().c_str());
        auto *settings = source ? obs_source_get_settings(source) : nullptr;
        QString path = QString::fromUtf8(obs_data_get_string(settings, "local_file"));
        obs_data_release(settings); obs_source_release(source);
        return path;
    };
    ui->library->play = [](const QString &path, bool switchScene) { replay_play_clip(path.toStdString(), switchScene); };
    ui->library->changeSpeed = [](int percent) { replay_set_speed(percent); };
    connect(ui->library->playbackMode, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int mode) {
        if (property("loadingReplaySettings").toBool()) return;
        QString error;
        if (replay_is_busy()) error = "Wait until the current replay or save finishes before changing player mode.";
        else configureReplayMode(ui->sceneLineEdit->text(), ui->replaySourceNameEdit->text(), mode == 1, error);
        if (!error.isEmpty()) {
            setProperty("loadingReplaySettings", true); ui->library->playbackMode->setCurrentIndex(mode == 1 ? 0 : 1); setProperty("loadingReplaySettings", false);
            replay_status(error.toStdString()); return;
        }
        replay_set_speed(ui->library->speed->value());
        replay_status(mode == 1 ? "VLC live slow motion is ready. Speed changes keep the clip running." : "Standard Media Source selected. Speed changes reload the clip; choose VLC for live slow motion.");
        saveSettings();
    });
    ui->matches->legacyFolder = [this] { return ui->filePathBackupEdit->text(); };

	QObject::connect(ui->closeButton->button(QDialogButtonBox::Close),
		SIGNAL(clicked()), this, SLOT(close()));
    auto browse = [this](QLineEdit *field) {
        const QString path = QFileDialog::getExistingDirectory(this, "Choose replay folder", field->text());
        if (!path.isEmpty()) { field->setText(path); saveSettings(); }
    };
    connect(ui->playButton, &QPushButton::clicked, this, [] { instant_replay_play_hotkey(nullptr, OBS_INVALID_HOTKEY_ID, nullptr, true); });
    connect(ui->replayBrowse, &QPushButton::clicked, this, [this, browse] { browse(ui->replayFolderEdit); });
    connect(ui->backupBrowse, &QPushButton::clicked, this, [this, browse] { browse(ui->filePathBackupEdit); });
}

void replayCallout::ShowHideDialog()
{
	if(!isVisible())
	{
		setVisible(true);
	}
	else
	{
		setVisible(false);
	}
}

void replayCallout::requestSettingsSave()
{
    if (!property("loadingReplaySettings").toBool()) {
        saveConfig(); // Persist before returning to OBS's event loop.
        settingsSaveTimer->start();
    }
}

bool replayCallout::saveConfig()
{
    if (!configReady || property("loadingReplaySettings").toBool()) return false;
    auto *snapshot = obs_data_create();
    save_replay_callout(snapshot, true, nullptr);
    const auto json = QJsonDocument::fromJson(QByteArray(obs_data_get_json(snapshot))).object();
    obs_data_release(snapshot);
    QString error;
    const auto path = property("irxtConfigPath").toString();
    const bool ok = SettingsStore(path).save(configCollection, json, error);
    auto *label = findChild<QLabel *>("settingsSaveStatus");
    label->setText(ok ? "Settings saved to config file." : error);
    label->setToolTip(path);
    if (!ok) blog(LOG_WARNING, "[InstantReplayXT] %s", qPrintable(error));
    return ok;
}

void replayCallout::saveSettings()
{
    settingsSaveTimer->stop();
    if (!property("loadingReplaySettings").toBool()) { saveConfig(); obs_frontend_save(); }
}

void replayCallout::hideEvent(QHideEvent *event)
{
    // Escape and the Tools menu hide a QDialog without calling closeEvent.
    if (settingsSaveTimer->isActive()) saveSettings();
    QDialog::hideEvent(event);
}

void replayCallout::closeEvent(QCloseEvent* event)
{
    saveSettings();
	QDialog::closeEvent(event);
}

static void save_replay_callout(obs_data_t *save_data, bool saving, void *)
{
	if (!co) return;
	if(saving)
	{
		obs_data_t *obj = obs_data_create();
        co->ui->matches->save(obj);
        obs_data_set_bool(obj, "vlcReplayPlayer", co->ui->library->playbackMode->currentIndex() == 1);
        obs_data_set_int(obj, "speedPercent", co->ui->library->speed->value());
        obs_data_set_bool(obj, "librarySwitchScene", co->ui->library->switchScene->isChecked());
		QString qstr;
		std::string str;

		obs_data_set_bool(obj, "autoSwitch", co->ui->autoSwitchCheck->isChecked());
		obs_data_set_bool(obj, "enable", co->ui->enableCheck->isChecked());
		obs_data_set_int(obj, "wait", co->ui->waitSpinBox->value());
		obs_data_set_int(obj, "pause", co->ui->pauseSpinBox->value());
		obs_data_set_int(obj, "replay", co->ui->replaySpinBox->value());
		qstr = co->ui->sceneLineEdit->text();
		str = qstr.toStdString();
		obs_data_set_string(obj, "scene", str.c_str());
		obs_data_set_bool(obj, "Dmute", co->ui->muteDCheck->isChecked());
		obs_data_set_bool(obj, "Amute", co->ui->muteACheck->isChecked());

	
		// Instant Replay XT extended settings
		obs_data_set_string(obj, "replayFolder", co->ui->replayFolderEdit->text().toUtf8().constData());
		obs_data_set_bool(obj, "backup", co->ui->enableBackupCheck->isChecked());
		
		qstr = co->ui->filePathBackupEdit->text();
		str = qstr.toStdString();
		obs_data_set_string(obj, "backupPath", str.c_str());

		qstr = co->ui->replaySourceNameEdit->text();
		str = qstr.toStdString();
		obs_data_set_string(obj, "sourceName", str.c_str());
		//---

		obs_data_set_obj(save_data, "replayCallout_data", obj);

		obs_data_release(obj);
	}
	else
	{
        co->configReady = false;
        co->settingsSaveTimer->stop();
        co->setProperty("loadingReplaySettings", true);
        char *collection = obs_frontend_get_current_scene_collection();
        co->configCollection = QString::fromUtf8(collection ? collection : ""); bfree(collection);
        const auto recovered = SettingsStore(co->property("irxtConfigPath").toString()).load(co->configCollection);
        if (recovered["replayCallout_data"].isObject()) {
            auto *config = obs_data_create_from_json(QJsonDocument(recovered).toJson().constData());
            for (const auto *key : {"replayCallout_data"}) {
                auto *value = obs_data_get_obj(config, key);
                if (value) obs_data_set_obj(save_data, key, value);
                obs_data_release(value);
            }
            obs_data_release(config);
        }
        obs_data_t *obj = obs_data_get_obj(save_data, "replayCallout_data");
		QString qstr;
		if(!obj)
		{
			obj = obs_data_create();
		}
		co->ui->matches->load(obj);
        co->ui->library->switchScene->setChecked(obs_data_get_bool(obj, "librarySwitchScene"));

        obs_data_set_default_int(obj, "wait", 10);
        obs_data_set_default_string(obj, "scene", "Replay Scene");
        obs_data_set_default_string(obj, "sourceName", "Instant Replay Media Source");
        co->ui->replayFolderEdit->setText(QString::fromUtf8(obs_data_get_string(obj, "replayFolder")));
		obs_data_set_default_int(obj, "pause", 300);
		obs_data_set_default_int(obj, "replay", 20);
		co->ui->autoSwitchCheck->setChecked(obs_data_get_bool(obj, "autoSwitch"));
		co->ui->enableCheck->setChecked(obs_data_get_bool(obj, "enable"));
		co->ui->waitSpinBox->setValue(obs_data_get_int(obj, "wait"));
		co->ui->pauseSpinBox->setValue(obs_data_get_int(obj, "pause"));
		co->ui->replaySpinBox->setValue(obs_data_get_int(obj, "replay"));
		qstr = QString::fromUtf8(obs_data_get_string(obj, "scene"));
		co->ui->sceneLineEdit->setText(qstr);
		co->ui->muteDCheck->setChecked(obs_data_get_bool(obj, "Dmute"));
		co->ui->muteACheck->setChecked(obs_data_get_bool(obj, "Amute"));
		
		// Instant Replay XT extended settings
		co->ui->enableBackupCheck->setChecked(obs_data_get_bool(obj, "backup"));
		qstr = QString::fromUtf8(obs_data_get_string(obj, "backupPath"));
		co->ui->filePathBackupEdit->setText(qstr);
		qstr = QString::fromUtf8(obs_data_get_string(obj, "sourceName"));
		co->ui->replaySourceNameEdit->setText(qstr);
        co->ui->library->playbackMode->setCurrentIndex(obs_data_get_bool(obj, "vlcReplayPlayer") ? 1 : 0);
        obs_data_set_default_int(obj, "speedPercent", 100);
        co->ui->library->speed->setValue(int(obs_data_get_int(obj, "speedPercent")));
        co->ui->sceneLineEdit->refresh(); co->ui->replaySourceNameEdit->refresh(); co->ui->matches->vlcName->refresh();
        co->setProperty("loadingReplaySettings", false);
        co->configReady = !co->configCollection.isEmpty();
        QTimer::singleShot(0, co.data(), [collection = co->configCollection] {
            if (!co || !co->configReady || co->configCollection != collection) return;
            auto *existing = obs_get_source_by_name((co->ui->replaySourceNameEdit->text() + " (VLC live)").toUtf8().constData());
            const bool restore = existing || co->ui->library->playbackMode->currentIndex() == 1;
            obs_source_release(existing);
            if (restore) {
                QString error;
                if (!configureReplayMode(co->ui->sceneLineEdit->text(), co->ui->replaySourceNameEdit->text(), co->ui->library->playbackMode->currentIndex() == 1, error))
                    replay_status(error.toStdString());
            }
            replay_set_speed(co->ui->library->speed->value());
        });
		//---

		obs_data_release(obj);
	}
}

void replay_callout_init()
{
	QAction *action = (QAction*)obs_frontend_add_tools_menu_qaction(
		obs_module_text("InstantReplaySettings"));
	
	obs_frontend_push_ui_translation(obs_module_get_string);

	QMainWindow *window = (QMainWindow*)obs_frontend_get_main_window();

	settingsAction = action;
	co = new replayCallout(window);

	auto cb = []()
	{
		co->ShowHideDialog();
	};

	obs_frontend_pop_ui_translation();

	
	obs_frontend_add_save_callback(save_replay_callout, nullptr);

	QObject::connect(action, &QAction::triggered, co.data(), cb);
}

void replay_callout_shutdown()
{
	obs_frontend_remove_save_callback(save_replay_callout, nullptr);
	delete settingsAction.data();
	settingsAction = nullptr;
	delete co.data();
	co = nullptr;
}
