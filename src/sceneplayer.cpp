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
#include <Qt3DAnimation/QClipAnimator>
#include <Qt3DAnimation/QChannelMapper>
#include <Qt3DAnimation/QChannelMapping>
#include <Qt3DAnimation/QAnimationClip>

ScenePlayer::ScenePlayer(QNode *parent)
    : Qt3DCore::QEntity(parent),
      m_parser(new SceneParser)
{
}

ScenePlayer::~ScenePlayer()
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
            cam->setPosition(QVector3D(0, 0, 10));
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
        light->setWorldDirection(QVector3D(0, 0, -1));
        Qt3DCore::QTransform *t = new Qt3DCore::QTransform;
        lightEntity->addComponent(light);
        lightEntity->addComponent(t);
        if (firstFrame.lightChanges.contains(lightId)) {
            const SceneData::LightChange &ch(firstFrame.lightChanges[lightId]);
            if (ch.change & SceneData::LightChange::Position)
                t->setTranslation(ch.position);
        }
    }

    // Add models and their animations.
    recursiveAddModels(sd, sd.models, this);
}

void ScenePlayer::recursiveAddModels(const SceneData &sd,
                                     const QHash<QByteArray, SceneData::Model> &models,
                                     Qt3DCore::QEntity *parentEntity)
{
    const SceneData::Frame &firstFrame(sd.frames.first());

    for (auto it = models.cbegin(), ite = models.cend(); it != ite; ++it) {
        const SceneData::Model &mdl(it.value());

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

        addAnimations(sd, it.key(), modelEntity, t, mat);

        recursiveAddModels(sd, mdl.childModels, modelEntity);
    }
}

void ScenePlayer::addAnimations(const SceneData &sd,
                                const QByteArray &modelId,
                                Qt3DCore::QEntity *modelEntity,
                                Qt3DCore::QTransform *modelTransform,
                                Qt3DRender::QMaterial *modelMaterial)
{
    int changes = 0;
    for (const SceneData::Frame &f : sd.frames) {
        if (f.t != 0 && f.modelChanges.contains(modelId)) {
            const SceneData::ModelChange &ch(f.modelChanges[modelId]);
            changes |= ch.change;
        }
    }
    if (!changes)
        return;

    Qt3DAnimation::QClipAnimator *animator = new Qt3DAnimation::QClipAnimator;
    Qt3DAnimation::QChannelMapper *mapper = new Qt3DAnimation::QChannelMapper;

    if (changes & SceneData::ModelChange::Translation) {
        Qt3DAnimation::QChannelMapping *mapping = new Qt3DAnimation::QChannelMapping;
        mapping->setChannelName(QStringLiteral("Translation"));
        mapping->setTarget(modelTransform);
        mapping->setProperty(QStringLiteral("translation"));
        mapper->addMapping(mapping);
    }

    if (changes & SceneData::ModelChange::Rotation) {
        Qt3DAnimation::QChannelMapping *mapping = new Qt3DAnimation::QChannelMapping;
        mapping->setChannelName(QStringLiteral("Rotation"));
        mapping->setTarget(modelTransform);
        mapping->setProperty(QStringLiteral("rotation"));
        mapper->addMapping(mapping);
    }

    if (changes & SceneData::ModelChange::Scale) {
        Qt3DAnimation::QChannelMapping *mapping = new Qt3DAnimation::QChannelMapping;
        mapping->setChannelName(QStringLiteral("Scale"));
        mapping->setTarget(modelTransform);
        mapping->setProperty(QStringLiteral("scale3D"));
        mapper->addMapping(mapping);
    }

    if (changes & SceneData::ModelChange::Color) {
        Qt3DAnimation::QChannelMapping *mapping = new Qt3DAnimation::QChannelMapping;
        mapping->setChannelName(QStringLiteral("Color"));
        mapping->setTarget(modelMaterial);
        mapping->setProperty(QStringLiteral("diffuse"));
        mapper->addMapping(mapping);
    }

    animator->setChannelMapper(mapper);

    Qt3DAnimation::QAnimationClip *clip = new Qt3DAnimation::QAnimationClip;
    Qt3DAnimation::QAnimationClipData clipData;

    Qt3DAnimation::QChannel trans(QStringLiteral("Translation"));
    Qt3DAnimation::QChannelComponent transX(QStringLiteral("Translation X"));
    Qt3DAnimation::QChannelComponent transY(QStringLiteral("Translation Y"));
    Qt3DAnimation::QChannelComponent transZ(QStringLiteral("Translation Z"));

    Qt3DAnimation::QChannel rot(QStringLiteral("Rotation"));
    Qt3DAnimation::QChannelComponent rotX(QStringLiteral("Rotation X"));
    Qt3DAnimation::QChannelComponent rotY(QStringLiteral("Rotation Y"));
    Qt3DAnimation::QChannelComponent rotZ(QStringLiteral("Rotation Z"));
    Qt3DAnimation::QChannelComponent rotW(QStringLiteral("Rotation W"));

    Qt3DAnimation::QChannel scale(QStringLiteral("Scale"));
    Qt3DAnimation::QChannelComponent scaleX(QStringLiteral("Scale X"));
    Qt3DAnimation::QChannelComponent scaleY(QStringLiteral("Scale Y"));
    Qt3DAnimation::QChannelComponent scaleZ(QStringLiteral("Scale Z"));

    QVector3D curTrans = modelTransform->translation();
    QQuaternion curRot = modelTransform->rotation();
    QVector3D curScale = modelTransform->scale3D();

    // First frame.
    transX.appendKeyFrame(Qt3DAnimation::QKeyFrame(QVector2D(0, curTrans.x())));
    transY.appendKeyFrame(Qt3DAnimation::QKeyFrame(QVector2D(0, curTrans.y())));
    transZ.appendKeyFrame(Qt3DAnimation::QKeyFrame(QVector2D(0, curTrans.z())));
    rotX.appendKeyFrame(Qt3DAnimation::QKeyFrame(QVector2D(0, curRot.x())));
    rotY.appendKeyFrame(Qt3DAnimation::QKeyFrame(QVector2D(0, curRot.y())));
    rotZ.appendKeyFrame(Qt3DAnimation::QKeyFrame(QVector2D(0, curRot.z())));
    rotW.appendKeyFrame(Qt3DAnimation::QKeyFrame(QVector2D(0, curRot.scalar())));
    scaleX.appendKeyFrame(Qt3DAnimation::QKeyFrame(QVector2D(0, curScale.x())));
    scaleY.appendKeyFrame(Qt3DAnimation::QKeyFrame(QVector2D(0, curScale.y())));
    scaleZ.appendKeyFrame(Qt3DAnimation::QKeyFrame(QVector2D(0, curScale.z())));

    for (const SceneData::Frame &f : sd.frames) {
        if (f.t == 0)
            continue;

        if (!f.modelChanges.contains(modelId))
            continue;

        const SceneData::ModelChange &ch(f.modelChanges[modelId]);

        // Channels must be fully specified, meaning 3 (or 4) components are required always for t/r/s.

        if (ch.change & SceneData::ModelChange::Translation) {
            if (ch.change & SceneData::ModelChange::TranslationX) {
                curTrans.setX(ch.translation.x());
                transX.appendKeyFrame(Qt3DAnimation::QKeyFrame(QVector2D(f.t, ch.translation.x())));
            } else {
                transX.appendKeyFrame(Qt3DAnimation::QKeyFrame(QVector2D(f.t, curTrans.x())));
            }

            if (ch.change & SceneData::ModelChange::TranslationY) {
                curTrans.setY(ch.translation.y());
                transY.appendKeyFrame(Qt3DAnimation::QKeyFrame(QVector2D(f.t, ch.translation.y())));
            } else {
                transY.appendKeyFrame(Qt3DAnimation::QKeyFrame(QVector2D(f.t, curTrans.y())));
            }

            if (ch.change & SceneData::ModelChange::TranslationZ) {
                curTrans.setZ(ch.translation.z());
                transZ.appendKeyFrame(Qt3DAnimation::QKeyFrame(QVector2D(f.t, ch.translation.z())));
            } else {
                transZ.appendKeyFrame(Qt3DAnimation::QKeyFrame(QVector2D(f.t, curTrans.z())));
            }
        }

        if (ch.change & SceneData::ModelChange::Rotation) {
            QVector3D r = curRot.toEulerAngles();
            if (ch.change & SceneData::ModelChange::RotationX)
                r.setX(ch.rotation.x());
            if (ch.change & SceneData::ModelChange::RotationY)
                r.setY(ch.rotation.y());
            if (ch.change & SceneData::ModelChange::RotationZ)
                r.setZ(ch.rotation.z());
            curRot = QQuaternion::fromEulerAngles(r);
            rotX.appendKeyFrame(Qt3DAnimation::QKeyFrame(QVector2D(f.t, curRot.x())));
            rotY.appendKeyFrame(Qt3DAnimation::QKeyFrame(QVector2D(f.t, curRot.y())));
            rotZ.appendKeyFrame(Qt3DAnimation::QKeyFrame(QVector2D(f.t, curRot.z())));
            rotW.appendKeyFrame(Qt3DAnimation::QKeyFrame(QVector2D(f.t, curRot.scalar())));
        }

        if (ch.change & SceneData::ModelChange::Scale) {
            if (ch.change & SceneData::ModelChange::ScaleX) {
                curScale.setX(ch.scale.x());
                scaleX.appendKeyFrame(Qt3DAnimation::QKeyFrame(QVector2D(f.t, ch.scale.x())));
            } else {
                scaleX.appendKeyFrame(Qt3DAnimation::QKeyFrame(QVector2D(f.t, curScale.x())));
            }

            if (ch.change & SceneData::ModelChange::ScaleY) {
                curScale.setY(ch.scale.y());
                scaleY.appendKeyFrame(Qt3DAnimation::QKeyFrame(QVector2D(f.t, ch.scale.y())));
            } else {
                scaleY.appendKeyFrame(Qt3DAnimation::QKeyFrame(QVector2D(f.t, curScale.y())));
            }

            if (ch.change & SceneData::ModelChange::ScaleZ) {
                curScale.setZ(ch.scale.z());
                scaleZ.appendKeyFrame(Qt3DAnimation::QKeyFrame(QVector2D(f.t, ch.scale.z())));
            } else {
                scaleZ.appendKeyFrame(Qt3DAnimation::QKeyFrame(QVector2D(f.t, curScale.z())));
            }
        }
    }

    // Last frame.
    transX.appendKeyFrame(Qt3DAnimation::QKeyFrame(QVector2D(sd.totalTime, curTrans.x())));
    transY.appendKeyFrame(Qt3DAnimation::QKeyFrame(QVector2D(sd.totalTime, curTrans.y())));
    transZ.appendKeyFrame(Qt3DAnimation::QKeyFrame(QVector2D(sd.totalTime, curTrans.z())));
    rotX.appendKeyFrame(Qt3DAnimation::QKeyFrame(QVector2D(sd.totalTime, curRot.x())));
    rotY.appendKeyFrame(Qt3DAnimation::QKeyFrame(QVector2D(sd.totalTime, curRot.y())));
    rotZ.appendKeyFrame(Qt3DAnimation::QKeyFrame(QVector2D(sd.totalTime, curRot.z())));
    rotW.appendKeyFrame(Qt3DAnimation::QKeyFrame(QVector2D(sd.totalTime, curRot.scalar())));
    scaleX.appendKeyFrame(Qt3DAnimation::QKeyFrame(QVector2D(sd.totalTime, curScale.x())));
    scaleY.appendKeyFrame(Qt3DAnimation::QKeyFrame(QVector2D(sd.totalTime, curScale.y())));
    scaleZ.appendKeyFrame(Qt3DAnimation::QKeyFrame(QVector2D(sd.totalTime, curScale.z())));

    trans.appendChannelComponent(transX);
    trans.appendChannelComponent(transY);
    trans.appendChannelComponent(transZ);

    // QQuaternion(scalar, x, y, z)
    rot.appendChannelComponent(rotW);
    rot.appendChannelComponent(rotX);
    rot.appendChannelComponent(rotY);
    rot.appendChannelComponent(rotZ);

    scale.appendChannelComponent(scaleX);
    scale.appendChannelComponent(scaleY);
    scale.appendChannelComponent(scaleZ);

    if (changes & SceneData::ModelChange::Translation)
        clipData.appendChannel(trans);

    if (changes & SceneData::ModelChange::Rotation)
        clipData.appendChannel(rot);

    if (changes & SceneData::ModelChange::Scale)
        clipData.appendChannel(scale);

    clip->setClipData(clipData);

    animator->setClip(clip);
    animator->setLoopCount(9999);
    animator->setRunning(true);

    modelEntity->addComponent(animator);
}
