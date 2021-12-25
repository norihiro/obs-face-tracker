#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QAction>
#include <QMainWindow>
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
	QDockWidget::closeEvent(event);
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

void ft_dock_add(const char *name, obs_data_t *props)
{
	auto *main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	auto *dock = new FTDock(main_window);
	dock->name = name ? name : generate_unique_name();
	dock->setObjectName(QString::fromUtf8(dock->name.c_str()) + OBJ_NAME_SUFFIX);
	dock->setWindowTitle(dock->name.c_str());
	dock->setAllowedAreas(Qt::AllDockWidgetAreas);

	dock->load_properties(props);

	main_window->addDockWidget(Qt::BottomDockWidgetArea, dock);
	dock->action = (QAction*)obs_frontend_add_dock(dock);

	if (docks)
		docks->push_back(dock);
}

struct init_target_selector_s
{
	QComboBox *q;
	int index;
};

void init_target_selector_cb_add(struct init_target_selector_s *ctx, obs_source_t *source, obs_source_t *filter)
{
	QString text;
	QList<QVariant> val;

	const char *name = obs_source_get_name(source);
	text = QString::fromUtf8(name);
	val.append(QVariant(name));

	if (filter) {
		const char *name = obs_source_get_name(filter);
		text += " / ";
		text += QString::fromUtf8(name);
		val.append(QVariant(name));
	}

	if (ctx->index < ctx->q->count()) {
		ctx->q->setItemText(ctx->index, text);
		ctx->q->setItemData(ctx->index, QVariant(val));
	}
	else
		ctx->q->insertItem(ctx->index, text, QVariant(val));
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

static void init_target_selector(QComboBox *q)
{
	QString current = q->currentText();

	init_target_selector_s ctx = {q, 0};
	obs_enum_scenes(init_target_selector_cb_source, &ctx);
	obs_enum_sources(init_target_selector_cb_source, &ctx);

	while (q->count() > ctx.index)
		q->removeItem(ctx.index);

	if (current.length()) {
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
	: QDockWidget(parent)
{
	data = face_tracker_dock_create();

	resize(256, 256);
	setMinimumSize(128, 128);
	setAttribute(Qt::WA_DeleteOnClose);

	mainLayout = new QVBoxLayout(this);
	auto *dockWidgetContents = new QWidget;
	dockWidgetContents->setObjectName(QStringLiteral("contextContainer"));
	dockWidgetContents->setLayout(mainLayout);

	targetSelector = new QComboBox(this);
	init_target_selector(targetSelector);
	mainLayout->addWidget(targetSelector);
	connect(targetSelector, &QComboBox::currentTextChanged, this, &FTDock::targetSelectorChanged);

	pauseButton = new QCheckBox(obs_module_text("Pause"), this);
	mainLayout->addWidget(pauseButton);
	connect(pauseButton, &QCheckBox::stateChanged, this, &FTDock::pauseButtonChanged);

	resetButton = new QPushButton(obs_module_text("Reset"), this);
	mainLayout->addWidget(resetButton);
	connect(resetButton, &QPushButton::clicked, this, &FTDock::resetButtonClicked);

	setWidget(dockWidgetContents);

	connect(this, &FTDock::scenesMayChanged, this, &FTDock::checkTargetSelector);
	updateState();

	obs_frontend_add_event_callback(frontendEvent_cb, this);
}

FTDock::~FTDock()
{
	obs_frontend_remove_event_callback(frontendEvent_cb, this);

	face_tracker_dock_release(data);
	if (action)
		delete action;
	if (docks) for (size_t i=0; i<docks->size(); i++) {
		if ((*docks)[i] == this) {
			docks->erase(docks->begin()+i);
			break;
		}
	}
}

void FTDock::SetWidget(class FTWidget *w)
{
	w->SetData(data);
	widget = w;
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
}

void FTDock::targetSelectorChanged()
{
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

void FTDock::updateState()
{
	OBSSource target = get_source();
	proc_handler_t *ph = obs_source_get_proc_handler(target);
	if (!ph)
		return;

	calldata_t cd;
	uint8_t stack[128];
	calldata_init_fixed(&cd, stack, sizeof(stack));
	if (proc_handler_call(ph, "get_state", &cd)) {
		bool b;

		if (calldata_get_bool(&cd, "paused", &b)) {
			pauseButton->setCheckState(b ? Qt::Checked : Qt::Unchecked);
		}
	}
}

void FTDock::pauseButtonChanged(int state)
{
	OBSSource target = get_source();
	proc_handler_t *ph = obs_source_get_proc_handler(target);
	if (!ph)
		return;

	calldata_t cd;
	uint8_t stack[128];
	calldata_init_fixed(&cd, stack, sizeof(stack));
	calldata_set_bool(&cd, "paused", state==Qt::Checked);
	proc_handler_call(ph, "set_state", &cd);
}

void FTDock::resetButtonClicked(bool checked)
{
	UNUSED_PARAMETER(checked);

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
			ft_dock_add(name, obj);
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
		ft_dock_add(NULL, props);
		obs_data_release(props);
	};
	QAction::connect(action, &QAction::triggered, cb);
}

void ft_docks_release()
{
	delete docks;
	docks = NULL;
}

void FTDock::default_properties(obs_data_t *props)
{
}

void FTDock::save_properties(obs_data_t *props)
{
	// Save indicates a source or a filter has been changed.
	scenesMayChanged();

	// pthread_mutex_lock(&data->mutex);
	// TODO: implement me
	// pthread_mutex_unlock(&data->mutex);
}

void FTDock::load_properties(obs_data_t *props)
{
	// pthread_mutex_lock(&data->mutex);
	// TODO: implement me
	// pthread_mutex_unlock(&data->mutex);
}
