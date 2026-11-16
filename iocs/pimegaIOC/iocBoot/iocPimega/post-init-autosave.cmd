# Post IOC initialization setup for autosave
#
# Configuration parameters:
#
# $(PREFIX)
# Prefix for all records
#
# $(MODEL_NAME)
# Detector model name, according to the list in pimega-template.cmd.

create_monitor_set("ioc.req", 30, "P=$(PREFIX), MODEL_NAME=$(MODEL_NAME)")
set_savefile_name("ioc.req", "$(PREFIX).sav")
