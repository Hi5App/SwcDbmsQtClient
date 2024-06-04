#include "EditorAttachmentView.h"

#include "ui_EditorAttachmentView.h"

#include "EditorAnoAttachment.h"
#include "EditorApoAttachment.h"
#include "EditorSwcAttachment.h"

EditorAttachmentView::EditorAttachmentView(const std::string& swcUuid, const std::string& swcName, QWidget *parent) :
    QDialog(parent), ui(new Ui::EditorAttachmentView) , m_SwcUuid(swcUuid),m_SwcName(swcName){
    ui->setupUi(this);

    connect(ui->AnoBtn,&QPushButton::clicked,this,[&]() {
        EditorAnoAttachment view(m_SwcUuid, m_SwcName,this);
        view.exec();
    });

    connect(ui->ApoBtn,&QPushButton::clicked,this,[&]() {
        EditorApoAttachment view(m_SwcUuid, m_SwcName,this);
        view.exec();
    });

    connect(ui->SwcAttachmentBtn,&QPushButton::clicked,this,[&]() {
        EditorSwcAttachment view(m_SwcUuid, m_SwcName,this);
        view.exec();
    });
}

EditorAttachmentView::~EditorAttachmentView() {
    delete ui;
}
