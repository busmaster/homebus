#include <QProcess>
#include <iostream>

#include "moduleservice.h"

moduleservice::moduleservice(QObject *parent) : QObject(parent) {

    modulservice = new QProcess;

    connect(modulservice, SIGNAL(readyRead()), this, SLOT(readStdOut()));
    connect(modulservice, SIGNAL(started()), this, SLOT(onStarted()));
    connect(modulservice, SIGNAL(finished(int)), this, SLOT(onFinished(int)));

#ifdef AP420
    QString program = "/root/git/homebus/tools/modulservice/bin/modulservice -c /dev/hausbus1 -o 250 -s";
#else
    QString program = "/home/germana/Oeffentlich/git/homebus/tools/modulservice/bin/modulservice -c /dev/hausbus1 -o 250 -s";
#endif
    modulservice->start(program);

}

void moduleservice::readStdOut() {
   std::cout << QString(modulservice->readAllStandardOutput()).toUtf8().constData() << std::endl;
}

void moduleservice::onStarted(){
    std::cout << "modulservice started" << std::endl;
}

void moduleservice::onFinished(int sig) {
    std::cout << "modulservice finished: " << sig << std::endl;
}

bool moduleservice::command(const char *cmd) {

//    std::cout << cmd << std::endl;

    modulservice->write(cmd);
    modulservice->write("\n");

    return true;
}

bool moduleservice::waitForFinished(void) {

    return modulservice->waitForFinished();
}
