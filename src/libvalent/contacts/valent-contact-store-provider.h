// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_CONTACTS_INSIDE) && !defined (VALENT_CONTACTS_COMPILATION)
# error "Only <libvalent-contacts.h> can be included directly."
#endif

#include "valent-contact-store.h"

#include <gio/gio.h>

G_BEGIN_DECLS

#define VALENT_TYPE_CONTACT_STORE_PROVIDER (valent_contact_store_provider_get_type())

G_DECLARE_DERIVABLE_TYPE (ValentContactStoreProvider, valent_contact_store_provider, VALENT, CONTACT_STORE_PROVIDER, GObject)

struct _ValentContactStoreProviderClass
{
  GObjectClass         parent_class;

  /* virtual functions */
  void                 (*load_async)    (ValentContactStoreProvider  *provider,
                                         GCancellable                *cancellable,
                                         GAsyncReadyCallback          callback,
                                         gpointer                     user_data);
  gboolean             (*load_finish)   (ValentContactStoreProvider  *provider,
                                         GAsyncResult                *result,
                                         GError                     **error);

  /* signals */
  void                 (*store_added)   (ValentContactStoreProvider  *provider,
                                         ValentContactStore          *store);
  void                 (*store_removed) (ValentContactStoreProvider  *provider,
                                         ValentContactStore          *store);
};

void                 valent_contact_store_provider_emit_store_added   (ValentContactStoreProvider  *provider,
                                                                       ValentContactStore          *store);
void                 valent_contact_store_provider_emit_store_removed (ValentContactStoreProvider  *provider,
                                                                       ValentContactStore          *store);
GPtrArray          * valent_contact_store_provider_get_stores         (ValentContactStoreProvider  *provider);
void                 valent_contact_store_provider_load_async         (ValentContactStoreProvider  *provider,
                                                                       GCancellable                *cancellable,
                                                                       GAsyncReadyCallback          callback,
                                                                       gpointer                     user_data);
gboolean             valent_contact_store_provider_load_finish        (ValentContactStoreProvider  *provider,
                                                                       GAsyncResult                *result,
                                                                       GError                     **error);

G_END_DECLS

