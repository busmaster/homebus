
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <QPixmap>
#include <QTcpSocket>

#include "kameraeingangwindow.h"
#include "ui_kameraeingangwindow.h"

kameraeingangwindow::kameraeingangwindow(QWidget *parent, ioState *state) :
    QDialog(parent),
    ui(new Ui::kameraeingangwindow) {
    ui->setupUi(this);
    io = state;
    isVisible = false;
    socket = new QTcpSocket(this);
    connect(parent, SIGNAL(ioChanged(void)), this, SLOT(onIoStateChanged(void)));
    connect(parent, SIGNAL(screenSaverActivated()), this, SLOT(onScreenSaverActivation()));
    connect(this, SIGNAL(disableScreenSaver()), parent, SLOT(onDisableScreenSaver()));
    connect(socket, SIGNAL(readyRead()), SLOT(readTcpData()));
    httpState = eStateHttpHdr;
}

kameraeingangwindow::~kameraeingangwindow() {
    delete ui;
}

void kameraeingangwindow::readTcpData(void) {
    data = socket->readAll();
    proto((char*)(data.data()), data.count());
}

void kameraeingangwindow::onScreenSaverActivation(void) {
    if (isVisible) {
        hide();
    }
}

void kameraeingangwindow::onIoStateChanged(void) {
    if (io->glocke_taster && !isVisible) {
        emit disableScreenSaver();
        show();
    }
}

void kameraeingangwindow::hide(void) {
    isVisible = false;
    socket->disconnectFromHost();
    QDialog::hide();
}

void kameraeingangwindow::show(void) {
    isVisible = true;
    httpState = eStateHttpGet;
    kameraeingangwindow::proto(0, 0);
    QDialog::show();
}

char *kameraeingangwindow::copyJpgData(char *buf, unsigned int bufSize) {

    char *ch = buf;

    while ((jpgIdx < jpgLen)      &&
           (ch < (buf + bufSize)) &&
           (chunkIdx < chunkLen)  &&
           (jpgIdx < sizeof(jpgBuf))) {
        jpgBuf[jpgIdx] = *ch;
        jpgIdx++;
        ch++;
        chunkIdx++;
    }

    return ch;
}

void kameraeingangwindow::proto(char *buf, unsigned int bufSize) {

    bool         again = false;
    char         *ch = buf;
    char         *pos;

    do {
        switch (httpState) {
        case eStateHttpGet:
            socket->connectToHost("10.0.0.203", 8081);
            socket->write("GET / HTTP/1.1\r\nHost: 10.0.0.203:8081\r\nConnection: keep-alive\r\n\r\n");
            httpState = eStateHttpHdr;
            break;
        case eStateHttpHdr:
            httpState = eStateChunkHdr;
            jpgLen = 0;
            break;
        case eStateChunkHdr:
            if ((bufSize - (ch - buf)) == 0) {
                again = false;
                break;
            }
            chunkLen = strtoul(ch, &ch, 16);
            ch += 2; /* \r\n */
            chunkIdx = 0;
            if (jpgLen == 0) {
                httpState = eStateChunkFirst;
                jpgIdx = 0;
            } else {
                httpState = eStateChunkNext;
            }
            again = true;
            break;
        case eStateChunkFirst:
            /* the first chunk contains Content-Length, i.e. the jpg size */
            pos = ch;
            ch = strstr(ch, "Content-Length:");
            if (ch) {
                ch += sizeof("Content-Length:");
                jpgLen = strtoul(ch, &ch, 10);
                ch += 4; /* \r\n\r\n */
                chunkIdx += (ch - pos);
                ch = copyJpgData(ch, bufSize - (ch - buf));
                if ((bufSize - (ch - buf)) > 0) {
                    again = true;
                    httpState = eStateChunkHdr;
                } else {
                    again = false;
                    httpState = eStateChunkNext;
                }
            } else {
                httpState = eStateHttpGet;
                again = true;
                socket->disconnectFromHost();
            }
            break;
        case eStateChunkNext:
            ch = copyJpgData(ch, bufSize - (ch - buf));
            if (jpgIdx == jpgLen) {
                pic.loadFromData(jpgBuf, jpgLen, "JPG");
                small = pic.scaledToHeight(480);
                ui->label->setPixmap(small);
                ch += 4; /* terminating \r\n\r\n */
                jpgLen = 0;
                httpState = eStateChunkHdr;
                again = true;
                break;
            }
            if (chunkIdx == chunkLen) {
                ch += 2; /* \r\n */
                httpState = eStateChunkHdr;
                again = true;
            } else {
                again = false;
            }
            break;
        default:
            break;
        }
    } while (again);

}

void kameraeingangwindow::on_pushButtonBack_clicked() {
    hide();
}
