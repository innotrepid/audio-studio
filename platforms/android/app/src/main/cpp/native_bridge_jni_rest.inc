
extern "C" JNIEXPORT void JNICALL
Java_com_innotrepid_audiostudio_MainActivity_nativePrepareNoiseGateProcessing(
    JNIEnv* /* env */, jobject /* thiz */) {
    g_cancel_noise_gate.store(false, std::memory_order_release);
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_innotrepid_audiostudio_MainActivity_nativeProcessNoiseGate(
    JNIEnv* env, jobject /* thiz */, jstring input_path, jstring output_path,
    jfloat threshold_dbfs, jfloat attenuation_db, jfloat gain_db, jfloat high_pass_hz,
    jint processing_engine, jfloat denoiser_strength, jstring model_directory, jobject progress_callback) {
    if (input_path == nullptr || output_path == nullptr) {
        return env->NewStringUTF("ERROR: Input and output paths are required.");
    }
    const char* input_chars = env->GetStringUTFChars(input_path, nullptr);
    if (input_chars == nullptr) return env->NewStringUTF("ERROR: Could not read input path.");
    const std::string input(input_chars);
    env->ReleaseStringUTFChars(input_path, input_chars);

    const char* output_chars = env->GetStringUTFChars(output_path, nullptr);
    if (output_chars == nullptr) return env->NewStringUTF("ERROR: Could not read output path.");
    const std::string output(output_chars);
    env->ReleaseStringUTFChars(output_path, output_chars);

    std::string model_dir;
    if (model_directory != nullptr) {
        const char* model_chars = env->GetStringUTFChars(model_directory, nullptr);
        if (model_chars == nullptr) return env->NewStringUTF("ERROR: Could not read model directory.");
        model_dir.assign(model_chars);
        env->ReleaseStringUTFChars(model_directory, model_chars);
    }

    jclass callback_class = progress_callback == nullptr ? nullptr : env->GetObjectClass(progress_callback);
    jmethodID progress_method = callback_class == nullptr ? nullptr :
        env->GetMethodID(callback_class, "onProgress", "(JJ)V");

    audio_studio::NoiseGateSettings settings{};
    settings.threshold_dbfs = threshold_dbfs;
    settings.attenuation_db = attenuation_db;

    std::string error;
    const auto progress = [env, progress_callback, progress_method](std::uint64_t processed, std::uint64_t total) {
        if (progress_callback != nullptr && progress_method != nullptr) {
            env->CallVoidMethod(progress_callback, progress_method,
                                static_cast<jlong>(processed), static_cast<jlong>(total));
        }
    };
    if (!audio_studio::process_wav_with_noise_gate(
            input, output, settings, &error, &g_cancel_noise_gate, progress,
            audio_studio::DspEffectsSettings{gain_db, high_pass_hz, true},
            static_cast<audio_studio::ProcessingEngine>(processing_engine), denoiser_strength, model_dir)) {
        const std::string message = "ERROR: " + error;
        return env->NewStringUTF(message.c_str());
    }
    return env->NewStringUTF("Processed copy saved.");
}

extern "C" JNIEXPORT void JNICALL
Java_com_innotrepid_audiostudio_MainActivity_nativeCancelNoiseGateProcessing(
    JNIEnv* /* env */, jobject /* thiz */) {
    g_cancel_noise_gate.store(true, std::memory_order_release);
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_innotrepid_audiostudio_MainActivity_nativeRecoverPartialWav(
    JNIEnv* env, jobject /* thiz */, jstring partial_path, jstring output_path) {
    if (partial_path == nullptr || output_path == nullptr) {
        return env->NewStringUTF("ERROR: Partial and output paths are required.");
    }
    const char* partial_chars = env->GetStringUTFChars(partial_path, nullptr);
    if (partial_chars == nullptr) return env->NewStringUTF("ERROR: Could not read partial path.");
    const std::string partial(partial_chars);
    env->ReleaseStringUTFChars(partial_path, partial_chars);
    const char* output_chars = env->GetStringUTFChars(output_path, nullptr);
    if (output_chars == nullptr) return env->NewStringUTF("ERROR: Could not read output path.");
    const std::string output(output_chars);
    env->ReleaseStringUTFChars(output_path, output_chars);

    std::string error;
    if (!audio_studio::recover_partial_wav(partial, output, &error)) {
        const std::string message = "ERROR: " + error;
        return env->NewStringUTF(message.c_str());
    }
    return env->NewStringUTF("Recovered WAV saved.");
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_innotrepid_audiostudio_MainActivity_nativeAnalyzeWavWaveform(
    JNIEnv* env, jobject /* thiz */, jstring input_path, jint bucket_count,
    jlongArray out_total_frames, jintArray out_sample_rate, jintArray out_channels) {
    if (input_path == nullptr || bucket_count <= 0) return nullptr;
    const char* chars = env->GetStringUTFChars(input_path, nullptr);
    if (chars == nullptr) return nullptr;
    const std::string path(chars);
    env->ReleaseStringUTFChars(input_path, chars);

    std::vector<float> peaks;
    std::uint64_t total_frames = 0;
    std::uint32_t sample_rate = 0;
    std::uint16_t channels = 0;
    std::string error;
    if (!audio_studio::analyze_wav_waveform(
            path, static_cast<unsigned int>(bucket_count), peaks, total_frames,
            sample_rate, channels, &error)) {
        return nullptr;
    }
    if (out_total_frames != nullptr && env->GetArrayLength(out_total_frames) >= 1) {
        jlong frames = static_cast<jlong>(total_frames);
        env->SetLongArrayRegion(out_total_frames, 0, 1, &frames);
    }
    if (out_sample_rate != nullptr && env->GetArrayLength(out_sample_rate) >= 1) {
        jint rate = static_cast<jint>(sample_rate);
        env->SetIntArrayRegion(out_sample_rate, 0, 1, &rate);
    }
    if (out_channels != nullptr && env->GetArrayLength(out_channels) >= 1) {
        jint ch = static_cast<jint>(channels);
        env->SetIntArrayRegion(out_channels, 0, 1, &ch);
    }
    jfloatArray result = env->NewFloatArray(static_cast<jsize>(peaks.size()));
    if (result != nullptr && !peaks.empty()) {
        env->SetFloatArrayRegion(result, 0, static_cast<jsize>(peaks.size()), peaks.data());
    }
    return result;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_innotrepid_audiostudio_MainActivity_nativeTrimWav(
    JNIEnv* env, jobject /* thiz */, jstring input_path, jstring output_path,
    jlong start_frame, jlong end_frame) {
    if (input_path == nullptr || output_path == nullptr) return JNI_FALSE;
    const char* input_chars = env->GetStringUTFChars(input_path, nullptr);
    if (input_chars == nullptr) return JNI_FALSE;
    const std::string input(input_chars);
    env->ReleaseStringUTFChars(input_path, input_chars);
    const char* output_chars = env->GetStringUTFChars(output_path, nullptr);
    if (output_chars == nullptr) return JNI_FALSE;
    const std::string output(output_chars);
    env->ReleaseStringUTFChars(output_path, output_chars);
    std::string error;
    if (!audio_studio::trim_wav(
            input, output, static_cast<std::uint64_t>(start_frame),
            static_cast<std::uint64_t>(end_frame), &error)) {
        return JNI_FALSE;
    }
    return JNI_TRUE;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_innotrepid_audiostudio_MainActivity_nativeTranscribe(
    JNIEnv* env, jobject /* thiz */, jstring model_path, jfloatArray pcm,
    jint sample_rate, jstring language) {
    if (model_path == nullptr || pcm == nullptr) return nullptr;
    const char* model_chars = env->GetStringUTFChars(model_path, nullptr);
    if (model_chars == nullptr) return nullptr;
    const std::string model(model_chars);
    env->ReleaseStringUTFChars(model_path, model_chars);
    std::string lang;
    if (language != nullptr) {
        const char* lang_chars = env->GetStringUTFChars(language, nullptr);
        if (lang_chars != nullptr) {
            lang.assign(lang_chars);
            env->ReleaseStringUTFChars(language, lang_chars);
        }
    }
    const jsize count = env->GetArrayLength(pcm);
    std::vector<float> samples(static_cast<std::size_t>(std::max(count, 0)));
    if (count > 0) env->GetFloatArrayRegion(pcm, 0, count, samples.data());
    audio_studio::TranscriptionRequest request{};
    request.model_path = model;
    request.pcm = samples.data();
    request.sample_count = samples.size();
    request.sample_rate = static_cast<std::uint32_t>(sample_rate);
    request.language = lang;
    const auto result = audio_studio::transcribe(request);
    if (!result) {
        const std::string payload = std::string(R"json({"error":)") + json_escape(result.error_message) +
            R"json("})json";
        return env->NewStringUTF(payload.c_str());
    }
    std::string payload = std::string(R"json({"text":)") + json_escape(result.text) +
        R"json(","detectedLanguage":)") + json_escape(result.detected_language) +
        R"json(","segments":[)json";
    for (std::size_t i = 0; i < result.segments.size(); ++i) {
        if (i != 0U) payload += ",";
        const auto& segment = result.segments[i];
        payload += std::string(R"json({"startMs":)json") + std::to_string(segment.start_ms) +
            R"json(,"endMs":)json" + std::to_string(segment.end_ms) +
            R"json(,"text":)") + json_escape(segment.text) + R"json("})json";
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
    constexpr std::uint64_t kMaxSamples = 2ULL * 60ULL * 60ULL * 16000ULL;
    if (!audio_studio::load_wav_for_transcription(path, mono, &error, kMaxSamples)) {
        const std::string message = "WAV transcription decode failed: " + error;
        jclass exception_class = env->FindClass("java/lang/IllegalArgumentException");
        if (exception_class != nullptr) env->ThrowNew(exception_class, message.c_str());
        return nullptr;
    }
    if (mono.size() > static_cast<std::size_t>(std::numeric_limits<jsize>::max())) {
        jclass exception_class = env->FindClass("java/lang/IllegalArgumentException");
        if (exception_class != nullptr) {
            env->ThrowNew(exception_class, "Decoded WAV exceeds Android array limits.");
        }
        return nullptr;
    }
    jfloatArray result = env->NewFloatArray(static_cast<jsize>(mono.size()));
    if (result != nullptr && !mono.empty()) {
        env->SetFloatArrayRegion(result, 0, static_cast<jsize>(mono.size()), mono.data());
    }
    return result;
}
