/**
Released as open source by NCC Group Plc - http://www.nccgroup.com/

Developed by Gabriel Caudrelier, gabriel dot caudrelier at nccgroup dot com

https://github.com/nccgroup/pip3line

Released under AGPL see LICENSE for more information
**/

#ifndef RUBYPLUGIN_GLOBAL_H
#define RUBYPLUGIN_GLOBAL_H

#include <QtCore/qglobal.h>

#if defined(RUBYPLUGIN_LIBRARY)
#  define RUBYPLUGINSHARED_EXPORT Q_DECL_EXPORT
#else
#  define RUBYPLUGINSHARED_EXPORT Q_DECL_IMPORT
#endif

#endif // RUBYPLUGIN_GLOBAL_H
