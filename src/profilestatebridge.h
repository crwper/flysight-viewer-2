#ifndef PROFILESTATEBRIDGE_H
#define PROFILESTATEBRIDGE_H

#include "profile.h"

namespace FlySight {

class MainWindow;

/**
 * Captures the current application state into a Profile struct.
 * The id and displayName fields are left empty; the caller fills those in.
 */
Profile captureCurrentState(MainWindow *mainWindow);

/**
 * Applies a Profile to the application, restoring each captured state dimension.
 * Only fields where has_value() is true are applied.
 */
void applyProfile(const Profile &profile, MainWindow *mainWindow);

} // namespace FlySight

#endif // PROFILESTATEBRIDGE_H
