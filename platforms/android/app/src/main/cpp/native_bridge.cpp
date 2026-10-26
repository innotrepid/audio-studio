#include <jni.h>

#include "audio_studio/audio_types.hpp"

extern "C" JNIEXPORT jstring JNICALL
Java_com_innotrepid_audiostudio_MainActivity_nativeEngineStatus(
    JNIEnv* env,
    jobject /* thiz */) {
    const audio_studio::AudioFormat baseline{48000U, 1U};
    const bool core_ready = baseline.is_valid();
    const char* status = core_ready
        ? "Native C++ core connected · Oboe dependency linked"
        : "Native audio core failed its baseline validation";
    return env->NewStringUTF(status);
}
