/**
Released as open source by NCC Group Plc - http://www.nccgroup.com/

Developed by Gabriel Caudrelier, gabriel dot caudrelier at nccgroup dot com

https://github.com/nccgroup/pip3line

Released under AGPL see LICENSE for more information
**/

#include "md4.h"
#include <QCryptographicHash>

const QString Md4::id = "MD4";

QString Md4::name() const {
    return id;
}

QString Md4::description() const {
    return tr("Classic Md4 hash function");
}

bool Md4::isTwoWays() {
    return false;
}

QString Md4::help() const
{
    QString help;
    help.append("<p>Classic MD4 hash</p><p>This transformation is using the QT internal hash function.</p>");
    return help;
}

void Md4::transform(const QByteArray &input, QByteArray &output) {
    output = QCryptographicHash::hash(input,QCryptographicHash::Md4);
}
