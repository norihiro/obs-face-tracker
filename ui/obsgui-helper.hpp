#pragma once
#include <QWindow>
#if !defined(_WIN32) && !defined(__APPLE__) // if Linux
#include <obs-nix-platform.h>
#include <QGuiApplication>
#include <qpa/qplatformnativeinterface.h>
#endif

// copied from obs-studio/UI/qt-wrappers.cpp and modified to support OBS-26
static inline
bool QTToGSWindow(QWindow *window, gs_window &gswindow)
{
	bool success = true;

#ifdef _WIN32
	gswindow.hwnd = (HWND)window->winId();
#elif __APPLE__
	gswindow.view = (id)window->winId();
#else
#ifdef ENABLE_WAYLAND
	switch (obs_get_nix_platform()) {
	case OBS_NIX_PLATFORM_X11_GLX:
	case OBS_NIX_PLATFORM_X11_EGL:
#endif // ENABLE_WAYLAND
		gswindow.id = window->winId();
		gswindow.display = obs_get_nix_platform_display();
#ifdef ENABLE_WAYLAND
		break;
	case OBS_NIX_PLATFORM_WAYLAND:
		QPlatformNativeInterface *native =
			QGuiApplication::platformNativeInterface();
		gswindow.display =
			native->nativeResourceForWindow("surface", window);
		success = gswindow.display != nullptr;
		break;
	}
#endif // ENABLE_WAYLAND
#endif
	return success;
}

// copied from obs-studio/UI/display-helpers.hpp
static inline QSize GetPixelSize(QWidget *widget)
{
	return widget->size() * widget->devicePixelRatioF();
}
