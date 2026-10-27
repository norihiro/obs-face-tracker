#pragma once

#include <obs.h>
#include <QWidget>
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
	void showEvent(QShowEvent *event) override;
	void hideEvent(QHideEvent *event) override;

signals:
	void removeDock();

public:
	FTWidget(struct face_tracker_dock_s *data, QWidget *parent);
	~FTWidget();
	void setShown(bool shown);
	void dockClosed();
	void openMenu(const class QPoint &pos);

private:
	struct face_tracker_dock_s *data;
	bool display_stale = false;
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
