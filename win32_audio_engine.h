#pragma once

#include <assert.h>
#include <cstdint>
#include <type_traits>
#include <array>
#include <atomic>

#include "win32_xaudio2_inc.h"

namespace base {
  namespace audio {
    class win32_audio_engine
    {
    public:
      enum class operation_id {
        immediate,
        multithreaded,
        multithreaded_1,
        multithreaded_2,
        multithreaded_3,
        multithreaded_4,
        multithreaded_5,
        multithreaded_6,
        multithreaded_7,
        multithreaded_8,
        multithreaded_9,
        multithreaded_10,
        multithreaded_11,
        multithreaded_12,
        multithreaded_13,
        multithreaded_14,
        multithreaded_15,
        multithreaded_16,
        multithreaded_17,
        multithreaded_18,
        multithreaded_19,
        multithreaded_20,
        multithreaded_21,
        multithreaded_22,
        multithreaded_23,
        multithreaded_24,
        multithreaded_25,
        multithreaded_26,
        multithreaded_27,
        multithreaded_28,
        multithreaded_29,
        multithreaded_30,
        multithreaded_31
      };
    private:
      class master_voice {
      public:
        master_voice() {};
        master_voice(IXAudio2MasteringVoice* pVoice);

        master_voice(const master_voice&) = delete;
        master_voice& operator= (const master_voice&) = delete;

        master_voice(master_voice&& other) noexcept {
          swap(other);
        }
        master_voice& operator=(master_voice&& rhs) noexcept {
          swap(rhs);
          return *this;
        }

        void swap(master_voice& other);

        ~master_voice() noexcept;

        void set_volume(float value, operation_id operation = operation_id::immediate) const {
          _pVoice->SetVolume(value, (UINT32)operation);
        }
        float get_volume() const {
          float value = -1;
          _pVoice->GetVolume(&value);
          return value;
        }

        IXAudio2MasteringVoice* get_ptr() noexcept {
          return _pVoice;
        }

      private:
        IXAudio2MasteringVoice* _pVoice = nullptr;
      };
    public:
      struct clip {
        clip() {}
        clip(char* pData, char loopCount = 0, bool single = true) :
         single(single)
        {
          buffer.AudioBytes = 1 * 48000 * (32 / 8);
          buffer.pAudioData = reinterpret_cast<const BYTE*>(pData);
          buffer.Flags = single? XAUDIO2_END_OF_STREAM : 0;
          buffer.LoopCount = loopCount == -1 ? XAUDIO2_LOOP_INFINITE : (UINT32)loopCount;
        }

        void mark_break(bool value) {
          buffer.Flags = value ? XAUDIO2_END_OF_STREAM : 0;
        }
        void set_loops(char value) {
          buffer.LoopCount = value == -1 ? XAUDIO2_LOOP_INFINITE : (UINT32)value;
        }

        XAUDIO2_BUFFER buffer = { 0 };
        bool single = false;
      };
      class source_voice {
      private:
        class callbacks : public IXAudio2VoiceCallback {
          void __stdcall OnVoiceProcessingPassStart(UINT32 BytesRequired) override;
          void __stdcall OnVoiceProcessingPassEnd(void) override;
          void __stdcall OnStreamEnd(void) override;
          void __stdcall OnBufferStart(void* pBufferContext) override;
          void __stdcall OnBufferEnd(void* pBufferContext) override;
          void __stdcall OnLoopEnd(void* pBufferContext) override;
          void __stdcall OnVoiceError(void* pBufferContext, HRESULT Error) override;

        public:
          bool is_playing() const noexcept {
            return _isPlaying.load();
          }

        private:
          std::atomic<bool> _isPlaying;
        };

      public:
        source_voice(win32_audio_engine& engine);

        source_voice(const source_voice&) = delete;
        source_voice& operator= (const source_voice&) = delete;

        ~source_voice() noexcept;

        void set_volume(float value, operation_id operation = operation_id::immediate) const {
          _pVoice->SetVolume(value, (UINT32)operation);
        }
        float get_volume() const {
          float value = -1;
          _pVoice->GetVolume(&value);
          return value;
        }
        bool is_playing() const noexcept {
          return _callback.is_playing();
        }

        void play(operation_id operation = operation_id::immediate) const;
        void queue(const clip& clip, operation_id operation = operation_id::immediate) const;
        void stop(operation_id operation = operation_id::immediate) const;
        void break_queue() const;
        void clear_queue() const;

        void commit(operation_id operation);

        void set_emitter_position(float x, float y);
        void set_listener_position(float x, float y);
        void update_effects(operation_id operation = operation_id::immediate);

      private:
        IXAudio2SourceVoice* _pVoice = nullptr;
        callbacks _callback;
        std::reference_wrapper<win32_audio_engine> _rEngine;

        X3DAUDIO_EMITTER _emitter;
        X3DAUDIO_LISTENER _listener;
      };

    public:
      win32_audio_engine();
      ~win32_audio_engine() noexcept {
        if (_pEngine) {
          _pEngine->StopEngine();
        }
      }

      float get_volume() const {
        return _masterVoice.get_volume();
      }

      void set_volume(float value, operation_id operation = operation_id::immediate) const {
        _masterVoice.set_volume(value, operation);
      }
      void commit_changes(operation_id operation) const {
        auto result = _pEngine->CommitChanges((UINT32)operation);
        assert(SUCCEEDED(result));
      }

    private:
      Microsoft::WRL::ComPtr<IXAudio2> _pEngine;
      master_voice _masterVoice;
      X3DAUDIO_HANDLE _pX3DEngine;
    };
  }
}