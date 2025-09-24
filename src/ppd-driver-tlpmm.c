/*
 * Copyright (c) 2020 Bastien Nocera <hadess@hadess.net>
 * Copyright (c) 2023 Rong Zhang <i@rong.moe>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3 as published by
 * the Free Software Foundation.
 *
 */
#define G_LOG_DOMAIN "PlatformDriver"
#include "ppd-utils.h"
#include "ppd-driver-tlpmm.h"


static gboolean tlpmm_get_cmd_path(gchar *cmd_name,gchar **cmd_result) {
  g_autofree gchar *stdout_buf = NULL;
  g_autofree gchar *stderr_buf = NULL;
  g_autoptr(GError) internal_error = NULL;
  g_autofree gchar *cmd_line = g_strconcat("which ",cmd_name,NULL);
  gint exit_status;
  gboolean success = g_spawn_command_line_sync(
    cmd_line,
    &stdout_buf,
    &stderr_buf,
    &exit_status,
    &internal_error
  );

  if ((!success) || (exit_status != 0)) {
    return FALSE;
  }
  *cmd_result = g_strdup(g_strstrip(stdout_buf));
  return TRUE;
}

static gboolean tlpmm_get_profile_base(gchar *TLPMM_CTL_PATH , gchar *mode , gchar **cmd_result) {
  g_autofree gchar *stdout_buf = NULL;
  g_autofree gchar *stderr_buf = NULL;
  g_autoptr(GError) internal_error = NULL;
  g_autofree gchar *cmd_line = g_strconcat(TLPMM_CTL_PATH," ",mode,NULL);
  gint exit_status;
  gboolean success = g_spawn_command_line_sync(
    cmd_line,
    &stdout_buf,
    &stderr_buf,
    &exit_status,
    &internal_error
  );
  if ((!success) || (exit_status != 0)) {
    return FALSE;
  }
  *cmd_result = g_strdup(g_strstrip(stdout_buf));
  return TRUE;
}

static gboolean tlpmm_get_profile_default(gchar *TLPMM_CTL_PATH , gchar **cmd_result) {
  return tlpmm_get_profile_base(TLPMM_CTL_PATH,"getdefault",cmd_result);
}

static gboolean tlpmm_get_profile_active(gchar *TLPMM_CTL_PATH , gchar **cmd_result) {
  g_autofree gchar *result1 = NULL;
  if ( !tlpmm_get_profile_base(TLPMM_CTL_PATH,"get",&result1) ) {
    return FALSE;
  }
  if (g_str_equal(result1,"default")) {
    return tlpmm_get_profile_default(TLPMM_CTL_PATH,cmd_result);
  }
  else{
    *cmd_result = g_strdup(result1);
    return TRUE;
  }
  return tlpmm_get_profile_base(TLPMM_CTL_PATH,"getdefault",cmd_result);
}

struct _PpdDriverTlpmm
{
    PpdDriverPlatform   parent_instance;

    PpdProfile activated_profile;
    gboolean initialized;
};

G_DEFINE_TYPE (PpdDriverTlpmm, ppd_driver_tlpmm, PPD_TYPE_DRIVER_PLATFORM)

static gboolean ppd_driver_tlpmm_activate_profile (PpdDriver                   *driver,
                                                 PpdProfile                   profile,
                                                 PpdProfileActivationReason   reason,
                                                 GError                     **error);

static GObject*
ppd_driver_tlpmm_constructor (GType                  type,
                            guint                  n_construct_params,
                            GObjectConstructParam *construct_params)
{
    GObject *object;

    object = G_OBJECT_CLASS (ppd_driver_tlpmm_parent_class)->constructor (type,
                                                                        n_construct_params,
                                                                        construct_params);
    g_object_set (object,
                  "driver-name", "tlp-multimode",
                  "profiles", PPD_PROFILE_PERFORMANCE | PPD_PROFILE_BALANCED | PPD_PROFILE_POWER_SAVER,
                  NULL);

    return object;
}

static PpdProfile
read_tlpmm_profile (void)
{
    PpdProfile new_profile = PPD_PROFILE_UNSET;
    g_autofree gchar *TLPMM_CTL_PATH = NULL;
    g_autofree gchar *TLPMM_ACTIVE_PROFILE = NULL;
    if ( !tlpmm_get_cmd_path("tlp-multimode-ctl",&TLPMM_CTL_PATH) ){
      g_debug ("Failed to get contents for %s",TLPMM_CTL_PATH);
      return PPD_PROFILE_UNSET;
    }
    if ( !tlpmm_get_profile_active(TLPMM_CTL_PATH,&TLPMM_ACTIVE_PROFILE) ){
      g_debug ("Failed to get contents for %s","TLPMM_ACTIVE_PROFILE");
      return PPD_PROFILE_UNSET;
    }
    if (g_str_equal(TLPMM_ACTIVE_PROFILE,"balanced")){
      new_profile = PPD_PROFILE_BALANCED;
    }
    else if (g_str_equal(TLPMM_ACTIVE_PROFILE,"power-saver")){
      new_profile = PPD_PROFILE_POWER_SAVER;
    }
    else if (g_str_equal(TLPMM_ACTIVE_PROFILE,"performance")){
      new_profile = PPD_PROFILE_PERFORMANCE;
    }
    g_debug ("Detect tlp-multimode profile as PPD profile: %s\n",ppd_profile_to_str(new_profile));
    return new_profile;
}

static const char *
profile_to_tlpmm_subcommand (PpdProfile profile)
{
    switch (profile) {
        case PPD_PROFILE_POWER_SAVER:
            return "set power-saver";
        case PPD_PROFILE_BALANCED:
            return "set balanced";
        case PPD_PROFILE_PERFORMANCE:
            return "set performance";
    }

    g_assert_not_reached ();
}

static gboolean
call_tlpmm (const char  *subcommand,
          GError     **error)
{
    gboolean ret = TRUE;
    g_autofree char *cmd = NULL;
    g_autoptr(GError) internal_error = NULL;
    g_autofree gchar *TLPMM_CTL_PATH;
    if ( !tlpmm_get_cmd_path("tlp-multimode-ctl",&TLPMM_CTL_PATH) ){
      g_debug ("Failed to get contents for %s",TLPMM_CTL_PATH);
      return FALSE;
    }
    cmd = g_strdup_printf ("%s %s", TLPMM_CTL_PATH, subcommand);
    g_debug ("Executing '%s'", cmd);
    if (!g_spawn_command_line_sync (cmd,
                                    NULL,
                                    NULL,
                                    NULL,
                                    &internal_error)) {
        g_warning ("Failed to execute '%s': %s",
                   cmd,
                   internal_error->message);
        ret = FALSE;
        g_propagate_error (error, internal_error);
    }

    return ret;
}

static PpdProbeResult
probe_tlpmm (PpdDriverTlpmm *tlpmm)
{
    g_autofree gchar *TLPMM_CTL_PATH = NULL;
    if ( !tlpmm_get_cmd_path("tlp-multimode-ctl",&TLPMM_CTL_PATH) ){
      g_debug ("tlp-multimode is not installed, %s","tlp-multimode-ctl not detected");
      return PPD_PROBE_RESULT_FAIL;
    }
    return PPD_PROBE_RESULT_SUCCESS;
}

static PpdProbeResult
ppd_driver_tlpmm_probe (PpdDriver  *driver)
{
    PpdDriverTlpmm *tlpmm = PPD_DRIVER_TLPMM (driver);
    PpdProbeResult ret = PPD_PROBE_RESULT_FAIL;
    PpdProfile new_profile;

    ret = probe_tlpmm (tlpmm);

    if (ret != PPD_PROBE_RESULT_SUCCESS)
        goto out;

    new_profile = read_tlpmm_profile ();
    tlpmm->activated_profile = new_profile;
    tlpmm->initialized = new_profile != PPD_PROFILE_UNSET;

    if (!tlpmm->initialized) {
        /*
        call_tlpmm ("init start", NULL);
        new_profile = read_tlpmm_profile ();
        tlpmm->activated_profile = new_profile;
        tlpmm->initialized = TRUE;
         */
        g_warning ("tlp-multimode not initialized, try initializing now");
        gboolean ret_tmp = call_tlpmm("default",NULL);
        if (ret_tmp) {
          ret = PPD_PROBE_RESULT_SUCCESS;
        }
        else{
          ret = PPD_PROBE_RESULT_FAIL;
        }
    }

    out:
    g_debug ("%s tlp-multimode",
             ret == PPD_PROBE_RESULT_SUCCESS ? "Found" : "Didn't find");
    return ret;
}

static gboolean
ppd_driver_tlpmm_activate_profile (PpdDriver                    *driver,
                                 PpdProfile                   profile,
                                 PpdProfileActivationReason   reason,
                                 GError                     **error)
{
    PpdDriverTlpmm *tlpmm = PPD_DRIVER_TLPMM (driver);
    gboolean ret = FALSE;
    const char *subcommand;

    g_return_val_if_fail (tlpmm->initialized, FALSE);

    if (tlpmm->initialized) {
        subcommand = profile_to_tlpmm_subcommand (profile);
        ret = call_tlpmm (subcommand, error);
        if (!ret)
            return ret;
    }

    if (ret)
        tlpmm->activated_profile = profile;

    return ret;
}

static void
ppd_driver_tlpmm_finalize (GObject *object)
{
    G_OBJECT_CLASS (ppd_driver_tlpmm_parent_class)->finalize (object);
}

static void
ppd_driver_tlpmm_class_init (PpdDriverTlpmmClass *klass)
{
    GObjectClass *object_class;
    PpdDriverClass *driver_class;

    object_class = G_OBJECT_CLASS(klass);
    object_class->constructor = ppd_driver_tlpmm_constructor;
    object_class->finalize = ppd_driver_tlpmm_finalize;

    driver_class = PPD_DRIVER_CLASS(klass);
    driver_class->probe = ppd_driver_tlpmm_probe;
    driver_class->activate_profile = ppd_driver_tlpmm_activate_profile;
}

static void
ppd_driver_tlpmm_init (PpdDriverTlpmm *self)
{
}
