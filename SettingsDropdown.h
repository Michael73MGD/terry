#pragma once

#include <WiFiManager.h>
#include "TerryData.h"

// Native form controls work in captive browsers without JavaScript. Values are
// read from the form's POST arguments in the save-parameters callback.
class SettingsDropdown : public WiFiManagerParameter {
public:
  enum class Field { TimeZone, ClockFormat };

  explicit SettingsDropdown(Field field) : WiFiManagerParameter(""), field(field) {}

  void setSelected(const char *value) {
    snprintf(selected, sizeof(selected), "%s", value);
  }

  const char *getCustomHTML() const override {
    const char *name = field == Field::TimeZone ? "time_zone" : "clock_format";
    const char *label = field == Field::TimeZone ? "Time zone" : "Clock format";
    html = "<p><label for='";
    html += name;
    html += "'>";
    html += label;
    html += "</label><select id='";
    html += name;
    html += "' name='";
    html += name;
    html += "' style='width:100%;padding:8px'>";
    if (field == Field::TimeZone) {
      for (const auto &zone : TIME_ZONES) addOption(zone.name, zone.label);
    } else {
      addOption("12", "12-hour (01:00 to 12:59)");
      addOption("24", "24-hour (00:00 to 23:59)");
    }
    html += "</select></p>";
    return html.c_str();
  }

private:
  void addOption(const char *value, const char *label) const {
    html += "<option value='";
    html += value;
    html += "'";
    if (strcmp(selected, value) == 0) html += " selected";
    html += ">";
    html += label;
    html += "</option>";
  }

  Field field;
  char selected[40] = "";
  mutable String html;
};
