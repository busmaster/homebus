
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <QPixmap>
#include <QTcpSocket>

#include "kameraeingangwindow.h"
#include "ui_kameraeingangwindow.h"

const unsigned char crlfcrlf[] = { '\r', '\n', '\r', '\n'};
const unsigned char lfcrlf[] = { '\n', '\r', '\n'};
const unsigned char jpgHdr[] = { '-',  '-',  'B',  'o',  'u',  'n',  'd',  'a',
                                 'r',  'y',  'S',  't',  'r',  'i',  'n',  'g',
                                 '\r', '\n', 'C',  'o',  'n',  't',  'e',  'n',
                                 't',  '-',  't',  'y',  'p',  'e',  ':',  ' ',
                                 'i',  'm',  'a',  'g',  'e',  '/',  'j',  'p',
                                 'e',  'g',  '\r', '\n', 'C',  'o',  'n',  't',
                                 'e',  'n',  't',  '-',  'L',  'e',  'n',  'g',
                                 't',  'h',  ':'
                               };

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
    jpgState = eStateInit;
    rate_reduction = -1;
}

kameraeingangwindow::~kameraeingangwindow() {
    delete ui;
}

void kameraeingangwindow::readTcpData(void) {
    data = socket->readAll();
    proto((unsigned char*)(data.data()), data.count());
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
    rate_reduction = -1;
    jpgState = eStateInit;
    socket->connectToHost("10.0.0.203", 8081);
    QDialog::show();
}

bool kameraeingangwindow::seqCompare(
    const unsigned char *seq, unsigned int seqLen,
    const unsigned char *data, unsigned int dataLen,
    unsigned int *seqIdx, unsigned int *bufIdx) {

    unsigned int i;
    unsigned int j = *seqIdx;

    for (i = 0; (i < dataLen) && (j < seqLen); i++) {
       if (seq[j] == data[i]) {
          j++;
       } else {
          j = 0;
       }
    }
    *bufIdx += i;
    if (j == seqLen) {
        return true;
    } else {
        *seqIdx = j;
        return false;
    }
}

void kameraeingangwindow::proto(unsigned char *buf, unsigned int bufSize) {

    unsigned int bufIdx = 0;
    bool         again = false;
    char         *endPtr;

    do {
        switch (jpgState) {
        case eStateInit:
            seqIdx = 0;
            jpgState = eStateHttpHdr;
            again = true;
            break;
        case eStateHttpHdr:
            if (seqCompare(crlfcrlf, sizeof(crlfcrlf), buf + bufIdx,
                           bufSize - bufIdx, &seqIdx, &bufIdx)) {
                jpgState = eStateJpgHdr;
                seqIdx = 0;
                again = true;
            } else {
                again = false;
            }
            break;
        case eStateJpgHdr:
            if (seqCompare(jpgHdr, sizeof(jpgHdr), buf + bufIdx,
                           bufSize - bufIdx, &seqIdx, &bufIdx)) {
                jpgState = eStateJpgLen;
                seqIdx = 0;
                jpgLenIdx = 0;
                again = true;
            } else {
                again = false;
            }
            break;
        case eStateJpgLen:
            while ((jpgLenIdx < sizeof(jpgLenBuf)) &&
                   (bufIdx < bufSize) &&
                   (buf[bufIdx] != '\r')) {
                jpgLenBuf[jpgLenIdx] = buf[bufIdx];
                jpgLenIdx++;
                bufIdx++;
            }
            if (buf[bufIdx] == '\r') {
                jpgLenBuf[jpgLenIdx] = '\0';
                jpgLen = strtoul(jpgLenBuf, &endPtr, 0);
                jpgState = eStateJpgLenTermination;
                again = true;
            } else {
                again = false;
            }
            break;
        case eStateJpgLenTermination:
            if (seqCompare(lfcrlf, sizeof(lfcrlf), buf + bufIdx,
                           bufSize - bufIdx, &seqIdx, &bufIdx)) {
                jpgState = eStateJpgData;
                seqIdx = 0;
                jpgIdx = 0;
                again = true;
            } else {
                again = false;
            }
            break;
        case eStateJpgData:
            while ((bufIdx < bufSize) &&
                   (jpgIdx < jpgLen) &&
                   (jpgIdx < sizeof(jpgBuf))) {
                jpgBuf[jpgIdx] = buf[bufIdx];
                jpgIdx++;
                bufIdx++;
            }
            if (jpgIdx == jpgLen) {
                if (rate_reduction >= 0) {
                    pic.loadFromData(jpgBuf, jpgLen, "JPG");
                    small = pic.scaledToHeight(480);
                    ui->label->setPixmap(small);
                    rate_reduction = 0;
                }
                rate_reduction++;
                /* next jpg */
                jpgState = eStateJpgHdr;
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
