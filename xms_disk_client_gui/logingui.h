#pragma once

#include <QDialog>
#include "ui_logingui.h"

class LoginGUI : public QDialog
{
    Q_OBJECT

public:
    LoginGUI(QWidget *parent = Q_NULLPTR);
    ~LoginGUI();
    void mouseMoveEvent(QMouseEvent *ev);
    void mousePressEvent(QMouseEvent *ev);
    void mouseReleaseEvent(QMouseEvent *ev);
    //void accept() override;
    //void reject() override;
public slots:
    void Login();
private:
    Ui::LoginGUI ui;
    
};
