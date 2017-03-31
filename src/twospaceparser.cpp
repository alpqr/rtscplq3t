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

#include "twospaceparser.h"
#include <QtConcurrentRun>
#include <QFile>
#include <QStack>

QDebug operator<<(QDebug dbg, const SceneData::Model &model)
{
    QDebugStateSaver saver(dbg);
    dbg.space() << "Model(" << model.filename << model.childModels << ")";
    return dbg;
}

QDebug operator<<(QDebug dbg, const SceneData::Frame &frame)
{
    QDebugStateSaver saver(dbg);
    dbg.space() << "Frame(" << frame.t << frame.cameraChanges << frame.lightChanges << frame.modelChanges << ")";
    return dbg;
}

QDebug operator<<(QDebug dbg, const SceneData::CameraChange &ch)
{
    QDebugStateSaver saver(dbg);
    dbg.space() << "CameraChange(" << ch.change << ch.position << ch.viewCenter << ")";
    return dbg;
}

QDebug operator<<(QDebug dbg, const SceneData::LightChange &ch)
{
    QDebugStateSaver saver(dbg);
    dbg.space() << "LightChange(" << ch.change << ch.position << ")";
    return dbg;
}

QDebug operator<<(QDebug dbg, const SceneData::ModelChange &ch)
{
    QDebugStateSaver saver(dbg);
    dbg.space() << "ModelChange(" << ch.change << ch.translation << ch.rotation << ch.scale << ch.color << ")";
    return dbg;
}

void SceneParser::load(const QString &fn)
{
    reset();
    m_maybeRunning = true;
    m_future = QtConcurrent::run([fn]() {
        SceneData scene;
        QFile f(fn);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qWarning("Failed to open %s", qPrintable(fn));
            return scene;
        }

        int lineIdx = 0;
        bool inScene = false, inFrames = false;
        int lastModelSpc = 2;
        QByteArray lastModelId;
        QStack<QByteArray> parentModelIdStack;
        SceneData::Frame *currentFrame = nullptr;

        auto parseModel = [&](const QByteArrayList &c, int spc) {
            SceneData::Model mdl;
            mdl.filename = QString::fromUtf8(c[2]);
            if (spc > lastModelSpc)
                parentModelIdStack.push(lastModelId);
            else if (spc < lastModelSpc) {
                int s = lastModelSpc;
                while (spc < s) {
                    parentModelIdStack.pop();
                    s -= 2;
                }
            }
            lastModelSpc = spc;
            lastModelId = c[1];
            return mdl;
        };

        while (!f.atEnd()) {
            ++lineIdx;
            QByteArray line = f.readLine();
            int spc = 0;
            while (spc < line.count() && line[spc] == ' ')
                ++spc;
            if (spc % 2) {
                qWarning("%s: Malformed line %d, invalid space count %d", qPrintable(fn), lineIdx, spc);
                return scene;
            }

            if (line.mid(spc).startsWith("//"))
                continue;

            line = line.trimmed();
            if (line.isEmpty())
                continue;

            const QByteArrayList c = line.split(' ');

            if (spc == 0) {
                if (c[0] == "scene" && c.count() == 1) {
                    inScene = true;
                    inFrames = false;
                } else if (c[0] == "frames" && c.count() == 2) {
                    inFrames = true;
                    inScene = false;
                    bool ok = false;
                    scene.totalTime = c[1].toInt(&ok);
                    if (!ok) {
                        qWarning("%s: Malformed total time at line %d", qPrintable(fn), lineIdx);
                        return scene;
                    }
                } else {
                    qWarning("%s: Malformed line %d", qPrintable(fn), lineIdx);
                    return scene;
                }
            } else {
                if (inScene) {
                    if (spc == 2) {
                        if (c[0] == "camera" && c.count() == 2) {
                            scene.cameras.insert(c[1]);
                        } else if (c[0] == "light" && c.count() == 2) {
                            scene.lights.insert(c[1]);
                        } else if (c[0] == "model" && c.count() == 3) {
                            scene.models.insert(c[1], parseModel(c, spc));
                        } else {
                            qWarning("%s: Malformed line %d, unknown entry %s", qPrintable(fn), lineIdx, c[0].constData());
                            return scene;
                        }
                    } else {
                        if (c[0] == "model" && c.count() == 3) {
                            if (spc - lastModelSpc > 2) {
                                qWarning("%s: Too many spaces at line %d", qPrintable(fn), lineIdx);
                                return scene;
                            }
                            SceneData::Model mdl = parseModel(c, spc);
                            // lazy. just walk the tree for now.
                            QHash<QByteArray, SceneData::Model> *coll = &scene.models;
                            for (const QByteArray &id : parentModelIdStack) {
                                if (!coll->contains(id)) {
                                    qWarning("%s: Malformed tree at line %d", qPrintable(fn), lineIdx);
                                    return scene;
                                }
                                coll = &(*coll)[id].childModels;
                            }
                            coll->insert(lastModelId, mdl);
                        } else {
                            qWarning("%s: Malformed line %d, unknown entry %s", qPrintable(fn), lineIdx, c[0].constData());
                            return scene;
                        }
                    }
                } else if (inFrames) {
                    if (spc == 2) {
                        bool ok = false;
                        scene.frames.append(SceneData::Frame());
                        currentFrame = &scene.frames.last();
                        currentFrame->t = c[0].toInt(&ok);
                        if (!ok) {
                            qWarning("%s: Invalid keyframe position at line %d", qPrintable(fn), lineIdx);
                            return scene;
                        }
                    } else if (spc == 4 && currentFrame) {
                        if (scene.cameras.contains(c[0])) {
                            SceneData::CameraChange ch;
                            for (int i = 1; i < c.count(); ++i) {
                                QByteArrayList propRef = c[i].split('.');
                                if (propRef.count() == 2) {
                                    if (propRef[0] == "pos") {
                                        ch.change |= SceneData::CameraChange::Position;
                                        if (propRef[1] == "x") {
                                            ch.position.setX(c[i+1].toFloat());
                                            ++i;
                                        } else if (propRef[1] == "y") {
                                            ch.position.setY(c[i+1].toFloat());
                                            ++i;
                                        } else if (propRef[1] == "z") {
                                            ch.position.setZ(c[i+1].toFloat());
                                            ++i;
                                        }
                                    } else if (propRef[0] == "view") {
                                        ch.change |= SceneData::CameraChange::ViewCenter;
                                        if (propRef[1] == "x") {
                                            ch.viewCenter.setX(c[i+1].toFloat());
                                            ++i;
                                        } else if (propRef[1] == "y") {
                                            ch.viewCenter.setY(c[i+1].toFloat());
                                            ++i;
                                        } else if (propRef[1] == "z") {
                                            ch.viewCenter.setZ(c[i+1].toFloat());
                                            ++i;
                                        }
                                    } else {
                                        qWarning("%s: Unknown camera property reference '%s' at line %d", qPrintable(fn), propRef[0].constData(), lineIdx);
                                    }
                                }
                            }
                            currentFrame->cameraChanges.insert(c[0], ch);
                        } else if (scene.lights.contains(c[0])) {
                            SceneData::LightChange ch;
                            for (int i = 1; i < c.count(); ++i) {
                                QByteArrayList propRef = c[i].split('.');
                                if (propRef.count() == 2) {
                                    if (propRef[0] == "pos") {
                                        ch.change |= SceneData::LightChange::Position;
                                        if (propRef[1] == "x") {
                                            ch.position.setX(c[i+1].toFloat());
                                            ++i;
                                        } else if (propRef[1] == "y") {
                                            ch.position.setY(c[i+1].toFloat());
                                            ++i;
                                        } else if (propRef[1] == "z") {
                                            ch.position.setZ(c[i+1].toFloat());
                                            ++i;
                                        }
                                    } else {
                                        qWarning("%s: Unknown light property reference '%s' at line %d", qPrintable(fn), propRef[0].constData(), lineIdx);
                                    }
                                }
                            }
                            currentFrame->lightChanges.insert(c[0], ch);
                        } else if (scene.model(c[0])) {
                            SceneData::ModelChange ch;
                            for (int i = 1; i < c.count(); ++i) {
                                QByteArrayList propRef = c[i].split('.');
                                if (propRef.count() == 2) {
                                    if (propRef[0] == "trans") {
                                        if (propRef[1] == "x") {
                                            ch.change |= SceneData::ModelChange::TranslationX;
                                            ch.translation.setX(c[i+1].toFloat());
                                            ++i;
                                        } else if (propRef[1] == "y") {
                                            ch.change |= SceneData::ModelChange::TranslationY;
                                            ch.translation.setY(c[i+1].toFloat());
                                            ++i;
                                        } else if (propRef[1] == "z") {
                                            ch.change |= SceneData::ModelChange::TranslationZ;
                                            ch.translation.setZ(c[i+1].toFloat());
                                            ++i;
                                        }
                                    } else if (propRef[0] == "rot") {
                                        if (propRef[1] == "x") {
                                            ch.change |= SceneData::ModelChange::RotationX;
                                            ch.rotation.setX(c[i+1].toFloat());
                                            ++i;
                                        } else if (propRef[1] == "y") {
                                            ch.change |= SceneData::ModelChange::RotationY;
                                            ch.rotation.setY(c[i+1].toFloat());
                                            ++i;
                                        } else if (propRef[1] == "z") {
                                            ch.change |= SceneData::ModelChange::RotationZ;
                                            ch.rotation.setZ(c[i+1].toFloat());
                                            ++i;
                                        }
                                    } else if (propRef[0] == "scale") {
                                        if (propRef[1] == "x") {
                                            ch.change |= SceneData::ModelChange::ScaleX;
                                            ch.scale.setX(c[i+1].toFloat());
                                            ++i;
                                        } else if (propRef[1] == "y") {
                                            ch.change |= SceneData::ModelChange::ScaleY;
                                            ch.scale.setY(c[i+1].toFloat());
                                            ++i;
                                        } else if (propRef[1] == "z") {
                                            ch.change |= SceneData::ModelChange::ScaleZ;
                                            ch.scale.setZ(c[i+1].toFloat());
                                            ++i;
                                        }
                                    } else {
                                        qWarning("%s: Unknown model property reference '%s' at line %d", qPrintable(fn), propRef[0].constData(), lineIdx);
                                        return scene;
                                    }
                                } else if (propRef.count() == 1) {
                                    if (propRef[0] == "color") {
                                        ch.change |= SceneData::ModelChange::Color;
                                        ch.color = QColor(QString::fromUtf8(c[i+1]));
                                        ++i;
                                    } else {
                                        qWarning("%s: Unknown model property reference '%s' at line %d", qPrintable(fn), propRef[0].constData(), lineIdx);
                                        return scene;
                                    }
                                }
                            }
                            currentFrame->modelChanges.insert(c[0], ch);
                        }
                    } else {
                        Q_UNREACHABLE();
                    }
                }
            }
        }

        scene.valid = true;
        return scene;
    });
}

SceneData *SceneParser::data()
{
    if (m_maybeRunning && !m_data.isValid())
        m_data = m_future.result();

    return &m_data;
}

void SceneParser::reset()
{
    *data() = SceneData();
    m_maybeRunning = false;
}

static void gatherFn(const QHash<QByteArray, SceneData::Model> &c, QSet<QString> *fn)
{
    for (const SceneData::Model &mdl : c) {
        fn->insert(mdl.filename);
        gatherFn(mdl.childModels, fn);
    }
}

QSet<QString> SceneData::allModelFilenames() const
{
    QSet<QString> fn;
    gatherFn(models, &fn);
    return fn;
}

static const SceneData::Model *findModel(const QByteArray &id, QHash<QByteArray, SceneData::Model> &c)
{
    if (c.contains(id))
        return &c[id];

    for (SceneData::Model &mdl : c) {
        if (const SceneData::Model *m = findModel(id, mdl.childModels))
            return m;
    }

    return nullptr;
}

const SceneData::Model *SceneData::model(const QByteArray &id)
{
    return findModel(id, models);
}
