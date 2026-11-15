TEMPLATE = subdirs
SUBDIRS += daemon app
app.depends = daemon
