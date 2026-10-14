TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

#----------------------------------------------------
#  Optimization of db files using dbst (DEFAULT: NO)
#DB_OPT = YES

#----------------------------------------------------
# Create and install (or just install)
# databases, templates, substitutions like this

DB += module.template
DB += pimega.template
DB += pimega.req

DB += pimega45d.db
DB += pimega45d.req
DB += pimega135dl.db
DB += pimega135dl.req
DB += pimega135d.db
DB += pimega135d.req
DB += pimega540d.db
DB += pimega540d.req
DB += pimega450d.db
DB += pimega450d.req
DB += pimega450ds.db
DB += pimega450ds.req

#----------------------------------------------------
# If <anyname>.db template is not named <anyname>*.template add
# <anyname>_template = <templatename>

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

