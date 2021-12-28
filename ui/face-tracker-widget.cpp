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

		gs_set_viewport((cx-w)*0.5, (cy-h)*0.5, w, h);
		gs_ortho(0.0f, w_src, -1.0f, h_src, -100.0f, 100.0f);

		obs_source_video_render(data->src_monitor);

		gs_viewport_pop();
		gs_projection_pop();
	}
err:

	gs_blend_state_pop();

	pthread_mutex_unlock(&data->mutex);
}

FTWidget::FTWidget(struct face_tracker_dock_s *data_, QWidget *parent)
	: QWidget(parent)
	, eventFilter(BuildEventFilter())
{
	face_tracker_dock_addref((data = data_));
	setAttribute(Qt::WA_PaintOnScreen);
	setAttribute(Qt::WA_StaticContents);
	setAttribute(Qt::WA_NoSystemBackground);
	setAttribute(Qt::WA_OpaquePaintEvent);
	setAttribute(Qt::WA_DontCreateNativeAncestors);
	setAttribute(Qt::WA_NativeWindow);

	setMouseTracking(true);
	QObject::installEventFilter(eventFilter.get());
}

FTWidget::~FTWidget()
{
	removeEventFilter(eventFilter.get());
	face_tracker_dock_release(data);
}

OBSEventFilter *FTWidget::BuildEventFilter()
{
	return new OBSEventFilter([this](QObject *obj, QEvent *event) {
		UNUSED_PARAMETER(obj);

		switch (event->type()) {
		case QEvent::MouseButtonPress:
		case QEvent::MouseButtonRelease:
		case QEvent::MouseButtonDblClick:
			return this->HandleMouseClickEvent(
				static_cast<QMouseEvent *>(event));
		case QEvent::MouseMove:
		case QEvent::Enter:
		case QEvent::Leave:
			return this->HandleMouseMoveEvent(
				static_cast<QMouseEvent *>(event));

		case QEvent::Wheel:
			return this->HandleMouseWheelEvent(
				static_cast<QWheelEvent *>(event));
		case QEvent::KeyPress:
		case QEvent::KeyRelease:
			return this->HandleKeyEvent(
				static_cast<QKeyEvent *>(event));
		default:
			return false;
		}
	});
}

void FTWidget::CreateDisplay()
{
	if (!data)
		return;
	if (data->disp || !windowHandle()->isExposed())
		return;

	blog(LOG_INFO, "FTWidget::CreateDisplay %p", this);

	QSize size = GetPixelSize(this);
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
}

void FTWidget::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);
	CreateDisplay();

	QSize size = GetPixelSize(this);
	obs_display_resize(data->disp, size.width(), size.height());
}

void FTWidget::paintEvent(QPaintEvent *event)
{
	CreateDisplay();
}

class QPaintEngine *FTWidget::paintEngine() const
{
	return NULL;
}

void FTWidget::closeEvent(QCloseEvent *event)
{
	setShown(false);
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

#define INTERACT_KEEP_SOURCE (1<<30)

static int TranslateQtKeyboardEventModifiers(QInputEvent *event,
					     bool mouseEvent)
{
	int obsModifiers = INTERACT_NONE;

	if (event->modifiers().testFlag(Qt::ShiftModifier))
		obsModifiers |= INTERACT_SHIFT_KEY;
	if (event->modifiers().testFlag(Qt::AltModifier))
		obsModifiers |= INTERACT_ALT_KEY;
#ifdef __APPLE__
	// Mac: Meta = Control, Control = Command
	if (event->modifiers().testFlag(Qt::ControlModifier))
		obsModifiers |= INTERACT_COMMAND_KEY;
	if (event->modifiers().testFlag(Qt::MetaModifier))
		obsModifiers |= INTERACT_CONTROL_KEY;
#else
	// Handle windows key? Can a browser even trap that key?
	if (event->modifiers().testFlag(Qt::ControlModifier))
		obsModifiers |= INTERACT_CONTROL_KEY;
#endif

	if (!mouseEvent) {
		if (event->modifiers().testFlag(Qt::KeypadModifier))
			obsModifiers |= INTERACT_IS_KEY_PAD;
	}

	return obsModifiers;
}

static int TranslateQtMouseEventModifiers(QMouseEvent *event)
{
	int modifiers = TranslateQtKeyboardEventModifiers(event, true);

	if (event->buttons().testFlag(Qt::LeftButton))
		modifiers |= INTERACT_MOUSE_LEFT;
	if (event->buttons().testFlag(Qt::MiddleButton))
		modifiers |= INTERACT_MOUSE_MIDDLE;
	if (event->buttons().testFlag(Qt::RightButton))
		modifiers |= INTERACT_MOUSE_RIGHT;

	return modifiers;
}

bool FTWidget::HandleMouseClickEvent(QMouseEvent *event)
{
	bool mouseUp = event->type() == QEvent::MouseButtonRelease;
	int clickCount = 1;
	if (event->type() == QEvent::MouseButtonDblClick)
		clickCount = 2;

	struct obs_mouse_event mouseEvent = {};

	mouseEvent.modifiers = TranslateQtMouseEventModifiers(event);

	int32_t button = 0;

	switch (event->button()) {
	case Qt::LeftButton:
		button = MOUSE_LEFT;
		if (mouseUp) mouseEvent.modifiers |= INTERACT_KEEP_SOURCE; // Not to change i_mouse if released outside
		break;
	case Qt::MiddleButton:
		button = MOUSE_MIDDLE;
		break;
	case Qt::RightButton:
		button = MOUSE_RIGHT;
		break;
	default:
		blog(LOG_WARNING, "unknown button type %d", event->button());
		return false;
	}

	return true;
}

bool FTWidget::HandleMouseMoveEvent(QMouseEvent *event)
{
	struct obs_mouse_event mouseEvent = {};

	bool mouseLeave = event->type() == QEvent::Leave;

	if (!mouseLeave)
		mouseEvent.modifiers = TranslateQtMouseEventModifiers(event);

	int x = event->x();
	int y = event->y();

	return true;
}

bool FTWidget::HandleMouseWheelEvent(QWheelEvent *event)
{
	struct obs_mouse_event mouseEvent = {};

	mouseEvent.modifiers = TranslateQtKeyboardEventModifiers(event, true);

	int xDelta = 0;
	int yDelta = 0;

	const QPoint angleDelta = event->angleDelta();
	if (!event->pixelDelta().isNull()) {
		if (angleDelta.x())
			xDelta = event->pixelDelta().x();
		else
			yDelta = event->pixelDelta().y();
	} else {
		if (angleDelta.x())
			xDelta = angleDelta.x();
		else
			yDelta = angleDelta.y();
	}

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
	const QPointF position = event->position();
	const int x = position.x();
	const int y = position.y();
#else
	const int x = event->x();
	const int y = event->y();
#endif

	return true;
}

bool FTWidget::HandleKeyEvent(QKeyEvent *event)
{
	struct obs_key_event keyEvent;

	QByteArray text = event->text().toUtf8();
	keyEvent.modifiers = TranslateQtKeyboardEventModifiers(event, false);
	keyEvent.text = text.data();
	keyEvent.native_modifiers = event->nativeModifiers();
	keyEvent.native_scancode = event->nativeScanCode();
	keyEvent.native_vkey = event->nativeVirtualKey();

	bool keyUp = event->type() == QEvent::KeyRelease;

	// TODO: implement me obs_source_send_key_click(source, &keyEvent, keyUp);

	return true;
}
