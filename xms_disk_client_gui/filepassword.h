#pragma once

#include <QDialog>
#include "ui_filepassword.h"
#include<string>
class FilePassword : public QDialog
{
    Q_OBJECT

public:
    FilePassword(QWidget *parent = Q_NULLPTR);
    ~FilePassword();
    void accept() override;
    std::string password="";

private:
    Ui::FilePassword ui;
};
