#include "recent_projects.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <algorithm>

QString projectFormatDisplayName(ProjectFormat format) {
    switch (format) {
    case ProjectFormat::Auto: return QStringLiteral("Auto-detect");
    case ProjectFormat::VBirdNative: return QStringLiteral("vBird Project (.json)");
    case ProjectFormat::VortexLegacy: return QStringLiteral("Vortex Studio Legacy (.json)");
    }
    return QStringLiteral("Auto-detect");
}

QString projectFormatToKey(ProjectFormat format) {
    switch (format) {
    case ProjectFormat::Auto: return QStringLiteral("auto");
    case ProjectFormat::VBirdNative: return QStringLiteral("vbird");
    case ProjectFormat::VortexLegacy: return QStringLiteral("vortex_legacy");
    }
    return QStringLiteral("auto");
}

ProjectFormat projectFormatFromKey(const QString &key) {
    if (key == QStringLiteral("vbird")) return ProjectFormat::VBirdNative;
    if (key == QStringLiteral("vortex_legacy")) return ProjectFormat::VortexLegacy;
    return ProjectFormat::Auto;
}

QString RecentProjects::filePath() {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/recent_projects.json");
}

QList<RecentProjectEntry> RecentProjects::load() {
    QList<RecentProjectEntry> result;
    QFile file(filePath());
    if (!file.open(QIODevice::ReadOnly)) return result;

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isArray()) return result;

    for (const QJsonValue &value : doc.array()) {
        const QJsonObject object = value.toObject();
        RecentProjectEntry entry;
        entry.path = object.value(QStringLiteral("path")).toString();
        entry.format = projectFormatFromKey(object.value(QStringLiteral("format")).toString());
        entry.lastOpened = QDateTime::fromString(object.value(QStringLiteral("last_opened")).toString(), Qt::ISODate);
        if (!entry.path.isEmpty()) result.append(entry);
    }
    return result;
}

void RecentProjects::save(const QList<RecentProjectEntry> &entries) {
    QJsonArray array;
    for (const RecentProjectEntry &entry : entries) {
        QJsonObject object;
        object[QStringLiteral("path")] = entry.path;
        object[QStringLiteral("format")] = projectFormatToKey(entry.format);
        object[QStringLiteral("last_opened")] = entry.lastOpened.toString(Qt::ISODate);
        array.append(object);
    }

    QFile file(filePath());
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        file.write(QJsonDocument(array).toJson(QJsonDocument::Indented));
}

void RecentProjects::addEntry(const RecentProjectEntry &entry, int maxEntries) {
    auto entries = load();
    entries.erase(std::remove_if(entries.begin(), entries.end(), [&](const RecentProjectEntry &item) {
        return item.path == entry.path;
    }), entries.end());
    entries.prepend(entry);
    while (entries.size() > maxEntries) entries.removeLast();
    save(entries);
}

void RecentProjects::removeEntry(const QString &path) {
    auto entries = load();
    entries.erase(std::remove_if(entries.begin(), entries.end(), [&](const RecentProjectEntry &item) {
        return item.path == path;
    }), entries.end());
    save(entries);
}
