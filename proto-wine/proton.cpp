/* Copyright (c) 2019 Stanislaw Halik <sthalik@misaki.pl>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 */

#ifndef OTR_WINE_NO_WRAPPER

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QtGlobal>


static const char* steam_paths[] = {
    "/.steam/steam/steamapps/compatdata",
    "/.local/share/Steam/steamapps/compatdata",
};

static const char* runtime_paths[] = {
    "/.local/share/Steam/ubuntu12_32/steam-runtime",
    "/.steam/ubuntu12_32/steam-runtime",
};


std::tuple<QProcessEnvironment, QString, bool> make_steam_environ(const QString& proton_path, int appid)
{
    using ret = std::tuple<QProcessEnvironment, QString, bool>;
    auto env = QProcessEnvironment::systemEnvironment();
    QString error = "";
    QString home = qgetenv("HOME");
    QString runtime_path, app_wineprefix;

    auto expand = [&](QString x) {
                      x.replace("HOME", home);
                      x.replace("PROTON_PATH", proton_path);
                      x.replace("RUNTIME_PATH", runtime_path);
                      return x;
                  };

    for (const char* path : runtime_paths) {
        QDir dir(QDir::homePath() + path);
        if (dir.exists())
            runtime_path = dir.absolutePath();
    }

    if (runtime_path.isEmpty())
        error = QString("Couldn't find a Steam runtime.");

    for (const char* path : steam_paths) {
        QDir dir(QDir::homePath() + path + expand("/%1/pfx").arg(appid));
        if (dir.exists())
            app_wineprefix = dir.absolutePath();
    }
    if (app_wineprefix.isEmpty())
        error = QString("Couldn't find a Wineprefix for AppId %1").arg(appid);

    QString path = expand(
        ":PROTON_PATH/dist/bin"
    );
    path += ':'; path += qgetenv("PATH");
    env.insert("PATH", path);

    QString library_path = expand(
        ":PROTON_PATH/dist/lib"
        ":PROTON_PATH/dist/lib64"
        ":RUNTIME_PATH/pinned_libs_32"
        ":RUNTIME_PATH/pinned_libs_64"
        ":RUNTIME_PATH/i386/lib/i386-linux-gnu"
        ":RUNTIME_PATH/i386/lib"
        ":RUNTIME_PATH/i386/usr/lib/i386-linux-gnu"
        ":RUNTIME_PATH/i386/usr/lib"
        ":RUNTIME_PATH/amd64/lib/x86_64-linux-gnu"
        ":RUNTIME_PATH/amd64/lib"
        ":RUNTIME_PATH/amd64/usr/lib/x86_64-linux-gnu"
        ":RUNTIME_PATH/amd64/usr/lib"
    );
    library_path += ':'; library_path += qgetenv("LD_LIBRARY_PATH");
    env.insert("LD_LIBRARY_PATH", library_path);
    env.insert("WINEPREFIX", app_wineprefix);

    return ret(env, error, error.isEmpty());
}

QString proton_path(const QString& proton_path)
{
    return proton_path + "/dist/bin/wine";
}

#endif
