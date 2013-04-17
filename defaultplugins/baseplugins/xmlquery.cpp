/**
Released as open source by NCC Group Plc - http://www.nccgroup.com/

Developed by Gabriel Caudrelier, gabriel dot caudrelier at nccgroup dot com

https://github.com/nccgroup/pip3line

Released under AGPL see LICENSE for more information
**/

#include "xmlquery.h"
#include "confgui/xmlquerywidget.h"
#include <QXmlQuery>
#include <QtGlobal>
#include <QTextStream>
#include <QBuffer>
#include <QXmlFormatter>
#include <QDebug>

const QString XmlQuery::id = "XML XPath/XQuery";

XmlQuery::XmlQuery()
{
    messHandler = new XmlMessageHandler(this);
}

XmlQuery::~XmlQuery()
{
    delete messHandler;
}

QString XmlQuery::name() const
{
    return id;
}

QString XmlQuery::description() const
{
    return tr("XML XPath/XQuery plugin. Used to apply an XML search/transformation to a XML document.");
}

void XmlQuery::transform(const QByteArray &input, QByteArray &output)
{
    if (input.isEmpty() || queryString.isEmpty())
        return;
    QString doc = QString::fromUtf8(input);
    output.clear();
    QXmlQuery query;

    query.setMessageHandler(messHandler);
    query.setFocus(doc);
    query.setQuery(queryString);

    if (query.isValid()) {
        QStringList qout;
        if (query.evaluateTo(&qout)) {
            if (qout.size() < 1) {
                return;
            }
            if (qout.size() == 0) {
                emit warning("No results returned",id);
            }
            output.clear();
            for (int i = 0; i < qout.size(); i++) {
                if (!qout.at(i).isEmpty())
                    output.append(qout.at(i)).append('\n');
            }
        } else {
            QBuffer buffer(&output);
            buffer.open(QIODevice::WriteOnly);
            QXmlFormatter formatter(query, &buffer);
            if (!query.evaluateTo(&formatter)) {
                emit error(tr("Error while evaluating the query"),id);
            }
        }
    } else {
        emit error(tr("Invalid Query"),id);
    }

}

bool XmlQuery::isTwoWays()
{
    return false;
}

QHash<QString, QString> XmlQuery::getConfiguration()
{
    QHash<QString, QString> properties = TransformAbstract::getConfiguration();
    properties.insert(XMLQUERYSTRING,QString(queryString.toLatin1().toBase64()));

    return properties;
}

bool XmlQuery::setConfiguration(QHash<QString, QString> propertiesList)
{
    bool res = TransformAbstract::setConfiguration(propertiesList);

    queryString = QString(QByteArray::fromBase64(propertiesList.value(XMLQUERYSTRING).toUtf8()));
    return res;
}

QWidget *XmlQuery::requestGui(QWidget *parent)
{
    return new XmlQueryWidget(this, parent);
}

QString XmlQuery::help() const
{
    QString help;
    help.append("<p>Apply an XMLQuery on the input (UTF-8)</p><p>The input must be an XML document in order to make sense.</p>");
    help.append("<p>There are two output modes, depending on what comes out the transformation:");
    help.append("<ul><li>Pure text, if the XMLQuery produce text only</li><li>XML document</li></ul></p>");
    help.append("<p>For example, if we process an nmap scan in XML format, this query: ");
    help.append("<ul><li>/nmaprun/host/ports</li></ul> will return an XML document containing all the \"ports\" nodes, whereas:");
    help.append("<ul><li>/nmaprun/host/ports/port/@portid/string()</li></ul>will return the list of port numbers, one by line.</p>");
    help.append("<p><b>Warning: </b>the current engine does not support external entities, in short no SOAP support</p>");
    return help;
}

void XmlQuery::logMessage(QtMsgType type, const QString &description)
{
    if (type == QtWarningMsg  || type == QtDebugMsg) {
        emit warning(description, id);
    } else {
        emit error(description,id);
    }
}

QString XmlQuery::getQueryString()
{
    return queryString;
}

void XmlQuery::setQueryString(QString query)
{
    queryString = query;
    emit confUpdated();
}

XmlMessageHandler::XmlMessageHandler(XmlQuery *parent)
{
    parentClass = parent;
}

void XmlMessageHandler::handleMessage(QtMsgType type, const QString &description, const QUrl & identifier, const QSourceLocation & location)
{
    qDebug() << "Message received";
    parentClass->logMessage(type,QString("line %2 column %3 : %4 (%1)").arg(identifier.toString()).arg(location.line()).arg(location.column()).arg(description));
}

