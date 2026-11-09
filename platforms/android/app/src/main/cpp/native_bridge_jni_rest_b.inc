
extern "C" JNIEXPORT jstring JNICALL
Java_com_innotrepid_audiostudio_MainActivity_nativeRecoverPartialWav(
    JNIEnv* env, jobject /* thiz */, jstring partial_path, jstring output_path) {
    if (partial_path == nullptr || output_path == nullptr) {
        return env->NewStringUTF("ERROR: Recovery paths are required.");
    }
    const char* partial_chars = env->GetStringUTFChars(partial_path, nullptr);
    if (partial_chars == nullptr) return env->NewStringUTF("ERROR: Could not read temporary path.");
    const std::string partial(partial_chars);
    env->ReleaseStringUTFChars(partial_path, partial_chars);

    const char* output_chars = env->GetStringUTFChars(output_path, nullptr);
    if (output_chars == nullptr) return env->NewStringUTF("ERROR: Could not read destination path.");
    const std::string output(output_chars);
    env->ReleaseStringUTFChars(output_path, output_chars);

    std::string error;
    if (!audio_studio::recover_partial_wav(partial, output, &error)) {
        const std::string message = "ERROR: " + error;
        return env->NewStringUTF(message.c_str());
    }
    return env->NewStringUTF("RECOVERED");
}


extern "C" JNIEXPORT jstring JNICALL
Java_com_innotrepid_audiostudio_MainActivity_nativeTranscribe(
    JNIEnv* env, jobject /* thiz */, jstring model_path, jfloatArray pcm,
    jstring language, jboolean translate_to_english) {
    if (model_path == nullptr || pcm == nullptr || language == nullptr)
        return env->NewStringUTF(R"({"error":"Invalid transcription request"})");
    const char* model_chars = env->GetStringUTFChars(model_path, nullptr);
    const char* language_chars = env->GetStringUTFChars(language, nullptr);
    if (model_chars == nullptr || language_chars == nullptr) {
        if (model_chars != nullptr) env->ReleaseStringUTFChars(model_path, model_chars);
        if (language_chars != nullptr) env->ReleaseStringUTFChars(language, language_chars);
        return env->NewStringUTF(R"({"error":"Could not read transcription parameters"})");
    }
    audio_studio::TranscriptionRequest request{};
    request.model_path = model_chars;
    request.language = language_chars;
    request.translate_to_english = translate_to_english == JNI_TRUE;
    env->ReleaseStringUTFChars(model_path, model_chars);
    env->ReleaseStringUTFChars(language, language_chars);
    const jsize count = env->GetArrayLength(pcm);
    if (count <= 0) return env->NewStringUTF(R"({"error":"Audio is empty"})");
    request.mono_pcm_16khz.resize(static_cast<std::size_t>(count));
    env->GetFloatArrayRegion(pcm, 0, count, request.mono_pcm_16khz.data());
    if (env->ExceptionCheck()) return nullptr;
    const auto result = audio_studio::transcribe(request);
    if (!result) {
        // Fixed: open quote after "error": before json_escape (was ":) which broke JSON).
        const std::string payload =
            std::string(R"json({"error":")json") + json_escape(result.error_message) +
            R"json("})json";
        return env->NewStringUTF(payload.c_str());
    }
    // Fixed: string values must be quoted with " not closed with ).
    std::string payload =
        std::string(R"json({"text":")json") + json_escape(result.text) +
        R"json(","detectedLanguage":")json" + json_escape(result.detected_language) +
        R"json(","segments":[)json";
    for (std::size_t i = 0; i < result.segments.size(); ++i) {
        if (i != 0U) payload += ",";
        const auto& segment = result.segments[i];
        payload += R"json({"startMs":)json" + std::to_string(segment.start_ms) +
            R"json(,"endMs":)json" + std::to_string(segment.end_ms) +
            R"json(,"text":")json" + json_escape(segment.text) + R"json("})json";
    }
    payload += "]}";
    return env->NewStringUTF(payload.c_str());
}

extern "C" JNIEXPORT jfloatArray JNICALL
Java_com_innotrepid_audiostudio_MainActivity_nativeDecodeWavForTranscription(
    JNIEnv* env, jobject /* thiz */, jstring input_path) {
    if (input_path == nullptr) return nullptr;
    const char* chars = env->GetStringUTFChars(input_path, nullptr);
    if (chars == nullptr) return nullptr;
    const std::string path(chars);
    env->ReleaseStringUTFChars(input_path, chars);
    std::vector<float> mono;
    std::string error;
    // Keep mobile allocations bounded: two hours at 16 kHz is ~460 MiB.
    constexpr std::uint64_t kMaxSamples = 2ULL * 60ULL * 60ULL * 16000ULL;
    if (!audio_studio::load_wav_for_transcription(path, mono, &error, kMaxSamples)) {
        const std::string message = "WAV transcription decode failed: " + error;
        jclass exception_class = env->FindClass("java/lang/IllegalArgumentException");
        if (exception_class != nullptr) env->ThrowNew(exception_class, message.c_str());
        return nullptr;
    }
    if (mono.size() > static_cast<std::size_t>(std::numeric_limits<jsize>::max())) {
        jclass exception_class = env->FindClass("java/lang/IllegalArgumentException");
        if (exception_class != nullptr) env->ThrowNew(exception_class, "Decoded WAV exceeds Android array limits.");
        return nullptr;
    }
    jfloatArray result = env->NewFloatArray(static_cast<jsize>(mono.size()));
    if (result != nullptr && !mono.empty()) {
        env->SetFloatArrayRegion(result, 0, static_cast<jsize>(mono.size()), mono.data());
    }
    return result;
}
