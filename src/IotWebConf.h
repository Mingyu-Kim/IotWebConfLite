/**
 * IotWebConf.h -- IotWebConf is an ESP8266/ESP32
 *   non blocking WiFi/AP web configuration library for Arduino.
 *   https://github.com/prampec/IotWebConf
 *
 * Copyright (C) 2020 Balazs Kelemen <prampec+arduino@gmail.com>
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef IotWebConf_h
#define IotWebConf_h

#include <Arduino.h>
#include <IotWebConfParameter.h>
#include <IotWebConfSettings.h>
#include <IotWebConfWebServerWrapper.h>

#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#elif defined(ESP32)
#include <WiFi.h>
#include <WebServer.h>
#endif
#include <DNSServer.h> // -- For captive portal

#ifdef ESP8266
#ifndef WebServer
#define WebServer ESP8266WebServer
#endif
#endif

// -- HTML page fragments
const char IOTWEBCONF_HTML_HEAD[] PROGMEM =
    "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" "
    "content=\"width=device-width, initial-scale=1, "
    "user-scalable=no\"/><title>{v}</title>\n";
const char IOTWEBCONF_HTML_STYLE_INNER[] PROGMEM =
    ".de{background-color:#ffaaaa;} "
    ".em{font-size:0.8em;color:#bb0000;padding-bottom:0px;} .c{text-align: "
    "center;} div,input,select{padding:5px;font-size:1em;} input{width:95%;} "
    "select{width:100%} "
    "input[type=checkbox]{width:auto;scale:1.5;margin:10px;} body{text-align: "
    "center;font-family:verdana;} "
    "button{border:0;border-radius:0.3rem;background-color:#16A1E7;color:#fff;"
    "line-height:2.4rem;font-size:1.2rem;width:100%;} "
    "fieldset{border-radius:0.3rem;margin: 0px;}\n";
const char IOTWEBCONF_HTML_SCRIPT_INNER[] PROGMEM =
    "function "
    "c(l){document.getElementById('s').value=l.innerText||l.textContent;"
    "document.getElementById('p').focus();}; function pw(id) { var "
    "x=document.getElementById(id); if(x.type==='password') {x.type='text';} "
    "else {x.type='password';} };";
const char IOTWEBCONF_HTML_HEAD_END[] PROGMEM = "</head><body>";
const char IOTWEBCONF_HTML_BODY_INNER[] PROGMEM =
    "<div style='text-align:left;display:inline-block;min-width:260px;'>\n";
const char IOTWEBCONF_HTML_FORM_START[] PROGMEM =
    "<form action='' method='post'><input type='hidden' name='iotSave' "
    "value='true'>\n";
const char IOTWEBCONF_HTML_FORM_END[] PROGMEM =
    "<button type='submit' style='margin-top: 10px;'>Apply</button></form>\n";
const char IOTWEBCONF_HTML_SAVED[] PROGMEM =
    "<div>Configuration saved<br />Return to <a href='/'>home "
    "page</a>.</div>\n";
const char IOTWEBCONF_HTML_END[] PROGMEM = "</div></body></html>";
const char IOTWEBCONF_HTML_UPDATE[] PROGMEM =
    "<div style='padding-top:25px;'><a href='{u}'>Firmware update</a></div>\n";
const char IOTWEBCONF_HTML_CONFIG_VER[] PROGMEM =
    "<div style='font-size: .6em;'>Firmware config version '{v}'</div>\n";


// -- User name on login.
#define IOTWEBCONF_ADMIN_USER_NAME "admin"

namespace iotwebconf
{

class IotWebConf;

typedef struct WifiAuthInfo
{
  const char* ssid;
  const char* password;
} WifiAuthInfo;

/**
 * Class for providing HTML format segments.
 */
class HtmlFormatProvider
{
public:
  virtual String getHead() { return FPSTR(IOTWEBCONF_HTML_HEAD); }
  virtual String getStyle() { return "<style>" + getStyleInner() + "</style>"; }
  virtual String getScript()
  {
    return "<script>" + getScriptInner() + "</script>";
  }
  virtual String getHeadExtension() { return ""; }
  virtual String getHeadEnd()
  {
    return String(FPSTR(IOTWEBCONF_HTML_HEAD_END)) + getBodyInner();
  }
  virtual String getFormStart() { return FPSTR(IOTWEBCONF_HTML_FORM_START); }
  virtual String getFormEnd() { return FPSTR(IOTWEBCONF_HTML_FORM_END); }
  virtual String getFormSaved() { return FPSTR(IOTWEBCONF_HTML_SAVED); }
  virtual String getEnd() { return FPSTR(IOTWEBCONF_HTML_END); }
  virtual String getUpdate() { return FPSTR(IOTWEBCONF_HTML_UPDATE); }
  virtual String getConfigVer() { return FPSTR(IOTWEBCONF_HTML_CONFIG_VER); }

protected:
  virtual String getStyleInner() { return FPSTR(IOTWEBCONF_HTML_STYLE_INNER); }
  virtual String getScriptInner()
  {
    return FPSTR(IOTWEBCONF_HTML_SCRIPT_INNER);
  }
  virtual String getBodyInner() { return FPSTR(IOTWEBCONF_HTML_BODY_INNER); }
};

class StandardWebRequestWrapper : public WebRequestWrapper
{
public:
  StandardWebRequestWrapper(WebServer* server) { this->_server = server; };

  const String hostHeader() const override
  {
    return this->_server->hostHeader();
  };
  IPAddress localIP() override { return this->_server->client().localIP(); };
  const String uri() const { return this->_server->uri(); };
  bool authenticate(const char* username, const char* password) override
  {
    return this->_server->authenticate(username, password);
  };
  void requestAuthentication() override
  {
    this->_server->requestAuthentication();
  };
  bool hasArg(const String& name) override
  {
    return this->_server->hasArg(name);
  };
  String arg(const String name) override { return this->_server->arg(name); };
  void sendHeader(
      const String& name, const String& value, bool first = false) override
  {
    this->_server->sendHeader(name, value, first);
  };
  void setContentLength(const size_t contentLength) override
  {
    this->_server->setContentLength(contentLength);
  };
  void send(
      int code, const char* content_type = NULL,
      const String& content = String("")) override
  {
    this->_server->send(code, content_type, content);
  };
  void sendContent(const String& content) override
  {
    this->_server->sendContent(content);
  };
  void stop() override { this->_server->client().stop(); };

private:
  WebServer* _server;
  friend IotWebConf;
};

class StandardWebServerWrapper : public WebServerWrapper
{
public:
  StandardWebServerWrapper(WebServer* server) { this->_server = server; };

  void handleClient() override { this->_server->handleClient(); };
  void begin() override { this->_server->begin(); };

private:
  StandardWebServerWrapper(){};
  WebServer* _server;
  friend IotWebConf;
};


/**
 * Main class of the module.
 */
class IotWebConf
{
public:
  /**
   * Create a new configuration handler.
   *   @thingName - Initial value for the thing name. Used in many places like
   * AP name, can be changed by the user.
   *   @dnsServer - A created DNSServer, that can be configured for captive
   * portal.
   *   @server - A created web server. Will be started upon connection success.
   *   @initialApPassword - Initial value for AP mode. Can be changed by the
   * user.
   *   @configVersion - When the software is updated and the configuration is
   * changing, this key should also be changed, so that the config portal will
   * force the user to reenter all the configuration values.
   */
  IotWebConf(
      const char* thingName, DNSServer* dnsServer, WebServer* server,
      const char* initialApPassword, const char* configVersion = "init")
      : IotWebConf(
            thingName, dnsServer, &this->_standardWebServerWrapper,
            initialApPassword, configVersion)
  {
    this->_standardWebServerWrapper._server = server;
  }

  IotWebConf(
      const char* thingName, DNSServer* dnsServer, WebServerWrapper* server,
      const char* initialApPassword, const char* configVersion = "init");



  /**
   * Start up the IotWebConf module.
   * Loads all configuration from the EEPROM, and initialize the system.
   * Will return false, if no configuration (with specified config version) was
   * found in the EEPROM.
   */
  bool init();

  /**
   * IotWebConf is a non-blocking, state controlled system. Therefor it should
   * be regularly triggered from the user code. So call this method any time you
   * can.
   */
  void doLoop();

  /**
   * Each WebServer URL handler method should start with calling this method.
   * If this method return true, the request was already served by it.
   */
  bool handleCaptivePortal(WebRequestWrapper* webRequestWrapper);
  bool handleCaptivePortal()
  {
    StandardWebRequestWrapper webRequestWrapper =
        StandardWebRequestWrapper(this->_standardWebServerWrapper._server);
    return handleCaptivePortal(&webRequestWrapper);
  }

  /**
   * Config URL web request handler. Call this method to handle config request.
   */
  void handleConfig(WebRequestWrapper* webRequestWrapper);
  void handleConfig()
  {
    StandardWebRequestWrapper webRequestWrapper =
        StandardWebRequestWrapper(this->_standardWebServerWrapper._server);
    handleConfig(&webRequestWrapper);
  }

  /**
   * URL-not-found web request handler. Used for handling captive portal
   * request.
   */
  void handleNotFound(WebRequestWrapper* webRequestWrapper);
  void handleNotFound()
  {
    StandardWebRequestWrapper webRequestWrapper =
        StandardWebRequestWrapper(this->_standardWebServerWrapper._server);
    handleNotFound(&webRequestWrapper);
  }


  /**
   * Specify a callback method, that will be called when settings is being
   * changed. This is very handy if you have other routines, that are modifying
   * the "EEPROM" parallel to IotWebConf, now this is the time to disable these
   * routines. Should be called before init()!
   */
  void setConfigSavingCallback(std::function<void(int size)> func);

  /**
   * Specify a callback method, that will be called when settings have been
   * changed. All pending EEPROM manipulations are done by the time this method
   * is called. Should be called before init()!
   */
  void setConfigSavedCallback(std::function<void()> func);

  /**
   * Specify a callback method, that will be called when form validation is
   * required. If the method will return false, the configuration will not be
   * saved. Should be called before init()!
   */
  void setFormValidator(
      std::function<bool(WebRequestWrapper* webRequestWrapper)> func);


  /**
   * Add a custom parameter group, that will be handled by the IotWebConf
   * module. The parameters in this group will be saved to/loaded from EEPROM
   * automatically, and will appear on the config portal. Must be called before
   * init()!
   */
  void addParameterGroup(ParameterGroup* group);

  /**
   * Add a custom parameter group, that will be handled by the IotWebConf
   * module. The parameters in this group will be saved to/loaded from EEPROM
   * automatically, but will NOT appear on the config portal. Must be called
   * before init()!
   */
  void addHiddenParameter(ConfigItem* parameter);

  /**
   * Add a custom parameter group, that will be handled by the IotWebConf
   * module. The parameters in this group will be saved to/loaded from EEPROM
   * automatically, but will NOT appear on the config portal. Must be called
   * before init()!
   */
  void addSystemParameter(ConfigItem* parameter);

  /**
   * Getter for the actually configured thing name.
   */
  char* getThingName();

  /**
   * Use this delay, to prevent blocking IotWebConf.
   */
  void delay(unsigned long millis);


  /**
   * Get internal parameters, for manual handling.
   * Normally you don't need to access these parameters directly.
   * Note, that changing valueBuffer of these parameters should be followed by
   * saveConfig()!
   */
  ParameterGroup* getSystemParameterGroup()
  {
    return &this->_systemParameters;
  };
  Parameter* getThingNameParameter() { return &this->_thingNameParameter; };
  Parameter* getApPasswordParameter() { return &this->_apPasswordParameter; };

  /**
   * If config parameters are modified directly, the new values can be saved by
   * this method. Note, that init() must pretend saveConfig()! Also note, that
   * saveConfig writes to EEPROM, and EEPROM can be written only some thousand
   * times in the lifetime of an ESP8266 module.
   */
  void saveConfig();

  /**
   * Loads all configuration from the EEPROM without initializing the system.
   * Will return false, if no configuration (with specified config version) was
   * found in the EEPROM.
   */
  bool loadConfig();

  /**
   * With this method you can override the default HTML format provider to
   * provide custom HTML segments.
   */
  void setHtmlFormatProvider(HtmlFormatProvider* customHtmlFormatProvider)
  {
    this->htmlFormatProvider = customHtmlFormatProvider;
  }
  HtmlFormatProvider* getHtmlFormatProvider()
  {
    return this->htmlFormatProvider;
  }
  bool isIp(String str);
  String toStringIp(IPAddress ip);

private:
  const char* _initialApPassword = NULL;
  const char* _configVersion;
  DNSServer* _dnsServer;
  WebServerWrapper* _webServerWrapper;
  StandardWebServerWrapper _standardWebServerWrapper =
      StandardWebServerWrapper();
  std::function<void(const char* _updatePath)> _updateServerSetupFunction =
      NULL;
  std::function<void(const char* userName, char* password)>
      _updateServerUpdateCredentialsFunction = NULL;
  ParameterGroup _allParameters = ParameterGroup("iwcAll");
  ParameterGroup _systemParameters =
      ParameterGroup("iwcSys", "System configuration");
  ParameterGroup _customParameterGroups = ParameterGroup("iwcCustom");
  ParameterGroup _hiddenParameters = ParameterGroup("hidden");
  TextParameter _thingNameParameter = TextParameter(
      "Thing name", "iwcThingName", this->_thingName, IOTWEBCONF_WORD_LEN);
  PasswordParameter _apPasswordParameter = PasswordParameter(
      "AP password", "iwcApPassword", this->_apPassword,
      IOTWEBCONF_PASSWORD_LEN);
  char _thingName[IOTWEBCONF_WORD_LEN];
  char _apPassword[IOTWEBCONF_PASSWORD_LEN];
  std::function<void(int)> _configSavingCallback = NULL;
  std::function<void()> _configSavedCallback = NULL;
  std::function<bool(WebRequestWrapper* webRequestWrapper)> _formValidator = NULL;
  HtmlFormatProvider htmlFormatProviderInstance;
  HtmlFormatProvider* htmlFormatProvider = &htmlFormatProviderInstance;

  int initConfig();
  bool testConfigVersion();
  void saveConfigVersion();
  void readEepromValue(int start, byte* valueBuffer, int length);
  void writeEepromValue(int start, byte* valueBuffer, int length);

  bool validateForm(WebRequestWrapper* webRequestWrapper);
};

} // namespace iotwebconf

using iotwebconf::IotWebConf;

#endif
