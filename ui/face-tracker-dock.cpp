#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QAction>
#include <QMainWindow>
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
	dock->resize(256, 256);
	dock->setMinimumSize(128, 128);
	dock->setAllowedAreas(Qt::AllDockWidgetAreas);

	FTWidget *w = new FTWidget(dock);
	dock->SetWidget(w);
	dock->load_properties(props);

	main_window->addDockWidget(Qt::BottomDockWidgetArea, dock);
	dock->action = (QAction*)obs_frontend_add_dock(dock);

	if (docks)
		docks->push_back(dock);
}

FTDock::FTDock(QWidget *parent)
	: QDockWidget(parent)
{
	data = face_tracker_dock_create();
	setAttribute(Qt::WA_DeleteOnClose);
}

FTDock::~FTDock()
{
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
	setWidget((QWidget*)w);
}

void FTDock::showEvent(QShowEvent *)
{
	blog(LOG_INFO, "FTDock::showEvent");
	widget->setShown(true);
}

void FTDock::hideEvent(QHideEvent *)
{
	blog(LOG_INFO, "FTDock::hideEvent");
	widget->setShown(false);
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
