#
# Copyright (C) 2011 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=asterisk16-chan-datacard
PKG_REV:=310
PKG_VERSION:=1.0.r$(PKG_REV)
PKG_RELEASE:=198

#PKG_SOURCE_URL:=http://datacard.googlecode.com/svn/trunk/
#PKG_SOURCE_VERSION:=$(PKG_RELEASE)
#PKG_SOURCE_PROTO:=svn
# or 
PKG_SOURCE:=chan_datacard-$(PKG_VERSION).tgz
PKG_SOURCE_URL=http://datacard.googlecode.com/files/
PKG_MD5SUM:=8cff8433f8f3ff67bd1f19d0384671f1

PKG_SOURCE_SUBDIR:=chan_datacard-$(PKG_VERSION)
PKG_BUILD_DIR=$(BUILD_DIR)/$(PKG_SOURCE_SUBDIR)

WITH_ASTERISK=asterisk-1.6.2.14

include $(INCLUDE_DIR)/package.mk

define Package/asterisk16-chan-datacard
	SUBMENU:=asterisk16 (Complete Open Source PBX), v1.6.x
	SECTION:=net
	CATEGORY:=Network
	URL:=http://www.asterisk.org/
	MAINTAINER:=Hans Zandbelt <hans.zandbelt@gmail.com>
	DEPENDS:= +asterisk16 +libiconv-full
	TITLE:=Huawei UMTS 3G datacards support
endef

define Package/asterisk16-chan-datacard/description
	Asterisk channel driver for Huawei UMTS 3G datacards
endef

MAKE_ARGS:= \
	CC="$(TARGET_CC)" \
	LD="$(TARGET_CC)" \
	CFLAGS="$(TARGET_CFLAGS) -DLOW_MEMORY $(TARGET_CPPFLAGS) -I$(STAGING_DIR)/usr/lib/libiconv-full/include -I$(BUILD_DIR)/$(WITH_ASTERISK)/include -DHAVE_CONFIG_H -I. -fPIC" \
	LDFLAGS="$(TARGET_LDFLAGS)" \
	DESTDIR="$(PKG_INSTALL_DIR)/usr/lib/asterisk/modules"

# for svn 
#        (cd $(PKG_BUILD_DIR); rm -rf config.{cache,status}; \
#                        autoconf \
#                                );
#        $(call Build/Configure/Default)

define Build/Configure
	$(call Build/Configure/Default, \
	    --with-asterisk=$(BUILD_DIR)/$(WITH_ASTERISK)/include \
	    $(MAKE_ARGS) \
	)
endef

define Build/Compile
	mkdir -p $(PKG_INSTALL_DIR)/usr/lib/asterisk/modules
	$(MAKE) -C "$(PKG_BUILD_DIR)" $(MAKE_ARGS) all install
endef

define Package/asterisk16-chan-datacard/conffiles
/etc/asterisk/datacard.conf
endef

define Package/asterisk16-chan-datacard/install
	$(INSTALL_DIR) $(1)/etc/asterisk
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/etc/datacard.conf $(1)/etc/asterisk/
	$(INSTALL_DIR) $(1)/usr/lib/asterisk/modules
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/lib/asterisk/modules/chan_datacard.so $(1)/usr/lib/asterisk/modules/
endef

$(eval $(call BuildPackage,asterisk16-chan-datacard))
