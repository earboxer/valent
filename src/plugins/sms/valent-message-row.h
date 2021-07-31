// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gtk/gtk.h>
#include <libvalent-contacts.h>

#include "valent-sms-message.h"

G_BEGIN_DECLS

#define VALENT_TYPE_MESSAGE_ROW (valent_message_row_get_type())

G_DECLARE_FINAL_TYPE (ValentMessageRow, valent_message_row, VALENT, MESSAGE_ROW, GtkListBoxRow)

GtkWidget        * valent_message_row_new           (ValentSmsMessage *message,
                                                     EContact         *contact);
gint64             valent_message_row_get_thread_id (ValentMessageRow *row);
gint64             valent_message_row_get_date      (ValentMessageRow *row);
ValentSmsMessage * valent_message_row_get_message   (ValentMessageRow *row);
void               valent_message_row_set_message   (ValentMessageRow *row,
                                                     ValentSmsMessage *message);
EContact         * valent_message_row_get_contact   (ValentMessageRow *row);
void               valent_message_row_set_contact   (ValentMessageRow *row,
                                                     EContact         *contact);
void               valent_message_row_update        (ValentMessageRow *row);

G_END_DECLS