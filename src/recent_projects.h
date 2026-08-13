#pragma once

#include <QString>
#include <QDateTime>
#include <QList>

// The set of project-file shapes vBird knows how to read. Kept as an enum
// rather than sniffing the JSON every time so the user's choice at the
// startup dialog is authoritative -- auto-detection is a nice-to-have, not
// a requirement, for a file extension (.json) that isn't unique to us.
enum class ProjectFormat {
    Auto,          // inspect the file and guess
    VBirdNative,   // vBird's own schema
    VortexLegacy,  // project files exported by the original Vortex Studio
};

QString projectFormatDisplayName(ProjectFormat format);
QString projectFormatToKey(ProjectFormat format);       // stable string for JSON storage
ProjectFormat projectFormatFromKey(const QString &key);  // inverse; defaults to Auto on unknown input

struct RecentProjectEntry {
    QString path;
    ProjectFormat format = ProjectFormat::Auto;
    QDateTime lastOpened;
};

// Reads/writes the recent-projects list at
// %APPDATA%/vbird/recent_projects.json (QStandardPaths::AppDataLocation on
// other platforms). Deliberately minimal: no caching beyond the in-memory
// list returned by load(), no file watching. Good enough for a handful of
// entries opened from one dialog.
class RecentProjects {
public:
    // Reads the list from disk, newest first. Returns an empty list (not an
    // error) if the file doesn't exist yet -- that's just a fresh install.
    static QList<RecentProjectEntry> load();

    // Rewrites the whole file. Callers mutate the list returned by load()
    // and pass it back rather than this class tracking state itself.
    static void save(const QList<RecentProjectEntry> &entries);

    // Convenience: load, move-or-insert `entry` to the front (deduped by
    // path), trim to `maxEntries`, save. This is what StartupDialog calls
    // when the user actually opens something.
    static void addEntry(const RecentProjectEntry &entry, int maxEntries = 10);

    // Convenience: load, drop the entry with this path, save.
    static void removeEntry(const QString &path);

private:
    static QString filePath();
};
