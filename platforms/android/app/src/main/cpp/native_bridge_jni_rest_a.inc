
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
    const std::string message = "Processed WAV: " + output;
    return env->NewStringUTF(message.c_str());
}

extern "C" JNIEXPORT void JNICALL
Java_com_innotrepid_audiostudio_MainActivity_nativeCancelNoiseGateProcessing(
    JNIEnv* /* env */, jobject /* thiz */) {
    g_cancel_noise_gate.store(true, std::memory_order_release);
}

extern "C" JNIEXPORT jfloatArray JNICALL
Java_com_innotrepid_audiostudio_MainActivity_nativeAnalyzeWavWaveform(
    JNIEnv* env, jobject /* thiz */, jstring input_path, jint bucket_count) {
    if (input_path == nullptr || bucket_count <= 0 || bucket_count > 1000000) return env->NewFloatArray(0);
    const char* chars = env->GetStringUTFChars(input_path, nullptr);
    if (chars == nullptr) return env->NewFloatArray(0);
    const std::string input(chars);
    env->ReleaseStringUTFChars(input_path, chars);
    std::vector<audio_studio::WaveformPeak> peaks;
    std::string error;
    if (!audio_studio::analyze_wav_waveform(input, static_cast<std::size_t>(bucket_count), peaks, nullptr, nullptr, &error))
        return env->NewFloatArray(0);
    std::vector<jfloat> values;
    values.reserve(peaks.size() * 4U);
    for (const auto& peak : peaks) {
        values.push_back(peak.min_left);
        values.push_back(peak.max_left);
        values.push_back(peak.min_right);
        values.push_back(peak.max_right);
    }
    jfloatArray result = env->NewFloatArray(static_cast<jsize>(values.size()));
    if (result != nullptr && !values.empty())
        env->SetFloatArrayRegion(result, 0, static_cast<jsize>(values.size()), values.data());
    return result;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_innotrepid_audiostudio_MainActivity_nativeWavAnalysisError(
    JNIEnv* env, jobject /* thiz */, jstring input_path) {
    if (input_path == nullptr) return env->NewStringUTF("ERROR: No WAV file path was provided.");
    const char* chars = env->GetStringUTFChars(input_path, nullptr);
    if (chars == nullptr) return env->NewStringUTF("ERROR: Could not read the selected file path.");
    const std::string input(chars);
    env->ReleaseStringUTFChars(input_path, chars);
    std::vector<audio_studio::WaveformPeak> peaks;
    std::string error;
    if (audio_studio::analyze_wav_waveform(input, 1U, peaks, nullptr, nullptr, &error)) {
        return env->NewStringUTF("OK");
    }
    const std::string message = "ERROR: " + error;
    return env->NewStringUTF(message.c_str());
}

extern "C" JNIEXPORT jlongArray JNICALL
Java_com_innotrepid_audiostudio_MainActivity_nativeGetWavMetadata(
    JNIEnv* env, jobject /* thiz */, jstring input_path) {
    if (input_path == nullptr) return env->NewLongArray(0);
    const char* chars = env->GetStringUTFChars(input_path, nullptr);
    if (chars == nullptr) return env->NewLongArray(0);
    const std::string input(chars);
    env->ReleaseStringUTFChars(input_path, chars);
    std::vector<audio_studio::WaveformPeak> peaks;
    audio_studio::AudioFormat format{};
    std::uint64_t total_frames = 0U;
    std::string error;
    if (!audio_studio::analyze_wav_waveform(input, 1U, peaks, &format, &total_frames, &error))
        return env->NewLongArray(0);
    const jlong values[3] = {
        static_cast<jlong>(total_frames),
        static_cast<jlong>(format.sample_rate_hz),
        static_cast<jlong>(format.channel_count)
    };
    jlongArray result = env->NewLongArray(3);
    if (result != nullptr) env->SetLongArrayRegion(result, 0, 3, values);
    return result;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_innotrepid_audiostudio_MainActivity_nativeTrimWav(
    JNIEnv* env, jobject /* thiz */, jstring input_path, jstring output_path,
    jlong start_frame, jlong end_frame) {
    if (input_path == nullptr || output_path == nullptr || start_frame < 0 ||
        end_frame <= start_frame) {
        return env->NewStringUTF("ERROR: Choose a valid non-empty trim range.");
    }
    const char* input_chars = env->GetStringUTFChars(input_path, nullptr);
    if (input_chars == nullptr) return env->NewStringUTF("ERROR: Could not read source path.");
    const std::string input(input_chars);
    env->ReleaseStringUTFChars(input_path, input_chars);
    const char* output_chars = env->GetStringUTFChars(output_path, nullptr);
    if (output_chars == nullptr) return env->NewStringUTF("ERROR: Could not read destination path.");
    const std::string output(output_chars);
    env->ReleaseStringUTFChars(output_path, output_chars);
    std::vector<audio_studio::WaveformPeak> metadata_peaks;
    std::uint64_t total_frames = 0U;
    std::string error;
    if (!audio_studio::analyze_wav_waveform(input, 1U, metadata_peaks, nullptr,
                                            &total_frames, &error)) {
        const std::string message = "ERROR: " + error;
        return env->NewStringUTF(message.c_str());
    }
    const auto start_frame_value = static_cast<std::uint64_t>(start_frame);
    const auto end_frame_value = static_cast<std::uint64_t>(end_frame);
    if (end_frame_value > total_frames) {
        return env->NewStringUTF("ERROR: The selected frame range exceeds the source WAV.");
    }
    if (!audio_studio::trim_wav(input, output, start_frame_value, end_frame_value, &error)) {
        const std::string message = "ERROR: " + error;
        return env->NewStringUTF(message.c_str());
    }
    return env->NewStringUTF("TRIMMED");
}
