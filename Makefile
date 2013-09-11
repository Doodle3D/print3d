################################################
# OpenWrt Makefile for Doodle3D Print3D driver #
################################################
include $(TOPDIR)/rules.mk

PKG_NAME := print3d
PKG_VERSION := 0.1.0
PKG_RELEASE := 1

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

CMAKE_OPTIONS = -DLUAPATH=/usr/lib/lua

define Package/print3d
	SECTION:=mods
	CATEGORY:=Doodle3D
	TITLE:=3D printer driver
	#DEPENDS:=+uclibcxx
	DEPENDS:=+uclibcxx +libstdcpp #fix to solve 'missing dependencies'
endef
define Package/usbconnectiontester/description
	This package provides an abstracted 3D printing interface supporting many different brands and types of 3D printers.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Package/print3d/install
	$(INSTALL_DIR) $(1)/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/server/print3d $(1)/bin/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/frontends/cmdline/p3d $(1)/bin/

endef

$(eval $(call BuildPackage,print3d))
