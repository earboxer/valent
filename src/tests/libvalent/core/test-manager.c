// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include "config.h"

#include <gio/gio.h>
#include <libvalent-core.h>
#include <libvalent-test.h>

#define DEVICE_INTERFACE "ca.andyholmes.Valent.Device"


typedef struct
{
  GMainLoop     *loop;
  ValentManager *manager;
  ValentDevice  *device;
  gpointer       data;
} ManagerFixture;


static void
manager_fixture_set_up (ManagerFixture *fixture,
                        gconstpointer   user_data)
{
  g_autofree char *path = NULL;
  g_autoptr (ValentData) data = NULL;
  g_autoptr (JsonNode) packets = NULL;
  JsonNode *identity;
  g_autofree char *identity_json = NULL;
  g_autofree char *identity_path = NULL;

  /* Copy the mock device configuration */
  data = valent_data_new (NULL, NULL);
  path = g_build_filename (valent_data_get_config_path (data),
                           "test-device",
                           NULL);
  g_mkdir_with_parents (path, 0700);

  packets = valent_test_load_json (TEST_DATA_DIR"core.json");
  identity = json_object_get_member (json_node_get_object (packets), "identity");
  identity_json = json_to_string (identity, TRUE);
  identity_path = g_build_filename (path, "identity.json", NULL);
  g_file_set_contents (identity_path, identity_json, -1, NULL);

  /* Init the manager */
  fixture->loop = g_main_loop_new (NULL, FALSE);
  fixture->manager = valent_manager_new_sync (data, NULL, NULL);
}

static void
manager_fixture_tear_down (ManagerFixture *fixture,
                           gconstpointer   user_data)
{
  valent_manager_stop (fixture->manager);

  v_assert_finalize_object (fixture->manager);
  g_clear_pointer (&fixture->loop, g_main_loop_unref);
}


static void
on_device_added (ValentManager  *manager,
                 ValentDevice   *device,
                 ManagerFixture *fixture)
{
  fixture->device = device;
}

static void
on_device_removed (ValentManager  *manager,
                   ValentDevice   *device,
                   ManagerFixture *fixture)
{
  if (fixture->device == device)
    fixture->device = NULL;
}

static void
test_manager_management (ManagerFixture *fixture,
                         gconstpointer   user_data)
{
  ValentChannelService *service;
  GPtrArray *devices;

  g_signal_connect (fixture->manager,
                    "device-added",
                    G_CALLBACK (on_device_added),
                    fixture);
  g_signal_connect (fixture->manager,
                    "device-removed",
                    G_CALLBACK (on_device_removed),
                    fixture);

  /* Loads devices from config directory */
  fixture->device = valent_manager_get_device (fixture->manager, "test-device");
  g_assert_true (VALENT_IS_DEVICE (fixture->device));

  devices = valent_manager_get_devices (fixture->manager);
  g_assert_cmpint (devices->len, ==, 1);
  g_clear_pointer (&devices, g_ptr_array_unref);

  /* Removes unpaired devices that disconnect */
  g_object_notify (G_OBJECT (fixture->device), "state");
  g_assert_false (VALENT_IS_DEVICE (fixture->device));

  devices = valent_manager_get_devices (fixture->manager);
  g_assert_cmpint (devices->len, ==, 0);
  g_clear_pointer (&devices, g_ptr_array_unref);

  /* Adds devices for channels */
  valent_manager_start (fixture->manager);

  while ((service = valent_mock_channel_service_get_instance ()) == NULL)
    g_main_context_iteration (NULL, FALSE);

  valent_manager_identify (fixture->manager, NULL);
  g_assert_true (VALENT_IS_DEVICE (fixture->device));

  devices = valent_manager_get_devices (fixture->manager);
  g_assert_cmpint (devices->len, ==, 1);
  g_clear_pointer (&devices, g_ptr_array_unref);

  /* Retains paired devices that disconnect */
  g_object_notify (G_OBJECT (fixture->device), "state");
  g_assert_true (VALENT_IS_DEVICE (fixture->device));

  valent_manager_stop (fixture->manager);

  while ((service = valent_mock_channel_service_get_instance ()) != NULL)
    g_main_context_iteration (NULL, FALSE);
}

static void
manager_finish (GObject        *object,
                GAsyncResult   *result,
                ManagerFixture *fixture)
{
  GError *error = NULL;

  fixture->data = g_dbus_object_manager_client_new_finish (result, &error);
  g_assert_no_error (error);

  g_main_loop_quit (fixture->loop);
}

static void
on_action_added (GActionGroup   *group,
                 char           *name,
                 ManagerFixture *fixture)
{
  g_main_loop_quit (fixture->loop);
}

static void
on_properties_changed (GDBusProxy     *proxy,
                       GVariant       *changed_properties,
                       GStrv           invalidated_properties,
                       ManagerFixture *fixture)
{
  fixture->data = proxy;
  g_main_loop_quit (fixture->loop);
}

static void
on_object_removed (GDBusObjectManager *manager,
                   GDBusObject        *object,
                   ManagerFixture     *fixture)
{
  g_main_loop_quit (fixture->loop);
}

static void
test_manager_dbus (ManagerFixture *fixture,
                   gconstpointer   user_data)
{
  ValentDevice *device;
  g_autoptr (GDBusConnection) connection = NULL;
  g_autoptr (GDBusObjectManager) manager = NULL;
  g_autolist (GDBusObject) objects = NULL;
  g_autoptr (GDBusInterface) interface = NULL;
  g_autoptr (GDBusActionGroup) actions = NULL;
  g_auto (GStrv) action_names = NULL;
  g_autoptr (GDBusMenuModel) menu = NULL;
  const char *unique_name;
  const char *object_path;

  /* Exports current devices */
  connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  valent_manager_export (fixture->manager, connection);

  unique_name = g_dbus_connection_get_unique_name (connection);
  g_dbus_object_manager_client_new (connection,
                                    G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_NONE,
                                    unique_name,
                                    APPLICATION_PATH,
                                    NULL, NULL, NULL,
                                    NULL,
                                    (GAsyncReadyCallback)manager_finish,
                                    fixture);
  g_main_loop_run (fixture->loop);
  manager = g_steal_pointer (&fixture->data);

  /* Exports devices */
  objects = g_dbus_object_manager_get_objects (manager);
  g_assert_cmpuint (g_list_length (objects), ==, 1);

  object_path = g_dbus_object_get_object_path (objects->data);
  interface = g_dbus_object_get_interface (objects->data, DEVICE_INTERFACE);
  g_assert_nonnull (interface);

  g_signal_connect (interface,
                    "g-properties-changed",
                    G_CALLBACK (on_properties_changed),
                    fixture);

  device = valent_manager_get_device (fixture->manager, "test-device");
  g_object_notify (G_OBJECT (device), "type");
  g_main_loop_run (fixture->loop);

  g_assert_true (fixture->data == interface);
  fixture->data = NULL;

  g_clear_pointer (&objects, valent_object_list_free);

  /* Exports Actions */
  actions = g_dbus_action_group_get (connection, unique_name, object_path);

  g_signal_connect (actions,
                    "action-added",
                    G_CALLBACK (on_action_added),
                    fixture);
  action_names = g_action_group_list_actions (G_ACTION_GROUP (actions));
  g_clear_pointer (&action_names, g_strfreev);
  g_main_loop_run (fixture->loop);

  action_names = g_action_group_list_actions (G_ACTION_GROUP (actions));
  g_assert_cmpuint (g_strv_length (action_names), >, 0);
  g_clear_pointer (&action_names, g_strfreev);

  /* Exports Menus */
  menu = g_dbus_menu_model_get (connection, unique_name, object_path);

  /* Unexports devices */
  g_signal_connect (manager,
                    "object-removed",
                    G_CALLBACK (on_object_removed),
                    fixture);

  valent_manager_unexport (fixture->manager);
  g_main_loop_run (fixture->loop);
}

static void
test_manager_dispose (ManagerFixture *fixture,
                      gconstpointer   user_data)
{
  PeasEngine *engine;
  ValentChannelService *service = NULL;
  g_autoptr (GSettings) settings = NULL;

  /* Wait for the channel service */
  valent_manager_start (fixture->manager);

  while ((service = valent_mock_channel_service_get_instance ()) == NULL)
    g_main_context_iteration (NULL, FALSE);

  /* Disable & Enabled channel service */
  settings = valent_component_new_settings ("network", "mock");

  g_settings_set_boolean (settings, "enabled", FALSE);

  while ((service = valent_mock_channel_service_get_instance ()) != NULL)
    g_main_context_iteration (NULL, FALSE);

  g_settings_set_boolean (settings, "enabled", TRUE);

  while ((service = valent_mock_channel_service_get_instance ()) == NULL)
    g_main_context_iteration (NULL, FALSE);

  /* Unload plugin */
  engine = valent_get_engine ();
  peas_engine_unload_plugin (engine, peas_engine_get_plugin_info (engine, "mock"));

  while ((service = valent_mock_channel_service_get_instance ()) != NULL)
    g_main_context_iteration (NULL, FALSE);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, G_TEST_OPTION_ISOLATE_DIRS, NULL);

  g_test_add ("/core/manager/management",
              ManagerFixture, NULL,
              manager_fixture_set_up,
              test_manager_management,
              manager_fixture_tear_down);

#if VALENT_TEST_DBUS
  g_test_add ("/core/manager/dbus",
              ManagerFixture, NULL,
              manager_fixture_set_up,
              test_manager_dbus,
              manager_fixture_tear_down);
#endif

  g_test_add ("/core/manager/dispose",
              ManagerFixture, NULL,
              manager_fixture_set_up,
              test_manager_dispose,
              manager_fixture_tear_down);

  return g_test_run ();
}

