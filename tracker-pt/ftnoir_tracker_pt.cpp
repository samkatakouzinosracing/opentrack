/* Copyright (c) 2012 Patrick Ruoff
 * Copyright (c) 2014-2016 Stanislaw Halik <sthalik@misaki.pl>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 */

#include "ftnoir_tracker_pt.h"
#include "pt-api.hpp"
#include "cv/init.hpp"
#include "video/video-widget.hpp"
#include "compat/math-imports.hpp"
#include "compat/check-visible.hpp"
#include "compat/thread-name.hpp"

#include <QHBoxLayout>
#include <QDebug>
#include <QFile>
#include <QCoreApplication>

using namespace options;

namespace pt_impl {

Tracker_PT::Tracker_PT(pointer<pt_runtime_traits> const& traits) :
    traits { traits },
    s { traits->get_module_name() },
    point_extractor { traits->make_point_extractor() },
    camera { traits->make_camera() },
    frame { traits->make_frame() },
    preview_frame { traits->make_preview(preview_width, preview_height) }
{
    opencv_init();

    connect(s.b.get(), &bundle_::saving, this, &Tracker_PT::maybe_reopen_camera, Qt::DirectConnection);
    connect(s.b.get(), &bundle_::reloading, this, &Tracker_PT::maybe_reopen_camera, Qt::DirectConnection);

    connect(&s.fov, value_::value_changed<int>(), this, &Tracker_PT::set_fov, Qt::DirectConnection);
    set_fov(s.fov);
}

Tracker_PT::~Tracker_PT()
{
    requestInterruption();
    wait();

    QMutexLocker l(&camera_mtx);
    camera->stop();
}

void Tracker_PT::run()
{
    portable::set_curthread_name("tracker/pt");

    if (!maybe_reopen_camera())
        return;

    while(!isInterruptionRequested())
    {
        pt_camera_info info;
        bool new_frame = false;

        {
            QMutexLocker l(&camera_mtx);
            std::tie(new_frame, info) = camera->get_frame(*frame);
        }

        if (new_frame)
        {
            const bool preview_visible = check_is_visible();

            if (preview_visible)
                *preview_frame = *frame;

            point_extractor->extract_points(*frame, *preview_frame, points);
            point_count.store(points.size(), std::memory_order_relaxed);

            const bool success = points.size() >= PointModel::N_POINTS;

            Affine X_CM;

            {
                QMutexLocker l(&center_lock);

                if (success)
                {
                    int dynamic_pose_ms = s.dynamic_pose ? s.init_phase_timeout : 0;

                    point_tracker.track(points,
                                        PointModel(s),
                                        info,
                                        dynamic_pose_ms);
                    ever_success.store(true, std::memory_order_relaxed);
                }

                QMutexLocker l2(&data_lock);
                X_CM = point_tracker.pose();
            }

            if (preview_visible)
            {
                const f fx = pt_camera_info::get_focal_length(info.fov, info.res_x, info.res_y);
                Affine X_MH(mat33::eye(), vec3(s.t_MH_x, s.t_MH_y, s.t_MH_z));
                Affine X_GH = X_CM * X_MH;
                vec3 p = X_GH.t; // head (center?) position in global space

                if (p[2] > f(.1))
                    preview_frame->draw_head_center((p[0] * fx) / p[2], (p[1] * fx) / p[2]);

                widget->update_image(preview_frame->get_bitmap());

                auto [ w, h ] = widget->preview_size();
                if (w != preview_width || h != preview_height)
                {
                    preview_width = w; preview_height = h;
                    preview_frame = traits->make_preview(w, h);
                }
            }
        }
    }
}

bool Tracker_PT::maybe_reopen_camera()
{
    QMutexLocker l(&camera_mtx);

    return camera->start(s.camera_name,
                         s.cam_fps, s.cam_res_x, s.cam_res_y);
}

void Tracker_PT::set_fov(int value)
{
    QMutexLocker l(&camera_mtx);
    camera->set_fov(value);
}

module_status Tracker_PT::start_tracker(QFrame* video_frame)
{
    //video_frame->setAttribute(Qt::WA_NativeWindow);

    widget = std::make_unique<video_widget>(video_frame);
    layout = std::make_unique<QHBoxLayout>(video_frame);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(widget.get());
    video_frame->setLayout(layout.get());
    //video_widget->resize(video_frame->width(), video_frame->height());
    video_frame->show();

    start(QThread::HighPriority);

    return {};
}

void Tracker_PT::data(double *data)
{
    if (ever_success.load(std::memory_order_relaxed))
    {
        Affine X_CM;
        {
            QMutexLocker l(&data_lock);
            X_CM = point_tracker.pose();
        }

        Affine X_MH(mat33::eye(), vec3(s.t_MH_x, s.t_MH_y, s.t_MH_z));
        Affine X_GH(X_CM * X_MH);

        // translate rotation matrix from opengl (G) to roll-pitch-yaw (E) frame
        // -z -> x, y -> z, x -> -y
        mat33 R_EG(0, 0,-1,
                   -1, 0, 0,
                   0, 1, 0);
        mat33 R(R_EG *  X_GH.R * R_EG.t());

        // get translation(s)
        const vec3& t = X_GH.t;

        // extract rotation angles
        auto r00 = (double)R(0, 0);
        auto r10 = (double)R(1,0), r20 = (double)R(2,0);
        auto r21 = (double)R(2,1), r22 = (double)R(2,2);

        double beta  = atan2(-r20, sqrt(r21*r21 + r22*r22));
        double alpha = atan2(r10, r00);
        double gamma = atan2(r21, r22);

        constexpr double rad2deg = 180/M_PI;

        data[Yaw]   = rad2deg * alpha;
        data[Pitch] = -rad2deg * beta;
        data[Roll]  = rad2deg * gamma;

        // convert to cm
        data[TX] = (double)t[0] / 10;
        data[TY] = (double)t[1] / 10;
        data[TZ] = (double)t[2] / 10;
    }
}

bool Tracker_PT::center()
{
    QMutexLocker l(&center_lock);

    point_tracker.reset_state();
    return false;
}

int Tracker_PT::get_n_points()
{
    return (int)point_count.load(std::memory_order_relaxed);
}

bool Tracker_PT::get_cam_info(pt_camera_info& info)
{
    QMutexLocker l(&camera_mtx);
    bool ret;

    std::tie(ret, info) = camera->get_info();
    return ret;
}

Affine Tracker_PT::pose() const
{
    QMutexLocker l(&data_lock);
    return point_tracker.pose();
}

} // ns pt_impl
