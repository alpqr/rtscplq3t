/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "sceneplayer.h"
#include <Qt3DCore/QTransform>
#include <Qt3DRender/QCamera>
#include <Qt3DRender/QDirectionalLight>
#include <Qt3DRender/QMesh>
#include <Qt3DExtras/QForwardRenderer>
#include <Qt3DExtras/QPhongMaterial>
#include <Qt3DAnimation/QKeyframeAnimation>
#include <QPropertyAnimation>

ScenePlayer::ScenePlayer(QNode *parent)
    : Qt3DCore::QEntity(parent),
      m_parser(new SceneParser)
{
}

QString ScenePlayer::filename() const
{
    return m_filename;
}

void ScenePlayer::setFilename(const QString &fn)
{
    m_filename = fn;
    m_parser->load(fn);
    QObject::connect(&m_watcher, &QFutureWatcherBase::finished, [this] {
        setupScene(m_watcher.result());
    });
    m_watcher.setFuture(*m_parser->future());
}

void ScenePlayer::setAspectRatio(qreal ratio)
{
    if (m_aspectRatio != ratio) {
        m_aspectRatio = ratio;
        Qt3DExtras::QForwardRenderer *r = qobject_cast<Qt3DExtras::QForwardRenderer *>(m_renderer);
        if (r) {
            Qt3DRender::QCamera *cam = qobject_cast<Qt3DRender::QCamera *>(r->camera());
            if (cam)
                cam->setAspectRatio(m_aspectRatio);
        }
    }
}

void ScenePlayer::setupScene(const SceneData &sd)
{
    Qt3DExtras::QForwardRenderer *r = qobject_cast<Qt3DExtras::QForwardRenderer *>(m_renderer);
    Q_ASSERT(r); // must have been set by the time the async file parsing finishes

    if (sd.frames.isEmpty()) {
        qWarning("No keyframes");
        return;
    }

    const SceneData::Frame &firstFrame(sd.frames.first());
    if (firstFrame.t != 0) {
        qWarning("Keyframe @0 is mandatory"); // for now
        return;
    }

    // Initial camera settings
    if (!sd.cameras.isEmpty()) {
        if (sd.cameras.count() > 1)
            qWarning("Multiple cameras; only one will be used");

        const QByteArray &camId(*sd.cameras.constBegin());
        SceneData::CameraChange ch = firstFrame.cameraChanges[camId];

        Qt3DRender::QCamera *cam = new Qt3DRender::QCamera(this);
        cam->setProjectionType(Qt3DRender::QCameraLens::PerspectiveProjection);
        cam->setFieldOfView(45);
        cam->setAspectRatio(m_aspectRatio);
        cam->setNearPlane(0.01f);
        cam->setFarPlane(1000.0f);
        cam->setUpVector(QVector3D(0.0f, 1.0f, 0.0f));
        if (ch.change & SceneData::CameraChange::Position)
            cam->setPosition(ch.position);
        else
            cam->setPosition(QVector3D(0, 0, -10));
        if (ch.change & SceneData::CameraChange::ViewCenter)
            cam->setViewCenter(ch.viewCenter);
        else
            cam->setViewCenter(QVector3D(0, 0, 0));

        r->setCamera(cam);
    } else {
        qWarning("No camera");
    }

    // Initial light settings
    for (const QByteArray &lightId : sd.lights) {
        Qt3DCore::QEntity *lightEntity = new Qt3DCore::QEntity(this);
        Qt3DRender::QDirectionalLight *light = new Qt3DRender::QDirectionalLight;
        light->setWorldDirection(QVector3D(0, 0, 1));
        Qt3DCore::QTransform *t = new Qt3DCore::QTransform;
        lightEntity->addComponent(light);
        lightEntity->addComponent(t);
        if (firstFrame.lightChanges.contains(lightId)) {
            const SceneData::LightChange &ch(firstFrame.lightChanges[lightId]);
            if (ch.change & SceneData::LightChange::Position)
                t->setTranslation(ch.position);
        }
    }

    // Models.
    recursiveAddModels(sd.models, firstFrame, this);

    // Animation.
    addAnimations(sd);
}

void ScenePlayer::recursiveAddModels(const QHash<QByteArray, SceneData::Model> &models,
                                     const SceneData::Frame &firstFrame,
                                     Qt3DCore::QEntity *parentEntity)
{
    for (auto it = models.cbegin(), ite = models.cend(); it != ite; ++it) {
        const SceneData::Model &mdl(it.value());
        mdl.filename;

        Qt3DCore::QEntity *modelEntity = new Qt3DCore::QEntity(parentEntity);
        Qt3DRender::QMesh *mesh = new Qt3DRender::QMesh;
        mesh->setSource(QUrl("qrc:/" + mdl.filename));
        Qt3DExtras::QPhongMaterial *mat = new Qt3DExtras::QPhongMaterial;
        Qt3DCore::QTransform *t = new Qt3DCore::QTransform;
        modelEntity->addComponent(mesh);
        modelEntity->addComponent(mat);
        modelEntity->addComponent(t);

        if (firstFrame.modelChanges.contains(it.key())) {
            const SceneData::ModelChange &ch(firstFrame.modelChanges[it.key()]);
            if (ch.change & SceneData::ModelChange::Translation)
                t->setTranslation(ch.translation);
            if (ch.change & SceneData::ModelChange::Rotation) {
                t->setRotationX(ch.rotation.x());
                t->setRotationY(ch.rotation.y());
                t->setRotationZ(ch.rotation.z());
            }
            if (ch.change & SceneData::ModelChange::Scale)
                t->setScale3D(ch.scale);
            if (ch.change & SceneData::ModelChange::Color)
                mat->setDiffuse(ch.color);
        }

        AnimData a;
        a.t = t;
        m_animTargets.insert(it.key(), a);

        recursiveAddModels(mdl.childModels, firstFrame, modelEntity);
    }
}

void ScenePlayer::addAnimations(const SceneData &sd)
{
    // ## only support animating one single model for now

    Qt3DAnimation::QKeyframeAnimation *a = new Qt3DAnimation::QKeyframeAnimation;
    QVector<Qt3DCore::QTransform *> animTransforms;
    QVector<float> animPos;

    for (const SceneData::Frame &f : sd.frames) {
        if (f.t == 0)
            continue;
        if (!f.modelChanges.isEmpty()) {
            for (auto it = f.modelChanges.cbegin(), ite = f.modelChanges.cend(); it != ite; ++it) {
                AnimData &animTarget(m_animTargets[it.key()]);
//                if (!a.a)
//                    a.a = new Qt3DAnimation::QKeyframeAnimation;
                const SceneData::ModelChange &ch(it.value());
                Qt3DCore::QTransform *t = new Qt3DCore::QTransform;
                if (animTransforms.isEmpty()) {
                    t->setTranslation(animTarget.t->translation());
                    t->setRotation(animTarget.t->rotation());
                    t->setScale(animTarget.t->scale());
                    qDebug() << "taking from original" << t->rotationX() << t->rotationY() << t->rotationZ();
                    Qt3DCore::QTransform *initialTrans = new Qt3DCore::QTransform;
                    initialTrans->setTranslation(t->translation());
                    initialTrans->setRotation(t->rotation());
                    initialTrans->setScale(t->scale());
                    animPos.append(0);
                    animTransforms.append(initialTrans);
                } else {
                    t->setTranslation(animTransforms.last()->translation());
                    t->setRotation(animTransforms.last()->rotation());
                    t->setScale(animTransforms.last()->scale());
                    qDebug() << "taking from previous" << t->rotationX() << t->rotationY() << t->rotationZ();
                }
                if (ch.change & SceneData::ModelChange::TranslationX) {
                    QVector3D v = t->translation();
                    v.setX(ch.translation.x());
                    t->setTranslation(v);
                }
                if (ch.change & SceneData::ModelChange::TranslationY) {
                    QVector3D v = t->translation();
                    v.setY(ch.translation.y());
                    t->setTranslation(v);
                }
                if (ch.change & SceneData::ModelChange::TranslationZ) {
                    QVector3D v = t->translation();
                    v.setZ(ch.translation.z());
                    t->setTranslation(v);
                }
                if (ch.change & SceneData::ModelChange::RotationX)
                    t->setRotationX(ch.rotation.x());
                if (ch.change & SceneData::ModelChange::RotationY)
                    t->setRotationY(ch.rotation.y());
                if (ch.change & SceneData::ModelChange::RotationZ)
                    t->setRotationZ(ch.rotation.z());
                if (ch.change & SceneData::ModelChange::ScaleX) {
                    QVector3D v = t->scale3D();
                    v.setX(ch.scale.x());
                    t->setScale3D(v);
                }
                if (ch.change & SceneData::ModelChange::ScaleY) {
                    QVector3D v = t->scale3D();
                    v.setY(ch.scale.y());
                    t->setScale3D(v);
                }
                if (ch.change & SceneData::ModelChange::ScaleZ) {
                    QVector3D v = t->scale3D();
                    v.setZ(ch.scale.z());
                    t->setScale3D(v);
                }
                if (ch.change & SceneData::ModelChange::Color)
                    qWarning("Color animation not yet supported");
                animTransforms.append(t);
                animPos.append(f.t);
                a->setTarget(animTarget.t);
            }
        }
    }

    animPos.append(sd.totalTime);
    animTransforms.append(animTransforms.last());

    a->setFramePositions(animPos);
    a->setKeyframes(animTransforms);
    qDebug() << animPos;
    qDebug() << animTransforms;

    QPropertyAnimation *driverAnim = new QPropertyAnimation(a);
    driverAnim->setDuration(sd.totalTime);
    driverAnim->setStartValue(QVariant::fromValue(0.0f));
    driverAnim->setEndValue(QVariant::fromValue(float(sd.totalTime)));
    driverAnim->setLoopCount(-1);
    driverAnim->setTargetObject(a);
    driverAnim->setPropertyName("position");
    driverAnim->start();
}
