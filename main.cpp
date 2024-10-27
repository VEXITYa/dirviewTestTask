/****************************************************************************
**
** Copyright (C) 2020 The Qt Company Ltd.
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


#include <QApplication>
#include <QFileSystemModel>
#include <QFileIconProvider>
#include <QScreen>
#include <QScroller>
#include <QTreeView>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QLineEdit>
#include <QSortFilterProxyModel>
#include <QVBoxLayout>
#include <QDir>
#include <QSize>
#include <QPushButton>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QWidget>
#include <QDebug>


class FileSystemModelWithSize : public QFileSystemModel {
    Q_OBJECT

public:
    FileSystemModelWithSize(QObject *parent = nullptr) : QFileSystemModel(parent) {}

    int columnCount(const QModelIndex &parent = QModelIndex()) const override {
        return QFileSystemModel::columnCount(parent) + 1;
    }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override {
        if (role == Qt::DisplayRole) {
            if (index.column() == 1) {
                if (isDir(index)) {
                    return calculateFolderSize(filePath(index));
                }
                return QFileSystemModel::data(index, role);
            } else if (index.column() == columnCount() - 1) {
                return QString();
            }
        }
        return QFileSystemModel::data(index, role);
    }

    QString calculateFolderSize(const QString &path) const {
        QDir dir(path);
        qint64 size = 0;
        foreach (QFileInfo fileInfo, dir.entryInfoList(QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot)) {
            size += fileInfo.size();
        }
        return QString::number(size / 1024) + " KB";
    }
};


class UpdateButtonDelegate : public QStyledItemDelegate {
    Q_OBJECT

public:
    explicit UpdateButtonDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        QStyledItemDelegate::paint(painter, option, index);

        if (index.column() == index.model()->columnCount() - 1) {
            QRect buttonRect = option.rect;
            buttonRect.adjust(5, 5, -5, -5);

            painter->save();
            painter->drawText(buttonRect, Qt::AlignCenter, "Refresh");
            painter->restore();
        }
    }

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        if (index.column() == index.model()->columnCount() - 1) {
            QPushButton *button = new QPushButton("Refresh", parent);
            connect(button, &QPushButton::clicked, [this, index]() {
                emit updateFolderSize(index);
            });
            return button;
        }
        return QStyledItemDelegate::createEditor(parent, option, index);
    }

signals:
    void updateFolderSize(const QModelIndex &index) const;
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QCoreApplication::setApplicationVersion(QT_VERSION_STR);
    QCommandLineParser parser;
    parser.setApplicationDescription("Qt Dir View Example");
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption dontUseCustomDirectoryIconsOption("c", "Set QFileSystemModel::DontUseCustomDirectoryIcons");
    parser.addOption(dontUseCustomDirectoryIconsOption);
    QCommandLineOption dontWatchOption("w", "Set QFileSystemModel::DontWatch");
    parser.addOption(dontWatchOption);
    parser.addPositionalArgument("directory", "The directory to start in.");
    parser.process(app);

    const QString rootPath = parser.positionalArguments().isEmpty() ? QDir::homePath() : parser.positionalArguments().first();
    FileSystemModelWithSize model;
    model.setRootPath(rootPath);
    model.setFilter(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);

    if (parser.isSet(dontUseCustomDirectoryIconsOption))
        model.setOption(QFileSystemModel::DontUseCustomDirectoryIcons);
    if (parser.isSet(dontWatchOption))
        model.setOption(QFileSystemModel::DontWatchForChanges);

    QSortFilterProxyModel proxyModel;
    proxyModel.setSourceModel(&model);
    proxyModel.setFilterCaseSensitivity(Qt::CaseInsensitive);
    proxyModel.setFilterKeyColumn(0);

    QLineEdit filterLineEdit;
    filterLineEdit.setPlaceholderText(QObject::tr("Search.."));
    QObject::connect(&filterLineEdit, &QLineEdit::textChanged, &proxyModel, &QSortFilterProxyModel::setFilterFixedString);

    QTreeView tree;
    tree.setModel(&proxyModel);
    tree.setRootIndex(proxyModel.mapFromSource(model.index(rootPath)));

    tree.setAnimated(false);
    tree.setIndentation(20);
    tree.setSortingEnabled(true);
    const QSize availableSize = tree.screen()->availableGeometry().size();
    tree.resize(availableSize / 2);
    tree.setColumnWidth(0, tree.width() / 3);

    QScroller::grabGesture(&tree, QScroller::TouchGesture);

    UpdateButtonDelegate *delegate = new UpdateButtonDelegate(&tree);
    tree.setItemDelegateForColumn(model.columnCount() - 1, delegate);

    QObject::connect(delegate, &UpdateButtonDelegate::updateFolderSize, [&model](const QModelIndex &index) {
        QString folderPath = model.filePath(index);
        QString folderSize = model.calculateFolderSize(folderPath);
        model.setData(index.sibling(index.row(), 1), folderSize);
    });

    QWidget window;
    QVBoxLayout layout(&window);
    layout.addWidget(&filterLineEdit);
    layout.addWidget(&tree);

    window.setWindowTitle(QObject::tr("Dir View"));
    window.show();

    return app.exec();
}

#include "main.moc"
