#include <QProcess>
#include <iostream>
#include <stdio.h>
#include <stdint.h>
#include "moduleservice.h"

moduleservice::moduleservice(QObject *parent) : QObject(parent) {

    modulservice = new QProcess;

    connect(modulservice, SIGNAL(readyRead()), this, SLOT(readStdOut()));
    connect(modulservice, SIGNAL(started()), this, SLOT(onStarted()));
    connect(modulservice, SIGNAL(finished(int)), this, SLOT(onFinished(int)));

#ifdef AP420
    QString program = "/root/git/homebus/tools/modulservice/bin/modulservice";
#else
    QString program = "/home/germana/Oeffentlich/git/homebus/tools/modulservice/bin/modulservice";
#endif
    QStringList arguments;
    arguments << "-c" << "/dev/hausbus1" << "-o" << "250" << "-s";

    modulservice->start(program, arguments);

}

void moduleservice::readStdOut() {

//   std::cout << QString(modulservice->readAllStandardOutput()).toUtf8().constData() << std::endl;

//   int i;
   bool doEmit = false;
   QByteArray output = modulservice->readAllStandardOutput();
   QList<QByteArray> lines = output.split('\n');

//   for (i = 0; i < lines.size(); i++) {
//       printf("%s\n", lines[i].data());
//   }


   if (resultState == eRsWaitForState) {
       if (qstrcmp(lines[0].data(), "OK") == 0) {
           res.data.state = eCmdOk;
       } else {
           res.data.state = eCmdError;
       }
       resultState = eRsIdle;
       res.type = eCmdState;
       doEmit = true;
   } else if (resultState == eRsWaitForActval) {
       resultState = eRsIdle;
       res.type = eActvalState;
       if ((lines.size() >= 10) && (qstrcmp(lines[9].data(), "OK") == 0)) {
           if (qstrcmp(lines[0].data(), "devType: SMIF") == 0) {
               res.data.actval.type = eDevSmif;
               if ((lines.size() >= 10) &&
                   (qstrcmp(lines[9].data(), "OK") == 0)) {
                   res.data.state = eCmdOk;
                   res.data.actval.data.smif.a_plus = strtol(strchr(lines[1].data(), ' ') + 1, 0, 0);
                   res.data.actval.data.smif.a_minus = strtol(strchr(lines[2].data(), ' ') + 1, 0, 0);
                   res.data.actval.data.smif.r_plus = strtol(strchr(lines[3].data(), ' ') + 1, 0, 0);
                   res.data.actval.data.smif.r_minus = strtol(strchr(lines[4].data(), ' ') + 1, 0, 0);
                   res.data.actval.data.smif.p_plus = strtol(strchr(lines[5].data(), ' ') + 1, 0, 0);
                   res.data.actval.data.smif.p_minus = strtol(strchr(lines[6].data(), ' ') + 1, 0, 0);
                   res.data.actval.data.smif.q_plus = strtol(strchr(lines[7].data(), ' ') + 1, 0, 0);
                   res.data.actval.data.smif.q_minus = strtol(strchr(lines[8].data(), ' ') + 1, 0, 0);
               } else {
                   res.data.state = eCmdError;
               }
               doEmit = true;
           }
       }
   }
   if (doEmit) {
       emit cmdConf(&res, resultDialog);
   }
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

bool moduleservice::command(const struct cmd *cmd, QDialog *dialog) {

    const char *str;
    char       cmdbuf[256];
    int        len;
    int        i;

//printf("moduleservice::command\n");

    switch (cmd->type) {
    case eSetvaldo31_do:
        str = "setvaldo31_do";
        break;
    case eSetvalpwm4:
        str = "setvalpwm4";
        break;
    case eSwitchstate:
        str = "switchstate";
        break;
    case eActval:
        str = "actval";
        break;
    default:
        str = 0;
        break;
    }
    if (str == 0) {
        // unsupported cmd
        return false;
    }
    len = snprintf(cmdbuf, sizeof(cmdbuf),  "-a %d -%s", cmd->destAddr, str);

    switch (cmd->type) {
    case eSetvaldo31_do:
        for (i = 0; i < 31; i++) {
            len += snprintf(cmdbuf + len, sizeof(cmdbuf) - len, " %d", cmd->data.setvaldo31_do.setval[i]);
        }
        resultState = eRsWaitForState;
        break;
    case eSetvalpwm4:
        for (i = 0; i < 4; i++) {
            len += snprintf(cmdbuf + len, sizeof(cmdbuf) - len, " %d %d", cmd->data.setvalpwm4.set[i], cmd->data.setvalpwm4.pwm[i]);
        }
        resultState = eRsWaitForState;
        break;
    case eSwitchstate:
        len += snprintf(cmdbuf + len, sizeof(cmdbuf) - len, " %d", cmd->data.switchstate);
        len += snprintf(cmdbuf + len, sizeof(cmdbuf) - len, " -o %d", cmd->ownAddr);
        resultState = eRsWaitForState;
        break;
    case eActval:
        resultState = eRsWaitForActval;
        break;
    default:
        break;
    }
    resultDialog = dialog;
    return command(cmdbuf);
}



bool moduleservice::waitForFinished(void) {

    return modulservice->waitForFinished();
}
