// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <libvalent-core.h>
#include <gio/gio.h>
#include <libpeas/peas.h>

G_BEGIN_DECLS

#define VALENT_TYPE_LAN_CHANNEL_SERVICE (valent_lan_channel_service_get_type())

G_DECLARE_FINAL_TYPE (ValentLanChannelService, valent_lan_channel_service, VALENT, LAN_CHANNEL_SERVICE, ValentChannelService)

GTlsCertificate * valent_lan_channel_service_get_certificate (ValentLanChannelService *lan_channel_service);

G_END_DECLS
