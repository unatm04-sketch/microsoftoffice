#pragma once

#include <QString>

#ifndef OMNI_VERSION_STRING
#define OMNI_VERSION_STRING "1.0.0"
#endif

namespace AppInfo {

inline QString organizationName() { return QStringLiteral("OmniOffice"); }
inline QString applicationName() { return QStringLiteral("OmniOffice"); }
inline QString displayName() { return QStringLiteral("OmniOffice"); }
inline QString version() { return QStringLiteral(OMNI_VERSION_STRING); }

} // namespace AppInfo
