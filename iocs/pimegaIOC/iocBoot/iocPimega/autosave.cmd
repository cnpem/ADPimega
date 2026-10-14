# Save and restore setup
#
# Configuration parameters:
#
# $(PREFIX)
# Prefix for all records
#
# $(MODEL_NAME)
# Detector model name, according to the list in pimega-template.cmd.

save_restoreSet_status_prefix("$(PREFIX)")
save_restoreSet_Debug(0)

save_restoreSet_IncompleteSetsOk(1)
save_restoreSet_DatedBackupFiles(1)

save_restoreSet_NumSeqFiles(3)
save_restoreSet_SeqPeriodInSeconds(300)

set_savefile_path("/opt", "autosave")

set_pass0_restoreFile("$(PREFIX).sav")
set_pass1_restoreFile("$(PREFIX).sav")

set_requestfile_path(".")
set_requestfile_path(".", "pimega")
set_requestfile_path("$(CALC)", "calcApp/Db")
set_requestfile_path("$(ADCORE)", "ADApp/Db")
set_requestfile_path("$(ADPIMEGA)", "db")

afterIocRunning create_monitor_set("ioc.req", 30, "P=$(PREFIX), MODEL_NAME=$(MODEL_NAME)")
afterIocRunning set_savefile_name("ioc.req", "$(PREFIX).sav")
