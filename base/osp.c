/* openvas-libraries/base
 * $Id$
 * Description: API to handle OSP implementation.
 *
 * Authors:
 * Hani Benhabiles <hani.benhabiles@greenbone.net>
 *
 * Copyright:
 * Copyright (C) 2014 Greenbone Networks GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2,
 * or, at your option, any later version as published by the Free
 * Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <glib.h>

#include "openvas_hosts.h"
#include "../misc/openvas_server.h"
#include "../misc/openvas_uuid.h"
#include "../omp/xml.h"


#undef  G_LOG_DOMAIN
#define G_LOG_DOMAIN "lib  osp"

typedef struct osp_connection {
  gnutls_session_t session;
  int socket;
  char *host;
  int port;
} osp_connection_t;

/* @brief Open a new connection to an OSP server.
 *
 * @param[in]   host    Host of OSP server.
 * @param[in]   port    Port of OSP server.
 * @param[in]   cacert  CA certificate path.
 * @param[in]   cert    Client certificate path.
 * @param[in]   key     Client private key path.
 *
 * @return New osp connection, NULL if error.
 */
osp_connection_t *
osp_connection_new (const char *host, int port, const char *cacert,
                    const char *cert, const char *key)
{
  osp_connection_t *connection;

  if (port <= 0 || port > 65535)
    return NULL;
  if (!host || openvas_get_host_type (host) == -1)
    return NULL;
  if (!cert || !key || !cacert)
    return NULL;

  connection = g_malloc0(sizeof (*connection));
  connection->socket = openvas_server_open_with_cert
                        (&connection->session, host, port, cacert, cert, key);
  if (connection->socket == -1)
    {
      g_free (connection);
      return NULL;
    }

  connection->host = g_strdup (host);
  connection->port = port;
  return connection;
}

/* @brief Send a command to an OSP server.
 *
 * @param[in]   connection  Connection to OSP server.
 * @param[in]   command     OSP Command to send.
 * @param[out]  response    Response from OSP server.
 *
 * @return 0 and response, 1 if error.
 */
static int
osp_send_command (osp_connection_t *connection, const char *command,
                  entity_t *response)
{
 if (!connection || !command || !response)
   return 1;

 if (openvas_server_send (&connection->session, command) == -1)
   return 1;

 if (read_entity (&connection->session, response))
    return 1;

 return 0;
}

/* @brief Close a connection to an OSP server.
 *
 * @param[in]   connection  Connection to OSP server to close.
 */
void
osp_connection_close (osp_connection_t *connection)
{
  if (!connection)
    return;

  openvas_server_close (connection->socket, connection->session);
  g_free (connection->host);
  g_free (connection);
}

/* @brief Get the scanner version from an OSP server.
 *
 * @param[in]   connection  Connection to an OSP server.
 * @param[out]  version     Scanner version received.
 *
 * @return 0 and version value, 1 if error.
 */
int
osp_get_scanner_version (osp_connection_t *connection, char **version)
{
  entity_t entity, child;

  if (!connection)
    return 1;

  if (osp_send_command (connection, "<get_version/>", &entity))
    return 1;

  /* Extract version. */
  child = entity_child (entity, "scanner");
  if (!child)
    {
      free_entity (entity);
      return 1;
    }
  child = entity_child (child, "version");
  if (!child)
    {
      free_entity (entity);
      return 1;
    }
  if (version)
    *version = g_strdup (entity_text (child));

  free_entity (entity);
  return 0;
}

int
osp_get_scan (osp_connection_t *connection, const char *scan_id,
              char **report_xml)
{
  entity_t entity, child;
  char command[128];
  GString *string;
  int progress;

  if (!connection)
    return 1;

  snprintf (command, sizeof (command), "<get_scans scan_id='%s'/>", scan_id);
  if (osp_send_command (connection, command, &entity))
    return 1;

  child = entity_child (entity, "scan");
  if (!child)
    {
      free_entity (entity);
      return -1;
    }
  progress = atoi (entity_attribute (child, "progress"));
  string = g_string_new ("");
  print_entity_to_string (child, string);
  free_entity (entity);
  *report_xml = g_string_free (string, FALSE);
  return progress;
}

static void
option_concat_as_xml (gpointer key, gpointer value, gpointer pstr)
{
  char *options_str, *tmp, *key_escaped, *value_escaped;

  options_str = *(char **) pstr;

  key_escaped = g_markup_escape_text ((char *) key, -1);
  value_escaped = g_markup_escape_text ((char *) value, -1);
  tmp = g_strdup_printf ("%s<%s>%s</%s>", options_str ?: "", key_escaped,
                         value_escaped, key_escaped);

  g_free (options_str);
  g_free (key_escaped);
  g_free (value_escaped);
  *(char **) pstr = tmp;
}
/* return scan id (to be used as report id!), null if otherwise. */
char *
osp_start_scan (osp_connection_t *connection, const char *target, void *options)
{
  entity_t entity;
  char *options_str = NULL, *start_scan, *scan_id = NULL;
  int status;

  if (!target)
    return NULL;

  /* Construct options string. */
  if (!options)
    options_str = g_strdup ("");
  else
    g_hash_table_foreach (options, option_concat_as_xml, &options_str);
  start_scan = g_strdup_printf ("<start_scan target='%s'>%s</start_scan>",
                                target, options_str ?: "");
  g_free (options_str);

  if (osp_send_command (connection, start_scan, &entity))
    return NULL;

  status = atoi (entity_attribute (entity, "status"));
  if (status == 200)
    {
      entity_t child = entity_child (entity, "id");
      assert (child);
      assert (entity_text (child));
      scan_id = g_strdup (entity_text (child));
    }
  else
    {
      const char *text = entity_attribute (entity, "status_text");
      assert (text);
      g_log (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "start_scan failure: %s\n",
             text);
    }
  free_entity (entity);
  return scan_id;
}
