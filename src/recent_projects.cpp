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
        case ProjectFormat::Auto:         return QStringLiteral("Auto-detect");
        case ProjectFormat::VBirdNative:  return QStringLiteral("vBird Project (.json)");
        case ProjectFormat::VortexLegacy: return QStringLiteral("Vortex Studio Legacy (.json)");
    }
    return QStringLiteral("Unknown");
}

QString projectFormatToKey(ProjectFormat format) {
    switch (format) {
        case ProjectFormat::Auto:         return QStringLiteral("auto");
        case ProjectFormat::VBirdNative:  return QStringLiteral("vbird");
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
    if (!file.open(QIODevice::ReadOnly)) {
        return result; // no file yet -- not an error
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isArray()) {
        return result;
    }

    for (const QJsonValue &value : doc.array()) {
        const QJsonObject obj = value.toObject();
        RecentProjectEntry entry;
        entry.path = obj.value(QStringLiteral("path")).toString();
        entry.format = projectFormatFromKey(obj.value(QStringLiteral("format")).toString());
        entry.lastOpened = QDateTime::fromString(
            obj.value(QStringLiteral("last_opened")).toString(), Qt::ISODate);
        if (!entry.path.isEmpty()) {
            result.append(entry);
        }
    }
    return result;
}

void RecentProjects::save(const QList<RecentProjectEntry> &entries) {
    QJsonArray array;
    for (const RecentProjectEntry &entry : entries) {
        QJsonObject obj;
        obj[QStringLiteral("path")] = entry.path;
        obj[QStringLiteral("format")] = projectFormatToKey(entry.format);
        obj[QStringLiteral("last_opened")] = entry.lastOpened.toString(Qt::ISODate);
        array.append(obj);
    }

    QFile file(filePath());
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(QJsonDocument(array).toJson(QJsonDocument::Indented));
    }
}

void RecentProjects::addEntry(const RecentProjectEntry &entry, int maxEntries) {
    QList<RecentProjectEntry> entries = load();

    // Dedupe by path -- reopening something just bumps it to the top.
    entries.erase(
        std::remove_if(entries.begin(), entries.end(),
                        [&](const RecentProjectEntry &e) { return e.path == entry.path; }),
        entries.end());

    entries.prepend(entry);
    while (entries.size() > maxEntries) {
        entries.removeLast();
    }
    save(entries);
}

void RecentProjects::removeEntry(const QString &path) {
    QList<RecentProjectEntry> entries = load();
    entries.erase(
        std::remove_if(entries.begin(), entries.end(),
                        [&](const RecentProjectEntry &e) { return e.path == path; }),
        entries.end());
    save(entries);
}
