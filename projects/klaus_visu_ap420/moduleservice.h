#ifndef MODULESERVICE_H
#define MODULESERVICE_H

#include <QObject>
#include <QProcess>

class moduleservice : public QObject {
    Q_OBJECT
public:
    explicit moduleservice(QObject *parent = 0);
    bool command(const char *cmd);
    bool waitForFinished(void);

private slots:
    void readStdOut();
    void onStarted();
    void onFinished(int sig);

private:
    QProcess *modulservice;

};

#endif // MODULESERVICE_H
