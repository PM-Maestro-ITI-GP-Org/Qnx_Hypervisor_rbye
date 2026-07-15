#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <cstdlib>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;
    const QUrl url(QStringLiteral("qrc:/main.qml"));

    engine.load(url);

    if (engine.rootObjects().isEmpty())
        return -1;

    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    if (window) {
        const char *w = std::getenv("APP_WIDTH");
        const char *h = std::getenv("APP_HEIGHT");
        if (w) window->setWidth(std::atoi(w));
        if (h) window->setHeight(std::atoi(h));
    }

    return app.exec();
}
