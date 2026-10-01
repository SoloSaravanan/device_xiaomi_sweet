LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := SweetDebloatOverrides
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .marker
LOCAL_SRC_FILES := debloat_overrides.marker
LOCAL_UNINSTALLABLE_MODULE := true

# Build-time package filter metadata; this module is not installed.
LOCAL_OVERRIDES_MODULES := \
    FM2 \
    PhotoTable \
    LocalContactsBackup \
    QuickAccessWallet \
    LMOFreeform \
    LMOFreeformSidebar \
    BasicDreams \
    BluetoothMidiService \
    Stk \
    WallpaperBackup \
    CallLogBackup \
    CellBroadcastLegacyApp \
    DeviceDiagnostics \
    LiveWallpapersPicker \
    Tag \
    BtHelper \
    BtHelperAdapter \
    EmergencyInfo \
    Updater \
    AccessibilityMenu \
    OmniJaws \
    Gramophone \
    Jellyfish \
    ANGLE \
    AppSearchAiSealConfig \
    SimAppDialog \
    BackupRestoreConfirmation \
    Datura \
    FaceUnlock \
    GameSpace \
    Seedvault \
    VoltageSetupWizard \
    SoterService
include $(BUILD_PREBUILT)
