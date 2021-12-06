#pragma once

#ifdef __cplusplus
#include <QDockWidget>
#include <QPointer>
#include <QAction>
#include <string>
#include <obs.h>

class FTDock : public QDockWidget {
	Q_OBJECT

public:
	class FTWidget *widget;
	std::string name;
	QPointer<QAction> action = 0;
	struct face_tracker_dock_s *data;

public:
	FTDock(QWidget *parent = nullptr);
	~FTDock();
	void closeEvent(QCloseEvent *event) override;

	static void default_properties(obs_data_t *);
	void save_properties(obs_data_t*);
	void load_properties(obs_data_t*);

	void SetWidget(class FTWidget *w);

private:
	void showEvent(QShowEvent *event) override;
	void hideEvent(QHideEvent *event) override;
};

extern "C" {
#endif // __cplusplus
void ft_dock_add(const char *name, obs_data_t *props);
void ft_docks_init();
void ft_docks_release();

#ifdef __cplusplus
} // extern "C"
#endif
