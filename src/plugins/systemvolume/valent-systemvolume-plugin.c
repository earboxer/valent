// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-systemvolume-plugin"

#include "config.h"

#include <libpeas/peas.h>
#include <libvalent-core.h>
#include <libvalent-mixer.h>

#include "valent-systemvolume-plugin.h"


struct _ValentSystemvolumePlugin
{
  PeasExtensionBase  parent_instance;

  ValentDevice      *device;
  GSettings         *settings;

  ValentMixer       *mixer;
  GHashTable        *state_cache;
  unsigned long      stream_added_id;
  unsigned long      stream_removed_id;
  unsigned long      stream_changed_id;
};


static void valent_systemvolume_plugin_handle_request     (ValentSystemvolumePlugin *self,
                                                           JsonNode                 *packet);
static void valent_systemvolume_plugin_handle_sink_change (ValentSystemvolumePlugin *self,
                                                           JsonNode                 *packet);
static void valent_systemvolume_plugin_send_sinklist      (ValentSystemvolumePlugin *self);


static void valent_device_plugin_iface_init (ValentDevicePluginInterface *iface);

G_DEFINE_TYPE_WITH_CODE (ValentSystemvolumePlugin, valent_systemvolume_plugin, PEAS_TYPE_EXTENSION_BASE,
                         G_IMPLEMENT_INTERFACE (VALENT_TYPE_DEVICE_PLUGIN, valent_device_plugin_iface_init))

enum {
  PROP_0,
  PROP_DEVICE,
  N_PROPERTIES
};


/*
 * StreamState Cache
 */
typedef struct
{
  ValentMixerStream *stream;
  char              *name;
  char              *description;
  unsigned int       volume;
  gboolean           muted;
} StreamState;

static StreamState *
stream_state_new (ValentMixerStream *stream)
{
  StreamState *state;

  g_assert (VALENT_IS_MIXER_STREAM (stream));

  state = g_new0 (StreamState, 1);
  state->stream = g_object_ref (stream);
  state->name = g_strdup (valent_mixer_stream_get_name (stream));
  state->description = g_strdup (valent_mixer_stream_get_description (stream));
  state->volume = valent_mixer_stream_get_level (stream);
  state->muted = valent_mixer_stream_get_muted (stream);

  return state;
}

static void
stream_state_free (gpointer data)
{
  StreamState *state = data;

  g_clear_object (&state->stream);
  g_clear_pointer (&state->name, g_free);
  g_clear_pointer (&state->description, g_free);
  g_free (state);
}

/*
 * ValentMixer Callbacks
 */
static void
on_stream_changed (ValentMixer              *mixer,
                   ValentMixerStream        *stream,
                   ValentSystemvolumePlugin *self)
{
  StreamState *state;
  const char *name;
  const char *description;
  gboolean muted;
  unsigned int volume;
  JsonBuilder *builder;
  g_autoptr (JsonNode) packet = NULL;

  g_assert (VALENT_IS_MIXER (mixer));
  g_assert (VALENT_IS_MIXER_STREAM (stream));
  g_assert (VALENT_IS_SYSTEMVOLUME_PLUGIN (self));

  /* If this is a new stream or the label changed, we need to send the list */
  name = valent_mixer_stream_get_name (stream);
  state = g_hash_table_lookup (self->state_cache, name);
  description = valent_mixer_stream_get_description (stream);

  if (state == NULL || g_strcmp0 (state->description, description) != 0)
    {
      valent_systemvolume_plugin_send_sinklist (self);
      return;
    }

  /* If neither volume/mute changed we can avoid a packet */
  muted = valent_mixer_stream_get_muted (stream);
  volume = valent_mixer_stream_get_level (stream);

  if (state->volume == volume && state->muted == muted)
    return;

  state->muted = muted;
  state->volume = volume;

  /* Sink update */
  builder = valent_packet_start ("kdeconnect.systemvolume");
  json_builder_set_member_name (builder, "name");
  json_builder_add_string_value (builder, state->name);
  json_builder_set_member_name (builder, "muted");
  json_builder_add_boolean_value (builder, state->muted);
  json_builder_set_member_name (builder, "volume");
  json_builder_add_int_value (builder, state->volume);
  packet = valent_packet_finish (builder);

  valent_device_queue_packet (self->device, packet);
}

static void
on_stream_added (ValentMixer              *mixer,
                 ValentMixerStream        *stream,
                 ValentSystemvolumePlugin *self)
{
  g_assert (VALENT_IS_MIXER (mixer));
  g_assert (VALENT_IS_MIXER_STREAM (stream));
  g_assert (VALENT_IS_SYSTEMVOLUME_PLUGIN (self));

  valent_systemvolume_plugin_send_sinklist (self);
}

static void
on_stream_removed (ValentMixer              *mixer,
                   ValentMixerStream        *stream,
                   ValentSystemvolumePlugin *self)
{
  const char *name;

  g_assert (VALENT_IS_MIXER (mixer));
  g_assert (VALENT_IS_MIXER_STREAM (stream));
  g_assert (VALENT_IS_SYSTEMVOLUME_PLUGIN (self));

  name = valent_mixer_stream_get_name (stream);

  if (g_hash_table_remove (self->state_cache, name))
    valent_systemvolume_plugin_send_sinklist (self);
}

/*
 * Packet Providers
 */
static void
valent_systemvolume_plugin_send_sinklist (ValentSystemvolumePlugin *self)
{
  JsonBuilder *builder;
  g_autoptr (JsonNode) packet = NULL;
  g_autoptr (GPtrArray) sinks = NULL;
  unsigned int max_volume = 100;

  g_assert (VALENT_IS_SYSTEMVOLUME_PLUGIN (self));

  sinks = valent_mixer_get_outputs (self->mixer);

  /* Sink List */
  builder = valent_packet_start ("kdeconnect.systemvolume");
  json_builder_set_member_name (builder, "sinkList");
  json_builder_begin_array (builder);

  for (unsigned int i = 0; i < sinks->len; i++)
    {
      ValentMixerStream *sink = g_ptr_array_index (sinks, i);
      StreamState *state;

      /* Cache entry */
      state = stream_state_new (sink);
      g_hash_table_replace (self->state_cache, g_strdup (state->name), state);

      /* List entry */
      json_builder_begin_object (builder);
      json_builder_set_member_name (builder, "name");
      json_builder_add_string_value (builder, state->name);
      json_builder_set_member_name (builder, "description");
      json_builder_add_string_value (builder, state->description);
      json_builder_set_member_name (builder, "muted");
      json_builder_add_boolean_value (builder, state->muted);
      json_builder_set_member_name (builder, "volume");
      json_builder_add_int_value (builder, state->volume);
      json_builder_set_member_name (builder, "maxVolume");
      json_builder_add_int_value (builder, max_volume);
      json_builder_end_object (builder);
    }

  json_builder_end_array (builder);
  packet = valent_packet_finish (builder);

  valent_device_queue_packet (self->device, packet);
}

/*
 * Packet Handlers
 */
static void
valent_systemvolume_plugin_handle_sink_change (ValentSystemvolumePlugin *self,
                                               JsonNode                 *packet)
{
  StreamState *state;
  JsonObject *body;
  const char *name;

  g_assert (VALENT_IS_SYSTEMVOLUME_PLUGIN (self));
  g_assert (VALENT_IS_PACKET (packet));

  body = valent_packet_get_body (packet);
  name = json_object_get_string_member (body, "name");

  /* The device shouldn't know about streams we haven't told it about */
  if ((state = g_hash_table_lookup (self->state_cache, name)) == NULL)
    {
      valent_systemvolume_plugin_send_sinklist (self);
      return;
    }

  /* Update StreamState and Change */
  if (json_object_has_member (body, "volume"))
    {
      state->volume = json_object_get_int_member (body, "volume");
      valent_mixer_stream_set_level (state->stream, state->volume);
    }

  if (json_object_has_member (body, "muted"))
    {
      state->muted = json_object_get_boolean_member (body, "muted");
      valent_mixer_stream_set_muted (state->stream, state->muted);
    }
}

static void
valent_systemvolume_plugin_handle_request (ValentSystemvolumePlugin *self,
                                           JsonNode                 *packet)
{
  JsonObject *body;

  g_assert (VALENT_IS_SYSTEMVOLUME_PLUGIN (self));

  body = valent_packet_get_body (packet);

  if (json_object_has_member (body, "requestSinks"))
    valent_systemvolume_plugin_send_sinklist (self);

  else if (json_object_has_member (body, "name"))
    valent_systemvolume_plugin_handle_sink_change (self, packet);

  else
    g_warn_if_reached ();
}

/*
 * ValentDevicePlugin
 */
static void
valent_systemvolume_plugin_enable (ValentDevicePlugin *plugin)
{
  ValentSystemvolumePlugin *self = VALENT_SYSTEMVOLUME_PLUGIN (plugin);
  const char *device_id;

  g_assert (VALENT_IS_SYSTEMVOLUME_PLUGIN (self));

  /* Setup GSettings */
  device_id = valent_device_get_id (self->device);
  self->settings = valent_device_plugin_new_settings (device_id, "systemvolume");

  /* Setup stream state cache */
  self->state_cache = g_hash_table_new_full (g_str_hash,
                                             g_str_equal,
                                             g_free,
                                             stream_state_free);
}

static void
valent_systemvolume_plugin_disable (ValentDevicePlugin *plugin)
{
  ValentSystemvolumePlugin *self = VALENT_SYSTEMVOLUME_PLUGIN (plugin);

  g_clear_object (&self->settings);
  g_clear_pointer (&self->state_cache, g_hash_table_unref);
}

static void
valent_systemvolume_plugin_update_state (ValentDevicePlugin *plugin)
{
  ValentSystemvolumePlugin *self = VALENT_SYSTEMVOLUME_PLUGIN (plugin);
  gboolean connected;
  gboolean paired;
  gboolean available;

  g_assert (VALENT_IS_SYSTEMVOLUME_PLUGIN (self));

  connected = valent_device_get_connected (self->device);
  paired = valent_device_get_paired (self->device);
  available = (connected && paired);

  if (self->mixer == NULL)
    self->mixer = valent_mixer_get_default ();

  /* Watch stream changes */
  if (available)
    {
      if (self->stream_added_id == 0)
        self->stream_added_id = g_signal_connect (self->mixer,
                                                  "stream-added::output",
                                                  G_CALLBACK (on_stream_added),
                                                  self);

      if (self->stream_removed_id == 0)
        self->stream_removed_id = g_signal_connect (self->mixer,
                                                    "stream-removed::output",
                                                    G_CALLBACK (on_stream_removed),
                                                    self);

      if (self->stream_changed_id == 0)
        self->stream_changed_id = g_signal_connect (self->mixer,
                                                    "stream-changed::output",
                                                    G_CALLBACK (on_stream_changed),
                                                    self);

      valent_systemvolume_plugin_send_sinklist (self);
    }
  /* Stop watching stream changes */
  else
    {
      g_clear_signal_handler (&self->stream_added_id, self->mixer);
      g_clear_signal_handler (&self->stream_removed_id, self->mixer);
      g_clear_signal_handler (&self->stream_changed_id, self->mixer);
    }
}

static void
valent_systemvolume_plugin_handle_packet (ValentDevicePlugin *plugin,
                                          const char         *type,
                                          JsonNode           *packet)
{
  ValentSystemvolumePlugin *self = VALENT_SYSTEMVOLUME_PLUGIN (plugin);

  g_assert (VALENT_IS_SYSTEMVOLUME_PLUGIN (self));
  g_assert (type != NULL);
  g_assert (VALENT_IS_PACKET (packet));

  if (g_strcmp0 (type, "kdeconnect.systemvolume.request") == 0)
    valent_systemvolume_plugin_handle_request (self, packet);
  else
    g_assert_not_reached ();
}

static void
valent_device_plugin_iface_init (ValentDevicePluginInterface *iface)
{
  iface->enable = valent_systemvolume_plugin_enable;
  iface->disable = valent_systemvolume_plugin_disable;
  iface->handle_packet = valent_systemvolume_plugin_handle_packet;
  iface->update_state = valent_systemvolume_plugin_update_state;
}

/*
 * GObject
 */
static void
valent_systemvolume_plugin_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  ValentSystemvolumePlugin *self = VALENT_SYSTEMVOLUME_PLUGIN (object);

  switch (prop_id)
    {
    case PROP_DEVICE:
      g_value_set_object (value, self->device);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_systemvolume_plugin_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  ValentSystemvolumePlugin *self = VALENT_SYSTEMVOLUME_PLUGIN (object);

  switch (prop_id)
    {
    case PROP_DEVICE:
      self->device = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_systemvolume_plugin_init (ValentSystemvolumePlugin *self)
{
}

static void
valent_systemvolume_plugin_class_init (ValentSystemvolumePluginClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = valent_systemvolume_plugin_get_property;
  object_class->set_property = valent_systemvolume_plugin_set_property;

  g_object_class_override_property (object_class, PROP_DEVICE, "device");
}

