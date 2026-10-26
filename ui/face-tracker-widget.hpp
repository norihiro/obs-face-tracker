#pragma once

#include <obs.h>
#include <QWidget>
#include <QWindow>
#include <QPointer>
#include <memory>

#define SCOPE_WIDGET_N_SRC 4

class OBSEventFilter;

class FTWidget : public QWidget {
	Q_OBJECT

	void CreateDisplay();
	void resizeEvent(QResizeEvent *event) override;
	void paintEvent(QPaintEvent *event) override;
	class QPaintEngine *paintEngine() const override;
	void closeEvent(QCloseEvent *event) override;
	bool event(QEvent *event) override;
	bool eventFilter(QObject *watched, QEvent *event) override;
	void DestroyDisplay();

signals:
	void removeDock();

public:
	FTWidget(struct face_tracker_dock_s *data, QWidget *parent);
	~FTWidget();
	void setShown(bool shown);
	void openMenu(const class QPoint &pos);

private:
	struct face_tracker_dock_s *data;
	// The window the display draws to, which is replaced when the widget is
	// docked, tabbed or floated
	QPointer<QWindow> watched_window;
};

typedef std::function<bool(QObject *, QEvent *)> EventFilterFunc;

class OBSEventFilter : public QObject {
	Q_OBJECT
public:
	OBSEventFilter(EventFilterFunc filter_) : filter(filter_) {}

protected:
	bool eventFilter(QObject *obj, QEvent *event) { return filter(obj, event); }

public:
	EventFilterFunc filter;
};
