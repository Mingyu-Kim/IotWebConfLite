/**
 * IotWebConf.cpp -- IotWebConf is an ESP8266/ESP32
 *   non blocking WiFi/AP web configuration library for Arduino.
 *   https://github.com/prampec/IotWebConf
 *
 * Copyright (C) 2020 Balazs Kelemen <prampec+arduino@gmail.com>
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <EEPROM.h>

#include "IotWebConf.h"

#ifdef IOTWEBCONF_CONFIG_USE_MDNS
# ifdef ESP8266
#  include <ESP8266mDNS.h>
# elif defined(ESP32)
#  include <ESPmDNS.h>
# endif
#endif


////////////////////////////////////////////////////////////////

namespace iotwebconf
{

IotWebConf::IotWebConf(
    const char* defaultThingName, DNSServer* dnsServer, WebServerWrapper* webServerWrapper,
    const char* initialApPassword, const char* configVersion)
{
  this->_thingNameParameter.defaultValue = defaultThingName;
  this->_dnsServer = dnsServer;
  this->_webServerWrapper = webServerWrapper;
  this->_initialApPassword = initialApPassword;
  this->_configVersion = configVersion;

  this->_apTimeoutParameter.visible = false;
  this->_systemParameters.addItem(&this->_thingNameParameter);
  this->_systemParameters.addItem(&this->_apPasswordParameter);

  this->_allParameters.addItem(&this->_systemParameters);
  this->_allParameters.addItem(&this->_customParameterGroups);
  this->_allParameters.addItem(&this->_hiddenParameters);

}

char* IotWebConf::getThingName()
{
  return this->_thingName;
}


bool IotWebConf::init()
{

  // -- Load configuration from EEPROM.
  bool validConfig = this->loadConfig();
  if (!validConfig)
  {
    // -- No config
    strncpy(this->_apPassword, this->_initialApPassword, sizeof(this->_apPassword));
  }

  // -- Setup mdns
#ifdef ESP8266
  WiFi.hostname(this->_thingName);
#elif defined(ESP32)
  WiFi.setHostname(this->_thingName);
#endif
#ifdef IOTWEBCONF_CONFIG_USE_MDNS
  MDNS.begin(this->_thingName);
  MDNS.addService("http", "tcp", 80);
#endif

  return validConfig;
}

//////////////////////////////////////////////////////////////////

void IotWebConf::addParameterGroup(ParameterGroup* group)
{
  this->_customParameterGroups.addItem(group);
}

void IotWebConf::addHiddenParameter(ConfigItem* parameter)
{
  this->_hiddenParameters.addItem(parameter);
}

void IotWebConf::addSystemParameter(ConfigItem* parameter)
{
  this->_systemParameters.addItem(parameter);
}

int IotWebConf::initConfig()
{
  int size = this->_allParameters.getStorageSize();
#ifdef IOTWEBCONF_DEBUG_TO_SERIAL
  Serial.print("Config version: ");
  Serial.println(this->_configVersion);
  Serial.print("Config size: ");
  Serial.println(size);
#endif

  return size;
}

/**
 * Load the configuration from the eeprom.
 */
bool IotWebConf::loadConfig()
{
  int size = this->initConfig();
  EEPROM.begin(
    IOTWEBCONF_CONFIG_START + IOTWEBCONF_CONFIG_VERSION_LENGTH + size);

  if (this->testConfigVersion())
  {
    int start = IOTWEBCONF_CONFIG_START + IOTWEBCONF_CONFIG_VERSION_LENGTH;
    IOTWEBCONF_DEBUG_LINE(F("Loading configurations"));
    this->_allParameters.loadValue([&](SerializationData* serializationData)
    {
        this->readEepromValue(start, serializationData->data, serializationData->length);
        start += serializationData->length;
    });
#ifdef IOTWEBCONF_DEBUG_TO_SERIAL
    this->_allParameters.debugTo(&Serial);
#endif
    return true;
  }
  else
  {
    IOTWEBCONF_DEBUG_LINE(F("Wrong config version. Applying defaults."));
    this->_allParameters.applyDefaultValue();
#ifdef IOTWEBCONF_DEBUG_TO_SERIAL
    this->_allParameters.debugTo(&Serial);
#endif

    return false;
  }

  EEPROM.end();
}

void IotWebConf::saveConfig()
{
  int size = this->initConfig();
  if (this->_configSavingCallback != NULL)
  {
    this->_configSavingCallback(size);
  }
  EEPROM.begin(
    IOTWEBCONF_CONFIG_START + IOTWEBCONF_CONFIG_VERSION_LENGTH + size);

  this->saveConfigVersion();
  int start = IOTWEBCONF_CONFIG_START + IOTWEBCONF_CONFIG_VERSION_LENGTH;
  IOTWEBCONF_DEBUG_LINE(F("Saving configuration"));
#ifdef IOTWEBCONF_DEBUG_TO_SERIAL
  this->_allParameters.debugTo(&Serial);
  Serial.println();
#endif
  this->_allParameters.storeValue([&](SerializationData* serializationData)
  {
    this->writeEepromValue(start, serializationData->data, serializationData->length);
    start += serializationData->length;
  });

  EEPROM.end();

  this->_apTimeoutMs = atoi(this->_apTimeoutStr) * 1000;

  if (this->_configSavedCallback != NULL)
  {
    this->_configSavedCallback();
  }
}

void IotWebConf::readEepromValue(int start, byte* valueBuffer, int length)
{
  for (int t = 0; t < length; t++)
  {
    *((char*)valueBuffer + t) = EEPROM.read(start + t);
  }
}
void IotWebConf::writeEepromValue(int start, byte* valueBuffer, int length)
{
  for (int t = 0; t < length; t++)
  {
    EEPROM.write(start + t, *((char*)valueBuffer + t));
  }
}

bool IotWebConf::testConfigVersion()
{
  for (byte t = 0; t < IOTWEBCONF_CONFIG_VERSION_LENGTH; t++)
  {
    if (EEPROM.read(IOTWEBCONF_CONFIG_START + t) != this->_configVersion[t])
    {
      return false;
    }
  }
  return true;
}

void IotWebConf::saveConfigVersion()
{
  for (byte t = 0; t < IOTWEBCONF_CONFIG_VERSION_LENGTH; t++)
  {
    EEPROM.write(IOTWEBCONF_CONFIG_START + t, this->_configVersion[t]);
  }
}

void IotWebConf::setWifiConnectionCallback(std::function<void()> func)
{
  this->_wifiConnectionCallback = func;
}

void IotWebConf::setConfigSavingCallback(std::function<void(int size)> func)
{
  this->_configSavingCallback = func;
}

void IotWebConf::setConfigSavedCallback(std::function<void()> func)
{
  this->_configSavedCallback = func;
}

void IotWebConf::setFormValidator(
  std::function<bool(WebRequestWrapper* webRequestWrapper)> func)
{
  this->_formValidator = func;
}


////////////////////////////////////////////////////////////////////////////////

void IotWebConf::handleConfig(WebRequestWrapper* webRequestWrapper)
{
    // -- Authenticate
    if (!webRequestWrapper->authenticate(
            IOTWEBCONF_ADMIN_USER_NAME, this->_apPassword))
    {
      IOTWEBCONF_DEBUG_LINE(F("Requesting authentication."));
      webRequestWrapper->requestAuthentication();
      return;
    }

  bool dataArrived = webRequestWrapper->hasArg("iotSave");
  if (!dataArrived || !this->validateForm(webRequestWrapper))
  {
    // -- Display config portal
    IOTWEBCONF_DEBUG_LINE(F("Configuration page requested."));

    // Send chunked output instead of one String, to avoid
    // filling memory if using many parameters.
    webRequestWrapper->sendHeader(
        "Cache-Control", "no-cache, no-store, must-revalidate");
    webRequestWrapper->sendHeader("Pragma", "no-cache");
    webRequestWrapper->sendHeader("Expires", "-1");
    webRequestWrapper->setContentLength(CONTENT_LENGTH_UNKNOWN);
    webRequestWrapper->send(200, "text/html; charset=UTF-8", "");

    String content = htmlFormatProvider->getHead();
    content.replace("{v}", "Config ESP");
    content += htmlFormatProvider->getScript();
    content += htmlFormatProvider->getStyle();
    content += htmlFormatProvider->getHeadExtension();
    content += htmlFormatProvider->getHeadEnd();

    content += htmlFormatProvider->getFormStart();

    webRequestWrapper->sendContent(content);

#ifdef IOTWEBCONF_DEBUG_TO_SERIAL
    Serial.println("Rendering parameters:");
    this->_systemParameters.debugTo(&Serial);
    this->_customParameterGroups.debugTo(&Serial);
#endif
    // -- Add parameters to the form
    this->_systemParameters.renderHtml(dataArrived, webRequestWrapper);
    this->_customParameterGroups.renderHtml(dataArrived, webRequestWrapper);

    content = htmlFormatProvider->getFormEnd();

    if (this->_updatePath != NULL)
    {
      String pitem = htmlFormatProvider->getUpdate();
      pitem.replace("{u}", this->_updatePath);
      content += pitem;
    }

    // -- Fill config version string;
    {
      String pitem = htmlFormatProvider->getConfigVer();
      pitem.replace("{v}", this->_configVersion);
      content += pitem;
    }

    content += htmlFormatProvider->getEnd();

    webRequestWrapper->sendContent(content);
    webRequestWrapper->sendContent(F(""));
    webRequestWrapper->stop();
  }
  else
  {
    // -- Save config
    IOTWEBCONF_DEBUG_LINE(F("Updating configuration"));
#ifdef IOTWEBCONF_DEBUG_TO_SERIAL
    this->_systemParameters.debugTo(&Serial);
    this->_customParameterGroups.debugTo(&Serial);
    Serial.println();
#endif
    this->_systemParameters.update(webRequestWrapper);
    this->_customParameterGroups.update(webRequestWrapper);

    this->saveConfig();

    String page = htmlFormatProvider->getHead();
    page.replace("{v}", "Config ESP");
    page += htmlFormatProvider->getScript();
    page += htmlFormatProvider->getStyle();
//    page += _customHeadElement;
    page += htmlFormatProvider->getHeadExtension();
    page += htmlFormatProvider->getHeadEnd();
    page += "Configuration saved. ";
    page += F("Return to <a href='/'>home page</a>.");
    page += htmlFormatProvider->getEnd();

    webRequestWrapper->sendHeader("Content-Length", String(page.length()));
    webRequestWrapper->send(200, "text/html; charset=UTF-8", page);
  }
}

bool IotWebConf::validateForm(WebRequestWrapper* webRequestWrapper)
{
  // -- Clean previous error messages.
  this->_systemParameters.clearErrorMessage();
  this->_customParameterGroups.clearErrorMessage();

  // -- Call external validator.
  bool valid = true;
  if (this->_formValidator != NULL)
  {
    valid = this->_formValidator(webRequestWrapper);
  }

  // -- Internal validation.
  int l = webRequestWrapper->arg(this->_thingNameParameter.getId()).length();
  if (3 > l)
  {
    this->_thingNameParameter.errorMessage =
        "Give a name with at least 3 characters.";
    valid = false;
  }
  l = webRequestWrapper->arg(this->_apPasswordParameter.getId()).length();
  if ((0 < l) && (l < 8))
  {
    this->_apPasswordParameter.errorMessage =
        "Password length must be at least 8 characters.";
    valid = false;
  }

#ifdef IOTWEBCONF_DEBUG_TO_SERIAL
  Serial.print(F("Form validation result is: "));
  Serial.println(valid ? "positive" : "negative");
#endif

  return valid;
}

void IotWebConf::handleNotFound(WebRequestWrapper* webRequestWrapper)
{
  if (this->handleCaptivePortal(webRequestWrapper))
  {
    // If captive portal redirect instead of displaying the error page.
    return;
  }
#ifdef IOTWEBCONF_DEBUG_TO_SERIAL
  Serial.print(F("Requested a non-existing page '"));
  Serial.print(webRequestWrapper->uri());
  Serial.println("'");
#endif
  String message = "Requested a non-existing page\n\n";
  message += "URI: ";
  message += webRequestWrapper->uri();
  message += "\n";

  webRequestWrapper->sendHeader(
      "Cache-Control", "no-cache, no-store, must-revalidate");
  webRequestWrapper->sendHeader("Pragma", "no-cache");
  webRequestWrapper->sendHeader("Expires", "-1");
  webRequestWrapper->sendHeader("Content-Length", String(message.length()));
  webRequestWrapper->send(404, "text/plain", message);
}

/**
 * Redirect to captive portal if we got a request for another domain.
 * Return true in that case so the page handler do not try to handle the request
 * again. (Code from WifiManager project.)
 */
bool IotWebConf::handleCaptivePortal(WebRequestWrapper* webRequestWrapper)
{
  String host = webRequestWrapper->hostHeader();
  String thingName = String(this->_thingName);
  thingName.toLowerCase();
  if (!isIp(host) && !host.startsWith(thingName))
  {
#ifdef IOTWEBCONF_DEBUG_TO_SERIAL
    Serial.print("Request for ");
    Serial.print(host);
    Serial.print(" redirected to ");
    Serial.println(webRequestWrapper->localIP());
#endif
    webRequestWrapper->sendHeader(
      "Location", String("http://") + toStringIp(webRequestWrapper->localIP()), true);
    webRequestWrapper->send(302, "text/plain", ""); // Empty content inhibits Content-length header so we have to close the socket ourselves.
    webRequestWrapper->stop(); // Stop is needed because we sent no content length
    return true;
  }
  return false;
}

/** Is this an IP? */
bool IotWebConf::isIp(String str)
{
  for (size_t i = 0; i < str.length(); i++)
  {
    int c = str.charAt(i);
    if (c != '.' && (c < '0' || c > '9'))
    {
      return false;
    }
  }
  return true;
}

/** IP to String? */
String IotWebConf::toStringIp(IPAddress ip)
{
  String res = "";
  for (int i = 0; i < 3; i++)
  {
    res += String((ip >> (8 * i)) & 0xFF) + ".";
  }
  res += String(((ip >> 8 * 3)) & 0xFF);
  return res;
}

/////////////////////////////////////////////////////////////////////////////////

void IotWebConf::delay(unsigned long m)
{
  unsigned long delayStart = millis();
  while (m > millis() - delayStart)
  {
    this->doLoop();
    // -- Note: 1ms might not be enough to perform a full yield. So
    // 'yield' in 'doLoop' is eventually a good idea.
    delayMicroseconds(1000);
  }
}

void IotWebConf::doLoop()
{
  doBlink();
  yield(); // -- Yield should not be necessary, but cannot hurt either.
  this->_dnsServer->processNextRequest();
  this->_webServerWrapper->handleClient();
}

} // end namespace