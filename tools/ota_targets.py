#
# Registers PlatformIO custom targets that drive tools/ota.sh, so firmware and/or
# the LittleFS image can be pushed over the air from the command line:
#
#   pio run -t ota        # build + upload firmware AND filesystem
#   pio run -t ota-fw     # build + upload firmware only
#   pio run -t ota-fs     # build + upload LittleFS image only
#
Import("env")

SH = env.subst("$PROJECT_DIR") + "/tools/ota.sh"

for name, arg, title in (
    ("ota",    "all", "OTA firmware + filesystem"),
    ("ota-fw", "fw",  "OTA firmware"),
    ("ota-fs", "fs",  "OTA filesystem (LittleFS image)"),
):
    env.AddCustomTarget(
        name=name,
        dependencies=None,
        actions=['"%s" %s' % (SH, arg)],
        title=title,
        description="Build and upload over the air via ElegantOTA",
    )