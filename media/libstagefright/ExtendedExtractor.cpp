/*Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *      contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "ExtendedExtractor"
#include <utils/Log.h>
#include <dlfcn.h>  // for dlopen/dlclose
#include "include/ExtendedExtractor.h"

#if defined(ENABLE_AV_ENHANCEMENTS) || defined(QCOM_LEGACY_MMPARSER)

namespace android {

#ifdef QCOM_LEGACY_MMPARSER
static const char* EXTENDED_EXTRACTOR_LIB = "libmmparser.so";
static const char* EXTENDED_EXTRACTOR_CREATE = "createExtractor";
#else
static const char* EXTENDED_EXTRACTOR_LIB = "libExtendedExtractor.so";
static const char* EXTENDED_EXTRACTOR_CREATE = "CreateExtractor";
#endif
static const char* EXTENDED_EXTRACTOR_SNIFF = "SniffExtendedExtractor";

typedef MediaExtractor* (*ExtendedExtractorCreate)
                            (const sp<DataSource> &source, const char* mime);

typedef bool (*ExtendedExtractorSniff)
                 (const sp<DataSource> &source, String8 *mimeType,
                 float *confidence,sp<AMessage> *meta);

static void* loadExtendedExtractorLib() {
    static void* extendedExtractorLib = NULL;
    static bool alreadyTriedToLoadLib = false;

    if(!alreadyTriedToLoadLib) {
        alreadyTriedToLoadLib = true;

        extendedExtractorLib = ::dlopen(EXTENDED_EXTRACTOR_LIB, RTLD_LAZY);

        if(extendedExtractorLib == NULL) {
            ALOGV("Failed to load %s, dlerror = %s \n",
                EXTENDED_EXTRACTOR_LIB, dlerror());
        }
    }

    return extendedExtractorLib;
}

MediaExtractor* ExtendedExtractor::Create (
        const sp<DataSource> &source, const char *mime) {
    static ExtendedExtractorCreate create = NULL;
    static bool alreadyTriedToFindCreateFunction = false;
    MediaExtractor* extractor = NULL;

    if (!alreadyTriedToFindCreateFunction) {
        void *extendedExtractorLib = loadExtendedExtractorLib();

        if (extendedExtractorLib != NULL) {
            create = (ExtendedExtractorCreate) dlsym (
                    extendedExtractorLib, EXTENDED_EXTRACTOR_CREATE);
            alreadyTriedToFindCreateFunction = true;
        }
    }

    if (create == NULL) {
        ALOGE ("Failed to find symbol : %s, dlerror = %s",
            EXTENDED_EXTRACTOR_CREATE, dlerror());
        return NULL;
    }

    extractor = create (source, mime);
    if (extractor == NULL) {
        ALOGE("Failed to instantiate extractor \n");
    }

    return extractor;
}

#ifdef QCOM_LEGACY_MMPARSER
void ExtendedExtractor::RegisterSniffers() {
    void *extendedExtractorLib = loadExtendedExtractorLib();
    if (extendedExtractorLib == NULL) {
        return;
    }

    SnifferArrayFunc snifferArrayFunc = (SnifferArrayFunc) dlsym(
            extendedExtractorLib, MEDIA_SNIFFER_ARRAY);
    if(snifferArrayFunc==NULL) {
        ALOGE(" Unable to init Extended Sniffers, dlerror = %s \n", dlerror());
        return;
    }

    const DataSource::SnifferFunc *snifferArray = NULL;
    int snifferCount = 0;

    //Invoke function in libmmparser to return its array of sniffers.
    snifferArrayFunc(&snifferArray, &snifferCount);

    if(snifferArray==NULL) {
        ALOGE(" snifferArray is NULL \n");
        return;
    }

    bool flag = true;
    //Register the remote sniffers with the DataSource.
    for(int i=0; i<snifferCount; i++) {
        DataSource::RegisterSniffer_l(snifferArray[i], flag);
        flag = false;
    }
}
#endif

bool ExtendedExtractor::Sniff (
        const sp<DataSource> &source, String8 *mimeType,
        float *confidence,sp<AMessage> *meta) {
    void *extendedExtractorLib = loadExtendedExtractorLib();
    bool retVal = false;

    if (extendedExtractorLib != NULL) {
       ExtendedExtractorSniff sniff = (ExtendedExtractorSniff) dlsym (
               extendedExtractorLib, EXTENDED_EXTRACTOR_SNIFF);

        if (sniff == NULL) {
            ALOGE ("Failed to find symbol : %s, dlerror = %s",
                EXTENDED_EXTRACTOR_SNIFF, dlerror());
            return retVal;
        }

       retVal = sniff (source, mimeType, confidence, meta);

       if(!retVal) {
           ALOGV("Sniff Failed");
       }
    }
    return retVal;
}

}  // namespace android

#else //ENABLE_AV_ENHANCEMENTS

namespace android {

MediaExtractor* ExtendedExtractor::Create (
        const sp<DataSource> &source, const char *mime) {
    return NULL;
}
bool ExtendedExtractor::Sniff (
        const sp<DataSource> &source, String8 *mimeType,
        float *confidence,sp<AMessage> *meta) {
    *confidence = 0.0;
    return false;
}

}  // namespace android

#endif //ENABLE_AV_ENHANCEMENTS

