/**
Released as open source by NCC Group Plc - http://www.nccgroup.com/

Developed by Gabriel Caudrelier, gabriel dot caudrelier at nccgroup dot com

https://github.com/nccgroup/pip3line

Released under AGPL see LICENSE for more information
**/

#ifndef SPLIT_H
#define SPLIT_H

#include "transformabstract.h"

class Split : public TransformAbstract
{
    Q_OBJECT
    
    public:
        static const int MAXGROUPVALUE = 1000;
        explicit Split();
        ~Split();
        QString name() const;
        QString description() const;
        void transform(const QByteArray &input, QByteArray &output);
        bool isTwoWays();
        QHash<QString, QString> getConfiguration();
        bool setConfiguration(QHash<QString, QString> propertiesList);
        QWidget * requestGui(QWidget * parent);
        static const QString id;
        QString help() const;

        char getSeparator();
        int getSelectedGroup();
        bool doWeTakeAllGroup();
        void setSeparator(char val);
        bool setSelectedGroup(int val);
        void setTakeAllGroup(bool val);
    private:
        char separator;
        int group;
        bool allGroup;
};

#endif // SPLIT_H
