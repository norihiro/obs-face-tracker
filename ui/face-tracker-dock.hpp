#pragma once

#ifdef __cplusplus
#include <QFrame>
#include <string>
#include <obs.h>
#include <obs.hpp>
#include <obs-frontend-api.h>

class FTDock : public QFrame {
	Q_OBJECT

public:
	class FTWidget *widget;
	std::string name;
	struct face_tracker_dock_s *data;

	class QVBoxLayout *mainLayout;
	class QComboBox *targetSelector;
	class QPushButton *pauseButton;
	class QPushButton *resetButton;
	class QPushButton *enableButton;
	class QPushButton *propertyButton;
	class FTWidget *ftWidget;
	class QCheckBox *notrackButton;

	bool updating_widget = false;
	bool in_updateState = false;

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
	signal_handler_t *source_sh = NULL;

	static void onStateChanged(void *data, calldata_t *cd);

signals:
	void scenesMayChanged();
	void dataChanged();

public slots:
	void checkTargetSelector();
	void updateState();
	void updateWidget();
	void removeDock();

private slots:
	void targetSelectorChanged();
	void pauseButtonClicked(bool checked);
	void resetButtonClicked(bool checked);
	void enableButtonClicked(bool checked);
	void propertyButtonClicked(bool checked);
	void notrackButtonChanged(
#if QT_VERSION < QT_VERSION_CHECK(6, 7, 0)
		int state
#else
		Qt::CheckState state
#endif
		);
};

extern "C" {
#endif // __cplusplus
void ft_dock_add(const char *name, obs_data_t *props, bool show);
void ft_docks_init();
void ft_docks_release();

#ifdef __cplusplus
} // extern "C"
#endif
