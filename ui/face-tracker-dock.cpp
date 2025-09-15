#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QAction>
#include <QMainWindow>
#include <QDockWidget>
#include <QVBoxLayout>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include "plugin-macros.generated.h"
#include "face-tracker-dock.hpp"
#include "face-tracker-widget.hpp"
#include "face-tracker-dock-internal.hpp"

#define SAVE_DATA_NAME PLUGIN_NAME"-dock"
#define OBJ_NAME_SUFFIX "_ft_dock"

void FTDock::closeEvent(QCloseEvent *event)
{
	QFrame::closeEvent(event);
}

// accessed only from UI thread
static std::vector<FTDock*> *docks;

static std::string generate_unique_name()
{
	for (int n=0;; n++) {
		char name[32] = "FTDock";
		if (n)
			snprintf(name, sizeof(name), "FTDock-%d", n);
		bool found = false;
		if (docks) for (size_t i=0; i<docks->size(); i++) {
			if ((*docks)[i]->name == name)
				found = true;
		}
		if (!found)
			return name;
	}
}

void ft_dock_add(const char *name, obs_data_t *props, bool show)
{
	auto *main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	auto *dock = new FTDock(main_window);
	dock->name = name ? name : generate_unique_name();
	dock->setObjectName(QString::fromUtf8(dock->name.c_str()) + OBJ_NAME_SUFFIX);
	dock->setWindowTitle(dock->name.c_str());

	dock->load_properties(props);

	std::string id = "ft." + dock->name;
	if (!obs_frontend_add_dock_by_id(id.c_str(), dock->name.c_str(), static_cast<QWidget *>(dock)))
		return;

	if (docks)
		docks->push_back(dock);

	if (show) {
		/* Workaround to show the dock */
		QMetaObject::invokeMethod(
			dock,
			[dock]() {
				auto *main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
				if (!main_window)
					return;
				QList<QDockWidget *> dd = main_window->findChildren<QDockWidget *>();
				for (QDockWidget *d : dd) {
					if (d->widget() != dock)
						continue;
					d->setVisible(true);
				}
			},
			Qt::QueuedConnection);
	}
}

struct init_target_selector_s
{
	QComboBox *q;
	int index;
	const char *source_name;
	const char *filter_name;
};

static bool init_target_selector_compare_name(struct init_target_selector_s *ctx, const char *source_name, const char *filter_name)
{
	if (strcmp(ctx->source_name, source_name) != 0)
		return false;

	// both expect source
	if (!ctx->filter_name && !filter_name)
		return true;

	// either one expects source
	if (!ctx->filter_name || !filter_name)
		return false;

	return strcmp(ctx->filter_name, filter_name) == 0;
}

static void init_target_selector_cb_add(struct init_target_selector_s *ctx, obs_source_t *source, obs_source_t *filter)
{
	QString text;
	QList<QVariant> val;

	const char *name = obs_source_get_name(source);
	const char *filter_name = NULL;
	text = QString::fromUtf8(name);
	val.append(QByteArray(name));

	if (filter) {
		filter_name = obs_source_get_name(filter);
		text += " / ";
		text += QString::fromUtf8(filter_name);
		val.append(QByteArray(filter_name));
	}

	if (ctx->index < ctx->q->count()) {
		ctx->q->setItemText(ctx->index, text);
		ctx->q->setItemData(ctx->index, val);
	}
	else
		ctx->q->insertItem(ctx->index, text, val);

	if (ctx->source_name) {
		if (init_target_selector_compare_name(ctx, name, filter_name))
			ctx->q->setCurrentIndex(ctx->index);
	}

	ctx->index++;
}

static void init_target_selector_cb_filter(obs_source_t *parent, obs_source_t *child, void *param)
{
	auto *ctx = (struct init_target_selector_s *)param;

	const char *id = obs_source_get_id(child);
	if (!strcmp(id, "face_tracker_filter") || !strcmp(id, "face_tracker_ptz")) {
		init_target_selector_cb_add(ctx, parent, child);
	}
}

static bool init_target_selector_cb_source(void *data, obs_source_t *source)
{
	auto *ctx = (struct init_target_selector_s *)data;

	const char *id = obs_source_get_id(source);
	if (!strcmp(id, "face_tracker_source")) {
		init_target_selector_cb_add(ctx, source, NULL);
		return true;
	}

	obs_source_enum_filters(source, init_target_selector_cb_filter, data);

	return true;
}

static void init_target_selector(QComboBox *q, const char *source_name=NULL, const char *filter_name=NULL)
{
	QString current = q->currentText();

	if (filter_name && !*filter_name)
		filter_name = NULL;

	init_target_selector_s ctx = {q, 0, source_name, filter_name};
	obs_enum_scenes(init_target_selector_cb_source, &ctx);
	obs_enum_sources(init_target_selector_cb_source, &ctx);

	while (q->count() > ctx.index)
		q->removeItem(ctx.index);

	if (current.length() && !source_name) {
		int ix = q->findText(current);
		if (ix >= 0)
			q->setCurrentIndex(ix);
	}
}

void FTDock::checkTargetSelector()
{
	init_target_selector(targetSelector);
}

void FTDock::frontendEvent_cb(enum obs_frontend_event event, void *private_data)
{
	auto *dock = static_cast<FTDock*>(private_data);
	dock->frontendEvent(event);
}

FTDock::FTDock(QWidget *parent)
	: QFrame(parent)
{
	data = face_tracker_dock_create();

	data->src_monitor = obs_source_create_private("face_tracker_monitor", "monitor", NULL);

	resize(256, 256);
	setMinimumSize(128, 128);
	setAttribute(Qt::WA_DeleteOnClose);

	mainLayout = new QVBoxLayout(this);

	targetSelector = new QComboBox(this);
	init_target_selector(targetSelector);
	mainLayout->addWidget(targetSelector);
	connect(targetSelector, &QComboBox::currentTextChanged, this, &FTDock::targetSelectorChanged);

	pauseButton = new QPushButton(obs_module_text("Pause"), this);
	pauseButton->setCheckable(true);
	mainLayout->addWidget(pauseButton);
	connect(pauseButton, &QPushButton::clicked, this, &FTDock::pauseButtonClicked);

	resetButton = new QPushButton(obs_module_text("Reset"), this);
	mainLayout->addWidget(resetButton);
	connect(resetButton, &QPushButton::clicked, this, &FTDock::resetButtonClicked);

	enableButton = new QPushButton("", this);
	enableButton->setCheckable(true);
	mainLayout->addWidget(enableButton);
	connect(enableButton, &QPushButton::clicked, this, &FTDock::enableButtonClicked);

	propertyButton = new QPushButton(obs_module_text("Properties"), this);
	mainLayout->addWidget(propertyButton);
	connect(propertyButton, &QPushButton::clicked, this, &FTDock::propertyButtonClicked);

	ftWidget = new FTWidget(data, this);
	mainLayout->addWidget(ftWidget);

	connect(ftWidget, &FTWidget::removeDock, this, &FTDock::removeDock);

	notrackButton = new QCheckBox(obs_module_text("Show all region"), this);
	mainLayout->addWidget(notrackButton);
	connect(notrackButton,
#if QT_VERSION < QT_VERSION_CHECK(6, 7, 0)
			&QCheckBox::stateChanged,
#else
			&QCheckBox::checkStateChanged,
#endif
			this, &FTDock::notrackButtonChanged);

	setLayout(mainLayout);

	connect(this, &FTDock::scenesMayChanged, this, &FTDock::checkTargetSelector);
	updateState();

	connect(this, &FTDock::dataChanged, this, &FTDock::updateWidget);

	obs_frontend_add_event_callback(frontendEvent_cb, this);
}

FTDock::~FTDock()
{
	obs_frontend_remove_event_callback(frontendEvent_cb, this);

	if (source_sh) {
		signal_handler_disconnect(source_sh, "state_changed", onStateChanged, this);
		source_sh = NULL;
	}

	face_tracker_dock_release(data);
	if (docks) for (size_t i=0; i<docks->size(); i++) {
		if ((*docks)[i] == this) {
			docks->erase(docks->begin()+i);
			break;
		}
	}
}

void FTDock::showEvent(QShowEvent *)
{
	blog(LOG_INFO, "FTDock::showEvent");
}

void FTDock::hideEvent(QHideEvent *)
{
	blog(LOG_INFO, "FTDock::hideEvent");
}

void FTDock::frontendEvent(enum obs_frontend_event event)
{
	if (event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP ||
			event == OBS_FRONTEND_EVENT_EXIT) {
		if (docks) while (docks->size()) {
			(*docks)[docks->size()-1]->close();
			delete (*docks)[docks->size()-1];
		}
	}
}

void FTDock::targetSelectorChanged()
{
	if (updating_widget)
		return;

	updateState();
}

OBSSource FTDock::get_source()
{
	OBSSource target;
	QList<QVariant> data = targetSelector->currentData().toList();

	for (int i=0; i<data.count(); i++) {
		const char *name = data[i].toByteArray().constData();
		if (i==0) {
			target = obs_get_source_by_name(name);
			obs_source_release(target);
		} else if (i==1) {
			target = obs_source_get_filter_by_name(target, name);
			obs_source_release(target);
		}
	}

	return target;
}

static bool is_filter(QComboBox *targetSelector)
{
	return targetSelector->currentData().toList().count() == 2;
}

static inline void set_monitor(obs_source_t *monitor, const QList<QVariant> &target_data)
{
	blog(LOG_INFO, "set_monitor monitor=%p", monitor);
	OBSData data = obs_data_create();
	obs_data_release(data);

	if (target_data.count() < 1)
		return;

	obs_data_set_string(data, "source_name", target_data[0].toByteArray().constData());

	obs_data_set_string(data, "filter_name",
			target_data.count() > 1 ? target_data[1].toByteArray().constData() : "");

	obs_source_update(monitor, data);
}

static void set_enable_button(QPushButton *enableButton, bool is_filter, bool is_enabled)
{
	enableButton->setEnabled(is_filter);
	enableButton->setChecked(is_filter && is_enabled);
	if (is_filter && is_enabled)
		enableButton->setText(obs_module_text("Disable filter"));
	else
		enableButton->setText(obs_module_text("Enable filter"));
}

static void set_pause_button(QPushButton *pauseButton, bool is_paused)
{
	pauseButton->setEnabled(true);
	pauseButton->setChecked(is_paused);
	if (is_paused)
		pauseButton->setText(obs_module_text("Resume"));
	else
		pauseButton->setText(obs_module_text("Pause"));
}

void FTDock::onStateChanged(void *data, calldata_t *)
{
	FTDock *dock = (FTDock *)data;
	QMetaObject::invokeMethod(dock, "updateState", Qt::QueuedConnection);
}

void FTDock::updateState()
{
	OBSSource target = get_source();
	proc_handler_t *ph = obs_source_get_proc_handler(target);
	if (!ph) {
		if (source_sh) {
			signal_handler_disconnect(source_sh, "state_changed", onStateChanged, this);
			source_sh = NULL;
		}
		return;
	}

	in_updateState = true;

	calldata_t cd;
	uint8_t stack[128];
	calldata_init_fixed(&cd, stack, sizeof(stack));
	if (proc_handler_call(ph, "get_state", &cd)) {
		bool b;

		if (calldata_get_bool(&cd, "paused", &b)) {
			set_pause_button(pauseButton, b);
		}
	}

	set_enable_button(enableButton, is_filter(targetSelector), obs_source_enabled(target));

	signal_handler_t *sh = obs_source_get_signal_handler(target);
	if (sh && sh!=source_sh) {
		if (source_sh)
			signal_handler_disconnect(source_sh, "state_changed", onStateChanged, this);
		source_sh = sh;
		signal_handler_connect_ref(source_sh, "state_changed", onStateChanged, this);
	}

	in_updateState = false;

	if (!data)
		return;

	pthread_mutex_lock(&data->mutex);

	if (data->src_monitor)
		set_monitor(data->src_monitor, targetSelector->currentData().toList());

	pthread_mutex_unlock(&data->mutex);
}

void FTDock::updateWidget()
{
	if (updating_widget)
		return;
	updating_widget = true;
	pthread_mutex_lock(&data->mutex);

	if (data->src_monitor) {
		obs_data_t *props = obs_source_get_settings(data->src_monitor);

		const char *source_name = obs_data_get_string(props, "source_name");
		const char *filter_name = obs_data_get_string(props, "filter_name");
		init_target_selector(targetSelector, source_name, filter_name);

		set_enable_button(enableButton, is_filter(targetSelector),
				obs_source_enabled(get_source()));

		bool notrack = obs_data_get_bool(props, "notrack");
		notrackButton->setCheckState(notrack ? Qt::Checked : Qt::Unchecked);

		obs_data_release(props);
	}

	pthread_mutex_unlock(&data->mutex);
	updating_widget = false;

	updateState();
}

void FTDock::pauseButtonClicked(bool checked)
{
	if (in_updateState)
		return;

	OBSSource target = get_source();
	proc_handler_t *ph = obs_source_get_proc_handler(target);
	if (!ph)
		return;

	blog(LOG_INFO, "FTDock::pauseButtonClicked checked=%d", (int)checked);

	calldata_t cd;
	uint8_t stack[128];
	calldata_init_fixed(&cd, stack, sizeof(stack));
	calldata_set_bool(&cd, "paused", checked);
	proc_handler_call(ph, "set_state", &cd);

	set_pause_button(pauseButton, checked);
}

void FTDock::resetButtonClicked(bool checked)
{
	UNUSED_PARAMETER(checked);

	if (in_updateState)
		return;

	OBSSource target = get_source();
	proc_handler_t *ph = obs_source_get_proc_handler(target);
	if (!ph)
		return;

	calldata_t cd;
	uint8_t stack[128];
	calldata_init_fixed(&cd, stack, sizeof(stack));
	calldata_set_bool(&cd, "reset", true);
	proc_handler_call(ph, "set_state", &cd);
}

void FTDock::enableButtonClicked(bool checked)
{
	if (!is_filter(targetSelector))
		return;

	OBSSource target = get_source();

	if (target) {
		obs_source_set_enabled(target, checked);
		set_enable_button(enableButton, true, checked);
	}
}

void FTDock::propertyButtonClicked(bool checked)
{
	UNUSED_PARAMETER(checked);

	QList<QVariant> data = targetSelector->currentData().toList();
	if (data.count() < 1)
		return;

	const char *name = data[0].toByteArray().constData();

	obs_source_t *target = obs_get_source_by_name(name);
	if (!target) {
		blog(LOG_ERROR, "Cannot find the target for name='%s'", name);
		return;
	}

	if (data.count() == 1)
		obs_frontend_open_source_properties(target);
	else
		obs_frontend_open_source_filters(target);

	obs_source_release(target);
}

void FTDock::notrackButtonChanged(
#if QT_VERSION < QT_VERSION_CHECK(6, 7, 0)
		int state
#else
		Qt::CheckState state
#endif
		)
{
	if (!data || !data->src_monitor)
		return;
	obs_data_t *props = obs_data_create();
	obs_data_set_bool(props, "notrack", state==Qt::Checked);
	obs_source_update(data->src_monitor, props);
	obs_data_release(props);
}

static void save_load_ft_docks(obs_data_t *save_data, bool saving, void *)
{
	blog(LOG_INFO, "save_load_ft_docks saving=%d", (int)saving);
	if (!docks)
		return;
	if (saving) {
		obs_data_t *props = obs_data_create();
		obs_data_array_t *array = obs_data_array_create();
		for (size_t i=0; i<docks->size(); i++) {
			FTDock *d = (*docks)[i];
			obs_data_t *obj = obs_data_create();
			d->save_properties(obj);
			obs_data_set_string(obj, "name", d->name.c_str());
			obs_data_array_push_back(array, obj);
			obs_data_release(obj);
		}
		obs_data_set_array(props, "docks", array);
		obs_data_set_obj(save_data, SAVE_DATA_NAME, props);
		obs_data_array_release(array);
		obs_data_release(props);
	}

	else /* loading */ {
		if (docks) while (docks->size()) {
			(*docks)[docks->size()-1]->close();
			delete (*docks)[docks->size()-1];
		}

		obs_data_t *props = obs_data_get_obj(save_data, SAVE_DATA_NAME);
		if (!props) {
			blog(LOG_INFO, "save_load_ft_docks: creating default properties");
			props = obs_data_create();
		}

		obs_data_array_t *array = obs_data_get_array(props, "docks");
		size_t count = obs_data_array_count(array);
		for (size_t i=0; i<count; i++) {
			obs_data_t *obj = obs_data_array_item(array, i);
			FTDock::default_properties(obj);
			const char *name = obs_data_get_string(obj, "name");
			ft_dock_add(name, obj, false);
			obs_data_release(obj);
		}
		obs_data_array_release(array);
		obs_data_release(props);
	}
}

void ft_docks_init()
{
	docks = new std::vector<FTDock*>;
	obs_frontend_add_save_callback(save_load_ft_docks, NULL);

	QAction *action = static_cast<QAction *>(obs_frontend_add_tools_menu_qaction(
				obs_module_text("New Face Tracker Dock...") ));
	blog(LOG_INFO, "ft_docks_init: Adding face tracker dock menu action=%p", action);
	auto cb = [] {
		obs_data_t *props = obs_data_create();
		FTDock::default_properties(props);
		ft_dock_add(NULL, props, true);
		obs_data_release(props);
	};
	QAction::connect(action, &QAction::triggered, cb);
}

void ft_docks_release()
{
	delete docks;
	docks = NULL;
}

void FTDock::default_properties(obs_data_t *)
{
}

void FTDock::save_properties(obs_data_t *props)
{
	// Save indicates a source or a filter has been changed.
	scenesMayChanged();

	pthread_mutex_lock(&data->mutex);

	obs_data_t *prop = obs_source_get_settings(data->src_monitor);
	if (prop) {
		obs_data_set_obj(props, "monitor", prop);
		obs_data_release(prop);
	}

	pthread_mutex_unlock(&data->mutex);
}

void FTDock::load_properties(obs_data_t *props)
{
	pthread_mutex_lock(&data->mutex);

	if (data && data->src_monitor) {
		obs_data_t *prop = obs_data_get_obj(props, "monitor");
		if (prop) {
			obs_source_update(data->src_monitor, prop);
			obs_data_release(prop);
		}
	}

	pthread_mutex_unlock(&data->mutex);

	dataChanged();
}

void FTDock::removeDock()
{
	std::string id = "ft." + name;
	obs_frontend_remove_dock(id.c_str());
}

struct face_tracker_dock_s *face_tracker_dock_create()
{
	struct face_tracker_dock_s *data = (struct face_tracker_dock_s *)bzalloc(sizeof(struct face_tracker_dock_s));
	data->ref = 1;
	pthread_mutex_init(&data->mutex, NULL);
	return data;
}

void face_tracker_dock_destroy(struct face_tracker_dock_s *data)
{
	obs_display_destroy(data->disp);
	data->disp = NULL;
	obs_source_release(data->src_monitor);
	pthread_mutex_destroy(&data->mutex);
	bfree(data);
}
