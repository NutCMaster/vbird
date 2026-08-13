#pragma once

#include <QDateTime>
#include <QList>
#include <QString>

enum class ProjectFormat {
    Auto,
    VBirdNative,
    VortexLegacy
};

QString projectFormatDisplayName(ProjectFormat format);
QString projectFormatToKey(ProjectFormat format);
ProjectFormat projectFormatFromKey(const QString &key);

struct RecentProjectEntry {
    QString path;
    ProjectFormat format = ProjectFormat::Auto;
    QDateTime lastOpened;
};

class RecentProjects {
public:
    static QList<RecentProjectEntry> load();
    static void save(const QList<RecentProjectEntry> &entries);
    static void addEntry(const RecentProjectEntry &entry, int maxEntries = 10);
    static void removeEntry(const QString &path);

private:
    static QString filePath();
};
