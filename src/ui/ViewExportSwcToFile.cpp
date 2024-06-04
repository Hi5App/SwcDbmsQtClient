#include <QFileDialog>
#include <QStandardPaths>
#include "ViewExportSwcToFile.h"
#include "ui_ViewExportSwcToFile.h"
#include "src/framework/defination/ImageDefination.h"
#include "src/FileIo/SwcIo.hpp"
#include "Message/Response.pb.h"
#include "src/framework/service/WrappedCall.h"
#include "src/FileIo/ApoIo.hpp"
#include "src/FileIo/AnoIo.hpp"
#include <filesystem>

ViewExportSwcToFile::ViewExportSwcToFile(std::vector<ExportSwcData> &exportSwcData, bool getDataFromServer,
                                         QWidget *parent) : QDialog(parent), ui(new Ui::ViewExportSwcToFile) {
    ui->setupUi(this);
    std::string stylesheet = std::string("QListWidget::indicator:checked{image:url(")
                             + Image::ImageCheckBoxChecked + ");}" +
                             "QListWidget::indicator:unchecked{image:url(" +
                             Image::ImageCheckBoxUnchecked + ");}";
    ui->SwcList->setStyleSheet(QString::fromStdString(stylesheet));
    ui->SavePath->setReadOnly(true);

    m_ExportSwcData = exportSwcData;
    m_GetDataFromServer = getDataFromServer;

    for (auto &val: m_ExportSwcData) {
        auto userInfo = val;
        auto *item = new QListWidgetItem;
        item->setText(QString::fromStdString(val.swcMetaInfo.name()));
        item->setCheckState(Qt::Checked);
        ui->SwcList->addItem(item);
    }

    ui->ResultTable->clear();
    ui->ResultTable->setColumnCount(5);
    QStringList headerLabels;
    headerLabels
            << "Swc Name"
            << "Swc Type"
            << "Swc Node Number"
            << "Export Status"
            << "Save Path";
    ui->ResultTable->setHorizontalHeaderLabels(headerLabels);
    ui->ResultTable->setRowCount(m_ExportSwcData.size());
    ui->ResultTable->resizeColumnsToContents();

    connect(ui->SelectSavePathBtn, &QPushButton::clicked, this, [this]() {
        QFileDialog fileDialog(this);
        fileDialog.setWindowTitle("Select Swc Files");
        fileDialog.setDirectory(QStandardPaths::writableLocation(QStandardPaths::HomeLocation));
        fileDialog.setFileMode(QFileDialog::Directory);
        fileDialog.setViewMode(QFileDialog::Detail);

        QStringList fileNames;
        if (fileDialog.exec()) {
            fileNames = fileDialog.selectedFiles();
            if (!fileNames.empty()) {
                ui->SavePath->setText(fileNames[0]);
                m_SavePath = fileNames[0].toStdString();
                std::filesystem::path saveDir = m_SavePath;
                saveDir = saveDir / "ExportSwc";
                if (std::filesystem::exists(saveDir)) {
                    QMessageBox::information(this, "Warning",
                                             "Selected directory contains <ExportSwc> folder! Please remove it first!");
                    m_SavePath = "";
                    return;
                }
                if (!std::filesystem::create_directories(saveDir)) {
                    QMessageBox::critical(this, "Error", "Create <ExportSwc> directory in save path failed!");
                    m_SavePath = "";
                    return;
                }
                m_SavePath = saveDir.string();
            }
        }
    });

    connect(ui->CancelBtn, &QPushButton::clicked, this, [this]() {
        reject();
    });

    connect(ui->ExportBtn, &QPushButton::clicked, this, [this]() {
        if (m_SavePath.empty()) {
            QMessageBox::information(this, "Warning", "You did not select a valid save path!");
            return;
        }

        if (m_ActionExportComplete) {
            QMessageBox::information(this, "Warning",
                                     "Export action has completed! Please reopen this export window if you want export more data again!");
            return;
        }

        for (int i = 0; i < m_ExportSwcData.size(); i++) {
            QApplication::processEvents();

            ui->ResultTable->setItem(i, 0,
                                     new QTableWidgetItem(
                                             QString::fromStdString(m_ExportSwcData[i].swcMetaInfo.name())));
            ui->ResultTable->setItem(i, 1,
                                     new QTableWidgetItem(
                                             QString::fromStdString(m_ExportSwcData[i].swcMetaInfo.swctype())));
            ui->ResultTable->setItem(i, 2,
                                     new QTableWidgetItem(
                                             QString::fromStdString(
                                                     std::to_string(m_ExportSwcData[i].swcData.swcdata_size()))));

            QString statusMessage = "Ignored";
            QString fileSavePath = "";
            if (ui->SwcList->item(i)->checkState() == Qt::Checked) {
                if (!m_ExportSwcData[i].swcMetaInfo.swcattachmentapometainfo().attachmentuuid().empty()) {
                    std::filesystem::path apoSavePath = m_SavePath;
                    apoSavePath = apoSavePath / (m_ExportSwcData[i].swcMetaInfo.name() + ".ano.apo");
                    proto::GetSwcAttachmentApoResponse response;
                    auto attachmentUuid = m_ExportSwcData[i].swcMetaInfo.swcattachmentapometainfo().attachmentuuid();
                    if (!WrappedCall::getSwcAttachmentApoByUuid(m_ExportSwcData[i].swcMetaInfo.name(), attachmentUuid,
                                                          response, this)) {
                        QMessageBox::critical(this, "Error",
                                              QString::fromStdString(response.metainfo().message()));
                        ui->ResultTable->setItem(i, 3,
                                                 new QTableWidgetItem("Get Swc Attachment Apo Data Failed"));
                        ui->ResultTable->setItem(i, 4,
                                                 new QTableWidgetItem(fileSavePath));
                        setAllGridColor(i, Qt::red);
                    }

                    std::vector<proto::SwcAttachmentApoV1> swcAttachmentApoData;
                    for (auto &data: response.swcattachmentapo()) {
                        swcAttachmentApoData.push_back(data);
                    }

                    ApoIo io(apoSavePath.string());
                    std::vector<ApoUnit> units;
                    std::for_each(swcAttachmentApoData.begin(), swcAttachmentApoData.end(),
                                  [&](proto::SwcAttachmentApoV1 &val) {
                                      ApoUnit unit;
                                      unit.n = val.n();
                                      unit.orderinfo = val.orderinfo();
                                      unit.name = val.name();
                                      unit.comment = val.comment();
                                      unit.z = val.z();
                                      unit.x = val.x();
                                      unit.y = val.y();
                                      unit.pixmax = val.pixmax();
                                      unit.intensity = val.intensity();
                                      unit.sdev = val.sdev();
                                      unit.volsize = val.volsize();
                                      unit.mass = val.mass();
                                      unit.color_r = val.colorr();
                                      unit.color_g = val.colorg();
                                      unit.color_b = val.colorb();
                                      units.push_back(unit);
                                  });

                    io.setValue(units);
                    io.WriteToFile();
                }

                if (!m_ExportSwcData[i].swcMetaInfo.swcattachmentanometainfo().attachmentuuid().empty()) {
                    proto::GetSwcAttachmentAnoResponse get_swc_attachment_ano_response;
                    auto anoAttachmentUuid = m_ExportSwcData[i].swcMetaInfo.swcattachmentanometainfo().attachmentuuid();
                    if (!WrappedCall::getSwcAttachmentAnoByUuid(m_ExportSwcData[i].swcMetaInfo.name(), anoAttachmentUuid,
                                                          get_swc_attachment_ano_response,
                                                          this)) {
                        return;
                    }

                    auto apoData = get_swc_attachment_ano_response.swcattachmentano().apofile();
                    auto anoData = get_swc_attachment_ano_response.swcattachmentano().swcfile();

                    std::filesystem::path anoSavePath(m_SavePath);
                    anoSavePath = anoSavePath / (m_ExportSwcData[i].swcMetaInfo.name() + ".ano");

                    AnoIo io(anoSavePath.string());
                    AnoUnit unit;
                    unit.APOFILE = apoData;
                    unit.SWCFILE = anoData;
                    io.setValue(unit);
                    io.WriteToFile();
                }

                if (m_ExportSwcData[i].swcMetaInfo.swctype() == "swc") {
                    std::filesystem::path savePath(m_SavePath);
                    savePath = savePath / (m_ExportSwcData[i].swcMetaInfo.name() + ".ano.swc");

                    if (m_GetDataFromServer) {
                        if (!m_ExportSwcData[i].isSnapshot) {
                            proto::GetSwcFullNodeDataResponse response;
                            if (!WrappedCall::getSwcFullNodeDataByUuid(m_ExportSwcData[i].swcMetaInfo.name(), response,
                                                                 this)) {
                                QMessageBox::critical(this, "Error",
                                                      QString::fromStdString(response.metainfo().message()));
                                ui->ResultTable->setItem(i, 3,
                                                         new QTableWidgetItem("Get Data From Server Failed"));
                                ui->ResultTable->setItem(i, 4,
                                                         new QTableWidgetItem(fileSavePath));
                                setAllGridColor(i, Qt::red);
                                continue;
                            }
                            m_ExportSwcData[i].swcData = response.swcnodedata();
                        } else {
                            proto::GetSnapshotResponse response;
                            if (!WrappedCall::getSwcSnapshot(m_ExportSwcData[i].swcMetaInfo.name(), response,
                                                             this)) {
                                QMessageBox::critical(this, "Error",
                                                      QString::fromStdString(response.metainfo().message()));
                                ui->ResultTable->setItem(i, 3,
                                                         new QTableWidgetItem("Get Data From Server Failed"));
                                ui->ResultTable->setItem(i, 4,
                                                         new QTableWidgetItem(fileSavePath));
                                setAllGridColor(i, Qt::red);
                                continue;
                            }
                            m_ExportSwcData[i].swcData = response.swcnodedata();
                        }
                    }

                    std::vector<NeuronUnit> neurons;
                    auto swcData = m_ExportSwcData[i].swcData;
                    for (int j = 0; j < swcData.swcdata_size(); j++) {
                        NeuronUnit unit;
                        unit.n = swcData.swcdata(j).swcnodeinternaldata().n();
                        unit.type = swcData.swcdata(j).swcnodeinternaldata().type();
                        unit.x = swcData.swcdata(j).swcnodeinternaldata().x();
                        unit.y = swcData.swcdata(j).swcnodeinternaldata().y();
                        unit.z = swcData.swcdata(j).swcnodeinternaldata().z();
                        unit.radius = swcData.swcdata(j).swcnodeinternaldata().radius();
                        unit.parent = swcData.swcdata(j).swcnodeinternaldata().parent();
                        neurons.push_back(unit);
                    }

                    Swc swc(savePath.string());
                    swc.setValue(neurons);
                    if (swc.WriteToFile()) {
                        ui->ResultTable->setItem(i, 3,
                                                 new QTableWidgetItem("Successfully"));
                        ui->ResultTable->setItem(i, 4,
                                                 new QTableWidgetItem(QString::fromStdString(savePath.string())));
                        setAllGridColor(i, Qt::green);
                        ui->ResultTable->item(i, 2)->setText(QString::fromStdString(
                                std::to_string(m_ExportSwcData[i].swcData.swcdata_size())));
                    } else {
                        statusMessage = "Write Swc File Failed!";
                        fileSavePath = "";
                    }
                } else if (m_ExportSwcData[i].swcMetaInfo.swctype() == "eswc") {
                    std::filesystem::path savePath(m_SavePath);
                    savePath = savePath / (m_ExportSwcData[i].swcMetaInfo.name() + ".ano.eswc");

                    if (m_GetDataFromServer) {
                        if (!m_ExportSwcData[i].isSnapshot) {
                            proto::GetSwcFullNodeDataResponse response;
                            if (!WrappedCall::getSwcFullNodeDataByUuid(m_ExportSwcData[i].swcMetaInfo.name(), response,
                                                                 this)) {
                                QMessageBox::critical(this, "Error",
                                                      QString::fromStdString(response.metainfo().message()));
                                ui->ResultTable->setItem(i, 3,
                                                         new QTableWidgetItem("Get Data From Server Failed"));
                                ui->ResultTable->setItem(i, 4,
                                                         new QTableWidgetItem(fileSavePath));
                                setAllGridColor(i, Qt::red);
                                continue;
                            }
                            m_ExportSwcData[i].swcData = response.swcnodedata();
                        } else {
                            proto::GetSnapshotResponse response;
                            if (!WrappedCall::getSwcSnapshot(m_ExportSwcData[i].swcSnapshotCollectionName, response,
                                                             this)) {
                                QMessageBox::critical(this, "Error",
                                                      QString::fromStdString(response.metainfo().message()));
                                ui->ResultTable->setItem(i, 3,
                                                         new QTableWidgetItem("Get Data From Server Failed"));
                                ui->ResultTable->setItem(i, 4,
                                                         new QTableWidgetItem(fileSavePath));
                                setAllGridColor(i, Qt::red);
                                continue;
                            }
                            m_ExportSwcData[i].swcData = response.swcnodedata();
                        }
                    }

                    std::vector<NeuronUnit> neurons;
                    auto swcData = m_ExportSwcData[i].swcData;
                    for (int j = 0; j < swcData.swcdata_size(); j++) {
                        NeuronUnit unit;
                        unit.n = swcData.swcdata(j).swcnodeinternaldata().n();
                        unit.type = swcData.swcdata(j).swcnodeinternaldata().type();
                        unit.x = swcData.swcdata(j).swcnodeinternaldata().x();
                        unit.y = swcData.swcdata(j).swcnodeinternaldata().y();
                        unit.z = swcData.swcdata(j).swcnodeinternaldata().z();
                        unit.radius = swcData.swcdata(j).swcnodeinternaldata().radius();
                        unit.parent = swcData.swcdata(j).swcnodeinternaldata().parent();
                        unit.seg_id = swcData.swcdata(j).swcnodeinternaldata().seg_id();
                        unit.level = swcData.swcdata(j).swcnodeinternaldata().level();
                        unit.mode = swcData.swcdata(j).swcnodeinternaldata().mode();
                        unit.timestamp = swcData.swcdata(j).swcnodeinternaldata().timestamp();
                        unit.feature_value = swcData.swcdata(j).swcnodeinternaldata().feature_value();
                        neurons.push_back(unit);
                    }

                    ESwc eSwc(savePath.string());
                    eSwc.setValue(neurons);
                    if (eSwc.WriteToFile()) {
                        ui->ResultTable->setItem(i, 3,
                                                 new QTableWidgetItem("Successfully"));
                        ui->ResultTable->setItem(i, 4,
                                                 new QTableWidgetItem(QString::fromStdString(savePath.string())));
                        setAllGridColor(i, Qt::green);
                        ui->ResultTable->item(i, 2)->setText(QString::fromStdString(
                                std::to_string(m_ExportSwcData[i].swcData.swcdata_size())));
                        continue;
                    } else {
                        statusMessage = "Write Swc File Failed!";
                        fileSavePath = "";
                    }
                } else {
                    statusMessage = "Ignored, Unknown Swc Type";
                    fileSavePath = "";
                }
            }
            ui->ResultTable->setItem(i, 3,
                                     new QTableWidgetItem(statusMessage));
            ui->ResultTable->setItem(i, 4,
                                     new QTableWidgetItem(fileSavePath));
            setAllGridColor(i, Qt::red);
        }
        m_ActionExportComplete = true;
        ui->ResultTable->resizeColumnsToContents();
    });
}

ViewExportSwcToFile::~ViewExportSwcToFile() {
    delete ui;
}

void ViewExportSwcToFile::setAllGridColor(int row, QColor color) {
    ui->ResultTable->item(row, 0)->setBackground(QBrush(color));
    ui->ResultTable->item(row, 1)->setBackground(QBrush(color));
    ui->ResultTable->item(row, 2)->setBackground(QBrush(color));
    ui->ResultTable->item(row, 3)->setBackground(QBrush(color));
    ui->ResultTable->item(row, 4)->setBackground(QBrush(color));
}
