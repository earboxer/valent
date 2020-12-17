#include <libvalent-core.h>


static void
test_utils_packet_builder (void)
{
  // FIXME ???
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;

  builder = valent_packet_start ("kdeconnect.identity");
  packet = valent_packet_finish (builder);
}

gint
main (gint   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, G_TEST_OPTION_ISOLATE_DIRS, NULL);

  g_test_add_func ("/core/utils/packet-builder",
                   test_utils_packet_builder);
}
