import obspython as obs


def script_description():
    return (
        "Deprecated. Aitum Multistream sync is provided by the "
        "aitum-dock-sync OBS plugin. Remove this script from OBS."
    )


def script_load(settings):
    obs.script_log(
        obs.LOG_WARNING,
        "[aitum-sync] Deprecated. Use the aitum-dock-sync plugin and remove this script.",
    )
