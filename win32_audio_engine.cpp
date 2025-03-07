#include "win32_audio_engine.h"

using namespace base::audio;

win32_audio_engine::win32_audio_engine()
{
  auto result = XAudio2Create(_pEngine.GetAddressOf());
  assert(SUCCEEDED(result));

  IXAudio2MasteringVoice* pMasterVoice;
  result = _pEngine->CreateMasteringVoice(&pMasterVoice, 2);
  assert(SUCCEEDED(result));
  _masterVoice = std::move(master_voice(pMasterVoice));

  DWORD dwChannelMask;
  pMasterVoice->GetChannelMask(&dwChannelMask);
  result = X3DAudioInitialize(dwChannelMask, X3DAUDIO_SPEED_OF_SOUND, _pX3DEngine);
  assert(SUCCEEDED(result));
}

void win32_audio_engine::source_voice::set_listener_position(float x, float y)
{
  _listener.OrientFront = { 0.0f, 0.0f, 1.0f };
  _listener.OrientTop = { 0.0f, 1.0f, 0.0f };
  _listener.Velocity = { x - _listener.Velocity.x, y - _listener.Velocity.y, 0.0f };
  _listener.Position = { x, y, 0.0f };
  _listener.pCone = nullptr;
}

void win32_audio_engine::source_voice::update_effects(operation_id operation)
{
  X3DAUDIO_DSP_SETTINGS dspSettings = { 0 };
  FLOAT32 matrixCoefficients[2]; // Stereo output
  dspSettings.pMatrixCoefficients = matrixCoefficients;
  dspSettings.SrcChannelCount = 1;
  dspSettings.DstChannelCount = 2;

  // generating new settings
  X3DAudioCalculate(_rEngine.get()._pX3DEngine, &_listener, &_emitter,
    X3DAUDIO_CALCULATE_MATRIX | X3DAUDIO_CALCULATE_DOPPLER | X3DAUDIO_CALCULATE_LPF_DIRECT, &dspSettings);

  auto* pMaster = _rEngine.get()._masterVoice.get_ptr();

  // distance attenuation effect and panning
  auto result = _pVoice->SetOutputMatrix(pMaster, 1, 2, dspSettings.pMatrixCoefficients, (UINT32)operation);
  assert(SUCCEEDED(result));

  // doppler effect
  result = _pVoice->SetFrequencyRatio(dspSettings.DopplerFactor, (UINT32)operation);
  assert(SUCCEEDED(result));
}

win32_audio_engine::source_voice::source_voice(win32_audio_engine& engine) :
  _rEngine(engine),
  _emitter(),
  _listener()
{
  WAVEFORMATEX format = { 0 };
  format.wFormatTag = WAVE_FORMAT_PCM; // PCM format
  format.nChannels = 1; // single channel (mono)
  format.nSamplesPerSec = 48000; // 48kHz
  format.wBitsPerSample = 32; // 32-bit
  format.nBlockAlign = (format.nChannels * format.wBitsPerSample) / 8; // Block alignment
  format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign; // Average bytes per second
  format.cbSize = 0; // No extra information
  
  auto result = engine._pEngine->CreateSourceVoice(
    &_pVoice, &format, 0u, XAUDIO2_DEFAULT_FREQ_RATIO,
    &_callback);
  assert(SUCCEEDED(result));

  set_emitter_position(0, 0);
  set_listener_position(0, 0);
}

win32_audio_engine::source_voice::~source_voice() noexcept
{
  if (_pVoice) {
    _pVoice->DestroyVoice();
  }
}

void win32_audio_engine::source_voice::stop(operation_id operation) const
{
  _pVoice->Stop(0, (UINT32)operation);
}

void win32_audio_engine::source_voice::break_queue() const
{
  auto result = _pVoice->Discontinuity();
  assert(SUCCEEDED(result));
}

void win32_audio_engine::source_voice::clear_queue() const
{
  auto result = _pVoice->FlushSourceBuffers();
  assert(SUCCEEDED(result));
}

void win32_audio_engine::source_voice::commit(operation_id operation)
{
  _rEngine.get().commit_changes(operation);
}

void win32_audio_engine::source_voice::set_emitter_position(float x, float y)
{
  _emitter.OrientFront = { 0.0f, 0.0f, 1.0f };
  _emitter.OrientTop = { 0.0f, 1.0f, 0.0f };
  _emitter.Velocity = { x - _emitter.Position.x, y - _emitter.Position.y, 0.0f };
  _emitter.Position = { x, y, 0.0f };
  _emitter.pCone = nullptr;
  _emitter.InnerRadius = 0.0f;
  _emitter.InnerRadiusAngle = 0.0f;
  _emitter.ChannelCount = 1;
  _emitter.ChannelRadius = 0.0f;
  _emitter.pChannelAzimuths = nullptr;
  _emitter.pVolumeCurve = nullptr;
  _emitter.pLFECurve = nullptr;
  _emitter.pLPFDirectCurve = nullptr;
  _emitter.pLPFReverbCurve = nullptr;
  _emitter.pReverbCurve = nullptr;
  _emitter.CurveDistanceScaler = 1.0f;
  _emitter.DopplerScaler = 1.0f;
}

void win32_audio_engine::source_voice::play(operation_id operation) const
{
  auto result = _pVoice->Start(0, (UINT32)operation);
  assert(SUCCEEDED(result));
}

void win32_audio_engine::source_voice::queue(const clip& clip, operation_id operation) const
{
  auto result = _pVoice->SubmitSourceBuffer(&clip.buffer);
  assert(SUCCEEDED(result));
}

win32_audio_engine::master_voice::master_voice(IXAudio2MasteringVoice* pVoice)
  :
  _pVoice(pVoice) {}

void win32_audio_engine::master_voice::swap(master_voice& other)
{
  auto* myPtr = _pVoice;
  _pVoice = other._pVoice;
  other._pVoice = myPtr;
}

win32_audio_engine::master_voice::~master_voice() noexcept
{
  if (_pVoice) {
    _pVoice->DestroyVoice();
  }
}

void __stdcall win32_audio_engine::source_voice::callbacks::OnVoiceProcessingPassStart(UINT32 BytesRequired)
{
}
void __stdcall win32_audio_engine::source_voice::callbacks::OnVoiceProcessingPassEnd(void)
{
}
void __stdcall win32_audio_engine::source_voice::callbacks::OnStreamEnd(void)
{
  _isPlaying.store(false);
}
void __stdcall win32_audio_engine::source_voice::callbacks::OnBufferStart(void* pBufferContext)
{
  _isPlaying.store(true);
}
void __stdcall win32_audio_engine::source_voice::callbacks::OnBufferEnd(void* pBufferContext)
{}
void __stdcall win32_audio_engine::source_voice::callbacks::OnLoopEnd(void* pBufferContext)
{
}
void __stdcall win32_audio_engine::source_voice::callbacks::OnVoiceError(void* pBufferContext, HRESULT Error)
{
}