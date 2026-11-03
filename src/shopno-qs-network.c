/*
 * Copyright (C) 2026 FA Saikat
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/*
 * Wi-Fi, wired devices and the primary connection through libnm.
 *
 * There is no secret agent: new networks get their password from the
 * flyout, and NetworkManager stores it with the connection. When a saved
 * password stops working, "connect-failed" asks the flyout for a new one.
 */

#include "config.h"

#include <NetworkManager.h>

#include "shopno-qs-network.h"

enum
{
  CHANGED,
  NETWORKS_CHANGED,
  CONNECT_FAILED,
  N_SIGNALS
};

struct _ShopnoQsNetwork
{
  GObject        __parent__;

  NMClient      *client;
  GCancellable  *cancellable;
  NMDeviceWifi  *wifi;
  NMAccessPoint *active_ap;
  GPtrArray     *wired;

  gchar         *active_ssid;
  gchar         *connecting_ssid;
  gchar         *pending_ssid;
  GPtrArray     *networks;

  guint          changed_id;
  guint          networks_id;
};

static guint            signals[N_SIGNALS];
static ShopnoQsNetwork *default_network = NULL;

G_DEFINE_TYPE (ShopnoQsNetwork, shopno_qs_network, G_TYPE_OBJECT)

static void shopno_qs_network_update_device (ShopnoQsNetwork *self);



static void
shopno_qs_network_free_network (gpointer data)
{
  ShopnoQsWifiNetwork *network = data;

  g_free (network->ssid);
  g_free (network);
}



static gchar *
shopno_qs_network_ap_ssid (NMAccessPoint *ap)
{
  GBytes *ssid = nm_access_point_get_ssid (ap);

  /* Hidden networks have no name */
  if (ssid == NULL || g_bytes_get_size (ssid) == 0)
    return NULL;

  return nm_utils_ssid_to_utf8 (g_bytes_get_data (ssid, NULL), g_bytes_get_size (ssid));
}



static ShopnoQsWifiSecurity
shopno_qs_network_ap_security (NMAccessPoint *ap)
{
  NM80211ApFlags         flags = nm_access_point_get_flags (ap);
  NM80211ApSecurityFlags wpa = nm_access_point_get_wpa_flags (ap);
  NM80211ApSecurityFlags rsn = nm_access_point_get_rsn_flags (ap);
  NM80211ApSecurityFlags all = wpa | rsn;

  if ((all & NM_802_11_AP_SEC_KEY_MGMT_802_1X) != 0)
    return SHOPNO_QS_WIFI_ENTERPRISE;

  if ((all & (NM_802_11_AP_SEC_KEY_MGMT_PSK | NM_802_11_AP_SEC_KEY_MGMT_SAE)) != 0)
    return SHOPNO_QS_WIFI_PSK;

  /* Enhanced Open encrypts without a password */
  if ((all & NM_802_11_AP_SEC_KEY_MGMT_OWE) != 0)
    return SHOPNO_QS_WIFI_OPEN;

  /* WEP */
  if ((flags & NM_802_11_AP_FLAGS_PRIVACY) != 0)
    return SHOPNO_QS_WIFI_PSK;

  return SHOPNO_QS_WIFI_OPEN;
}



/* The strongest access point of a network */
static NMAccessPoint *
shopno_qs_network_find_ap (ShopnoQsNetwork *self,
                           const gchar     *ssid)
{
  const GPtrArray *aps;
  NMAccessPoint   *best = NULL;

  if (self->wifi == NULL)
    return NULL;

  aps = nm_device_wifi_get_access_points (self->wifi);
  for (guint i = 0; i < aps->len; i++)
    {
      NMAccessPoint *ap = g_ptr_array_index (aps, i);
      gchar         *name = shopno_qs_network_ap_ssid (ap);

      if (g_strcmp0 (name, ssid) == 0
          && (best == NULL || nm_access_point_get_strength (ap) > nm_access_point_get_strength (best)))
        best = ap;

      g_free (name);
    }

  return best;
}



/* Saved connections for an access point, most recently used first */
static gint
shopno_qs_network_compare_timestamp (gconstpointer a,
                                     gconstpointer b)
{
  NMSettingConnection *sa = nm_connection_get_setting_connection (*(NMConnection **) a);
  NMSettingConnection *sb = nm_connection_get_setting_connection (*(NMConnection **) b);
  guint64              ta = sa != NULL ? nm_setting_connection_get_timestamp (sa) : 0;
  guint64              tb = sb != NULL ? nm_setting_connection_get_timestamp (sb) : 0;

  return ta < tb ? 1 : ta > tb ? -1 : 0;
}



static gchar *
shopno_qs_network_connection_ssid (NMConnection *connection)
{
  NMSettingWireless *s_wifi = nm_connection_get_setting_wireless (connection);
  GBytes            *ssid;

  if (s_wifi == NULL)
    return NULL;

  ssid = nm_setting_wireless_get_ssid (s_wifi);
  if (ssid == NULL || g_bytes_get_size (ssid) == 0)
    return NULL;

  return nm_utils_ssid_to_utf8 (g_bytes_get_data (ssid, NULL), g_bytes_get_size (ssid));
}



/* Saved connections for a network name that this device may use, most
 * recently used first. Matched by name, as NetworkManager does when it
 * picks a connection: an access point's BSSID or band does not make a
 * saved network unknown. */
static GPtrArray *
shopno_qs_network_saved_connections (ShopnoQsNetwork *self,
                                     const gchar     *ssid)
{
  const GPtrArray *all = nm_client_get_connections (self->client);
  const gchar     *iface = nm_device_get_iface (NM_DEVICE (self->wifi));
  GPtrArray       *connections = g_ptr_array_new ();

  for (guint i = 0; all != NULL && i < all->len; i++)
    {
      NMConnection *connection = g_ptr_array_index (all, i);
      const gchar  *bound = nm_connection_get_interface_name (connection);
      gchar        *name;

      /* A connection tied to another interface, like a renamed one */
      if (bound != NULL && g_strcmp0 (bound, iface) != 0)
        continue;

      name = shopno_qs_network_connection_ssid (connection);
      if (g_strcmp0 (name, ssid) == 0)
        g_ptr_array_add (connections, connection);
      g_free (name);
    }

  g_ptr_array_sort (connections, shopno_qs_network_compare_timestamp);

  return connections;
}



/*
 * Change notification, batched so a burst of D-Bus updates becomes one
 * update of the flyout
 */

static gboolean
shopno_qs_network_emit_changed (gpointer data)
{
  ShopnoQsNetwork *self = data;

  self->changed_id = 0;
  g_signal_emit (self, signals[CHANGED], 0);

  return G_SOURCE_REMOVE;
}



static void
shopno_qs_network_schedule_changed (ShopnoQsNetwork *self)
{
  if (self->changed_id == 0)
    self->changed_id = g_idle_add (shopno_qs_network_emit_changed, self);
}



static gint
shopno_qs_network_compare_networks (gconstpointer a,
                                    gconstpointer b)
{
  const ShopnoQsWifiNetwork *na = *(ShopnoQsWifiNetwork **) a;
  const ShopnoQsWifiNetwork *nb = *(ShopnoQsWifiNetwork **) b;

  if (na->active != nb->active)
    return na->active ? -1 : 1;
  if (na->strength != nb->strength)
    return na->strength > nb->strength ? -1 : 1;

  return g_utf8_collate (na->ssid, nb->ssid);
}



static void
shopno_qs_network_build_networks (ShopnoQsNetwork *self)
{
  GHashTable      *by_ssid;
  const GPtrArray *aps;

  g_ptr_array_set_size (self->networks, 0);

  if (self->wifi == NULL || !shopno_qs_network_get_wifi_enabled (self))
    return;

  by_ssid = g_hash_table_new (g_str_hash, g_str_equal);
  aps = nm_device_wifi_get_access_points (self->wifi);

  for (guint i = 0; i < aps->len; i++)
    {
      NMAccessPoint       *ap = g_ptr_array_index (aps, i);
      gchar               *ssid = shopno_qs_network_ap_ssid (ap);
      ShopnoQsWifiNetwork *network;
      GPtrArray           *connections;

      if (ssid == NULL)
        continue;

      network = g_hash_table_lookup (by_ssid, ssid);
      if (network == NULL)
        {
          network = g_new0 (ShopnoQsWifiNetwork, 1);
          network->ssid = ssid;
          network->security = shopno_qs_network_ap_security (ap);
          network->active = g_strcmp0 (ssid, self->active_ssid) == 0;

          connections = shopno_qs_network_saved_connections (self, ssid);
          network->known = connections->len > 0;
          g_ptr_array_unref (connections);
          g_hash_table_insert (by_ssid, network->ssid, network);
          g_ptr_array_add (self->networks, network);
        }
      else
        {
          g_free (ssid);
        }

      network->strength = MAX (network->strength, nm_access_point_get_strength (ap));
    }

  g_hash_table_destroy (by_ssid);
  g_ptr_array_sort (self->networks, shopno_qs_network_compare_networks);
}



static gboolean
shopno_qs_network_emit_networks_changed (gpointer data)
{
  ShopnoQsNetwork *self = data;

  self->networks_id = 0;
  shopno_qs_network_build_networks (self);
  g_signal_emit (self, signals[NETWORKS_CHANGED], 0);

  return G_SOURCE_REMOVE;
}



static void
shopno_qs_network_schedule_networks (ShopnoQsNetwork *self)
{
  if (self->networks_id == 0)
    self->networks_id = g_idle_add (shopno_qs_network_emit_networks_changed, self);
}



/*
 * Device state
 */

static void
shopno_qs_network_update_state (ShopnoQsNetwork *self)
{
  NMDeviceState  state = NM_DEVICE_STATE_UNKNOWN;
  NMAccessPoint *ap = NULL;
  gchar         *ssid = NULL;

  if (self->wifi != NULL)
    {
      state = nm_device_get_state (NM_DEVICE (self->wifi));
      ap = nm_device_wifi_get_active_access_point (self->wifi);
    }

  /* Follow the strength of the connected access point for the icons */
  if (ap != self->active_ap)
    {
      if (self->active_ap != NULL)
        {
          g_signal_handlers_disconnect_by_data (self->active_ap, self);
          g_object_unref (self->active_ap);
        }

      self->active_ap = ap != NULL ? g_object_ref (ap) : NULL;

      if (self->active_ap != NULL)
        g_signal_connect_swapped (self->active_ap, "notify::" NM_ACCESS_POINT_STRENGTH,
                                  G_CALLBACK (shopno_qs_network_schedule_changed), self);
    }

  /* The name comes from the connection: the access point in use may be
   * a hidden BSSID of the same router, which has no name */
  if (self->wifi != NULL)
    {
      NMActiveConnection *active = nm_device_get_active_connection (NM_DEVICE (self->wifi));
      NMRemoteConnection *connection = active != NULL ? nm_active_connection_get_connection (active) : NULL;

      if (connection != NULL)
        ssid = shopno_qs_network_connection_ssid (NM_CONNECTION (connection));
    }

  if (ssid == NULL && ap != NULL)
    ssid = shopno_qs_network_ap_ssid (ap);

  g_clear_pointer (&self->active_ssid, g_free);
  g_clear_pointer (&self->connecting_ssid, g_free);

  if (state == NM_DEVICE_STATE_ACTIVATED)
    self->active_ssid = g_steal_pointer (&ssid);
  else if (state >= NM_DEVICE_STATE_PREPARE && state < NM_DEVICE_STATE_ACTIVATED)
    self->connecting_ssid = ssid != NULL ? g_steal_pointer (&ssid) : g_strdup (self->pending_ssid);

  g_free (ssid);

  shopno_qs_network_schedule_changed (self);
  shopno_qs_network_schedule_networks (self);
}



static void
shopno_qs_network_device_state_changed (NMDevice        *device,
                                        guint            new_state,
                                        guint            old_state,
                                        guint            reason,
                                        ShopnoQsNetwork *self)
{
  gchar *ssid;

  if (new_state == NM_DEVICE_STATE_FAILED && self->pending_ssid != NULL)
    {
      /* Without a secret agent, a wrong or missing password ends here */
      ssid = g_steal_pointer (&self->pending_ssid);
      g_signal_emit (self, signals[CONNECT_FAILED], 0, ssid,
                     reason == NM_DEVICE_STATE_REASON_NO_SECRETS
                     || reason == NM_DEVICE_STATE_REASON_SUPPLICANT_DISCONNECT);
      g_free (ssid);
    }
  else if (new_state == NM_DEVICE_STATE_ACTIVATED)
    {
      g_clear_pointer (&self->pending_ssid, g_free);
    }

  shopno_qs_network_update_state (self);
}



/* Physical Ethernet ports; virtual pairs of containers are not ports */
static gboolean
shopno_qs_network_is_wired (NMDevice *device)
{
  if (!NM_IS_DEVICE_ETHERNET (device))
    return FALSE;

#if NM_CHECK_VERSION (1, 30, 0)
  if (NM_IS_DEVICE_VETH (device))
    return FALSE;
#endif

  return TRUE;
}



static void
shopno_qs_network_update_wired (ShopnoQsNetwork *self)
{
  const GPtrArray *devices;

  for (guint i = 0; i < self->wired->len; i++)
    g_signal_handlers_disconnect_by_data (g_ptr_array_index (self->wired, i), self);
  g_ptr_array_set_size (self->wired, 0);

  if (self->client == NULL || !nm_client_get_nm_running (self->client))
    return;

  devices = nm_client_get_devices (self->client);
  for (guint i = 0; i < devices->len; i++)
    {
      NMDevice *device = g_ptr_array_index (devices, i);

      if (!shopno_qs_network_is_wired (device))
        continue;

      g_ptr_array_add (self->wired, g_object_ref (device));
      g_signal_connect_swapped (device, "notify::" NM_DEVICE_STATE,
                                G_CALLBACK (shopno_qs_network_schedule_changed), self);
    }
}



static void
shopno_qs_network_update_device (ShopnoQsNetwork *self)
{
  NMDeviceWifi    *wifi = NULL;
  const GPtrArray *devices;

  if (self->client != NULL && nm_client_get_nm_running (self->client))
    {
      devices = nm_client_get_devices (self->client);
      for (guint i = 0; i < devices->len && wifi == NULL; i++)
        if (NM_IS_DEVICE_WIFI (g_ptr_array_index (devices, i)))
          wifi = g_ptr_array_index (devices, i);
    }

  if (wifi != self->wifi)
    {
      if (self->wifi != NULL)
        {
          g_signal_handlers_disconnect_by_data (self->wifi, self);
          g_object_unref (self->wifi);
        }

      self->wifi = wifi != NULL ? g_object_ref (wifi) : NULL;

      if (self->wifi != NULL)
        {
          g_signal_connect (self->wifi, "state-changed",
                            G_CALLBACK (shopno_qs_network_device_state_changed), self);
          g_signal_connect_swapped (self->wifi, "notify::" NM_DEVICE_WIFI_ACTIVE_ACCESS_POINT,
                                    G_CALLBACK (shopno_qs_network_update_state), self);
          g_signal_connect_swapped (self->wifi, "notify::" NM_DEVICE_ACTIVE_CONNECTION,
                                    G_CALLBACK (shopno_qs_network_update_state), self);
          g_signal_connect_swapped (self->wifi, "access-point-added",
                                    G_CALLBACK (shopno_qs_network_schedule_networks), self);
          g_signal_connect_swapped (self->wifi, "access-point-removed",
                                    G_CALLBACK (shopno_qs_network_schedule_networks), self);
          g_signal_connect_swapped (self->wifi, "notify::" NM_DEVICE_WIFI_LAST_SCAN,
                                    G_CALLBACK (shopno_qs_network_schedule_networks), self);
          g_signal_connect_swapped (self->wifi, "notify::" NM_DEVICE_AVAILABLE_CONNECTIONS,
                                    G_CALLBACK (shopno_qs_network_schedule_networks), self);
        }
    }

  shopno_qs_network_update_wired (self);
  shopno_qs_network_update_state (self);
}



static void
shopno_qs_network_client_ready (GObject      *source,
                                GAsyncResult *result,
                                gpointer      data)
{
  ShopnoQsNetwork *self;
  NMClient        *client;
  GError          *error = NULL;

  client = nm_client_new_finish (result, &error);
  if (client == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed to connect to NetworkManager: %s", error->message);
      g_error_free (error);
      return;
    }

  self = SHOPNO_QS_NETWORK (data);
  self->client = client;

  g_signal_connect_swapped (client, "notify::" NM_CLIENT_NM_RUNNING,
                            G_CALLBACK (shopno_qs_network_update_device), self);
  g_signal_connect_swapped (client, "notify::" NM_CLIENT_WIRELESS_ENABLED,
                            G_CALLBACK (shopno_qs_network_update_state), self);
  g_signal_connect_swapped (client, "notify::" NM_CLIENT_PRIMARY_CONNECTION,
                            G_CALLBACK (shopno_qs_network_schedule_changed), self);
  g_signal_connect_swapped (client, "device-added",
                            G_CALLBACK (shopno_qs_network_update_device), self);
  g_signal_connect_swapped (client, "device-removed",
                            G_CALLBACK (shopno_qs_network_update_device), self);
  g_signal_connect_swapped (client, "connection-added",
                            G_CALLBACK (shopno_qs_network_schedule_networks), self);
  g_signal_connect_swapped (client, "connection-removed",
                            G_CALLBACK (shopno_qs_network_schedule_networks), self);

  shopno_qs_network_update_device (self);
}



/*
 * Actions
 */

static void
shopno_qs_network_scan_done (GObject      *source,
                             GAsyncResult *result,
                             gpointer      data)
{
  /* NetworkManager refuses scans that come too close together, which
   * is harmless */
  nm_device_wifi_request_scan_finish (NM_DEVICE_WIFI (source), result, NULL);
}



static void
shopno_qs_network_disconnect_done (GObject      *source,
                                   GAsyncResult *result,
                                   gpointer      data)
{
  GError *error = NULL;

  if (!nm_device_disconnect_finish (NM_DEVICE (source), result, &error))
    {
      g_warning ("Failed to disconnect: %s", error->message);
      g_error_free (error);
    }
}



static void
shopno_qs_network_wireless_done (GObject      *source,
                                 GAsyncResult *result,
                                 gpointer      data)
{
  GError *error = NULL;

  if (!nm_client_dbus_set_property_finish (NM_CLIENT (source), result, &error))
    {
      g_warning ("Failed to switch Wi-Fi: %s", error->message);
      g_error_free (error);
    }
}



static void
shopno_qs_network_activation_done (ShopnoQsNetwork    *self,
                                   NMActiveConnection *active,
                                   GError             *error)
{
  gchar *ssid;

  if (active != NULL)
    {
      g_object_unref (active);
      return;
    }

  /* self is gone when the request was cancelled */
  if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      g_warning ("Failed to connect: %s", error->message);

      ssid = g_steal_pointer (&self->pending_ssid);
      g_signal_emit (self, signals[CONNECT_FAILED], 0, ssid, FALSE);
      g_free (ssid);
      shopno_qs_network_update_state (self);
    }

  g_error_free (error);
}



static void
shopno_qs_network_activated (GObject      *source,
                             GAsyncResult *result,
                             gpointer      data)
{
  GError             *error = NULL;
  NMActiveConnection *active;

  active = nm_client_activate_connection_finish (NM_CLIENT (source), result, &error);
  shopno_qs_network_activation_done (data, active, error);
}



static void
shopno_qs_network_added (GObject      *source,
                         GAsyncResult *result,
                         gpointer      data)
{
  GError             *error = NULL;
  NMActiveConnection *active;

  active = nm_client_add_and_activate_connection_finish (NM_CLIENT (source), result, &error);
  shopno_qs_network_activation_done (data, active, error);
}



static void
shopno_qs_network_activate (ShopnoQsNetwork *self,
                            NMConnection    *connection,
                            NMAccessPoint   *ap)
{
  nm_client_activate_connection_async (self->client, connection, NM_DEVICE (self->wifi),
                                       nm_object_get_path (NM_OBJECT (ap)), self->cancellable,
                                       shopno_qs_network_activated, self);
}



typedef struct
{
  ShopnoQsNetwork *network;
  NMAccessPoint   *ap;
} CommitData;



static void
shopno_qs_network_committed (GObject      *source,
                             GAsyncResult *result,
                             gpointer      data)
{
  CommitData *commit = data;
  GError     *error = NULL;

  if (nm_remote_connection_commit_changes_finish (NM_REMOTE_CONNECTION (source), result, &error))
    {
      shopno_qs_network_activate (commit->network, NM_CONNECTION (source), commit->ap);
    }
  else
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed to save the new password: %s", error->message);
      g_error_free (error);
    }

  g_object_unref (commit->ap);
  g_object_unref (commit->network);
  g_free (commit);
}



static void
shopno_qs_network_set_password (NMConnection         *connection,
                                ShopnoQsWifiSecurity  security,
                                NMAccessPoint        *ap,
                                const gchar          *password)
{
  NMSettingWirelessSecurity *s_wsec;
  NM80211ApSecurityFlags     rsn = nm_access_point_get_rsn_flags (ap);
  NM80211ApSecurityFlags     wpa = nm_access_point_get_wpa_flags (ap);
  gsize                      length = strlen (password);

  if (security != SHOPNO_QS_WIFI_PSK)
    return;

  s_wsec = nm_connection_get_setting_wireless_security (connection);
  if (s_wsec == NULL)
    {
      s_wsec = NM_SETTING_WIRELESS_SECURITY (nm_setting_wireless_security_new ());
      nm_connection_add_setting (connection, NM_SETTING (s_wsec));
    }

  if (((rsn | wpa) & (NM_802_11_AP_SEC_KEY_MGMT_PSK | NM_802_11_AP_SEC_KEY_MGMT_SAE)) == 0)
    {
      /* WEP: 5 or 13 characters, or 10 or 26 hex digits, are keys */
      g_object_set (s_wsec,
                    NM_SETTING_WIRELESS_SECURITY_KEY_MGMT, "none",
                    NM_SETTING_WIRELESS_SECURITY_WEP_KEY_TYPE,
                    (length == 5 || length == 13 || length == 10 || length == 26)
                    ? NM_WEP_KEY_TYPE_KEY : NM_WEP_KEY_TYPE_PASSPHRASE,
                    NM_SETTING_WIRELESS_SECURITY_WEP_KEY_FLAGS, NM_SETTING_SECRET_FLAG_NONE,
                    NULL);
      nm_setting_wireless_security_set_wep_key (s_wsec, 0, password);
      return;
    }

  /* WPA3-only networks need SAE; mixed networks accept WPA-PSK */
  g_object_set (s_wsec,
                NM_SETTING_WIRELESS_SECURITY_KEY_MGMT,
                (rsn & NM_802_11_AP_SEC_KEY_MGMT_PSK) == 0 && (rsn & NM_802_11_AP_SEC_KEY_MGMT_SAE) != 0
                ? "sae" : "wpa-psk",
                NM_SETTING_WIRELESS_SECURITY_PSK, password,
                /* Stored by NetworkManager: without nm-applet there is no
                 * agent to keep it in a keyring */
                NM_SETTING_WIRELESS_SECURITY_PSK_FLAGS, NM_SETTING_SECRET_FLAG_NONE,
                NULL);
}



static NMConnection *
shopno_qs_network_new_connection (const gchar          *ssid,
                                  ShopnoQsWifiSecurity  security,
                                  NMAccessPoint        *ap,
                                  const gchar          *password)
{
  NMConnection *connection = nm_simple_connection_new ();
  NMSetting    *s_con;
  NMSetting    *s_wifi;
  GBytes       *bytes;

  s_con = nm_setting_connection_new ();
  g_object_set (s_con,
                NM_SETTING_CONNECTION_ID, ssid,
                NM_SETTING_CONNECTION_TYPE, NM_SETTING_WIRELESS_SETTING_NAME,
                NM_SETTING_CONNECTION_AUTOCONNECT, TRUE,
                NULL);
  nm_connection_add_setting (connection, s_con);

  bytes = g_bytes_new (ssid, strlen (ssid));
  s_wifi = nm_setting_wireless_new ();
  g_object_set (s_wifi,
                NM_SETTING_WIRELESS_SSID, bytes,
                NM_SETTING_WIRELESS_MODE, NM_SETTING_WIRELESS_MODE_INFRA,
                NULL);
  nm_connection_add_setting (connection, s_wifi);
  g_bytes_unref (bytes);

  if (password != NULL)
    shopno_qs_network_set_password (connection, security, ap, password);

  return connection;
}



/*
 * GObject
 */

static void
shopno_qs_network_dispose (GObject *object)
{
  ShopnoQsNetwork *self = SHOPNO_QS_NETWORK (object);

  g_cancellable_cancel (self->cancellable);

  if (self->changed_id != 0)
    {
      g_source_remove (self->changed_id);
      self->changed_id = 0;
    }

  if (self->networks_id != 0)
    {
      g_source_remove (self->networks_id);
      self->networks_id = 0;
    }

  if (self->active_ap != NULL)
    {
      g_signal_handlers_disconnect_by_data (self->active_ap, self);
      g_clear_object (&self->active_ap);
    }

  if (self->wifi != NULL)
    {
      g_signal_handlers_disconnect_by_data (self->wifi, self);
      g_clear_object (&self->wifi);
    }

  if (self->wired != NULL)
    {
      for (guint i = 0; i < self->wired->len; i++)
        g_signal_handlers_disconnect_by_data (g_ptr_array_index (self->wired, i), self);
      g_clear_pointer (&self->wired, g_ptr_array_unref);
    }

  if (self->client != NULL)
    {
      g_signal_handlers_disconnect_by_data (self->client, self);
      g_clear_object (&self->client);
    }

  G_OBJECT_CLASS (shopno_qs_network_parent_class)->dispose (object);
}



static void
shopno_qs_network_finalize (GObject *object)
{
  ShopnoQsNetwork *self = SHOPNO_QS_NETWORK (object);

  g_object_unref (self->cancellable);
  g_free (self->active_ssid);
  g_free (self->connecting_ssid);
  g_free (self->pending_ssid);
  g_ptr_array_unref (self->networks);

  G_OBJECT_CLASS (shopno_qs_network_parent_class)->finalize (object);
}



static void
shopno_qs_network_class_init (ShopnoQsNetworkClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = shopno_qs_network_dispose;
  gobject_class->finalize = shopno_qs_network_finalize;

  /* Availability, Wi-Fi radio, or the connection changed */
  signals[CHANGED] =
    g_signal_new ("changed", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  signals[NETWORKS_CHANGED] =
    g_signal_new ("networks-changed", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  /* (ssid, needs_password) */
  signals[CONNECT_FAILED] =
    g_signal_new ("connect-failed", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_BOOLEAN);
}



static void
shopno_qs_network_init (ShopnoQsNetwork *self)
{
  self->cancellable = g_cancellable_new ();
  self->networks = g_ptr_array_new_with_free_func (shopno_qs_network_free_network);
  self->wired = g_ptr_array_new_with_free_func (g_object_unref);

  nm_client_new_async (self->cancellable, shopno_qs_network_client_ready, self);
}



static void
shopno_qs_network_wired_activated (GObject      *source,
                                   GAsyncResult *result,
                                   gpointer      data)
{
  GError             *error = NULL;
  NMActiveConnection *active;

  if (data != NULL)
    active = nm_client_add_and_activate_connection_finish (NM_CLIENT (source), result, &error);
  else
    active = nm_client_activate_connection_finish (NM_CLIENT (source), result, &error);

  if (active != NULL)
    {
      g_object_unref (active);
      return;
    }

  if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    g_warning ("Failed to connect Ethernet: %s", error->message);
  g_error_free (error);
}



/*
 * Public API
 */

ShopnoQsNetwork *
shopno_qs_network_get_default (void)
{
  if (default_network != NULL)
    return g_object_ref (default_network);

  default_network = g_object_new (SHOPNO_TYPE_QS_NETWORK, NULL);
  g_object_add_weak_pointer (G_OBJECT (default_network), (gpointer *) &default_network);

  return default_network;
}



gboolean
shopno_qs_network_get_wifi_available (ShopnoQsNetwork *network)
{
  g_return_val_if_fail (SHOPNO_IS_QS_NETWORK (network), FALSE);

  return network->wifi != NULL;
}



gboolean
shopno_qs_network_get_wifi_enabled (ShopnoQsNetwork *network)
{
  g_return_val_if_fail (SHOPNO_IS_QS_NETWORK (network), FALSE);

  return network->client != NULL
         && nm_client_get_nm_running (network->client)
         && nm_client_wireless_get_enabled (network->client);
}



void
shopno_qs_network_set_wifi_enabled (ShopnoQsNetwork *network,
                                    gboolean         enabled)
{
  g_return_if_fail (SHOPNO_IS_QS_NETWORK (network));

  if (network->client == NULL)
    return;

  nm_client_dbus_set_property (network->client, NM_DBUS_PATH, NM_DBUS_INTERFACE,
                               "WirelessEnabled", g_variant_new_boolean (enabled),
                               -1, NULL, shopno_qs_network_wireless_done, NULL);
}



const gchar *
shopno_qs_network_get_active_ssid (ShopnoQsNetwork *network)
{
  g_return_val_if_fail (SHOPNO_IS_QS_NETWORK (network), NULL);

  return network->active_ssid;
}



const gchar *
shopno_qs_network_get_connecting_ssid (ShopnoQsNetwork *network)
{
  g_return_val_if_fail (SHOPNO_IS_QS_NETWORK (network), NULL);

  return network->connecting_ssid;
}



GPtrArray *
shopno_qs_network_get_wifi_networks (ShopnoQsNetwork *network)
{
  g_return_val_if_fail (SHOPNO_IS_QS_NETWORK (network), NULL);

  return network->networks;
}



void
shopno_qs_network_request_scan (ShopnoQsNetwork *network)
{
  g_return_if_fail (SHOPNO_IS_QS_NETWORK (network));

  if (network->wifi != NULL && shopno_qs_network_get_wifi_enabled (network))
    nm_device_wifi_request_scan_async (network->wifi, NULL, shopno_qs_network_scan_done, NULL);
}



void
shopno_qs_network_connect (ShopnoQsNetwork *network,
                           const gchar     *ssid,
                           const gchar     *password)
{
  NMAccessPoint        *ap;
  GPtrArray            *connections;
  ShopnoQsWifiSecurity  security;
  NMConnection         *connection;
  CommitData           *commit;

  g_return_if_fail (SHOPNO_IS_QS_NETWORK (network));
  g_return_if_fail (ssid != NULL);

  ap = shopno_qs_network_find_ap (network, ssid);
  if (ap == NULL || network->client == NULL)
    return;

  g_free (network->pending_ssid);
  network->pending_ssid = g_strdup (ssid);

  security = shopno_qs_network_ap_security (ap);
  connections = shopno_qs_network_saved_connections (network, ssid);

  if (connections->len > 0)
    {
      connection = g_ptr_array_index (connections, 0);

      if (password != NULL && security == SHOPNO_QS_WIFI_PSK)
        {
          /* A new password for a saved network: store it, then connect */
          shopno_qs_network_set_password (connection, security, ap, password);
          commit = g_new0 (CommitData, 1);
          commit->network = g_object_ref (network);
          commit->ap = g_object_ref (ap);
          nm_remote_connection_commit_changes_async (NM_REMOTE_CONNECTION (connection), TRUE,
                                                     network->cancellable,
                                                     shopno_qs_network_committed, commit);
        }
      else
        {
          shopno_qs_network_activate (network, connection, ap);
        }
    }
  else
    {
      connection = shopno_qs_network_new_connection (ssid, security, ap, password);
      nm_client_add_and_activate_connection_async (network->client, connection,
                                                   NM_DEVICE (network->wifi),
                                                   nm_object_get_path (NM_OBJECT (ap)),
                                                   network->cancellable,
                                                   shopno_qs_network_added, network);
      g_object_unref (connection);
    }

  g_ptr_array_unref (connections);
  shopno_qs_network_update_state (network);
}



void
shopno_qs_network_disconnect (ShopnoQsNetwork *network)
{
  g_return_if_fail (SHOPNO_IS_QS_NETWORK (network));

  if (network->wifi != NULL)
    nm_device_disconnect_async (NM_DEVICE (network->wifi), NULL,
                                shopno_qs_network_disconnect_done, NULL);
}



const gchar *
shopno_qs_network_strength_icon (guint8 strength)
{
  if (strength > 80)
    return "network-wireless-signal-excellent-symbolic";
  if (strength > 55)
    return "network-wireless-signal-good-symbolic";
  if (strength > 30)
    return "network-wireless-signal-ok-symbolic";
  if (strength > 5)
    return "network-wireless-signal-weak-symbolic";

  return "network-wireless-signal-none-symbolic";
}



const gchar *
shopno_qs_network_get_icon_name (ShopnoQsNetwork *network)
{
  NMActiveConnection *primary;
  const gchar        *type;

  g_return_val_if_fail (SHOPNO_IS_QS_NETWORK (network), NULL);

  if (network->client == NULL || !nm_client_get_nm_running (network->client))
    return "network-offline-symbolic";

  if (network->connecting_ssid != NULL)
    return "network-wireless-acquiring-symbolic";

  primary = nm_client_get_primary_connection (network->client);
  if (primary == NULL)
    {
      if (network->wifi != NULL && !shopno_qs_network_get_wifi_enabled (network))
        return "network-wireless-disabled-symbolic";
      if (network->wifi != NULL)
        return "network-wireless-offline-symbolic";
      return "network-offline-symbolic";
    }

  type = nm_active_connection_get_connection_type (primary);
  if (g_strcmp0 (type, NM_SETTING_WIRELESS_SETTING_NAME) == 0 && network->active_ap != NULL)
    return shopno_qs_network_strength_icon (nm_access_point_get_strength (network->active_ap));

  if (g_strcmp0 (type, NM_SETTING_VPN_SETTING_NAME) == 0
      || g_strcmp0 (type, NM_SETTING_WIREGUARD_SETTING_NAME) == 0)
    return "network-vpn-symbolic";

  return "network-wired-symbolic";
}



void
shopno_qs_network_refresh (ShopnoQsNetwork *network)
{
  g_return_if_fail (SHOPNO_IS_QS_NETWORK (network));

  if (network->client == NULL)
    return;

  /* Read everything again and update now, not in an idle */
  shopno_qs_network_update_state (network);

  if (network->networks_id != 0)
    {
      g_source_remove (network->networks_id);
      network->networks_id = 0;
    }
  shopno_qs_network_emit_networks_changed (network);
}



ShopnoQsWiredState
shopno_qs_network_get_wired_state (ShopnoQsNetwork *network)
{
  ShopnoQsWiredState state = SHOPNO_QS_WIRED_NONE;

  g_return_val_if_fail (SHOPNO_IS_QS_NETWORK (network), SHOPNO_QS_WIRED_NONE);

  /* The furthest along of all ports */
  for (guint i = 0; i < network->wired->len; i++)
    {
      NMDeviceState      device_state = nm_device_get_state (g_ptr_array_index (network->wired, i));
      ShopnoQsWiredState this_state;

      if (device_state == NM_DEVICE_STATE_ACTIVATED)
        this_state = SHOPNO_QS_WIRED_CONNECTED;
      else if (device_state >= NM_DEVICE_STATE_PREPARE && device_state < NM_DEVICE_STATE_ACTIVATED)
        this_state = SHOPNO_QS_WIRED_CONNECTING;
      else if (device_state >= NM_DEVICE_STATE_DISCONNECTED)
        this_state = SHOPNO_QS_WIRED_DISCONNECTED;
      else if (device_state == NM_DEVICE_STATE_UNAVAILABLE)
        this_state = SHOPNO_QS_WIRED_UNPLUGGED;
      else
        continue;

      state = MAX (state, this_state);
    }

  return state;
}



void
shopno_qs_network_set_wired_connected (ShopnoQsNetwork *network,
                                       gboolean         connected)
{
  g_return_if_fail (SHOPNO_IS_QS_NETWORK (network));

  if (network->client == NULL)
    return;

  for (guint i = 0; i < network->wired->len; i++)
    {
      NMDevice     *device = g_ptr_array_index (network->wired, i);
      NMDeviceState state = nm_device_get_state (device);

      if (!connected)
        {
          /* NetworkManager then keeps the port down until asked again */
          if (state >= NM_DEVICE_STATE_PREPARE && state <= NM_DEVICE_STATE_ACTIVATED)
            nm_device_disconnect_async (device, NULL, shopno_qs_network_disconnect_done, NULL);
        }
      else if (state == NM_DEVICE_STATE_DISCONNECTED || state == NM_DEVICE_STATE_FAILED)
        {
          /* The best saved connection, or a new one if there is none */
          if (nm_device_get_available_connections (device)->len > 0)
            nm_client_activate_connection_async (network->client, NULL, device, NULL,
                                                 network->cancellable,
                                                 shopno_qs_network_wired_activated, NULL);
          else
            nm_client_add_and_activate_connection_async (network->client, NULL, device, NULL,
                                                         network->cancellable,
                                                         shopno_qs_network_wired_activated,
                                                         GINT_TO_POINTER (TRUE));
        }
    }
}
