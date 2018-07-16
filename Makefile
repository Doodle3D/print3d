################################################
# OpenWrt Makefile for Doodle3D Print3D driver #
################################################
include $(TOPDIR)/rules.mk

PKG_NAME := print3d
PKG_VERSION := 0.1.1
PKG_RELEASE := 1

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

#include $(INCLUDE_DIR)/uclibc++.mk
include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

CMAKE_OPTIONS = -DLUAPATH=/usr/lib/lua

#Note: as mentioned in http://wiki.openwrt.org/doc/devel/dependencies, the inotifyd dep will not be checked on installation through opkg
define Package/print3d
	SECTION:=mods
	CATEGORY:=Doodle3D
	TITLE:=3D printer driver
	#DEPENDS:= +libuci +liblua +uclibcxx +kmod-usb-acm +kmod-usb-serial +kmod-usb-serial-ftdi +@BUSYBOX_CUSTOM +@BUSYBOX_CONFIG_INOTIFYD
	DEPENDS:= +libstdcpp +libuci +liblua +kmod-usb-acm +kmod-usb-serial +kmod-usb-serial-ftdi +@BUSYBOX_CUSTOM +@BUSYBOX_CONFIG_INOTIFYD
endef

define Package/print3d/description
	This package provides an abstracted 3D printing interface supporting many different brands and types of 3D printers.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Package/print3d/install
	$(INSTALL_DIR) $(1)/bin
	$(INSTALL_DIR) $(1)/usr/libexec
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_DIR) $(1)/usr/lib/lua
	
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/server/print3d $(1)/bin/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/frontends/cmdline/p3d $(1)/bin/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/frontends/lua/print3d.so $(1)/usr/lib/lua/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/frontends/lua/os_utils.so $(1)/usr/lib/lua/
	
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/script/print3d_init $(1)/etc/init.d/print3d
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/script/print3d-runner.sh $(1)/usr/libexec/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/script/print3d-new-device.sh $(1)/usr/libexec/
endef

define Package/print3d/postinst
	#!/bin/sh
	# check if we are on real system
	if [ -z "$${IPKG_INSTROOT}" ]; then
		echo 'Enabling rc.d symlink for print3d'
		$${IPKG_INSTROOT}/etc/init.d/print3d enable
		echo 'Start print3d service'
		$${IPKG_INSTROOT}/etc/init.d/print3d start
	fi
	exit 0
endef

define Package/print3d/prerm
	#!/bin/sh
	# check if we are on real system
	if [ -z "$${IPKG_INSTROOT}" ]; then
		echo 'Stop print3d service'
		$${IPKG_INSTROOT}/etc/init.d/print3d stop
		echo 'Disable print3d service'
		$${IPKG_INSTROOT}/etc/init.d/print3d disable
	fi
	exit 0
endef

$(eval $(call BuildPackage,print3d))
