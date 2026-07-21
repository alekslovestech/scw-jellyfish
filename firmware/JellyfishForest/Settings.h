#pragma once

#include <Preferences.h>
#include "Types.h"

namespace jelly {

class SettingsStore {
 public:
  void load();
  DeviceSettings& data() { return data_; }
  const DeviceSettings& data() const { return data_; }

  void saveIdentity();
  void saveHardware();
  void saveTransform();
  void savePattern();
  void saveShow();
  void saveSensor();
  void saveInteraction();

 private:
  String defaultDeviceName() const;
  Preferences preferences_;
  DeviceSettings data_;
};

}  // namespace jelly
