#include "InstantReplay_callout.h"
#include "InstantReplay.h"
#include <obs-frontend-api.h>
#include <obs-module.h>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMessageBox>
#include <QtGui/QAction>

#include <QPointer>
#include <QCloseEvent>
#include <QFileDialog>

QPointer<replayCallout> co;
static QPointer<QAction> settingsAction;

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
	return co->ui->replaySourceNameEdit->text().toStdString();
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
    ui->matches->legacyFolder = [this] { return ui->filePathBackupEdit->text(); };

	QObject::connect(ui->closeButton->button(QDialogButtonBox::Close),
		SIGNAL(clicked()), this, SLOT(close()));
    auto browse = [this](QLineEdit *field) {
        const QString path = QFileDialog::getExistingDirectory(this, "Choose replay folder", field->text());
        if (!path.isEmpty()) { field->setText(path); obs_frontend_save(); }
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

void replayCallout::closeEvent(QCloseEvent* event)
{
	obs_frontend_save();
	QDialog::closeEvent(event);
}

static void save_replay_callout(obs_data_t *save_data, bool saving, void *)
{
	if (!co) return;
	if(saving)
	{
		obs_data_t *obj = obs_data_create();
        co->ui->matches->save(obj);
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
		obs_data_t *obj = obs_data_get_obj(save_data, "replayCallout_data");
		QString qstr;
		if(!obj)
		{
			obj = obs_data_create();
		}
		co->ui->matches->load(obj);
        co->ui->library->switchScene->setChecked(obs_data_get_bool(obj, "librarySwitchScene"));
        co->ui->library->speed->setValue(100);
        replay_set_speed(100);
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
