#include "FileUtils.h"
#include "RPXLoading.h"
#include "globals.h"
#include "utils/StringTools.h"
#include "utils/logger.h"
#include "version.h"
#include <content_redirection/redirection.h>
#include <coreinit/cache.h>
#include <coreinit/debug.h>
#include <coreinit/title.h>
#include <mocha/mocha.h>
#include <nn/act.h>
#include <romfs_dev.h>
#include <string>
#include <sysapp/title.h>
#include <wuhb_utils/utils.h>
#include <wums.h>

#define VERSION "v0.3.2"

WUMS_MODULE_EXPORT_NAME("homebrew_rpx_loader");
WUMS_USE_WUT_DEVOPTAB();

WUMS_DEPENDS_ON(homebrew_content_redirection);
WUMS_DEPENDS_ON(homebrew_wuhb_utils);
WUMS_DEPENDS_ON(homebrew_functionpatcher);

WUMS_INITIALIZE() {
    initLogging();

    if (FunctionPatcher_InitLibrary() != FUNCTION_PATCHER_RESULT_SUCCESS) {
        OSFatal("homebrew_rpx_loader: FunctionPatcher_InitLibrary failed");
    }

    DEBUG_FUNCTION_LINE("Patch functions");
    for (uint32_t i = 0; i < rpx_utils_function_replacements_size; i++) {
        bool wasPatched = false;
        if (FunctionPatcher_AddFunctionPatch(&rpx_utils_function_replacements[i], nullptr, &wasPatched) != FUNCTION_PATCHER_RESULT_SUCCESS || !wasPatched) {
            OSFatal("homebrew_rpx_loader: Failed to patch function");
        }
    }
    DEBUG_FUNCTION_LINE("Patch functions finished");
    gReplacementInfo = {};

    ContentRedirectionStatus error;
    if ((error = ContentRedirection_InitLibrary()) != CONTENT_REDIRECTION_RESULT_SUCCESS) {
        DEBUG_FUNCTION_LINE_ERR("Failed to init ContentRedirection. Error %d", error);
        OSFatal("Failed to init ContentRedirection.");
    }

    WUHBUtilsStatus error2;
    if ((error2 = WUHBUtils_InitLibrary()) != WUHB_UTILS_RESULT_SUCCESS) {
        DEBUG_FUNCTION_LINE_ERR("Failed to init WUHBUtils. Error %d", error2);
        OSFatal("Failed to init WUHBUtils.");
    }


    // Init libmocha
    MochaUtilsStatus error3;
    if ((error3 = Mocha_InitLibrary()) != MOCHA_RESULT_SUCCESS) {
        DEBUG_FUNCTION_LINE_ERR("Failed to init libmocha. Error %s", Mocha_GetStatusStr(error3));
        OSFatal("Failed to init libmocha. Make sure to use the latest version of MochaPayload");
    }

    deinitLogging();
}

WUMS_APPLICATION_ENDS() {
    RL_UnmountCurrentRunningBundle();

    gReplacementInfo.rpxReplacementInfo.isRPXReplaced = false;

    RPXLoadingCleanUp();

    deinitLogging();
}

WUMS_APPLICATION_STARTS() {
    uint32_t upid = OSGetUPID();
    if (upid != 2 && upid != 15) {
        return;
    }
    OSReport("Running RPXLoadingModule " VERSION VERSION_EXTRA "\n");
    initLogging();
    if (gReplacementInfo.rpxReplacementInfo.willRPXBeReplaced) {
        gReplacementInfo.rpxReplacementInfo.willRPXBeReplaced = false;
        gReplacementInfo.rpxReplacementInfo.isRPXReplaced     = true;
    }

    if (_SYSGetSystemApplicationTitleId(SYSTEM_APP_ID_HEALTH_AND_SAFETY) != OSGetTitleID()) {
        gReplacementInfo.lastFileLoaded[0] = '\0';
    }

    if (_SYSGetSystemApplicationTitleId(SYSTEM_APP_ID_HEALTH_AND_SAFETY) == OSGetTitleID() &&
        strlen(gReplacementInfo.contentReplacementInfo.bundleMountInformation.toMountPath) > 0) {
        uint32_t currentHash           = StringTools::hash(gReplacementInfo.contentReplacementInfo.bundleMountInformation.toMountPath);
        std::string shortNameSanitized = sanitizeName(gReplacementInfo.rpxReplacementInfo.metaInformation.shortname);

        nn::act::Initialize();
        nn::act::PersistentId persistentId = nn::act::GetPersistentId();
        nn::act::Finalize();

        std::string basePath = string_format("fs:/vol/external01/wiiu/apps/save/%08X", currentHash);
        if (!shortNameSanitized.empty()) {
            basePath += string_format(" (%s)", shortNameSanitized.c_str());
        }

        std::string common = basePath + "/common";
        std::string user   = basePath + string_format("/%08X", 0x80000000 | persistentId);

        CreateSubfolder(common.c_str());
        CreateSubfolder(user.c_str());
        DEBUG_FUNCTION_LINE("Created %s and %s", common.c_str(), user.c_str());

        if (romfsMount(WUHB_ROMFS_NAME, gReplacementInfo.contentReplacementInfo.bundleMountInformation.toMountPath, RomfsSource_FileDescriptor_CafeOS) == 0) {
            auto device = GetDeviceOpTab(WUHB_ROMFS_PATH);
            if (device == nullptr || strcmp(device->name, WUHB_ROMFS_NAME) != 0) {
                romfsUnmount(WUHB_ROMFS_NAME);
                gReplacementInfo.contentReplacementInfo.bundleMountInformation.isMounted = false;
                DEBUG_FUNCTION_LINE_ERR("DeviceOpTab for %s not found.", WUHB_ROMFS_PATH);
                return;
            }
            int outRes = -1;
            if (ContentRedirection_AddDevice(device, &outRes) != CONTENT_REDIRECTION_RESULT_SUCCESS || outRes < 0) {
                DEBUG_FUNCTION_LINE_ERR("Failed to AddDevice to ContentRedirection");
                romfsUnmount(WUHB_ROMFS_NAME);
                gReplacementInfo.contentReplacementInfo.bundleMountInformation.isMounted = false;
                return;
            }
            auto res = ContentRedirection_AddFSLayer(&contentLayerHandle,
                                                     "WUHB Content",
                                                     WUHB_ROMFS_CONTENT_PATH,
                                                     FS_LAYER_TYPE_CONTENT_REPLACE);
            if (res == CONTENT_REDIRECTION_RESULT_SUCCESS) {
                res = ContentRedirection_AddFSLayer(&saveLayerHandle,
                                                    "WUHB Save",
                                                    basePath.c_str(),
                                                    FS_LAYER_TYPE_SAVE_REPLACE);
                if (res == CONTENT_REDIRECTION_RESULT_SUCCESS) {
                    gReplacementInfo.contentReplacementInfo.bundleMountInformation.mountedPath[0] = '\0';
                    strncpy(gReplacementInfo.contentReplacementInfo.bundleMountInformation.mountedPath,
                            gReplacementInfo.contentReplacementInfo.bundleMountInformation.toMountPath,
                            sizeof(gReplacementInfo.contentReplacementInfo.bundleMountInformation.mountedPath));
                    DEBUG_FUNCTION_LINE_VERBOSE("Mounted %s to /vol/content", gReplacementInfo.contentReplacementInfo.bundleMountInformation.mountedPath);
                    gReplacementInfo.contentReplacementInfo.bundleMountInformation.isMounted      = true;
                    gReplacementInfo.contentReplacementInfo.bundleMountInformation.toMountPath[0] = '\0';

                    OSMemoryBarrier();
                    return;
                } else {
                    if (contentLayerHandle != 0) {
                        ContentRedirection_RemoveFSLayer(contentLayerHandle);
                        contentLayerHandle = 0;
                    }
                    DEBUG_FUNCTION_LINE_ERR("ContentRedirection_AddFSLayer had failed for save");
                }
            } else {
                int outRes = -1;
                if (ContentRedirection_RemoveDevice(WUHB_ROMFS_PATH, &outRes) != CONTENT_REDIRECTION_RESULT_SUCCESS || res < 0) {
                    DEBUG_FUNCTION_LINE_ERR("Failed to remove device");
                }
                romfsUnmount(WUHB_ROMFS_PATH);
                DEBUG_FUNCTION_LINE_ERR("ContentRedirection_AddFSLayer had failed for content");
            }
        }
        DEBUG_FUNCTION_LINE_ERR("Failed to mount %s", gReplacementInfo.contentReplacementInfo.bundleMountInformation.toMountPath);
        gReplacementInfo.contentReplacementInfo.bundleMountInformation.isMounted = false;
        OSMemoryBarrier();
    }
}
