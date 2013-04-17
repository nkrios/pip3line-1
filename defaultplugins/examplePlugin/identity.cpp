/**
Released as open source by NCC Group Plc - http://www.nccgroup.com/

Developed by Gabriel Caudrelier, gabriel dot caudrelier at nccgroup dot com

https://github.com/nccgroup/pip3line

Released under AGPL see LICENSE for more information
**/

#include "identity.h"

const QString Identity::id = "Identity";

Identity::Identity()
{
    // constructor, nothing new
}

Identity::~Identity()
{
    // destructor, nothing new
}

QString Identity::name() const {
    // return the identifier of your transformation
    // it MUST be unique from the ones already loaded
    // In case of duplicate the application will keep the first one to appear, and ignore the rest
    return id;
}

QString Identity::description() const {
    // short description of what your transformation does
    return tr("Identity transformation (not really useful)");
}

bool Identity::isTwoWays() {
    // Function used to tell the Gui if
    // it needs to display the encode/decode buttons
    return true;
}

void Identity::transform(const QByteArray &input, QByteArray &output) {
    // And here is the main part, where you actually do the dirty work
    // this method is NOT protected against concurrent thread call, so be carefull if you
    // use resources that are shared
    output = input;
}

// Optional methods

QHash<QString, QString> Identity::getConfiguration()
{
    QHash<QString, QString> properties = TransformAbstract::getConfiguration();
    // add here whatever property you want to save
    // You can use the predefined tag names from
    // xmlxommons.h
    // but that's optional

    return properties;
}

bool Identity::setConfiguration(QHash<QString, QString> propertiesList)
{
    // used to configure this class from a list of properties
    // should return true if the configuration succeded
    // false if there has been any error
    return TransformAbstract::setConfiguration(propertiesList);
}

QWidget *Identity::requestGui(QWidget *parent)
{
    //Ideally this should be something like:
     return new QWidget(parent);

     // you don't need to delete this object yourself, either the upperlayer gui or the
     // abstract destructor will take care of it
     // you also don't need to worry about duplicate as the Abstract object is taking care of it
}

