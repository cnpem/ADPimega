# Save and restore setup
#
# Configuration parameters:
#
# $(PREFIX)
# Prefix for all records
#
# $(MODEL_NAME)
# Detector model name, according to the list in pimega-template.cmd.
#
# Optional parameters:
#
# $(AUTOSAVE_PATH), default "/var/opt/adpimega"
# Path to store the autosave SAV files. The path must exist in the filesystem.

save_restoreSet_Debug(0)

save_restoreSet_IncompleteSetsOk(1)
save_restoreSet_DatedBackupFiles(1)

save_restoreSet_NumSeqFiles(3)
save_restoreSet_SeqPeriodInSeconds(300)

set_savefile_path("$(AUTOSAVE_PATH=/var/opt/adpimega)", "")

set_pass0_restoreFile("$(PREFIX).sav")
set_pass1_restoreFile("$(PREFIX).sav")

set_requestfile_path(".")
set_requestfile_path(".", "pimega")
set_requestfile_path("$(CALC)", "calcApp/Db")
set_requestfile_path("$(ADCORE)", "ADApp/Db")
set_requestfile_path("$(ADPIMEGA)", "db")

save_restoreSet_status_prefix("$(PREFIX)")
dbLoadRecords("$(AUTOSAVE)/db/save_restoreStatus.db", "P=$(PREFIX)")

afterIocRunning "create_monitor_set('ioc.req', 30, 'P=$(PREFIX), MODEL_NAME=$(MODEL_NAME)')"
afterIocRunning "set_savefile_name('ioc.req', '$(PREFIX).sav')"
