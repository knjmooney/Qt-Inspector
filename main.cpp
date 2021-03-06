
#include "ExternalWidgetPicker.h"
#include "GdbLibraryInjector.h"
#include "PreloadInjector.h"
#include "TargetApplicationProxy.h"
#include "WidgetInspector.h"

#include "lib/InspectorServer.h"
#include "lib/ObjectProxy.h"
#include "lib/PlatformUtils.h"

#include <QApplication>
#include <QtCore/QLibrary>
#include <QtCore/QProcess>

#include <QtDebug>

QString injectorLibPath()
{
#if defined(Q_OS_LINUX) || defined(Q_OS_MAC)
	// get path of shared library containing the qtInspectorInit entry
	// point. See notes about this in CMakeLists.txt
	return PlatformUtils::binaryPath(reinterpret_cast<void*>(&qtInspectorInit));
#else
	return "lib/QtInspector.dll";
#endif
}

int main(int argc, char** argv)
{
	QApplication app(argc,argv);

	QStringList args = app.arguments();
	if (args.count() < 2)
	{
		qWarning() << "Usage: qtinspector <pid>|<program> (<args>...)";
		return -1;
	}

	QProcess process;
	int targetPid = args.at(1).toInt();

	// inject the helper library
	QScopedPointer<Injector> injector;
	if (targetPid != 0)
	{
#ifdef Q_OS_UNIX
		injector.reset(new GdbLibraryInjector);
#endif
		if (!injector->inject(targetPid, injectorLibPath(), "qtInspectorInit"))
		{
			return false;
		}
	}
	else
	{
#ifdef Q_OS_UNIX
		injector.reset(new PreloadInjector);
#endif
		QStringList programArgs;
		for (int i=2; i < args.count(); i++)
		{
			programArgs << args.at(i);
		}
		if (!injector->startAndInject(args.at(1),programArgs,injectorLibPath(),"qtInspectorInit",&targetPid))
		{
			return false;
		}
	}

	TargetApplicationProxy proxy;
	if (!proxy.connectToTarget(targetPid))
	{
		qWarning() << "Failed to inject helper library into process <pid>";
	}

	WidgetPicker* picker = new ExternalWidgetPicker(&proxy,0);

	WidgetInspector inspector(&proxy);
	inspector.setWidgetPicker(picker);
	inspector.show();

	int result = app.exec();

	if (process.state() == QProcess::Running && !process.waitForFinished())
	{
		qWarning() << "Failed to wait for process" << process.pid() << "to exit";
	}

	return result;
}

