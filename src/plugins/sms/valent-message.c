// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-message"

#include "config.h"

#include <gio/gio.h>

#include "valent-message.h"


struct _ValentMessage
{
  GObject           parent_instance;

  ValentMessageBox  box;
  gint64            date;
  gint64            id;
  GVariant         *metadata;
  unsigned int      read : 1;
  char             *sender;
  char             *text;
  gint64            thread_id;
};

G_DEFINE_TYPE (ValentMessage, valent_message, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_BOX,
  PROP_DATE,
  PROP_ID,
  PROP_METADATA,
  PROP_READ,
  PROP_SENDER,
  PROP_TEXT,
  PROP_THREAD_ID,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


/*
 * GObject
 */
static void
valent_message_finalize (GObject *object)
{
  ValentMessage *self = VALENT_MESSAGE (object);

  g_clear_pointer (&self->sender, g_free);
  g_clear_pointer (&self->text, g_free);
  g_clear_pointer (&self->metadata, g_variant_unref);

  G_OBJECT_CLASS (valent_message_parent_class)->finalize (object);
}

static void
valent_message_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  ValentMessage *self = VALENT_MESSAGE (object);

  switch (prop_id)
    {
    case PROP_BOX:
      g_value_set_uint (value, valent_message_get_box (self));
      break;

    case PROP_DATE:
      g_value_set_int64 (value, valent_message_get_date (self));
      break;

    case PROP_ID:
      g_value_set_int64 (value, valent_message_get_id (self));
      break;

    case PROP_METADATA:
      g_value_set_variant (value, valent_message_get_metadata (self));
      break;

    case PROP_READ:
      g_value_set_boolean (value, valent_message_get_read (self));
      break;

    case PROP_SENDER:
      g_value_set_string (value, valent_message_get_sender (self));
      break;

    case PROP_TEXT:
      g_value_set_string (value, valent_message_get_text (self));
      break;

    case PROP_THREAD_ID:
      g_value_set_int64 (value, valent_message_get_thread_id (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_message_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  ValentMessage *self = VALENT_MESSAGE (object);

  switch (prop_id)
    {
    case PROP_BOX:
      self->box = g_value_get_uint (value);
      break;

    case PROP_DATE:
      valent_message_set_date (self, g_value_get_int64 (value));
      break;

    case PROP_ID:
      valent_message_set_id (self, g_value_get_int64 (value));
      break;

    case PROP_METADATA:
      valent_message_set_metadata (self, g_value_get_variant (value));
      break;

    case PROP_READ:
      valent_message_set_read (self, g_value_get_boolean (value));
      break;

    case PROP_SENDER:
      valent_message_set_sender (self, g_value_get_string (value));
      break;

    case PROP_TEXT:
      valent_message_set_text (self, g_value_get_string (value));
      break;

    case PROP_THREAD_ID:
      valent_message_set_thread_id (self, g_value_get_int64 (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_message_class_init (ValentMessageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = valent_message_finalize;
  object_class->get_property = valent_message_get_property;
  object_class->set_property = valent_message_set_property;

  /**
   * ValentMessage:box:
   *
   * The #ValentMessageBox of the message.
   */
  properties [PROP_BOX] =
    g_param_spec_uint ("box",
                       "Category",
                        "The ValentMessageBox of the message",
                        VALENT_MESSAGE_BOX_ALL, VALENT_MESSAGE_BOX_FAILED,
                        VALENT_MESSAGE_BOX_ALL,
                        (G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  /**
   * ValentMessage:date:
   *
   * A UNIX epoch timestamp for the message.
   */
  properties [PROP_DATE] =
    g_param_spec_int64 ("date",
                        "Date",
                        "Integer indicating the date",
                        G_MININT64, G_MAXINT64,
                        0,
                        (G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  /**
   * ValentMessage:id:
   *
   * The unique ID for this message.
   */
  properties [PROP_ID] =
    g_param_spec_int64 ("id",
                        "ID",
                        "Unique ID for this message",
                        G_MININT64, G_MAXINT64,
                        0,
                        (G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  /**
   * ValentMessage:metadata:
   *
   * Ancillary data for the message, such as media.
   */
  properties [PROP_METADATA] =
    g_param_spec_variant ("metadata",
                          "Metadata",
                          "Ancillary data for the message",
                          G_VARIANT_TYPE_VARDICT,
                          NULL,
                          (G_PARAM_READWRITE |
                           G_PARAM_CONSTRUCT_ONLY |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  /**
   * ValentMessage:read:
   *
   * Whether the message has been read.
   */
  properties [PROP_READ] =
    g_param_spec_boolean ("read",
                          "Read",
                          "Whether the message has been read",
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  /**
   * ValentMessage:sender:
   *
   * The sender of the message. This will usually be a phone number or other
   * address form.
   */
  properties [PROP_SENDER] =
    g_param_spec_string ("sender",
                         "Sender",
                         "The sender of the message",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentMessage:text:
   *
   * The text content of the message.
   */
  properties [PROP_TEXT] =
    g_param_spec_string ("text",
                         "Text",
                         "The text content of the message",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentMessage:thread-id:
   *
   * The thread this message belongs to.
   */
  properties [PROP_THREAD_ID] =
    g_param_spec_int64 ("thread-id",
                        "Thread ID",
                        "The thread this message belongs to",
                        G_MININT64, G_MAXINT64,
                        0,
                        (G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
valent_message_init (ValentMessage *message)
{
}

/**
 * valent_message_get_box:
 * @message: a #ValentMessage
 *
 * Get the #ValentMessageBox of @message.
 *
 * Returns: a #ValentMessageBox
 */
ValentMessageBox
valent_message_get_box (ValentMessage *message)
{
  g_return_val_if_fail (VALENT_IS_MESSAGE (message), VALENT_MESSAGE_BOX_ALL);

  return message->box;
}

/**
 * valent_message_get_date:
 * @message: a #ValentMessage
 *
 * Get the timestamp for @message.
 *
 * Returns: the message timestamp
 */
gint64
valent_message_get_date (ValentMessage *message)
{
  g_return_val_if_fail (VALENT_IS_MESSAGE (message), 0);

  return message->date;
}

/**
 * valent_message_set_date:
 * @message: a #ValentMessage
 * @date: a UNIX Epoc timestamp (ms)
 *
 * Set the timestamp of @message to @date.
 */
void
valent_message_set_date (ValentMessage *message,
                         gint64         date)
{
  g_return_if_fail (VALENT_IS_MESSAGE (message));

  if (message->date == date)
    return;

  message->date = date;
  g_object_notify_by_pspec (G_OBJECT (message), properties [PROP_DATE]);
}

/**
 * valent_message_get_id:
 * @message: a #ValentMessage
 *
 * Get the unique ID for @message.
 *
 * Returns: the message ID
 */
gint64
valent_message_get_id (ValentMessage *message)
{
  g_return_val_if_fail (VALENT_IS_MESSAGE (message), 0);

  return message->id;
}

/**
 * valent_message_set_id:
 * @message: a #ValentMessage
 * @id: a unique ID
 *
 * Set the ID of @message to @id.
 */
void
valent_message_set_id (ValentMessage *message,
                       gint64         id)
{
  g_return_if_fail (VALENT_IS_MESSAGE (message));

  if (message->id == id)
    return;

  message->id = id;
  g_object_notify_by_pspec (G_OBJECT (message), properties [PROP_ID]);
}

/**
 * valent_message_get_metadata:
 * @message: a #ValentMessage
 *
 * Get the #GVariant dictionary of metadata.
 *
 * Returns: (transfer none) (nullable): the metadata
 */
GVariant *
valent_message_get_metadata (ValentMessage *message)
{
  g_return_val_if_fail (VALENT_IS_MESSAGE (message), NULL);

  return message->metadata;
}

/**
 * valent_message_set_metadata:
 * @message: a #ValentMessage
 * @metadata: a #GVariant
 *
 * Set the metadata of @message to @metadata.
 *
 * If @metadata is a floating reference (see g_variant_ref_sink()), @message
 * takes ownership of @metadata. @metadata should be a dictionary (ie. `a{sv}`).
 */
void
valent_message_set_metadata (ValentMessage *message,
                             GVariant      *metadata)
{
  g_return_if_fail (VALENT_IS_MESSAGE (message));

  if (message->metadata == NULL && metadata == NULL)
    return;

  g_clear_pointer (&message->metadata, g_variant_unref);

  if (message != NULL)
    message->metadata = g_variant_ref_sink (metadata);

  g_object_notify_by_pspec (G_OBJECT (message), properties [PROP_METADATA]);
}

/**
 * valent_message_get_read:
 * @message: a #ValentMessage
 *
 * Get the read status of @message.
 *
 * Returns: %TRUE if the message has been read
 */
gboolean
valent_message_get_read (ValentMessage *message)
{
  g_return_val_if_fail (VALENT_IS_MESSAGE (message), FALSE);

  return message->read;
}

/**
 * valent_message_set_read:
 * @message: a #ValentMessage
 * @read: whether the message is read
 *
 * Set the read status of @message to @read.
 */
void
valent_message_set_read (ValentMessage *message,
                         gboolean       read)
{
  g_return_if_fail (VALENT_IS_MESSAGE (message));

  if (message->read == read)
    return;

  message->read = read;
  g_object_notify_by_pspec (G_OBJECT (message), properties [PROP_READ]);
}

/**
 * valent_message_get_sender:
 * @message: a #ValentMessage
 *
 * Get the sender of @message.
 *
 * Returns: (transfer none) (nullable): the message sender
 */
const char *
valent_message_get_sender (ValentMessage *message)
{
  g_return_val_if_fail (VALENT_IS_MESSAGE (message), NULL);

  return message->sender;
}

/**
 * valent_message_set_sender:
 * @message: a #ValentMessage
 * @sender: a phone number or other address
 *
 * Set the sender of @message.
 */
void
valent_message_set_sender (ValentMessage *message,
                           const char    *sender)
{
  g_return_if_fail (VALENT_IS_MESSAGE (message));

  if (g_strcmp0 (message->sender, sender) == 0)
    return;

  g_clear_pointer (&message->sender, g_free);
  message->sender = g_strdup (sender);
  g_object_notify_by_pspec (G_OBJECT (message), properties [PROP_SENDER]);
}

/**
 * valent_message_get_text:
 * @message: a #ValentMessage
 *
 * Get the text content of @message.
 *
 * Returns: (transfer none) (nullable): the message text
 */
const char *
valent_message_get_text (ValentMessage *message)
{
  g_return_val_if_fail (VALENT_IS_MESSAGE (message), NULL);

  return message->text;
}

/**
 * valent_message_set_text:
 * @message: a #ValentMessage
 * @text: a phone number or other address
 *
 * Set the sender of @message.
 */
void
valent_message_set_text (ValentMessage *message,
                         const char    *text)
{
  g_return_if_fail (VALENT_IS_MESSAGE (message));

  if (g_strcmp0 (message->text, text) == 0)
    return;

  g_clear_pointer (&message->text, g_free);
  message->text = g_strdup (text);
  g_object_notify_by_pspec (G_OBJECT (message), properties [PROP_TEXT]);
}

/**
 * valent_message_get_thread_id:
 * @message: a #ValentMessage
 *
 * Get the thread ID @message belongs to.
 *
 * Returns: the thread ID
 */
gint64
valent_message_get_thread_id (ValentMessage *message)
{
  g_return_val_if_fail (VALENT_IS_MESSAGE (message), 0);

  return message->thread_id;
}

/**
 * valent_message_set_thread_id:
 * @message: a #ValentMessage
 * @thread_id: a thread ID
 *
 * Set the thread ID for @message to @thread_id.
 */
void
valent_message_set_thread_id (ValentMessage *message,
                              gint64         thread_id)
{
  g_return_if_fail (VALENT_IS_MESSAGE (message));

  if (message->thread_id == thread_id)
    return;

  message->thread_id = thread_id;
  g_object_notify_by_pspec (G_OBJECT (message), properties [PROP_THREAD_ID]);
}

/**
 * valent_message_update:
 * @message: a #ValentMessage
 * @update: (transfer full): a #ValentMessage
 *
 * Update @message with data from @update. The #ValentMessage:id property
 * must match on both objects.
 *
 * This function consumes @update and all its memory, so it should not be used
 * after calling this.
 */
void
valent_message_update (ValentMessage *message,
                       ValentMessage *update)
{
  g_return_if_fail (VALENT_IS_MESSAGE (message));
  g_return_if_fail (VALENT_IS_MESSAGE (update));
  g_return_if_fail (message->id == update->id);

  g_object_freeze_notify (G_OBJECT (message));

  if (message->box != update->box)
    {
      message->box = update->box;
      g_object_notify_by_pspec (G_OBJECT (message), properties [PROP_BOX]);
    }

  if (message->date != update->date)
    {
      message->date = update->date;
      g_object_notify_by_pspec (G_OBJECT (message), properties [PROP_DATE]);
    }

  g_clear_pointer (&message->metadata, g_variant_unref);
  message->metadata = g_steal_pointer (&update->metadata);

  if (message->read != update->read)
    {
      message->read = update->read;
      g_object_notify_by_pspec (G_OBJECT (message), properties [PROP_READ]);
    }

  if (g_strcmp0 (message->sender, update->sender) != 0)
    {
      g_clear_pointer (&message->sender, g_free);
      message->sender = g_steal_pointer (&update->sender);
      g_object_notify_by_pspec (G_OBJECT (message), properties [PROP_SENDER]);
    }

  if (g_strcmp0 (message->text, update->text) != 0)
    {
      g_clear_pointer (&message->text, g_free);
      message->text = g_steal_pointer (&update->text);
      g_object_notify_by_pspec (G_OBJECT (message), properties [PROP_TEXT]);
    }

  g_object_thaw_notify (G_OBJECT (message));
  g_object_unref (update);
}

