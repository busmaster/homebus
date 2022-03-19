#ifndef KAMERAEINGANGWINDOW_H
#define KAMERAEINGANGWINDOW_H

#include <QDialog>
#include <QTcpSocket>
#include "iostate.h"

namespace Ui {
class kameraeingangwindow;
}

class kameraeingangwindow : public QDialog
{
    Q_OBJECT

public:
    explicit kameraeingangwindow(QWidget *parent , ioState *state);
    ~kameraeingangwindow();
    void show(void);
    void hide(void);

signals:
    void disableScreenSaver(void);

private slots:
    void readTcpData(void);
    void onScreenSaverActivation(void);
    void on_pushButtonBack_clicked();
    void onIoStateChanged(void);

private:
    Ui::kameraeingangwindow *ui;
    bool isVisible;

    ioState *io;

    void proto(char *buf, unsigned int bufSize);
    char *copyJpgData(char *ch, unsigned int bufSize);

    QTcpSocket *socket;
    QByteArray data;

    enum {
        eStateHttpGet,
        eStateHttpHdr,
        eStateChunkFirst,
        eStateChunkNext,
        eStateChunkHdr
    } httpState;

    unsigned int  chunkLen;
    unsigned int  chunkIdx;

    unsigned int  jpgLen;
    unsigned int  jpgIdx;
    unsigned char jpgBuf[200000];

    QPixmap pic;
    QPixmap small;
};

#endif // KAMERAEINGANGWINDOW_H
