#include <jni.h>

#include "audio_studio/native_api.h"

#include <string>
#include <vector>

namespace {

std::string jstring_to_utf8(JNIEnv* env, jstring value) {
    if (value == nullptr) return {};
    const char* chars = env->GetStringUTFChars(value, nullptr);
    if (chars == nullptr) return {};
    std::string copy(chars);
    env->ReleaseStringUTFChars(value, chars);
    return copy;
}

jstring utf8_to_jstring(JNIEnv* env, const char* text) {
    return env->NewStringUTF(text != nullptr ? text : "");
}

}  // namespace

extern "C" JNIEXPORT jlong JNICALL
Java_com_innotrepid_audiostudio_SileroVadSession_nativeCreate(
    JNIEnv* env, jclass, jstring model_path, jint sample_rate, jint channels,
    jfloat speech_threshold, jlong minimum_speech_ms, jlong minimum_silence_ms,
    jlong padding_ms) {
    const std::string path = jstring_to_utf8(env, model_path);
    auto* handle = audio_studio_vad_create_utf8(
        path.c_str(), static_cast<uint32_t>(sample_rate),
        static_cast<uint32_t>(channels), speech_threshold,
        static_cast<uint64_t>(minimum_speech_ms),
        static_cast<uint64_t>(minimum_silence_ms),
        static_cast<uint64_t>(padding_ms));
    return reinterpret_cast<jlong>(handle);
}

extern "C" JNIEXPORT void JNICALL
Java_com_innotrepid_audiostudio_SileroVadSession_nativeDestroy(
    JNIEnv*, jclass, jlong handle) {
    audio_studio_vad_destroy(reinterpret_cast<AudioStudioVadHandle*>(handle));
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_innotrepid_audiostudio_SileroVadSession_nativeLastError(
    JNIEnv* env, jclass) {
    return utf8_to_jstring(env, audio_studio_vad_last_error());
}

extern "C" JNIEXPORT jint JNICALL
Java_com_innotrepid_audiostudio_SileroVadSession_nativePushAudio(
    JNIEnv* env, jclass, jlong handle, jfloatArray interleaved, jint frame_count,
    jlongArray out_starts_ms, jlongArray out_ends_ms, jintArray out_written) {
    if (handle == 0 || out_written == nullptr) return 0;
    const jsize capacity = out_starts_ms != nullptr ? env->GetArrayLength(out_starts_ms) : 0;
    if (out_ends_ms != nullptr && env->GetArrayLength(out_ends_ms) < capacity) return 0;

    std::vector<float> pcm;
    const float* pcm_ptr = nullptr;
    if (frame_count > 0) {
        if (interleaved == nullptr) return 0;
        const jsize samples = env->GetArrayLength(interleaved);
        if (samples <= 0) return 0;
        pcm.resize(static_cast<std::size_t>(samples));
        env->GetFloatArrayRegion(interleaved, 0, samples, pcm.data());
        pcm_ptr = pcm.data();
    }

    std::vector<AudioStudioSpeechRegion> regions(
        capacity > 0 ? static_cast<std::size_t>(capacity) : 0U);
    size_t written = 0;
    const int ok = audio_studio_vad_push_audio(
        reinterpret_cast<AudioStudioVadHandle*>(handle), pcm_ptr,
        static_cast<size_t>(frame_count),
        regions.empty() ? nullptr : regions.data(),
        static_cast<size_t>(capacity), &written);

    const jint written_i = static_cast<jint>(written > static_cast<size_t>(INT32_MAX)
                                                 ? INT32_MAX
                                                 : written);
    env->SetIntArrayRegion(out_written, 0, 1, &written_i);
    if (ok && written > 0 && out_starts_ms != nullptr && out_ends_ms != nullptr) {
        const jsize copy = static_cast<jsize>(
            written < static_cast<size_t>(capacity) ? written : static_cast<size_t>(capacity));
        std::vector<jlong> starts(static_cast<std::size_t>(copy));
        std::vector<jlong> ends(static_cast<std::size_t>(copy));
        for (jsize i = 0; i < copy; ++i) {
            starts[static_cast<std::size_t>(i)] =
                static_cast<jlong>(regions[static_cast<std::size_t>(i)].start_ms);
            ends[static_cast<std::size_t>(i)] =
                static_cast<jlong>(regions[static_cast<std::size_t>(i)].end_ms);
        }
        env->SetLongArrayRegion(out_starts_ms, 0, copy, starts.data());
        env->SetLongArrayRegion(out_ends_ms, 0, copy, ends.data());
    }
    return ok;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_innotrepid_audiostudio_SileroVadSession_nativeFlush(
    JNIEnv* env, jclass, jlong handle, jlongArray out_starts_ms,
    jlongArray out_ends_ms, jintArray out_written) {
    if (handle == 0 || out_written == nullptr) return 0;
    const jsize capacity = out_starts_ms != nullptr ? env->GetArrayLength(out_starts_ms) : 0;
    if (out_ends_ms != nullptr && env->GetArrayLength(out_ends_ms) < capacity) return 0;

    std::vector<AudioStudioSpeechRegion> regions(
        capacity > 0 ? static_cast<std::size_t>(capacity) : 0U);
    size_t written = 0;
    const int ok = audio_studio_vad_flush(
        reinterpret_cast<AudioStudioVadHandle*>(handle),
        regions.empty() ? nullptr : regions.data(),
        static_cast<size_t>(capacity), &written);

    const jint written_i = static_cast<jint>(written > static_cast<size_t>(INT32_MAX)
                                                 ? INT32_MAX
                                                 : written);
    env->SetIntArrayRegion(out_written, 0, 1, &written_i);
    if (ok && written > 0 && out_starts_ms != nullptr && out_ends_ms != nullptr) {
        const jsize copy = static_cast<jsize>(
            written < static_cast<size_t>(capacity) ? written : static_cast<size_t>(capacity));
        std::vector<jlong> starts(static_cast<std::size_t>(copy));
        std::vector<jlong> ends(static_cast<std::size_t>(copy));
        for (jsize i = 0; i < copy; ++i) {
            starts[static_cast<std::size_t>(i)] =
                static_cast<jlong>(regions[static_cast<std::size_t>(i)].start_ms);
            ends[static_cast<std::size_t>(i)] =
                static_cast<jlong>(regions[static_cast<std::size_t>(i)].end_ms);
        }
        env->SetLongArrayRegion(out_starts_ms, 0, copy, starts.data());
        env->SetLongArrayRegion(out_ends_ms, 0, copy, ends.data());
    }
    return ok;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_innotrepid_audiostudio_SileroVadSession_nativeReset(
    JNIEnv*, jclass, jlong handle) {
    return audio_studio_vad_reset(reinterpret_cast<AudioStudioVadHandle*>(handle));
}
