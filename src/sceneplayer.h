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

#ifndef SCENEPLAYER_H
#define SCENEPLAYER_H

#include <Qt3DCore/QEntity>
#include <QFutureWatcher>
#include "twospaceparser.h"

namespace Qt3DCore {
class QTransform;
}
namespace Qt3DExtras {
class QPhongMaterial;
}

class ScenePlayer : public Qt3DCore::QEntity
{
    Q_OBJECT
    Q_PROPERTY(QString source READ filename WRITE setFilename NOTIFY filenameChanged)
    Q_PROPERTY(QObject *renderer READ renderer WRITE setRenderer)
    Q_PROPERTY(qreal aspectRatio READ aspectRatio WRITE setAspectRatio)

public:
    explicit ScenePlayer(QNode *parent = nullptr);
    ~ScenePlayer();

    QString filename() const;
    void setFilename(const QString &fn);

    QObject *renderer() { return m_renderer; }
    void setRenderer(QObject *r) { m_renderer = r; }

    qreal aspectRatio() const { return m_aspectRatio; }
    void setAspectRatio(qreal ratio);

signals:
    void filenameChanged();

private:
    void setupScene(const SceneData &sd);
    void recursiveAddModels(const SceneData &sd,
                            const QHash<QByteArray, SceneData::Model> &models,
                            Qt3DCore::QEntity *parentEntity);
    void addAnimations(const SceneData &sd,
                       const QByteArray &modelId,
                       Qt3DCore::QEntity *modelEntity,
                       Qt3DCore::QTransform *modelTransform,
                       Qt3DExtras::QPhongMaterial *modelMaterial);

    QString m_filename;
    QScopedPointer<SceneParser> m_parser;
    QFutureWatcher<SceneData> m_watcher;
    QObject *m_renderer = nullptr;
    qreal m_aspectRatio = 16 / 9.0f;
};

#endif
