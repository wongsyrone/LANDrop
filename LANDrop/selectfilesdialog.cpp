/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2021, LANDrop
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QTreeView>
#include <QMessageBox>
#include <QMimeData>

#include "selectfilesdialog.h"
#include "sendtodialog.h"
#include "ui_selectfilesdialog.h"

SelectFilesDialog::SelectFilesDialog(QWidget *parent, DiscoveryService &discoveryService) :
    QDialog(parent), ui(new Ui::SelectFilesDialog), discoveryService(discoveryService)
{
    ui->setupUi(this);
    // TODO: for debug
    //setWindowFlag(Qt::WindowStaysOnTopHint);
    connect(ui->addButton, &QPushButton::clicked, this, &SelectFilesDialog::addButtonClicked);
    connect(ui->removeButton, &QPushButton::clicked, this, &SelectFilesDialog::removeButtonClicked);
    ui->filesListView->setModel(&filesStringListModel);

    ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Send"));
    ui->buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
}

SelectFilesDialog::~SelectFilesDialog()
{
    delete ui;
}

void SelectFilesDialog::addFile(const QString &filename)
{
    QFileInfo newSelectedFileInfo(filename);
    if (!newSelectedFileInfo.exists()) {
        QMessageBox::critical(this, QApplication::applicationName(),
                              tr("Unable to open file %1. Skipping.")
                              .arg(filename));
        return;
    }
    if (newSelectedFileInfo.isSymLink()) {
        auto symTarget = newSelectedFileInfo.symLinkTarget();
        newSelectedFileInfo = QFileInfo(symTarget);
        QMessageBox::warning(this, QApplication::applicationName(),
                             tr("Selected file/dir is symlink, replaced with %1.")
                             .arg(symTarget));
    }
    if (newSelectedFileInfo.isDir()) {
        QDir parentDir(newSelectedFileInfo.absoluteDir());
//        foreach (auto dir, dirs) {
//            if (*dir == newSelectedFileInfo.absoluteDir())
//                return;
//        }

        // selected current dir
        QSharedPointer<QDir> dir = QSharedPointer<QDir>::create(parentDir.relativeFilePath(newSelectedFileInfo.absolutePath()));
        if (!dir->exists()) {
            QMessageBox::critical(this, QApplication::applicationName(),
                                  tr("Unable to open dir %1. Skipping.")
                                  .arg(filename));
            return;
        }
        dirs.append(dir);

        // All dirs to create
        QDirIterator diDirs(newSelectedFileInfo.filePath(), QDir::Dirs | QDir::Filter::NoDotAndDotDot, QDirIterator::Subdirectories);
        while(diDirs.hasNext()) {
            QSharedPointer<QDir> dp = QSharedPointer<QDir>::create(parentDir.relativeFilePath(diDirs.next()));
            dirs.append(dp);
        }

        // All files
        QDirIterator di(filename, QDir::Files, QDirIterator::Subdirectories);
        while(di.hasNext()) {
            if (isFileValid(di.next())) {
                QSharedPointer<QFile> fp = QSharedPointer<QFile>::create(parentDir.relativeFilePath(di.next()));
                files.append(fp);
            }

        }


    } else if (newSelectedFileInfo.isFile()) {
//        foreach (auto file, files) {
//            if (file->fileName() == filename)
//                return;
//        }

        if (isFileValid(filename)) {
            QSharedPointer<QFile> fp = QSharedPointer<QFile>::create(newSelectedFileInfo.fileName());
            files.append(fp);
        }
    }
}

bool SelectFilesDialog::isFileValid(const QString &filename)
{
    QSharedPointer<QFile> fp = QSharedPointer<QFile>::create(filename);

    if (!fp->open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, QApplication::applicationName(),
                              tr("Unable to open file %1. Skipping.")
                              .arg(filename));
        return false;
    }
    if (fp->isSequential()) {
        QMessageBox::critical(this, QApplication::applicationName(),
                              tr("%1 is not a regular file. Skipping.")
                              .arg(filename));
        return false;
    }
    return true;
}

void SelectFilesDialog::updateFileStringListModel()
{
    QStringList l;
    foreach (auto dir, dirs) {
        l.append("DIR: " + dir->absolutePath());
    }
    foreach (auto file, files) {
        l.append("FILE: " + file->fileName());
    }
    filesStringListModel.setStringList(l);
}

void SelectFilesDialog::addButtonClicked()
{
    QFileDialog *_f_dlg = new QFileDialog(this, tr("Select File(s) to be Sent"));
    _f_dlg->setFileMode(QFileDialog::Directory);
    _f_dlg->setOption(QFileDialog::DontUseNativeDialog, true);

    // Try to select multiple files and directories at the same time in
    // QFileDialog
    QListView *l = _f_dlg->findChild<QListView *>("listView");
    if (l) {
        l->setSelectionMode(QAbstractItemView::MultiSelection);
    }
    QTreeView *t = _f_dlg->findChild<QTreeView *>();
    if (t) {
        t->setSelectionMode(QAbstractItemView::MultiSelection);
    }
    QStringList filenames;
    int nMode = _f_dlg->exec();
    if (nMode == DialogCode::Accepted) {
        filenames = _f_dlg->selectedFiles();
        delete _f_dlg;
    } else if (nMode == DialogCode::Rejected) {
        delete _f_dlg;
        return;
    }
    if (filenames.empty())
        return;

    foreach (const QString &filename, filenames) {
        addFile(filename);
    }

    updateFileStringListModel();
}

void SelectFilesDialog::removeButtonClicked()
{
    QModelIndexList indexes = ui->filesListView->selectionModel()->selectedIndexes();
    QList<const QSharedPointer<QFile> *> removeFileList;
    QList<const QSharedPointer<QDir> *> removeDirList;
    // dirs are at top
    auto dirLen = dirs.length();
    int indexOfDirs;
    int indexOfFiles;

    foreach (const QModelIndex &i, indexes) {
        auto rawIdx = i.row();
        if (rawIdx <= dirLen -1) {
            // in the dir range
            indexOfDirs = rawIdx;
            removeDirList.append(&dirs.at(indexOfDirs));
        } else {
            // in the file range
            indexOfFiles = rawIdx - dirLen;
            removeFileList.append(&files.at(indexOfFiles));
        }
    }

    foreach (auto f, removeFileList) {
        files.removeOne(*f);
    }
    foreach (auto d, removeDirList) {
        dirs.removeOne(*d);
    }
    updateFileStringListModel();
}

void SelectFilesDialog::accept()
{
    if (files.empty() && dirs.empty()) {
        QMessageBox::warning(this, QApplication::applicationName(), tr("No file or dirs to be sent."));
        return;
    }

    SendToDialog *d = new SendToDialog(nullptr, files, dirs, discoveryService);
    d->setAttribute(Qt::WA_DeleteOnClose);
    d->show();

    done(Accepted);
}

void SelectFilesDialog::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void SelectFilesDialog::dropEvent(QDropEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        foreach (const QUrl &url, event->mimeData()->urls()) {
            addFile(url.toLocalFile());
        }
        updateFileStringListModel();
        event->acceptProposedAction();
    }
}
