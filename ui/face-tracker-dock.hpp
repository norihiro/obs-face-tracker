#pragma once

#ifdef __cplusplus
#include <QDockWidget>
#include <QPointer>
#include <QAction>
#include <string>
#include <obs.h>
#include <obs.hpp>
#include <obs-frontend-api.h>

// the necessary APIs are implemented at 27.0.1-107-g5b18faeb4.
#if LIBOBS_API_VER > MAKE_SEMANTIC_VERSION(27, 0, 1)
#define HAVE_PROPERTY_BUTTON
#endif

class FTDock : public QDockWidget {
	Q_OBJECT

public:
	class FTWidget *widget;
	std::string name;
	QPointer<QAction> action = 0;
	struct face_tracker_dock_s *data;

	class QVBoxLayout *mainLayout;
	class QComboBox *targetSelector;
	class QCheckBox *pauseButton;
	class QPushButton *resetButton;
#ifdef HAVE_PROPERTY_BUTTON
	class QPushButton *propertyButton;
#endif
	class FTWidget *ftWidget;
	class QCheckBox *notrackButton;

	bool updating_widget = false;

public:
	FTDock(QWidget *parent = nullptr);
	~FTDock();
	void closeEvent(QCloseEvent *event) override;

	static void default_properties(obs_data_t *);
	void save_properties(obs_data_t*);
	void load_properties(obs_data_t*);

private:
	void showEvent(QShowEvent *event) override;
	void hideEvent(QHideEvent *event) override;

	void frontendEvent(enum obs_frontend_event event);
	static void frontendEvent_cb(enum obs_frontend_event event, void *private_data);

	OBSSource get_source();

signals:
	void scenesMayChanged();
	void dataChanged();

public slots:
	void checkTargetSelector();
	void updateState();
	void updateWidget();

private slots:
	void targetSelectorChanged();
	void pauseButtonChanged(int state);
	void resetButtonClicked(bool checked);
#ifdef HAVE_PROPERTY_BUTTON
	void propertyButtonClicked(bool checked);
#endif
	void notrackButtonChanged(int state);
};

extern "C" {
#endif // __cplusplus
void ft_dock_add(const char *name, obs_data_t *props);
void ft_docks_init();
void ft_docks_release();

#ifdef __cplusplus
} // extern "C"
#endif
