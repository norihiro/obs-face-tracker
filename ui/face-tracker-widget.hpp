#pragma once

#include <obs.h>
#include <QWidget>
#include <memory>

#define SCOPE_WIDGET_N_SRC 4

class OBSEventFilter;

class FTWidget : public QWidget {
	Q_OBJECT

	std::unique_ptr<OBSEventFilter> eventFilter;

	void CreateDisplay();
	void resizeEvent(QResizeEvent *event) override;
	void paintEvent(QPaintEvent *event) override;
	class QPaintEngine *paintEngine() const override;
	void closeEvent(QCloseEvent *event) override;

	// for interactions
	bool HandleMouseClickEvent(QMouseEvent *event);
	bool HandleMouseMoveEvent(QMouseEvent *event);
	bool HandleMouseWheelEvent(QWheelEvent *event);
	bool HandleKeyEvent(QKeyEvent *event);
	OBSEventFilter *BuildEventFilter();

public:
	FTWidget(QWidget *parent);
	~FTWidget();
	void SetData(struct face_tracker_dock_s *data);
	void setShown(bool shown);

private:
	struct face_tracker_dock_s *data;
};

typedef std::function<bool(QObject *, QEvent *)> EventFilterFunc;

class OBSEventFilter : public QObject
{
	Q_OBJECT
public:
	OBSEventFilter(EventFilterFunc filter_) : filter(filter_) {}

protected:
	bool eventFilter(QObject *obj, QEvent *event)
	{
		return filter(obj, event);
	}

public:
	EventFilterFunc filter;
};
