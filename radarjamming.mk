RADARJAMMING_VERSION:= 1.0.0
RADARJAMMING_SITE:= $(TOPDIR)/package/radarjamming
RADARJAMMING_SITE_METHOD=local
RADARJAMMING_INSTALL_TARGET=YES
RADARJAMMING_DEPENDENCIES = libiio 
 
define RADARJAMMING_BUILD_CMDS
	  $(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_LDFLAGS) \
                $(@D)/radarjamming.c $(@D)/ad9361.h  $(@D)/ad9361_baseband_auto_rate.c $(@D)/ad9361_calculate_rf_clock_chain.c $(@D)/ad9361_design_taps.c $(@D)/ad9361_fmcomms5_phase_sync.c $(@D)/ad9361_multichip_sync.c  $(@D)/internal_design_filter_cg.c    $(@D)/internal_design_filter_cg.h  $(@D)/internal_design_filter_cg_emxutil.c  $(@D)/internal_design_filter_cg_emxutil.h  $(@D)/internal_design_filter_cg_types.h  $(@D)/rt_defines.h  $(@D)/rtGetInf.c  $(@D)/rtGetInf.h  $(@D)/rtGetNaN.c   $(@D)/rtGetNaN.h  $(@D)/rt_nonfinite.c  $(@D)/rt_nonfinite.h   $(@D)/rtwtypes.h  -o $(@D)/radarjamming -lm -lpthread -liio 
endef
 
define RADARJAMMING_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/radarjamming $(TARGET_DIR)/bin
endef
 
define RADARJAMMING_PERMISSIONS
	/bin/radarjamming f 4755 0 0 - - - - - 
endef
 
$(eval $(generic-package))

