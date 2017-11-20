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

    bool seqCompare(const unsigned char *seq, unsigned int seq_len,
                    const unsigned char *data, unsigned int data_len,
                    unsigned int *seq_idx, unsigned int *buf_idx);
    void proto(unsigned char *buf, unsigned int bufSize);

    QTcpSocket *socket;
    QByteArray data;

    enum {
        eStateInit,
        eStateHttpHdr,
        eStateJpgHdr,
        eStateJpgLen,
        eStateJpgLenTermination,
        eStateJpgData,
        eStateJpgReady
    } jpgState;

    unsigned int seqIdx;

    char          jpgLenBuf[20];
    unsigned int  jpgLenIdx;
    unsigned int  jpgLen;

    unsigned char jpgBuf[200000];
    unsigned int  jpgIdx;

    unsigned int rate_reduction;

    QPixmap pic;
    QPixmap small;
};

#endif // KAMERAEINGANGWINDOW_H
