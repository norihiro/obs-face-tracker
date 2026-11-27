#pragma once

#include <obs.h>
#include <QWidget>
#include <NorisQTDisplay.hpp>

class FTWidget : public NorisQTDisplay {
	Q_OBJECT

	void closeEvent(QCloseEvent *event) override;
	void RegisterCallbackToDisplay();

signals:
	void removeDock();

public:
	FTWidget(struct face_tracker_dock_s *data, QWidget *parent);
	~FTWidget();
	void openMenu(const class QPoint &pos);

private:
	struct face_tracker_dock_s *data;
};
