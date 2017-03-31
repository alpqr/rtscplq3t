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

#ifndef TWOSPACEPARSER_H
#define TWOSPACEPARSER_H

#include <QString>
#include <QSet>
#include <QHash>
#include <QColor>
#include <QVector3D>
#include <QFuture>

struct SceneData
{
    bool isValid() const { return valid; }
    QSet<QString> allModelFilenames() const;
    struct Model;
    const Model *model(const QByteArray &id);

    bool valid = false;

    struct Model {
        QString filename;
        QHash<QByteArray, Model> childModels;
    };

    struct CameraChange {
        enum Change {
            Position = 0x01,
            ViewCenter = 0x02
        };
        int change = 0;
        QVector3D position;
        QVector3D viewCenter;
    };

    struct LightChange {
        enum Change {
            Position = 0x01
        };
        int change = 0;
        QVector3D position;
    };

    struct ModelChange {
        enum Which {
            TranslationX = 0x01,
            TranslationY = 0x02,
            TranslationZ = 0x04,
            Translation = TranslationX | TranslationY | TranslationZ,

            RotationX = 0x08,
            RotationY = 0x10,
            RotationZ = 0x20,
            Rotation = RotationX | RotationY | RotationZ,

            ScaleX = 0x40,
            ScaleY = 0x80,
            ScaleZ = 0x100,
            Scale = ScaleX | ScaleY | ScaleZ,

            Color = 0x200
        };
        int change = 0;
        QVector3D translation;
        QVector3D rotation;
        QVector3D scale;
        QColor color;
    };

    struct Frame {
        int t = 0;
        QHash<QByteArray, CameraChange> cameraChanges;
        QHash<QByteArray, LightChange> lightChanges;
        QHash<QByteArray, ModelChange> modelChanges;
    };

    QSet<QByteArray> cameras;
    QSet<QByteArray> lights;
    QHash<QByteArray, Model> models;
    int totalTime;
    QVector<Frame> frames;
};

class SceneParser
{
public:
    void load(const QString &fn);
    SceneData *data();
    bool isValid() { return data()->isValid(); }
    void reset();
    QFuture<SceneData> *future() { return &m_future; }

private:
    bool m_maybeRunning = false;
    QFuture<SceneData> m_future;
    SceneData m_data;
};

#endif
