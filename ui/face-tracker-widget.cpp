#include <obs-module.h>
#include <obs.h>
#include <obs-frontend-api.h>
#include <util/threading.h>
#include <string>
#include <algorithm>
#include <QMenu>
#include <QAction>
#include <QWindow>
#include <QMouseEvent>
#include "plugin-macros.generated.h"
#include "face-tracker-widget.hpp"
#include "face-tracker-dock-internal.hpp"
#include "obsgui-helper.hpp"

static void draw(void *param, uint32_t cx, uint32_t cy)
{
	auto *data = (struct face_tracker_dock_s *)param;

	if (pthread_mutex_trylock(&data->mutex))
		return;

	gs_blend_state_push();
	gs_reset_blend_state();

	if (data->src_monitor) {
		int w_src = obs_source_get_width(data->src_monitor);
		int h_src = obs_source_get_height(data->src_monitor);
		if (w_src <= 0 || h_src <= 0)
			goto err;
		int w, h;
		if (w_src * cy > h_src * cx) {
			w = cx;
			h = cx * h_src / w_src;
		} else {
			h = cy;
			w = cy * w_src / h_src;
		}

		gs_projection_push();
		gs_viewport_push();

		gs_set_viewport((cx - w) * 0.5, (cy - h) * 0.5, w, h);
		gs_ortho(0.0f, w_src, -1.0f, h_src, -100.0f, 100.0f);

		obs_source_video_render(data->src_monitor);

		gs_viewport_pop();
		gs_projection_pop();
	}
err:

	gs_blend_state_pop();

	pthread_mutex_unlock(&data->mutex);
}

FTWidget::FTWidget(struct face_tracker_dock_s *data_, QWidget *parent) : NorisQTDisplay(parent)
{
	face_tracker_dock_addref((data = data_));

	setContextMenuPolicy(Qt::CustomContextMenu);
	connect(this, &QWidget::customContextMenuRequested, this, &FTWidget::openMenu);

	connect(this, &NorisQTDisplay::DisplayCreated, this, &FTWidget::RegisterCallbackToDisplay);
}

FTWidget::~FTWidget()
{
	disconnect(this, &NorisQTDisplay::DisplayCreated, this, &FTWidget::RegisterCallbackToDisplay);

	if (data) {
		if (obs_display_t *display = GetDisplay())
			obs_display_remove_draw_callback(display, draw, data);
	}

	face_tracker_dock_release(data);
}

void FTWidget::RegisterCallbackToDisplay()
{
	obs_display_t *display = GetDisplay();
	obs_display_add_draw_callback(display, draw, data);
}

void FTWidget::closeEvent(QCloseEvent *)
{
	DestroyDisplay();
}

void FTWidget::openMenu(const class QPoint &pos)
{
	QMenu popup(this);

	auto *act = new QAction(obs_module_text("dock.menu.close"), this);
	connect(act, &QAction::triggered, this, &FTWidget::removeDock, Qt::QueuedConnection);
	popup.addAction(act);

	popup.exec(mapToGlobal(pos));
}
