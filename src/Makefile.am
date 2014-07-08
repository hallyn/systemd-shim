AM_CFLAGS = $(gio_CFLAGS)

systemd_imports = \
	macro.h		\
	util.h		\
	util.c		\
	virt.h		\
	virt.c

libexec_PROGRAMS = systemd-shim
systemd_shim_LDADD = $(gio_LIBS)
systemd_shim_SOURCES = \
	$(systemd_imports)	\
	unit.h			\
	unit.c			\
	ntp-unit.c		\
	power-unit.c		\
	scope-unit.c		\
	user-unit.c		\
	systemd-iface.h		\
	cgmanager.h		\
	systemd-shim.c

if ENABLE_CGMANAGER
AM_CPPFLAGS = -DHAVE_CGMANAGER
systemd_shim_SOURCES += cgmanager.c
systemd_shim_CFLAGS = \
	$(AM_CFLAGS) \
	$(CGMANAGER_CFLAGS)
systemd_shim_LDADD += \
	$(CGMANAGER_LIBS)
endif
