// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_TEST_INSIDE) && !defined (VALENT_TEST_COMPILATION)
# error "Only <libvalent-test.h> can be included directly."
#endif

#include <libvalent-core.h>

G_BEGIN_DECLS

#define VALENT_TYPE_TEST_CHANNEL (valent_test_channel_get_type())

G_DECLARE_FINAL_TYPE (ValentTestChannel, valent_test_channel, VALENT, TEST_CHANNEL, ValentChannel)

G_END_DECLS

