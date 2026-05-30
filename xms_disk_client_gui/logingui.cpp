#include "logingui.h"
#include <QMouseEvent>
#include "xauth_client.h"
#include <string>
using namespace std;
LoginGUI::LoginGUI(QWidget *parent)
    : QDialog(parent)
{
    ui.setupUi(this);

    setWindowFlags(Qt::FramelessWindowHint);

    setAttribute(Qt::WA_TranslucentBackground);
    ui.err_frame->hide();


}

LoginGUI::~LoginGUI()
{
}

void LoginGUI::Login()
{
    ui.err_frame->show();
    string username(ui.usernameEdit->text().toLocal8Bit().constData());
    string password(ui.passwordEdit->text().toLocal8Bit().constData());
    if (username.empty() || password.empty())
    {
        ui.err_msg->setText(QString::fromUtf8("用户名密码不能为空"));
        return;
    }

    if (!XAuthClient::Get()->Login(username, password))
    {

        ui.err_msg->setText(QString::fromUtf8("用户名或密码错误"));
        return;
    }
    static int count = 0;
    count++;
    
    ui.err_msg->setText(QString::number(count)+QString::fromUtf8("登录成功"));
    QDialog::accept();
}


static bool mouse_press = false;
static QPoint mouse_point;
void LoginGUI::mouseMoveEvent(QMouseEvent *ev)
{
    //û�а��£�����ԭ�¼�
    if (!mouse_press)
    {
        QWidget::mouseMoveEvent(ev);
        return;
    }
    auto cur_pos = ev->globalPos();
    this->move(cur_pos - mouse_point);
}
void LoginGUI::mousePressEvent(QMouseEvent *ev)
{
    //���������¼�¼λ��
    if (ev->button() == Qt::LeftButton)
    {
        mouse_press = true;
        mouse_point = ev->pos();
    }

}
void LoginGUI::mouseReleaseEvent(QMouseEvent *ev)
{
    mouse_press = false;
}