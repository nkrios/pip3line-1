/**
Released as open source by NCC Group Plc - http://www.nccgroup.com/

Developed by Gabriel Caudrelier, gabriel dot caudrelier at nccgroup dot com

https://github.com/nccgroup/pip3line

Released under AGPL see LICENSE for more information
**/

#include "transformmgmt.h"
#include <QObject>
#include <QMutexLocker>
#include <QTextStream>
#include <QPluginLoader>
#include <QFileInfo>
#include <QSet>
#include <QDebug>
#include <QDir>
#include <QCoreApplication>
#include <QSetIterator>
#include <QMetaType>
#include "composedtransform.h"
#include "../version.h"

const int TransformMgmt::MAX_TYPE_NAME_LENGTH = 20;
const QString TransformMgmt::APP_PLUGIN_DIRECTORY = "plugins";
const QString TransformMgmt::CONF_FILE = "pip3line.conf";
const QString TransformMgmt::SETTINGS_SAVED_CONF = "SavedChains";
const QRegExp TransformMgmt::TAG_NAME_REGEXP = QRegExp("^[a-zA-Z_][-a-zA-Z0-9_\\.]{1,100}$");
const int TransformMgmt::MAX_NESTING = 5;
const QString TransformMgmt::LOG_ID = "TransformMgmt";

using namespace Pip3lineConst;

TransformMgmt::TransformMgmt()
{
    id = tr("TransformMgmt");
    settings = NULL;
    cycleSem.release(MAX_NESTING);
}

TransformMgmt::~TransformMgmt()
{
    unloadTransforms();
    unloadPlugins();
    if (settings != NULL)
        delete settings;

    if (transformInstances.isEmpty()) {
        qDebug() << "No TransformAbstract instances left :D";
    } else {
        QTextStream cout(stderr);
        qWarning("TransformAbstract instances still present T_T (Memory leak)");
        QSetIterator<TransformAbstract *> i(transformInstances);
         while (i.hasNext())
             cout << " => " << i.next() << endl;
    }
}

bool TransformMgmt::initialize(const QString &baseDirectory)
{
    qRegisterMetaType<Messages>("Messages");
    if (baseDirectory.isEmpty()) {
        qDebug() << tr("Application dir path is empty. Plugins won't probably load.");
    }
    pluginsDirectories << QString("%1/%2").arg(baseDirectory).arg(APP_PLUGIN_DIRECTORY);
#if defined(Q_OS_LINUX)
    pluginsDirectories << QDir(QString("%1/../share/pip3line").arg(baseDirectory)).absolutePath();
#endif

    QString userDirectory = getHomeDirectory();
    if (!userDirectory.isEmpty())
        pluginsDirectories << userDirectory;

    QCoreApplication::setOrganizationName(APPNAME);
    QCoreApplication::setApplicationName(APPNAME);

    settings = getSettingsObj();

    emit status (tr("Using libtransform v.%1").arg(LIB_TRANSFORM_VERSION),id);

    bool ret = loadPlugins();
    return loadTransforms(false) && ret;
}

bool TransformMgmt::loadTransforms(bool verbose) {
    QMutexLocker locker(&listLocker);
    bool noError = true;
    QSet<QString> typesSet;
    transformTypesList.clear();
    transformNameList.clear();
    typesList.clear();
    savedConf.clear();

    // loading types names from the plugins
    QHashIterator<QString, TransformFactoryPluginInterface *> it(pluginsList);
     while (it.hasNext()) {
        it.next();
        QList<QString> list = it.value()->getTypesList();
        for (int j = 0; j < list.size(); j++) {
            if (list.at(j).size() > MAX_TYPE_NAME_LENGTH) {
                if (verbose)
                    emit error(tr("Type name too long (%1), cutting it down to %2 characters.").arg(list[j].size()).arg(MAX_TYPE_NAME_LENGTH),id);
                list[j] = list[j].mid(0,MAX_TYPE_NAME_LENGTH);
            }
        }

        typesSet.unite(QSet<QString>::fromList(list));
    }

    typesSet.insert(DEFAULT_TYPE_USER);

    typesList = typesSet.toList();
    qSort(typesList);

    // loading transforms from plugins
    QStringList enclist;
    for (int i = 0; i < typesList.size(); i++) {
        if (verbose)
            emit status(tr("Loading Transforms type \"%1\"").arg(typesList.at(i)),id);
        QHashIterator<QString, TransformFactoryPluginInterface *> j(pluginsList);
         while (j.hasNext()) {
             j.next();
             QList<QString> nameList = j.value()->getTransformList(typesList.at(i));
             int count = 0;
             for (int k = 0 ; k < nameList.size(); k++) {
                 if (!transformNameList.contains(nameList.at(k))) {
                     enclist.append(nameList.at(k));
                     transformNameList.insert(nameList.at(k), j.value());
                     count++;
                 } else {
                     emit warning(tr("Duplicate: a transformation named \"%1\" is already in use from \"%2\", ignoring the one from \"%3\"").arg(nameList.at(k)).arg(transformNameList.value(nameList.at(k))->pluginName()).arg(j.key()),id);
                 }
             }
             if (count != 0 && verbose)
                 emit status(tr("%1 transform(s) loaded for \"%2\"").arg(count).arg(j.key()),id);
         }
        transformTypesList.insert(typesList.at(i), enclist);
        enclist.clear();
    }

    //loading composed transforms from persistent storage
    settings->beginGroup(SETTINGS_SAVED_CONF);
    QStringList keys = settings->childKeys();

    for (int i = 0; i < keys.size(); i++) {
        QString name = keys.at(i);
        QString conf = settings->value(name, QString()).toString();
        if (conf.isEmpty()) {
            logError(tr("The saved configuration %1 returned an empty configuration").arg(name), id);
        } else if (transformNameList.contains(name)) {
            emit warning(tr("Duplicate: a transformation named \"%1\" is already in use from \"%2\", ignoring the one from the persistent store").arg(name).arg(transformNameList.value(name)->pluginName()),id);
        } else {
            savedConf.insert(name, conf);
            enclist.append(name);
        }
    }
    settings->endGroup();

    transformTypesList.insert(DEFAULT_TYPE_USER, enclist);

    enclist.clear();

    emit transformsUpdated();
    return noError;
}

void TransformMgmt::saveInstance(TransformAbstract *ta)
{
    if (ta == NULL)
        return;

    if (transformInstances.contains(ta)) {
        if (QString(ta->metaObject()->className()) == QString("ComposedTransform"))
            qDebug() << tr("Composed Class already registered") << ta;
        else
            qWarning() << tr("Class already registered T_T") << ta;
    }
    else {
        transformInstances.insert(ta);
        connect(ta,SIGNAL(destroyed()), this, SLOT(OnTransformDelete()));
    }
}

void TransformMgmt::OnTransformDelete()
{
    TransformAbstract* src = (TransformAbstract* ) sender();
    if (src != NULL) {
        if (transformInstances.contains(src)) {
            transformInstances.remove(src);
        } else {
            qDebug() << "Could not find " << src << " in the instance list";
        }
    }
}

bool TransformMgmt::loadPlugins()
{
    QMutexLocker locker(&listLocker);
    bool noError = true;

    // loading static plugins first
    foreach (QObject *plugin, QPluginLoader::staticInstances()) {
        TransformFactoryPluginInterface *tro = qobject_cast<TransformFactoryPluginInterface *>(plugin);
        if (tro != NULL) {
           registerPlugin(tro);
           emit status(tr("Loaded static plugin: \"%1\" version %2 (compiled with Qt %3)").arg(tro->pluginName()).arg(tro->pluginVersion()).arg(tro->compiledWithQTversion()),id);
        } else {
            emit error(tr("Static plugin is using the wrong interface"),id);
            noError = false;
        }
    }

    // then loading dynamic plugins
    for (int i = 0; i < pluginsDirectories.size(); i++) {
        emit status(tr("Looking for plugins in %1").arg(pluginsDirectories.at(i)),id);
        QDir pluginsDir = QDir(pluginsDirectories.at(i));
        QStringList filters;

#if defined(Q_OS_UNIX)
        filters << "*.so";
#elif defined(Q_OS_WIN)
        filters << "*.dll";
#endif

        pluginsDir.setNameFilters(filters);

        foreach (QString fileName, pluginsDir.entryList(QDir::Files)) {
            QString absName = pluginsDir.absoluteFilePath(fileName);

             QPluginLoader *loader = new(std::nothrow) QPluginLoader(absName);
             if (loader == NULL) {
                 qFatal("Cannot allocate memory for QPluginLoader X{");
                 return false;
             }
             QObject *plugin = loader->instance();
             if (plugin) {
                 TransformFactoryPluginInterface *tro = qobject_cast<TransformFactoryPluginInterface *>(plugin);
                 if (tro != NULL) {
                    if (tro->getLibTransformVersion() == LIB_TRANSFORM_VERSION) {
                        if (!pluginsList.contains(tro->pluginName())) {
                            registerPlugin(tro);
                            pluginLibs.append(loader);
                            emit status(tr("Loaded plugin: \"%1\" version %2 (compiled with %3)").arg(tro->pluginName()).arg(tro->pluginVersion()).arg(tro->compiledWithQTversion()),id);
                        } else {
                            emit error(tr("Another plugin with the name %1 has already been loaded, skipping this one.").arg(tro->pluginName()),id);
                            delete loader;
                            noError = false;
                        }

                    } else {
                        emit error(tr("The plugin \"%1\" use a different version (v.%2) of libtransform than the current executable (v.%3), skipping.").arg(tro->pluginName()).arg(tro->getLibTransformVersion()).arg(LIB_TRANSFORM_VERSION),id);
                        delete plugin;
                        delete loader;
                        noError = false;
                    }
                 } else {
                     emit error(tr("Plugin %1 is using a different Factory interface, skipping.").arg(pluginsDir.absoluteFilePath(fileName)),id);
                     delete plugin;
                     delete loader;
                     noError = false;
                 }

             } else {
                 emit error(tr("Could not load %1: %2").arg(pluginsDir.absoluteFilePath(fileName)).arg(loader->errorString()),id);
                 delete loader;
                 noError = false;
             }
        }
    }

    return noError;
}

void TransformMgmt::unloadPlugins()
{
    QMutexLocker locker(&listLocker);

    foreach (TransformFactoryPluginInterface * val, pluginsList)
         delete val;

    foreach (Pip3lineCallback * val, callbackList)
         delete val;

    QPluginLoader *loader = NULL;
    while (!pluginLibs.isEmpty()) {
        loader = pluginLibs.takeLast();
        loader->unload();
        delete loader;
    }
}

void TransformMgmt::unloadTransforms() {
    QMutexLocker locker(&listLocker);
    transformTypesList.clear();
    transformNameList.clear();
    typesList.clear();
    savedConf.clear();
}

TransformAbstract * TransformMgmt::getTransform(QString name) {

    TransformAbstract * ta = NULL;
    listLocker.lock();
    if (!cycleSem.tryAcquire()) {
        listLocker.unlock();
        logError(tr("Reached max nested limit (%1) or there is a cycle within the nested chains. And if you are a pentester, nice try XD").arg(MAX_NESTING),id);
        return ta;
    }

    if (transformNameList.contains(name)) {
        TransformFactoryPluginInterface *plugin = transformNameList.value(name);
        listLocker.unlock();
        ta = plugin->getTransform(name);

        if (ta == NULL)
            emit error(tr("The plugin could not instanciate the transformation object named \"%1\" v_v").arg(name),id);
    } else if (savedConf.contains(name)) {
        listLocker.unlock();

        TransformChain chain = loadChainFromSaved(name);
        if (!chain.isEmpty()) {
            ta = new(std::nothrow) ComposedTransform(chain);
            if (ta == NULL) {
                qFatal("Cannot allocate memory for ComposedTransform 1 X{");
            }
        }
    }else {
        listLocker.unlock();
        emit error(tr("No transformation named \"%1\" was found in the current plugins and the persistent storage").arg(name),id);
    }

    saveInstance(ta);
    cycleSem.release();
    return ta;
}

const QStringList TransformMgmt::getTransformsList(QString typeName) {
    QMutexLocker locker(&listLocker);
    return transformTypesList.value(typeName);
}

const QStringList TransformMgmt::getTypesList()
{
    return typesList;
}

bool TransformMgmt::saveConfToXML(const TransformChain &chain, QXmlStreamWriter *stream)
{
    QHash<QString, QString> properties;
    stream->setAutoFormatting(true);
    stream->writeStartDocument();

    stream->writeStartElement(XMLPIP3LINECONF);
    stream->writeAttribute(XMLVERSIONMAJOR, QString::number(LIB_TRANSFORM_VERSION));
    stream->writeAttribute(PROP_NAME, chain.getName());
    stream->writeAttribute(XMLDESCRIPTION, chain.getDescription());
    stream->writeAttribute(XMLHELP, chain.getHelp());
    stream->writeAttribute(XMLFORMAT, QString::number(chain.getFormat()));

    for (int i = 0 ; i < chain.size(); i++) {

        properties = chain.at(i)->getConfiguration();
        properties.insert(PROP_NAME, chain.at(i)->name());
        properties.insert(PROP_WAY, QString::number((int)chain.at(i)->way()));
        properties.insert(XMLORDER, QString::number(i));

        stream->writeStartElement(XMLTRANSFORM);

        QHashIterator<QString, QString> j(properties);
         while (j.hasNext()) {
             j.next();
             if (isValidAttributeName(j.key())) {
                stream->writeAttribute(j.key(), j.value());
             } else {
                 emit error(tr("Ignoring property \"%1\": Properties names need to match this regexp \"%2\"").arg(j.key()).arg(TAG_NAME_REGEXP.pattern()),id);
             }
         }
        stream->writeEndElement();
    }

    stream->writeEndElement(); // pip3line
    stream->writeEndDocument();
    if (stream->hasError()) {
        emit error(tr("XML writer: failed to write to the underlying device"), id);
        return false;
    }
    return true;
}

TransformChain TransformMgmt::loadConfFromXML(QXmlStreamReader *stream)
{
    QMap<int, TransformAbstract *> transformChildren;
    bool ok;
    QString name;
    TransformChain final;

    if (!stream->readNextStartElement() ) {
        emit error(stream->errorString().append(tr(" Cannot read first tag")),id);
        return final;
    }
    if (stream->name() != XMLPIP3LINECONF) {
        emit error(tr("Incorrect root tag name1: \"%1\" (should be \"%2\")").arg(stream->name().toString()).arg(XMLPIP3LINECONF),id);
        return final;
    }

//    removed because not really usefull
//
//    int fileVersion = stream->attributes().value(XMLVERSIONMAJOR).toString().toInt(&ok);
//    if (!ok) {
//        emit error(tr("Error while parsing the version number \"%1\". Ignoring it for now, et Advienne que pourra!").arg(stream->attributes().value(XMLVERSIONMAJOR).toString()),id);

//    } else if (fileVersion != LIB_TRANSFORM_VERSION) {
//        emit warning(tr("The libtransform version (v.%1) of the xml document differ from the current one (v.%2), this may (or may not) produce errors.").arg(fileVersion).arg(LIB_TRANSFORM_VERSION),id);
//    }

    final.setName(stream->attributes().value(PROP_NAME).toString());
    final.setDescription(stream->attributes().value(XMLDESCRIPTION).toString());
    final.setHelp(stream->attributes().value(XMLHELP).toString());
    int format = stream->attributes().value(XMLFORMAT).toString().toInt(&ok);
    if (ok && (format == 0 || format == 1)) {
        final.setFormat((format == 0 ? TEXTFORMAT : HEXAFORMAT));
    }

    int order = 0;
    TransformAbstract * ntw = NULL;
    QHash<QString, QString> properties;

    while (!stream->atEnd()) {

        if (!stream->readNextStartElement())
            continue;

        name = stream->attributes().value(PROP_NAME).toString();
        order = stream->attributes().value(XMLORDER).toString().toInt(&ok);

        if (ok) {
          ntw = getTransform(name);
          if (ntw != NULL) {
              connect(ntw,SIGNAL(error(QString,QString)), this, SLOT(logError(QString,QString)));
              connect(ntw,SIGNAL(warning(QString,QString)), this, SLOT(logError(QString,QString)));

              QXmlStreamAttributes attrs = stream->attributes();

              for (int j = 0; j < attrs.size(); j++) {
                  QString attrname = attrs.at(j).name().toString();
                  if ( attrname != XMLORDER && attrname != PROP_NAME) {
                      properties.insert(attrname,attrs.at(j).value().toString());
                  }
              }
              if (!ntw->setConfiguration(properties)) {
                  delete ntw;
              } else {
                  transformChildren[order] = ntw;
                  disconnect(ntw,SIGNAL(error(QString,QString)), this, SLOT(logError(QString,QString)));
                  disconnect(ntw,SIGNAL(warning(QString,QString)), this, SLOT(logError(QString,QString)));
              }
          }
        } else {
            if (name.isEmpty())
                name = tr("(Could not find the Name attribute)");
            emit error(tr("\"%1\": Error while parsing the order \"%2\"").arg(name).arg(stream->attributes().value(XMLORDER).toString()),id);
        }
    }
    final.append(transformChildren.values());

    return final;
}

TransformChain TransformMgmt::loadConfFromFile(const QString &fileName)
{
    TransformChain finalList;
    if (fileName.isEmpty()) {
        emit error(tr("Empty file name given"),id);
        return finalList;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        emit error(tr("Error while opening the file:\n \"%1\"").arg(file.errorString()),id);
        return finalList;
    }

    QXmlStreamReader streamin(&file);

    finalList = loadConfFromXML(&streamin);

    file.close();

    return finalList;
}

bool TransformMgmt::saveConfToFile(const QString &fileName, const TransformChain &transformList)
{
    if (transformList.size() == 0) {
        error(tr("No transformation selected, nothing to save!"),id);
        return false;
    }
    if (fileName.isEmpty()) {
        emit error(tr("Empty file name given"),id);
        return false;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        emit error(tr("Error while opening the file:\n \"%1\"").arg(file.errorString()),id);
        return false;
    }

    QXmlStreamWriter streamOut(&file);

    bool ret = saveConfToXML(transformList,&streamOut);

    file.close();

    return ret;
}

TransformAbstract *TransformMgmt::loadTransformFromConf(const QHash<QString, QString> confEle)
{
    TransformAbstract *transf = NULL;
    if (confEle.contains(PROP_NAME)) {
        transf = getTransform(confEle.value(PROP_NAME));
        if (transf != NULL) {
            connect(transf,SIGNAL(error(QString,QString)), this, SLOT(logError(QString,QString)));
            connect(transf,SIGNAL(warning(QString,QString)), this, SLOT(logError(QString,QString)));
            transf->setConfiguration(confEle);
            disconnect(transf,SIGNAL(error(QString,QString)), this, SLOT(logError(QString,QString)));
            disconnect(transf,SIGNAL(warning(QString,QString)), this, SLOT(logError(QString,QString)));
        }
    } else {
        emit error (tr("Missing property \"%1\" in the configuration").arg(PROP_NAME),id);
    }
    return transf;
}

QHash<QString, TransformFactoryPluginInterface *> TransformMgmt::getPlugins()
{
    return pluginsList;
}

QStringList TransformMgmt::getPluginsDirectory()
{
    return pluginsDirectories;
}

QString TransformMgmt::getHomeDirectory()
{
    QString userDirectory;
    QDir homeDir = QDir::home();

    if (homeDir.cd(USER_DIRECTORY)) {
        userDirectory = homeDir.absolutePath();
    } else {
        if (homeDir.mkpath(USER_DIRECTORY)) {
            userDirectory = homeDir.absolutePath().append(QDir::separator()).append(USER_DIRECTORY);
        } else {
            qWarning("Cannot create user directory");
        }
    }

    return userDirectory;
}

bool TransformMgmt::registerChainConf(const TransformChain &transfChain, bool persistent)
{
    QString name = transfChain.getName();
    if (name.isEmpty()) {
        logError(tr("Empty name passed to registerChainConf, ignoring"),id);
        return false;
    }

    listLocker.lock();
    if (transformNameList.contains(name)) {
        listLocker.unlock();
        logError(tr("Cannot register this transformation, the name \"%1\" is already taken").arg(transformNameList.value(name)->pluginName()),id);
        return false;
    }
    QString conf;
    QXmlStreamWriter writer(&conf);
    if (saveConfToXML(transfChain, &writer)) {
        savedConf.insert(transfChain.getName(), conf);
        QStringList userList = transformTypesList.value(DEFAULT_TYPE_USER, QStringList());
        if (!userList.contains(name)) {
            userList.append(name);
            qSort(userList);
            transformTypesList.insert(DEFAULT_TYPE_USER, userList);
        }
        listLocker.unlock();

        if (persistent) {
            setPersistance(transfChain.getName(), persistent);
        }
        emit transformsUpdated();
        emit savedUpdated();
    } else {
        listLocker.unlock();
    }
    return true;
}

bool TransformMgmt::unregisterChainConf(const QString &name)
{
    if (name.isEmpty()) {
        logError(tr("Empty name passed to unregisterChainConf, ignoring"),id);
        return false;
    }
    listLocker.lock();
    if (savedConf.contains(name)) {
        listLocker.unlock();
        setPersistance(name, false);
        listLocker.lock();
        savedConf.remove(name);
        QStringList userList = transformTypesList.value(DEFAULT_TYPE_USER, QStringList());
        userList.removeAll(name);
        transformTypesList.insert(DEFAULT_TYPE_USER, userList);
        listLocker.unlock();
        emit transformsUpdated();
        emit savedUpdated();
        return true;
    } else {
        listLocker.unlock();
        logError(tr("\"%1\" not found in the saved chains").arg(name),id);
    }
    return false;
}

void TransformMgmt::setPersistance(const QString &name, bool persistent)
{
    if (name.isEmpty()) {
        logError(tr("Empty name passed to setPersistance, ignoring"),id);
        return;
    }
    listLocker.lock();
    if (savedConf.contains(name)) {
        listLocker.unlock();

        if (persistent) {
            settings->beginGroup(SETTINGS_SAVED_CONF);
            settings->setValue(name,savedConf.value(name));
            settings->endGroup();
        } else {
            settings->beginGroup(SETTINGS_SAVED_CONF);
            settings->remove(name);
            settings->endGroup();
        }
    } else {
        listLocker.unlock();
        logError(tr("\"%1\" not found in the saved chains, ignoring").arg(name),id);
    }
}

TransformAbstract *TransformMgmt::loadComposedTransformFromXML(QXmlStreamReader *streamReader)
{
    TransformAbstract *ta = NULL;
    TransformChain chain = loadConfFromXML(streamReader);

    if (!chain.isEmpty()) {
        ta = new(std::nothrow) ComposedTransform(chain);
        if (ta == NULL) {
            qFatal("Cannot allocate memory for ComposedTransform 2 X{");
        } else {
            saveInstance(ta);
        }
    }

    return ta;
}

TransformAbstract *TransformMgmt::loadComposedTransformFromXML(const QString &conf)
{
    QXmlStreamReader reader(conf);
    return loadComposedTransformFromXML(&reader);
}

TransformChain TransformMgmt::loadConfFromXML(const QString &conf)
{
    QXmlStreamReader reader(conf);
    return loadConfFromXML(&reader);
}

QHash<QString, QString> TransformMgmt::getSavedConfs()
{
    QMutexLocker locker(&listLocker);
    return savedConf;
}

TransformChain TransformMgmt::loadChainFromSaved(const QString &name)
{
    TransformChain tc;
    listLocker.lock();
    if (savedConf.contains(name)) {
        QString conf = savedConf.value(name, QString());
        listLocker.unlock();
        if (!conf.isEmpty()) {
            QXmlStreamReader streamin(conf);
            tc = loadConfFromXML(&streamin);
        }
    } else {
        listLocker.unlock();
        logError(tr("\"%1\" not found in the saved chains, ignoring").arg(name),id);
    }
    return tc;
}

void TransformMgmt::logError(const QString mess, const QString source)
{
    if (!mess.isEmpty())
        emit error(mess,source);
}

void TransformMgmt::logWarning(const QString mess, const QString source)
{
    if (!mess.isEmpty())
        emit warning(mess,source);
}

void TransformMgmt::logStatus(const QString mess, const QString source)
{
    if (!mess.isEmpty())
        emit status(mess,source);
}

inline bool TransformMgmt::isValidAttributeName(QString name)
{
    return TAG_NAME_REGEXP.exactMatch(name);
}

void TransformMgmt::registerPlugin(TransformFactoryPluginInterface *plugin)
{
    pluginsList.insert(plugin->pluginName(),plugin);
    Pip3lineCallback * callback = new(std::nothrow) Pip3lineCallback(this, fileConf, plugin->pluginName());
    if (callback == NULL) {
        qFatal("Cannot allocate memory for Pip3lineCallback X{");
        return;
    }

    connect(callback, SIGNAL(error(QString,QString)), this, SLOT(logError(QString,QString)));
    connect(callback, SIGNAL(warning(QString,QString)), this, SLOT(logWarning(QString,QString)));
    connect(callback, SIGNAL(status(QString,QString)), this, SLOT(logStatus(QString,QString)));
    callbackList.insert(plugin, callback);
    plugin->setCallBack(callback);
    // placed here to avoid locking the gui if some transformed are created dynamically during initialisation
    // and the signal gets emitted
    connect(callback, SIGNAL(newTransform()), this, SLOT(loadTransforms()), Qt::QueuedConnection);
}

QSettings *TransformMgmt::getSettingsObj()
{
    fileConf = getHomeDirectory().append(QDir::separator()).append(CONF_FILE);
    QSettings * settings = new(std::nothrow) QSettings(fileConf,QSettings::IniFormat);
    if (settings == NULL) {
        qFatal("Cannot allocate memory for QSettings (lib) X{");
    }
    return settings;
}



