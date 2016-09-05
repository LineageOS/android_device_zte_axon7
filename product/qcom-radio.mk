# IRSC
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/configs/sec_config:system/etc/sec_config

# Permissions
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.telephony.gsm.xml:system/etc/permissions/android.hardware.telephony.gsm.xml \
    frameworks/native/data/etc/android.software.sip.voip.xml:system/etc/permissions/android.software.sip.voip.xml

# RIL
PRODUCT_PACKAGES += \
    librmnetctl \
    libxml2

# RIL Properties
PRODUCT_PROPERTY_OVERRIDES += \
     DEVICE_PROVISIONED=1 \
     persist.data.iwlan.enable=true \
     persist.data.qmi.adb_logmask=0 \
     persist.dpm.feature=0 \
     persist.net.doxlat=true \
     persist.radio.add_power_save=1 \
     persist.radio.apm_sim_not_pwdn=1 \
     persist.radio.calls.on.ims=true \
     persist.radio.csvt.enabled=false \
     persist.radio.custom_ecc=1 \
     persist.radio.domain.ps=false \
     persist.radio.flexmap_type=disabled \
     persist.radio.hw_mbn_update=1 \
     persist.radio.jbims=1 \
     persist.radio.mt_sms_ack=20 \
     persist.radio.multisim.config=dsds \
     persist.radio.rat_on=combine \
     persist.radio.sap_silent_pin=true \
     persist.radio.sib16_support=1 \
     persist.radio.start_ota_daemon=1 \
     persist.radio.sw_mbn_openmkt=1 \
     persist.radio.sw_mbn_update=1 \
     persist.radio.sw_mbn_volte=1 \
     persist.rild.nitz_long_ons_0= \
     persist.rild.nitz_long_ons_1= \
     persist.rild.nitz_long_ons_2= \
     persist.rild.nitz_long_ons_3= \
     persist.rild.nitz_plmn= \
     persist.rild.nitz_short_ons_0= \
     persist.rild.nitz_short_ons_1= \
     persist.rild.nitz_short_ons_2= \
     persist.rild.nitz_short_ons_3= \
     rild.libpath=/vendor/lib64/libril-qc-qmi-1.so \
     ril.subscription.types=NV,RUIM \
     ro.telephony.call_ring.multiple=false \
     ro.telephony.default_network=10,10 \
     telephony.lteOnCdmaDevice=1

PRODUCT_PROPERTY_OVERRIDES += \
     persist.data.mode=concurrent \
     persist.data.netmgrd.qos.enable=true \
     ro.use_data_netmgrd=true

PRODUCT_PROPERTY_OVERRIDES += \
     persist.data.df.agg.dl_pkt=10 \
     persist.data.df.agg.dl_size=4096 \
     persist.data.df.dev_name=rmnet_usb0 \
     persist.data.df.dl_mode=5 \
     persist.data.df.iwlan_mux=9 \
     persist.data.df.mux_count=8 \
     persist.data.df.ul_mode=5 \
     persist.data.iwlan.enable=true \
     persist.data.qmi.adb_logmask=0 \
     persist.data.wda.enable=true \
     persist.rmnet.data.enable=true
