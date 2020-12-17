// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-utils"

#include "config.h"

#include <gio/gio.h>
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#include <sys/time.h>

#include "valent-certificate.h"

#define DEFAULT_EXPIRATION (60L*60L*24L*10L*365L)
#define DEFAULT_KEY_SIZE   4096


/**
 * SECTION:valent-certificate
 * @short_description: Utilities for working with TLS Certificates
 * @title: Certificate Utilities
 * @stability: Unstable
 * @include: libvalent-core.h
 *
 * A small collection of helpers for working with TLS certificates.
 */

G_DEFINE_QUARK (VALENT_CERTIFICATE_ID, valent_certificate_id);
G_DEFINE_QUARK (VALENT_CERTIFICATE_SHA1, valent_certificate_sha1);


/**
 * valent_certificate_generate:
 * @key_path: file path to the private key
 * @cert_path: file path to the certificate
 * @common_name: common name for the certificate
 * @error: (nullable): a #GError
 *
 * Generate a private key and certificate for @common_name, saving them at
 * @key_path and @cert_path respectively.
 *
 * Returns: %TRUE if successful
 */
gboolean
valent_certificate_generate (const char  *key_path,
                             const char  *cert_path,
                             const char  *common_name,
                             GError     **error)
{
  int rc, ret;
  g_autofree char *dn = NULL;
  gnutls_x509_privkey_t privkey;
  gnutls_x509_crt_t crt;
  time_t timestamp;
  guint serial;
  gnutls_datum_t out;

  /*
   * Private Key
   */
  if ((rc = gnutls_x509_privkey_init (&privkey)) != GNUTLS_E_SUCCESS ||
      (rc = gnutls_x509_privkey_generate (privkey,
                                          GNUTLS_PK_RSA,
                                          DEFAULT_KEY_SIZE,
                                          0)) != GNUTLS_E_SUCCESS ||
      (rc = gnutls_x509_privkey_export2 (privkey,
                                         GNUTLS_X509_FMT_PEM,
                                         &out)) != GNUTLS_E_SUCCESS)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Generating private key: %s",
                   gnutls_strerror (rc));
      gnutls_x509_privkey_deinit (privkey);

      return FALSE;
    }

  /* Output the private key PEM to file */
  ret = g_file_set_contents_full (key_path,
                                  (const char *)out.data,
                                  out.size,
                                  G_FILE_SET_CONTENTS_DURABLE,
                                  0600,
                                  error);
  gnutls_free (out.data);

  if (!ret)
    {
      gnutls_x509_privkey_deinit (privkey);
      return FALSE;
    }

  /*
   * TLS Certificate
   */
  if ((rc = gnutls_x509_crt_init (&crt)) != GNUTLS_E_SUCCESS ||
      (rc = gnutls_x509_crt_set_key (crt, privkey)) != GNUTLS_E_SUCCESS ||
      (rc = gnutls_x509_crt_set_version (crt, 3)) != GNUTLS_E_SUCCESS)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Generating certificate: %s",
                   gnutls_strerror (rc));
      gnutls_x509_crt_deinit (crt);
      gnutls_x509_privkey_deinit (privkey);

      return FALSE;
    }

  /* Expiry (10 years) */
  timestamp = time (NULL);
  gnutls_x509_crt_set_activation_time (crt, timestamp);
  gnutls_x509_crt_set_expiration_time (crt, timestamp + DEFAULT_EXPIRATION);

  /* Serial Number */
  serial = GUINT32_TO_BE (10);
  gnutls_x509_crt_set_serial (crt, &serial, sizeof (unsigned int));

  /* Distinguished Name (RFC4514) */
  dn = g_strdup_printf ("O=%s,OU=%s,CN=%s", "Valent", "Valent", common_name);

  if ((rc = gnutls_x509_crt_set_dn (crt, dn, NULL)) != GNUTLS_E_SUCCESS)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Generating certificate: %s",
                   gnutls_strerror (rc));
      gnutls_x509_crt_deinit (crt);
      gnutls_x509_privkey_deinit (privkey);

      return FALSE;
    }

  /* Sign and export the certificate */
  if ((rc = gnutls_x509_crt_sign (crt, crt, privkey)) != GNUTLS_E_SUCCESS ||
      (rc = gnutls_x509_crt_export2 (crt, GNUTLS_X509_FMT_PEM, &out)) != GNUTLS_E_SUCCESS)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Signing certificate: %s",
                   gnutls_strerror (rc));
      gnutls_x509_crt_deinit (crt);
      gnutls_x509_privkey_deinit (privkey);

      return FALSE;
    }

  /* Output the certificate PEM to file */
  ret = g_file_set_contents_full (cert_path,
                                  (const char *)out.data,
                                  out.size,
                                  G_FILE_SET_CONTENTS_DURABLE,
                                  0600,
                                  error);
  gnutls_free (out.data);

  if (!ret)
    {
      gnutls_x509_crt_deinit (crt);
      gnutls_x509_privkey_deinit (privkey);

      return FALSE;
    }

  gnutls_x509_crt_deinit (crt);
  gnutls_x509_privkey_deinit (privkey);

  return TRUE;
}

/**
 * valent_certificate_get_id:
 * @certificate: a #GTlsCertificate
 * @error: (nullable): a #GError
 *
 * Get the common name from @certificate, which by convention is the single
 * source of truth for a device's ID.
 *
 * Returns: (transfer none) (nullable): the certificate ID
 */
const char *
valent_certificate_get_id (GTlsCertificate  *certificate,
                           GError          **error)
{
  const char *device_id;
  int rc;
  g_autoptr (GByteArray) ba = NULL;
  gnutls_x509_crt_t crt;
  gnutls_datum_t crt_der;
  char buf[64] = { 0, };
  size_t buf_size = 64;

  g_return_val_if_fail (G_IS_TLS_CERTIFICATE (certificate), NULL);

  /* Check */
  device_id = g_object_get_qdata (G_OBJECT (certificate),
                                  valent_certificate_id_quark());

  if G_LIKELY (device_id != NULL)
    return device_id;

  /* Extract the common name */
  g_object_get (certificate,
                "certificate", &ba,
                NULL);
  crt_der.data = ba->data;
  crt_der.size = ba->len;

  gnutls_x509_crt_init (&crt);

  if ((rc = gnutls_x509_crt_import (crt, &crt_der, 0)) != GNUTLS_E_SUCCESS ||
      (rc = gnutls_x509_crt_get_dn_by_oid (crt,
                                           GNUTLS_OID_X520_COMMON_NAME,
                                           0,
                                           0,
                                           &buf,
                                           &buf_size)) != GNUTLS_E_SUCCESS)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Reading common name: %s",
                   gnutls_strerror (rc));
      gnutls_x509_crt_deinit (crt);
      return NULL;
    }

  gnutls_x509_crt_deinit (crt);

  /* Intern the id as private data */
  g_object_set_qdata_full (G_OBJECT (certificate),
                           valent_certificate_id_quark(),
                           g_strndup (buf, buf_size),
                           g_free);

  return g_object_get_qdata (G_OBJECT (certificate),
                             valent_certificate_id_quark());
}

/**
 * valent_certificate_get_fingerprint:
 * @certificate: a #GTlsCertificate
 *
 * Get a SHA1 fingerprint hash of @certificate.
 *
 * Returns: (transfer none): a SHA1 hash
 */
const char *
valent_certificate_get_fingerprint (GTlsCertificate *certificate)
{
  g_autoptr (GByteArray) der = NULL;
  g_autofree char *check = NULL;
  const char *fingerprint;
  char buf[60] = { 0, };
  unsigned int c = 0;
  unsigned int f = 0;

  g_return_val_if_fail (G_IS_TLS_CERTIFICATE (certificate), NULL);

  fingerprint = g_object_get_qdata (G_OBJECT (certificate),
                                    valent_certificate_sha1_quark());

  if G_LIKELY (fingerprint != NULL)
    return fingerprint;

  g_object_get (certificate, "certificate", &der, NULL);
  check = g_compute_checksum_for_data (G_CHECKSUM_SHA1, der->data, der->len);

  while (c < 40)
    {
      buf[f++] = check[c++];
      buf[f++] = check[c++];
      buf[f++] = ':';
    }
  buf[59] = '\0';

  /* Intern the hash as private data */
  g_object_set_qdata_full (G_OBJECT (certificate),
                           valent_certificate_id_quark(),
                           g_strndup (buf, 60),
                           g_free);

  return g_object_get_qdata (G_OBJECT (certificate),
                             valent_certificate_id_quark());
}

