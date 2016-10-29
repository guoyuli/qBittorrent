/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2016  Vladimir Golovnev <glassez@yandex.ru>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link this program with the OpenSSL project's "OpenSSL" library (or with
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If you
 * modify file(s), you may extend this exception to your version of the file(s),
 * but you are not obligated to do so. If you do not wish to do so, delete this
 * exception statement from your version.
 */

#include "proxyconfigurationmanager.h"
#include "base/settingsstorage.h"

#define SETTINGS_KEY(name) "Network/Proxy/" name
const QString KEY_DISABLED = SETTINGS_KEY("Disabled");
const QString KEY_TYPE = SETTINGS_KEY("Type");
const QString KEY_IP = SETTINGS_KEY("IP");
const QString KEY_PORT = SETTINGS_KEY("Port");
const QString KEY_USERNAME = SETTINGS_KEY("Username");
const QString KEY_PASSWORD = SETTINGS_KEY("Password");

namespace
{
    inline SettingsStorage *settings() { return  SettingsStorage::instance(); }

    inline bool isSameConfig(const Net::ProxyConfiguration &conf1, const Net::ProxyConfiguration &conf2)
    {
        return conf1.type == conf2.type
                && conf1.ip == conf2.ip
                && conf1.port == conf2.port
                && conf1.username == conf2.username
                && conf1.password == conf2.password;
    }
}

using namespace Net;

ProxyConfigurationManager *ProxyConfigurationManager::m_instance = nullptr;

ProxyConfigurationManager::ProxyConfigurationManager(QObject *parent)
    : QObject(parent)
{
    m_proxyDisabled = settings()->loadValue(KEY_DISABLED, false).toBool();
    m_config.type = static_cast<ProxyType>(
                settings()->loadValue(KEY_TYPE, static_cast<int>(ProxyType::None)).toInt());
    if ((m_config.type < ProxyType::None) || (m_config.type > ProxyType::SOCKS4))
        m_config.type = ProxyType::None;
    m_config.ip = settings()->loadValue(KEY_IP, "0.0.0.0").toString();
    m_config.port = static_cast<ushort>(settings()->loadValue(KEY_PORT, 8080).toUInt());
    m_config.username = settings()->loadValue(KEY_USERNAME).toString();
    m_config.password = settings()->loadValue(KEY_PASSWORD).toString();
    configureProxy();
}

void ProxyConfigurationManager::initInstance()
{
    if (!m_instance)
        m_instance = new ProxyConfigurationManager;
}

void ProxyConfigurationManager::freeInstance()
{
    if (m_instance) {
        delete m_instance;
        m_instance = 0;
    }
}

ProxyConfigurationManager *ProxyConfigurationManager::instance()
{
    return m_instance;
}

ProxyConfiguration ProxyConfigurationManager::proxyConfiguration() const
{
    return m_config;
}

void ProxyConfigurationManager::setProxyConfiguration(const ProxyConfiguration &config)
{
    if (!isSameConfig(config, m_config)) {
        m_config = config;
        settings()->storeValue(KEY_TYPE, static_cast<int>(config.type));
        settings()->storeValue(KEY_IP, config.ip);
        settings()->storeValue(KEY_PORT, config.port);
        settings()->storeValue(KEY_USERNAME, config.username);
        settings()->storeValue(KEY_PASSWORD, config.password);
        configureProxy();

        emit proxyConfigurationChanged();
    }
}

bool ProxyConfigurationManager::isProxyDisabled() const
{
    return m_proxyDisabled;
}

void ProxyConfigurationManager::setProxyDisabled(bool disabled)
{
    if (m_proxyDisabled != disabled) {
        settings()->storeValue(KEY_DISABLED, disabled);
        m_proxyDisabled = disabled;
    }
}

bool ProxyConfigurationManager::isAuthenticationRequired() const
{
    return m_config.type == ProxyType::SOCKS5_PW
            || m_config.type == ProxyType::HTTP_PW;
}

void ProxyConfigurationManager::configureProxy()
{
    // Define environment variables for urllib in search engine plugins
    QString proxyStrHTTP, proxyStrSOCK;
    if (!m_proxyDisabled) {
        switch (m_config.type) {
        case ProxyType::HTTP_PW:
            proxyStrHTTP = QString("http://%1:%2@%3:%4").arg(m_config.username)
                    .arg(m_config.password).arg(m_config.ip).arg(m_config.port);
            break;
        case ProxyType::HTTP:
            proxyStrHTTP = QString("http://%1:%2").arg(m_config.ip).arg(m_config.port);
            break;
        case ProxyType::SOCKS5:
            proxyStrSOCK = QString("%1:%2").arg(m_config.ip).arg(m_config.port);
            break;
        case ProxyType::SOCKS5_PW:
            proxyStrSOCK = QString("%1:%2@%3:%4").arg(m_config.username)
                    .arg(m_config.password).arg(m_config.ip).arg(m_config.port);
            break;
        default:
            qDebug("Disabling HTTP communications proxy");
        }

        qDebug("HTTP communications proxy string: %s"
               , qPrintable((m_config.type == ProxyType::SOCKS5) || (m_config.type == ProxyType::SOCKS5_PW)
                            ? proxyStrSOCK : proxyStrHTTP));
    }

    qputenv("http_proxy", proxyStrHTTP.toLocal8Bit());
    qputenv("https_proxy", proxyStrHTTP.toLocal8Bit());
    qputenv("sock_proxy", proxyStrSOCK.toLocal8Bit());
}
