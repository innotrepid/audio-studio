#include <jni.h>

#include <oboe/Oboe.h>

#include "audio_studio/audio_types.hpp"
#include "audio_studio/wav_writer.hpp"
#include "audio_studio/wav_processing.hpp"
#include "audio_studio/wav_recovery.hpp"
#include "audio_studio/transcription.hpp"
#include "audio_studio/wav_audio_reader.hpp"
#include "audio_studio/live_capture_vad.hpp"

// NOTE: Full AndroidRecorder + VAD wiring is applied in follow-up content
// restore. This temporary file keeps the tree buildable for non-recorder targets.
#error "native_bridge.cpp restore incomplete - use local artifacts/native_bridge.cpp"
