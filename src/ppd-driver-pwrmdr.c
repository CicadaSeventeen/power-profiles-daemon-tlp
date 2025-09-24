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
#include "ppd-driver-pwrmdr.h"


static gboolean pwrmdr_get_cmd_path(gchar *cmd_name,gchar **cmd_result) {
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

static gboolean pwrmdr_get_profile_base(gchar *PWRMDR_CTL_PATH , gchar *mode , gchar **cmd_result) {
  g_autofree gchar *stdout_buf = NULL;
  g_autofree gchar *stderr_buf = NULL;
  g_autoptr(GError) internal_error = NULL;
  g_autofree gchar *cmd_line = g_strconcat(PWRMDR_CTL_PATH," ",mode,NULL);
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

static gboolean pwrmdr_get_profile_default(gchar *PWRMDR_CTL_PATH , gchar **cmd_result) {
  return pwrmdr_get_profile_base(PWRMDR_CTL_PATH,"getdefault",cmd_result);
}

static gboolean pwrmdr_get_profile_active(gchar *PWRMDR_CTL_PATH , gchar **cmd_result) {
  g_autofree gchar *result1 = NULL;
  if ( !pwrmdr_get_profile_base(PWRMDR_CTL_PATH,"get",&result1) ) {
    return FALSE;
  }
  if (g_str_equal(result1,"default")) {
    return pwrmdr_get_profile_default(PWRMDR_CTL_PATH,cmd_result);
  }
  else{
    *cmd_result = g_strdup(result1);
    return TRUE;
  }
  return pwrmdr_get_profile_base(PWRMDR_CTL_PATH,"getdefault",cmd_result);
}

struct _PpdDriverPwrmdr
{
    PpdDriverPlatform   parent_instance;

    PpdProfile activated_profile;
    gboolean initialized;
};

G_DEFINE_TYPE (PpdDriverPwrmdr, ppd_driver_pwrmdr, PPD_TYPE_DRIVER_PLATFORM)

static gboolean ppd_driver_pwrmdr_activate_profile (PpdDriver                   *driver,
                                                 PpdProfile                   profile,
                                                 PpdProfileActivationReason   reason,
                                                 GError                     **error);

static GObject*
ppd_driver_pwrmdr_constructor (GType                  type,
                            guint                  n_construct_params,
                            GObjectConstructParam *construct_params)
{
    GObject *object;

    object = G_OBJECT_CLASS (ppd_driver_pwrmdr_parent_class)->constructor (type,
                                                                        n_construct_params,
                                                                        construct_params);
    g_object_set (object,
                  "driver-name", "powermoder",
                  "profiles", PPD_PROFILE_PERFORMANCE | PPD_PROFILE_BALANCED | PPD_PROFILE_POWER_SAVER,
                  NULL);

    return object;
}

static PpdProfile
read_pwrmdr_profile (void)
{
    PpdProfile new_profile = PPD_PROFILE_UNSET;
    g_autofree gchar *PWRMDR_CTL_PATH = NULL;
    g_autofree gchar *PWRMDR_ACTIVE_PROFILE = NULL;
    if ( !pwrmdr_get_cmd_path("powermoderctl",&PWRMDR_CTL_PATH) ){
      g_debug ("Failed to get contents for %s",PWRMDR_CTL_PATH);
      return PPD_PROFILE_UNSET;
    }
    if ( !pwrmdr_get_profile_active(PWRMDR_CTL_PATH,&PWRMDR_ACTIVE_PROFILE) ){
      g_debug ("Failed to get contents for %s","PWRMDR_ACTIVE_PROFILE");
      return PPD_PROFILE_UNSET;
    }
    if (g_str_equal(PWRMDR_ACTIVE_PROFILE,"balanced")){
      new_profile = PPD_PROFILE_BALANCED;
    }
    else if (g_str_equal(PWRMDR_ACTIVE_PROFILE,"power-saver")){
      new_profile = PPD_PROFILE_POWER_SAVER;
    }
    else if (g_str_equal(PWRMDR_ACTIVE_PROFILE,"performance")){
      new_profile = PPD_PROFILE_PERFORMANCE;
    }
    g_debug ("Detect powermoder profile as PPD profile: %s\n",ppd_profile_to_str(new_profile));
    return new_profile;
}

static const char *
profile_to_pwrmdr_subcommand (PpdProfile profile)
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
call_pwrmdr (const char  *subcommand,
          GError     **error)
{
    gboolean ret = TRUE;
    g_autofree char *cmd = NULL;
    g_autoptr(GError) internal_error = NULL;
    g_autofree gchar *PWRMDR_CTL_PATH;
    if ( !pwrmdr_get_cmd_path("powermoderctl",&PWRMDR_CTL_PATH) ){
      g_debug ("Failed to get contents for %s",PWRMDR_CTL_PATH);
      return FALSE;
    }
    cmd = g_strdup_printf ("%s %s", PWRMDR_CTL_PATH, subcommand);
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
probe_pwrmdr (PpdDriverPwrmdr *pwrmdr)
{
    g_autofree gchar *PWRMDR_CTL_PATH = NULL;
    if ( !pwrmdr_get_cmd_path("powermoderctl",&PWRMDR_CTL_PATH) ){
      g_debug ("powermoder is not installed, %s","powermoderctl not detected");
      return PPD_PROBE_RESULT_FAIL;
    }
    return PPD_PROBE_RESULT_SUCCESS;
}

static PpdProbeResult
ppd_driver_pwrmdr_probe (PpdDriver  *driver)
{
    PpdDriverPwrmdr *pwrmdr = PPD_DRIVER_PWRMDR (driver);
    PpdProbeResult ret = PPD_PROBE_RESULT_FAIL;
    PpdProfile new_profile;

    ret = probe_pwrmdr (pwrmdr);

    if (ret != PPD_PROBE_RESULT_SUCCESS)
        goto out;

    new_profile = read_pwrmdr_profile ();
    pwrmdr->activated_profile = new_profile;
    pwrmdr->initialized = new_profile != PPD_PROFILE_UNSET;

    if (!pwrmdr->initialized) {
        /*
        call_pwrmdr ("init start", NULL);
        new_profile = read_pwrmdr_profile ();
        pwrmdr->activated_profile = new_profile;
        pwrmdr->initialized = TRUE;
         */
        g_warning ("powermoder not initialized, try initializing now");
        gboolean ret_tmp = call_pwrmdr("default",NULL);
        if (ret_tmp) {
          ret = PPD_PROBE_RESULT_SUCCESS;
        }
        else{
          ret = PPD_PROBE_RESULT_FAIL;
        }
    }

    out:
    g_debug ("%s powermoder",
             ret == PPD_PROBE_RESULT_SUCCESS ? "Found" : "Didn't find");
    return ret;
}

static gboolean
ppd_driver_pwrmdr_activate_profile (PpdDriver                    *driver,
                                 PpdProfile                   profile,
                                 PpdProfileActivationReason   reason,
                                 GError                     **error)
{
    PpdDriverPwrmdr *pwrmdr = PPD_DRIVER_PWRMDR (driver);
    gboolean ret = FALSE;
    const char *subcommand;

    g_return_val_if_fail (pwrmdr->initialized, FALSE);

    if (pwrmdr->initialized) {
        subcommand = profile_to_pwrmdr_subcommand (profile);
        ret = call_pwrmdr (subcommand, error);
        if (!ret)
            return ret;
    }

    if (ret)
        pwrmdr->activated_profile = profile;

    return ret;
}

static void
ppd_driver_pwrmdr_finalize (GObject *object)
{
    G_OBJECT_CLASS (ppd_driver_pwrmdr_parent_class)->finalize (object);
}

static void
ppd_driver_pwrmdr_class_init (PpdDriverPwrmdrClass *klass)
{
    GObjectClass *object_class;
    PpdDriverClass *driver_class;

    object_class = G_OBJECT_CLASS(klass);
    object_class->constructor = ppd_driver_pwrmdr_constructor;
    object_class->finalize = ppd_driver_pwrmdr_finalize;

    driver_class = PPD_DRIVER_CLASS(klass);
    driver_class->probe = ppd_driver_pwrmdr_probe;
    driver_class->activate_profile = ppd_driver_pwrmdr_activate_profile;
}

static void
ppd_driver_pwrmdr_init (PpdDriverPwrmdr *self)
{
}
