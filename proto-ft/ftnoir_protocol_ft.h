/* Copyright (c) 2013-2015 Stanislaw Halik <sthalik@misaki.pl>
 * Copyright (c) 2015 Wim Vriend
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 */

#pragma once
#include "ui_ftnoir_ftcontrols.h"
#include "api/plugin-api.hpp"

#include <QProcess>
#include <QString>
#include <QMutex>

#include <QDebug>

#include <cinttypes>
#include "freetrackclient/fttypes.h"

#include "compat/shm.h"
#include "options/options.hpp"

#include <memory>

using namespace options;

struct settings : opts {
    value<int> intUsedInterface;
    settings() :
        opts("proto-freetrack"),
        intUsedInterface(b, "used-interfaces", 0)
    {}
};

class freetrack : TR, public IProtocol
{
    Q_OBJECT

public:
    freetrack() = default;
    ~freetrack() override;
    module_status initialize() override;
    void pose(const double* pose, const double*) override;
    QString game_name() override;
private:
    settings s;
    shm_wrapper shm { FREETRACK_HEAP, FREETRACK_MUTEX, sizeof(FTHeap) };
    FTHeap* pMemData { (FTHeap*) shm.ptr() };

    QProcess dummyTrackIR;

    int intGameID = -1;
    QString connected_game;
    QMutex game_name_mutex;

    void start_dummy();

public:
    static void set_protocols(bool ft, bool npclient);
};

class FTControls: public IProtocolDialog
{
    Q_OBJECT
public:
    FTControls();
    void register_protocol(IProtocol *) {}
    void unregister_protocol() {}
private:
    Ui::UICFTControls ui;
    settings s;
private slots:
    void selectDLL();
    void doOK();
    void doCancel();
};

class freetrackDll : public Metadata
{
    Q_OBJECT

public:
    QString name() { return tr("freetrack 2.0 Enhanced"); }
    QIcon icon() { return QIcon(":/images/freetrack.png"); }
};
