#pragma once

#include <QIcon>
#include <QQuickImageProvider>

// image://icon/имя1,имя2 — первая найденная в теме иконка (тема как в GTK)
class IconProvider : public QQuickImageProvider
{
public:
    IconProvider()
        : QQuickImageProvider(QQuickImageProvider::Pixmap)
    {
    }

    QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requested) override
    {
        const int s = requested.width() > 0 ? requested.width() : 48;
        QIcon icon;
        for (const QString &name : id.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
            if (QIcon::hasThemeIcon(name)) {
                icon = QIcon::fromTheme(name);
                break;
            }
        }
        if (icon.isNull())
            icon = QIcon::fromTheme(QStringLiteral("text-x-generic"));
        const QPixmap pm = icon.pixmap(QSize(s, s));
        if (size)
            *size = pm.size();
        return pm;
    }
};
