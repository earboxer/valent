// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mock-session"

#include "config.h"

#include <libvalent-session.h>

#include "valent-mock-session-adapter.h"


struct _ValentMockSessionAdapter
{
  ValentSessionAdapter  parent_instance;

  unsigned int          active : 1;
  unsigned int          locked : 1;
};

G_DEFINE_TYPE (ValentMockSessionAdapter, valent_mock_session_adapter, VALENT_TYPE_SESSION_ADAPTER)


static ValentSessionAdapter *test_instance = NULL;

/*
 * ValentSessionAdapter
 */
static gboolean
valent_mock_session_adapter_get_active (ValentSessionAdapter *adapter)
{
  ValentMockSessionAdapter *self = VALENT_MOCK_SESSION_ADAPTER (adapter);

  return self->active;
}

static gboolean
valent_mock_session_adapter_get_locked (ValentSessionAdapter *adapter)
{
  ValentMockSessionAdapter *self = VALENT_MOCK_SESSION_ADAPTER (adapter);

  return self->locked;
}

static void
valent_mock_session_adapter_set_locked (ValentSessionAdapter *adapter,
                                        gboolean              state)
{
  ValentMockSessionAdapter *self = VALENT_MOCK_SESSION_ADAPTER (adapter);

  if (self->locked == state)
    return;

  self->locked = state;
  g_object_notify (G_OBJECT (self), "locked");
  valent_session_adapter_emit_changed (adapter);
}

/*
 * GObject
 */
static void
valent_mock_session_adapter_class_init (ValentMockSessionAdapterClass *klass)
{
  ValentSessionAdapterClass *session_class = VALENT_SESSION_ADAPTER_CLASS (klass);

  session_class->get_active = valent_mock_session_adapter_get_active;
  session_class->get_locked = valent_mock_session_adapter_get_locked;
  session_class->set_locked = valent_mock_session_adapter_set_locked;
}

static void
valent_mock_session_adapter_init (ValentMockSessionAdapter *self)
{
  if (test_instance == NULL)
    {
      test_instance = VALENT_SESSION_ADAPTER (self);
      g_object_add_weak_pointer (G_OBJECT (test_instance),
                                 (gpointer)&test_instance);
    }
}

/**
 * valent_mock_session_adapter_get_instance:
 *
 * Get the #ValentMockSessionAdapter instance.
 *
 * Returns: (transfer none) (nullable): a #ValentSessionAdapter
 */
ValentSessionAdapter *
valent_mock_session_adapter_get_instance (void)
{
  return test_instance;
}
