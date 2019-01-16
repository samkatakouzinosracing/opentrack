/* Copyright (c) 2013, 2015 Stanislaw Halik <sthalik@misaki.pl>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 */

#pragma once

#include "api/plugin-api.hpp"
#include "compat/euler.hpp"

#include "export.hpp"

#include <tuple>
#include <mutex>
#include <atomic>
#include <vector>

#include <QWidget>
#include <QThread>
#include <QImage>

namespace pose_widget_impl {

using num = float;
using vec3 = Mat<num, 3, 1>;
using vec2 = Mat<num, 2, 1>;
using vec2i = Mat<int, 2, 1>;
using vec2u = Mat<int, 2, 1>;

using rmat = Mat<num, 3, 3>;

using namespace euler;

using lock_guard = std::unique_lock<std::mutex>;

class pose_widget;

class Triangle {
    num dot00, dot01, dot11, invDenom;
    vec2 v0, v1, origin;
public:
    Triangle(const vec2& p1, const vec2& p2, const vec2& p3);
    bool barycentric_coords(const vec2& px, vec2& uv, int& i) const;
};

struct pose_transform final : QThread
{
    pose_transform(QWidget* dst, double device_pixel_ratio);
    ~pose_transform() override;

    void rotate_async(double xAngle, double yAngle, double zAngle, double x, double y, double z);
    void rotate_sync(double xAngle, double yAngle, double zAngle, double x, double y, double z);

    template<typename F>
    void with_rotate(F&& fun, double xAngle, double yAngle, double zAngle, double x, double y, double z);

    void run() override;

    vec2 project(const vec3& point);
    vec3 project2(const vec3& point);
    void project_quad_texture();
    std::pair<vec2i, vec2i> get_bounds(const vec2& size);

    template<typename F> inline void with_image_lock(F&& fun);

    rmat rotation, rotation_;
    vec3 translation, translation_;

    std::mutex mtx, mtx2;

    QWidget* dst;

    QImage front{QImage{":/images/side1.png"}.convertToFormat(QImage::Format_ARGB32)};
    QImage back{QImage{":/images/side6.png"}.convertToFormat(QImage::Format_ARGB32)};
    QImage image{w, h, QImage::Format_ARGB32};
    QImage image2{w, h, QImage::Format_ARGB32};

    struct uv_ // NOLINT(cppcoreguidelines-pro-type-member-init)
    {
        vec2 coords;
        int i;
    };

    std::vector<uv_> uv_vec;
    std::atomic<bool> fresh{false};

    static constexpr int w = 320, h = 240;
};

class OTR_POSE_WIDGET_EXPORT pose_widget final : public QWidget
{
public:
    pose_widget(QWidget *parent = nullptr);
    ~pose_widget() override;
    void rotate_async(double xAngle, double yAngle, double zAngle, double x, double y, double z);
    void rotate_sync(double xAngle, double yAngle, double zAngle, double x, double y, double z);

private:
    pose_transform xform;
    void paintEvent(QPaintEvent *event) override;
};

}

using pose_widget = pose_widget_impl::pose_widget;
