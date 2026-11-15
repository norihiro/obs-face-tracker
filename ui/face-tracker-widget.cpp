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

FTWidget::FTWidget(struct face_tracker_dock_s *data_, QWidget *parent) : QWidget(parent)
{
	face_tracker_dock_addref((data = data_));
	setAttribute(Qt::WA_PaintOnScreen);
	setAttribute(Qt::WA_StaticContents);
	setAttribute(Qt::WA_NoSystemBackground);
	setAttribute(Qt::WA_OpaquePaintEvent);
	setAttribute(Qt::WA_DontCreateNativeAncestors);
	setAttribute(Qt::WA_NativeWindow);

	setContextMenuPolicy(Qt::CustomContextMenu);
	connect(this, &QWidget::customContextMenuRequested, this, &FTWidget::openMenu);
}

FTWidget::~FTWidget()
{
	face_tracker_dock_release(data);
}

void FTWidget::CreateDisplay()
{
	if (!data)
		return;
	if (data->disp || !windowHandle()->isExposed())
		return;

	// A display with no area cannot be drawn to
	QSize size = GetPixelSize(this);
	if (size.isEmpty())
		return;

	blog(LOG_INFO, "FTWidget::CreateDisplay %p", this);

	gs_init_data info = {};
	info.cx = size.width();
	info.cy = size.height();
	info.format = GS_BGRA;
	info.zsformat = GS_ZS_NONE;
	QWindow *window = windowHandle();
	if (!window) {
		blog(LOG_ERROR, "FTWidget %p: windowHandle() returns NULL", this);
		return;
	}
	if (!QTToGSWindow(window, info.window)) {
		blog(LOG_ERROR, "FTWidget %p: QTToGSWindow failed", this);
		return;
	}
	data->disp = obs_display_create(&info, 0);
	obs_display_add_draw_callback(data->disp, draw, data);
#ifdef HAVE_DISPLAY_SET_INTERLEAVE
	blog(LOG_INFO, "calling obs_display_set_interleave interleave=2");
	obs_display_set_interleave(data->disp, 2);
#endif
}

void FTWidget::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);
	CreateDisplay();

	// Resized to no area, the display fails to draw every frame until it is
	// resized again, so keep its last size until the dock has room again
	QSize size = GetPixelSize(this);
	if (size.isEmpty()) {
		if (!event->oldSize().isEmpty())
			blog(LOG_INFO, "FTWidget %p: no area, keeping the display at its size", this);
		return;
	}
	obs_display_resize(data->disp, size.width(), size.height());
}

void FTWidget::paintEvent(QPaintEvent *)
{
	CreateDisplay();
}

class QPaintEngine *FTWidget::paintEngine() const
{
	return NULL;
}

void FTWidget::closeEvent(QCloseEvent *)
{
	setShown(false);
}

void FTWidget::showEvent(QShowEvent *event)
{
	QWidget::showEvent(event);
	if (display_stale) {
		// Disabled since the dock was hidden, so OBS no longer uses it
		blog(LOG_INFO, "FTWidget %p: making the display again after the dock was closed", this);
		setShown(false);
		display_stale = false;
	}
	CreateDisplay();
	if (data->disp)
		obs_display_set_enabled(data->disp, true);
}

// Closing a floating dock takes away the window the display draws to right
// after hiding it, so stop drawing to it once hidden. It is not destroyed, as
// on macOS OBS still uses a display for a moment after resizing it.
void FTWidget::hideEvent(QHideEvent *event)
{
	QWidget::hideEvent(event);
	if (data->disp)
		obs_display_set_enabled(data->disp, false);
}

// Closing a floating dock takes away the surface of its window, so the
// display cannot be used again once the dock is opened again
void FTWidget::dockClosed()
{
	display_stale = true;
}

void FTWidget::setShown(bool shown)
{
	if (shown && !data->disp) {
		CreateDisplay();
	}
	if (!shown && data->disp) {
		obs_display_destroy(data->disp);
		data->disp = NULL;
	}
}

void FTWidget::openMenu(const class QPoint &pos)
{
	QMenu popup(this);

	auto *act = new QAction(obs_module_text("dock.menu.close"), this);
	connect(act, &QAction::triggered, this, &FTWidget::removeDock, Qt::QueuedConnection);
	popup.addAction(act);

	popup.exec(mapToGlobal(pos));
}
