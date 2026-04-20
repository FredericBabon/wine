/* FAudio - XAudio Reimplementation for FNA
 *
 * Copyright (c) 2011-2024 Ethan Lee, Luigi Auriemma, and the MonoGame Team
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from
 * the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 * claim that you wrote the original software. If you use this software in a
 * product, an acknowledgment in the product documentation would be
 * appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and must not be
 * misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source distribution.
 *
 * Ethan "flibitijibibo" Lee <flibitijibibo@flibitijibibo.com>
 *
 */

#include "FAudio_internal.h"

#ifndef FAUDIO_DISABLE_DEBUGCONFIGURATION
void FAudio_INTERNAL_debug(
	FAudio *audio,
	const char *file,
	uint32_t line,
	const char *func,
	const char *fmt,
	...
) {
	char output[1024];
	char *out = output;
	va_list va;
	out[0] = '\0';

	/* Logging extras */
	if (audio->debug.LogThreadID)
	{
		out += FAudio_snprintf(
			out,
			sizeof(output) - (out - output),
			"0x%" FAudio_PRIx64 " ",
			FAudio_PlatformGetThreadID()
		);
	}
	if (audio->debug.LogFileline)
	{
		out += FAudio_snprintf(
			out,
			sizeof(output) - (out - output),
			"%s:%u ",
			file,
			line
		);
	}
	if (audio->debug.LogFunctionName)
	{
		out += FAudio_snprintf(
			out,
			sizeof(output) - (out - output),
			"%s ",
			func
		);
	}
	if (audio->debug.LogTiming)
	{
		out += FAudio_snprintf(
			out,
			sizeof(output) - (out - output),
			"%dms ",
			FAudio_timems()
		);
	}

	/* The actual message... */
	va_start(va, fmt);
	FAudio_vsnprintf(
		out,
		sizeof(output) - (out - output),
		fmt,
		va
	);
	va_end(va);

	/* Print, finally. */
	FAudio_Log(output);
}

static const char *get_wformattag_string(const FAudioWaveFormatEx *fmt)
{
#define FMT_STRING(suffix) \
	if (fmt->wFormatTag == FAUDIO_FORMAT_##suffix) \
	{ \
		return #suffix; \
	}
	FMT_STRING(PCM)
	FMT_STRING(MSADPCM)
	FMT_STRING(IEEE_FLOAT)
	FMT_STRING(XMAUDIO2)
	FMT_STRING(WMAUDIO2)
	FMT_STRING(WMAUDIO3)
	FMT_STRING(EXTENSIBLE)
#undef FMT_STRING
	return "UNKNOWN!";
}

static const char *get_subformat_string(const FAudioWaveFormatEx *fmt)
{
	const FAudioWaveFormatExtensible *fmtex = (const FAudioWaveFormatExtensible*) fmt;

	if (fmt->wFormatTag != FAUDIO_FORMAT_EXTENSIBLE)
	{
		return "N/A";
	}
	if (!FAudio_memcmp(&fmtex->SubFormat, &DATAFORMAT_SUBTYPE_IEEE_FLOAT, sizeof(FAudioGUID)))
	{
		return "IEEE_FLOAT";
	}
	if (!FAudio_memcmp(&fmtex->SubFormat, &DATAFORMAT_SUBTYPE_PCM, sizeof(FAudioGUID)))
	{
		return "PCM";
	}
	return "UNKNOWN!";
}

void FAudio_INTERNAL_debug_fmt(
	FAudio *audio,
	const char *file,
	uint32_t line,
	const char *func,
	const FAudioWaveFormatEx *fmt
) {
	FAudio_INTERNAL_debug(
		audio,
		file,
		line,
		func,
		(
			"{"
			"wFormatTag: 0x%x %s, "
			"nChannels: %u, "
			"nSamplesPerSec: %u, "
			"wBitsPerSample: %u, "
			"nBlockAlign: %u, "
			"SubFormat: %s"
			"}"
		),
		fmt->wFormatTag,
		get_wformattag_string(fmt),
		fmt->nChannels,
		fmt->nSamplesPerSec,
		fmt->wBitsPerSample,
		fmt->nBlockAlign,
		get_subformat_string(fmt)
	);
}
#endif /* FAUDIO_DISABLE_DEBUGCONFIGURATION */

void LinkedList_AddEntry(
	LinkedList **start,
	void* toAdd,
	FAudioMutex lock,
	FAudioMallocFunc pMalloc
) {
	LinkedList *newEntry, *latest;
	newEntry = (LinkedList*) pMalloc(sizeof(LinkedList));
	newEntry->entry = toAdd;
	newEntry->next = NULL;
	FAudio_PlatformLockMutex(lock);
	if (*start == NULL)
	{
		*start = newEntry;
	}
	else
	{
		latest = *start;
		while (latest->next != NULL)
		{
			latest = latest->next;
		}
		latest->next = newEntry;
	}
	FAudio_PlatformUnlockMutex(lock);
}

void LinkedList_PrependEntry(
	LinkedList **start,
	void* toAdd,
	FAudioMutex lock,
	FAudioMallocFunc pMalloc
) {
	LinkedList *newEntry;
	newEntry = (LinkedList*) pMalloc(sizeof(LinkedList));
	newEntry->entry = toAdd;
	FAudio_PlatformLockMutex(lock);
	newEntry->next = *start;
	*start = newEntry;
	FAudio_PlatformUnlockMutex(lock);
}

void LinkedList_RemoveEntry(
	LinkedList **start,
	void* toRemove,
	FAudioMutex lock,
	FAudioFreeFunc pFree
) {
	LinkedList *latest, *prev;
	FAudio_PlatformLockMutex(lock);
	latest = *start;
	prev = latest;
	while (latest != NULL)
	{
		if (latest->entry == toRemove)
		{
			if (latest == prev) /* First in list */
			{
				*start = latest->next;
			}
			else
			{
				prev->next = latest->next;
			}
			pFree(latest);
			FAudio_PlatformUnlockMutex(lock);
			return;
		}
		prev = latest;
		latest = latest->next;
	}
	FAudio_PlatformUnlockMutex(lock);
	FAudio_assert(0 && "LinkedList element not found!");
}

void FAudio_INTERNAL_InsertSubmixSorted(
	LinkedList **start,
	FAudioSubmixVoice *toAdd,
	FAudioMutex lock,
	FAudioMallocFunc pMalloc
) {
	LinkedList *newEntry, *latest;
	newEntry = (LinkedList*) pMalloc(sizeof(LinkedList));
	newEntry->entry = toAdd;
	newEntry->next = NULL;
	FAudio_PlatformLockMutex(lock);
	if (*start == NULL)
	{
		*start = newEntry;
	}
	else
	{
		latest = *start;

		/* Special case if the new stage is lower than everyone else */
		if (toAdd->mix.processingStage < ((FAudioSubmixVoice*) latest->entry)->mix.processingStage)
		{
			newEntry->next = latest;
			*start = newEntry;
		}
		else
		{
			/* If we got here, we know that the new stage is
			 * _at least_ as high as the first submix in the list.
			 *
			 * Each loop iteration checks to see if the new stage
			 * is smaller than `latest->next`, meaning it fits
			 * between `latest` and `latest->next`.
			 */
			while (latest->next != NULL)
			{
				if (toAdd->mix.processingStage < ((FAudioSubmixVoice *) latest->next->entry)->mix.processingStage)
				{
					newEntry->next = latest->next;
					latest->next = newEntry;
					break;
				}
				latest = latest->next;
			}
			/* If newEntry didn't get a `next` value, that means
			 * it didn't fall in between any stages and `latest`
			 * is the last entry in the list. Add it to the end!
			 */
			if (newEntry->next == NULL)
			{
				latest->next = newEntry;
			}
		}
	}
	FAudio_PlatformUnlockMutex(lock);
}

static uint32_t FAudio_INTERNAL_GetBytesRequested(
	FAudioSourceVoice *voice,
	uint32_t decoding
) {
	uint32_t end, result;
	FAudioBuffer *buffer;
	FAudioWaveFormatExtensible *fmt;
	FAudioBufferEntry *list = voice->src.bufferList;

	LOG_FUNC_ENTER(voice->audio)

#ifdef HAVE_WMADEC
	if (voice->src.wmadec != NULL)
	{
		/* Always 0, per the spec */
		LOG_FUNC_EXIT(voice->audio)
		return 0;
	}
#endif /* HAVE_WMADEC */
	while (list != NULL && decoding > 0)
	{
		buffer = &list->buffer;
		if (buffer->LoopCount > 0)
		{
			end = (
				/* Current loop... */
				((buffer->LoopBegin + buffer->LoopLength) - voice->src.curBufferOffset) +
				/* Remaining loops... */
				(buffer->LoopLength * buffer->LoopCount - 1) +
				/* ... Final iteration */
				buffer->PlayLength
			);
		}
		else
		{
			end = (buffer->PlayBegin + buffer->PlayLength) - voice->src.curBufferOffset;
		}
		if (end > decoding)
		{
			decoding = 0;
			break;
		}
		decoding -= end;
		list = list->next;
	}

	/* Convert samples to bytes, factoring block alignment */
	if (voice->src.format->wFormatTag == FAUDIO_FORMAT_MSADPCM)
	{
		fmt = (FAudioWaveFormatExtensible*) voice->src.format;
		result = (
			(decoding / fmt->Samples.wSamplesPerBlock) +
			((decoding % fmt->Samples.wSamplesPerBlock) > 0)
		) * voice->src.format->nBlockAlign;
	}
	else
	{
		result = decoding * voice->src.format->nBlockAlign;
	}

	LOG_FUNC_EXIT(voice->audio)
	return result;
}

static void FAudio_INTERNAL_DecodeBuffers(
	FAudioSourceVoice *voice,
	uint64_t *toDecode
) {
	uint32_t end, endRead, decoding, decoded = 0;
	FAudioBuffer *buffer = &voice->src.bufferList->buffer;
	FAudioBufferEntry *toDelete;

	LOG_FUNC_ENTER(voice->audio)

	/* This should never go past the max ratio size */
	FAudio_assert(*toDecode <= voice->src.decodeSamples);

	while (decoded < *toDecode && buffer != NULL)
	{
		decoding = (uint32_t) *toDecode - decoded;

		/* Start-of-buffer behavior */
		if (voice->src.newBuffer)
		{
			voice->src.newBuffer = 0;
			if (	voice->src.callback != NULL &&
				voice->src.callback->OnBufferStart != NULL	)
			{
				FAudio_PlatformUnlockMutex(voice->src.bufferLock);
				LOG_MUTEX_UNLOCK(voice->audio, voice->src.bufferLock)

				FAudio_PlatformUnlockMutex(voice->sendLock);
				LOG_MUTEX_UNLOCK(voice->audio, voice->sendLock)

				FAudio_PlatformUnlockMutex(voice->audio->sourceLock);
				LOG_MUTEX_UNLOCK(voice->audio, voice->audio->sourceLock)

				voice->src.callback->OnBufferStart(
					voice->src.callback,
					buffer->pContext
				);

				FAudio_PlatformLockMutex(voice->audio->sourceLock);
				LOG_MUTEX_LOCK(voice->audio, voice->audio->sourceLock)

				FAudio_PlatformLockMutex(voice->sendLock);
				LOG_MUTEX_LOCK(voice->audio, voice->sendLock)

				FAudio_PlatformLockMutex(voice->src.bufferLock);
				LOG_MUTEX_LOCK(voice->audio, voice->src.bufferLock)
			}
		}

		/* Check for end-of-buffer */
		end = (buffer->LoopCount > 0) ?
			(buffer->LoopBegin + buffer->LoopLength) :
			buffer->PlayBegin + buffer->PlayLength;
		endRead = FAudio_min(
			end - voice->src.curBufferOffset,
			decoding
		);

		/* Decode... */
		voice->src.decode(
			voice,
			buffer,
			voice->audio->decodeCache + (
				decoded * voice->src.format->nChannels
			),
			endRead
		);

		LOG_INFO(
			voice->audio,
			"Voice %p, buffer %p, decoded %u samples from [%u,%u)",
			(void*) voice,
			(void*) buffer,
			endRead,
			voice->src.curBufferOffset,
			voice->src.curBufferOffset + endRead
		)

		decoded += endRead;
		voice->src.curBufferOffset += endRead;
		voice->src.totalSamples += endRead;

		/* End-of-buffer behavior */
		if (endRead < decoding)
		{
			if (buffer->LoopCount > 0)
			{
				voice->src.curBufferOffset = buffer->LoopBegin;
				if (buffer->LoopCount < FAUDIO_LOOP_INFINITE)
				{
					buffer->LoopCount -= 1;
				}
				if (	voice->src.callback != NULL &&
					voice->src.callback->OnLoopEnd != NULL	)
				{
					FAudio_PlatformUnlockMutex(voice->src.bufferLock);
					LOG_MUTEX_UNLOCK(voice->audio, voice->src.bufferLock)

					FAudio_PlatformUnlockMutex(voice->sendLock);
					LOG_MUTEX_UNLOCK(voice->audio, voice->sendLock)

					FAudio_PlatformUnlockMutex(voice->audio->sourceLock);
					LOG_MUTEX_UNLOCK(voice->audio, voice->audio->sourceLock)

					voice->src.callback->OnLoopEnd(
						voice->src.callback,
						buffer->pContext
					);

					FAudio_PlatformLockMutex(voice->audio->sourceLock);
					LOG_MUTEX_LOCK(voice->audio, voice->audio->sourceLock)

					FAudio_PlatformLockMutex(voice->sendLock);
					LOG_MUTEX_LOCK(voice->audio, voice->sendLock)

					FAudio_PlatformLockMutex(voice->src.bufferLock);
					LOG_MUTEX_LOCK(voice->audio, voice->src.bufferLock)
				}
			}
			else
			{
#ifdef HAVE_WMADEC
				if (voice->src.wmadec != NULL)
				{
					FAudio_WMADEC_end_buffer(voice);
				}
#endif /* HAVE_WMADEC */
				/* For EOS we can stop storing fraction offsets */
				if (buffer->Flags & FAUDIO_END_OF_STREAM)
				{
					voice->src.curBufferOffsetDec = 0;
					voice->src.totalSamples = 0;
				}

				LOG_INFO(
					voice->audio,
					"Voice %p, finished with buffer %p",
					(void*) voice,
					(void*) buffer
				)

				/* Change active buffer, delete finished buffer */
				toDelete = voice->src.bufferList;
				voice->src.bufferList = voice->src.bufferList->next;
				if (voice->src.bufferList != NULL)
				{
					buffer = &voice->src.bufferList->buffer;
					voice->src.curBufferOffset = buffer->PlayBegin;
					LOG_INFO(
						voice->audio,
						"%p: BUFFER_TRANSITION new_buffer=%p play_begin=%u len=%u",
						(void*)voice,
						(void*)buffer,
						buffer->PlayBegin,
						buffer->PlayLength
					)
				}
				else
				{
					buffer = NULL;

					/* FIXME: I keep going past the buffer so fuck it */
					FAudio_zero(
						voice->audio->decodeCache + (
							decoded *
							voice->src.format->nChannels
						),
						sizeof(float) * (
							(*toDecode - decoded) *
							voice->src.format->nChannels
						)
					);
				}

				/* Callbacks */
				if (voice->src.callback != NULL)
				{
					FAudio_PlatformUnlockMutex(voice->src.bufferLock);
					LOG_MUTEX_UNLOCK(voice->audio, voice->src.bufferLock)

					FAudio_PlatformUnlockMutex(voice->sendLock);
					LOG_MUTEX_UNLOCK(voice->audio, voice->sendLock)

					FAudio_PlatformUnlockMutex(voice->audio->sourceLock);
					LOG_MUTEX_UNLOCK(voice->audio, voice->audio->sourceLock)

					if (voice->src.callback->OnBufferEnd != NULL)
					{
						voice->src.callback->OnBufferEnd(
							voice->src.callback,
							toDelete->buffer.pContext
						);
					}
					if (	toDelete->buffer.Flags & FAUDIO_END_OF_STREAM &&
						voice->src.callback->OnStreamEnd != NULL	)
					{
						voice->src.callback->OnStreamEnd(
							voice->src.callback
						);
					}

					FAudio_PlatformLockMutex(voice->audio->sourceLock);
					LOG_MUTEX_LOCK(voice->audio, voice->audio->sourceLock)

					FAudio_PlatformLockMutex(voice->sendLock);
					LOG_MUTEX_LOCK(voice->audio, voice->sendLock)

					FAudio_PlatformLockMutex(voice->src.bufferLock);
					LOG_MUTEX_LOCK(voice->audio, voice->src.bufferLock)

					/* One last chance at redemption */
					if (buffer == NULL && voice->src.bufferList != NULL)
					{
						buffer = &voice->src.bufferList->buffer;
						voice->src.curBufferOffset = buffer->PlayBegin;
					}

					if (buffer != NULL && voice->src.callback->OnBufferStart != NULL)
					{
						FAudio_PlatformUnlockMutex(voice->src.bufferLock);
						LOG_MUTEX_UNLOCK(voice->audio, voice->src.bufferLock)

						FAudio_PlatformUnlockMutex(voice->sendLock);
						LOG_MUTEX_UNLOCK(voice->audio, voice->sendLock)

						FAudio_PlatformUnlockMutex(voice->audio->sourceLock);
						LOG_MUTEX_UNLOCK(voice->audio, voice->audio->sourceLock)

						voice->src.callback->OnBufferStart(
							voice->src.callback,
							buffer->pContext
						);

						FAudio_PlatformLockMutex(voice->audio->sourceLock);
						LOG_MUTEX_LOCK(voice->audio, voice->audio->sourceLock)

						FAudio_PlatformLockMutex(voice->sendLock);
						LOG_MUTEX_LOCK(voice->audio, voice->sendLock)

						FAudio_PlatformLockMutex(voice->src.bufferLock);
						LOG_MUTEX_LOCK(voice->audio, voice->src.bufferLock)
					}
				}

				voice->audio->pFree(toDelete);
			}
		}
	}

	/* ... FIXME: I keep going past the buffer so fuck it */
	if (buffer)
	{
		end = (buffer->LoopCount > 0) ?
			(buffer->LoopBegin + buffer->LoopLength) :
			buffer->PlayBegin + buffer->PlayLength;
		endRead = FAudio_min(
			end - voice->src.curBufferOffset,
			EXTRA_DECODE_PADDING
		);

		voice->src.decode(
			voice,
			buffer,
			voice->audio->decodeCache + (
				decoded * voice->src.format->nChannels
			),
			endRead
		);
		/* Do NOT increment curBufferOffset! */

		if (endRead < EXTRA_DECODE_PADDING)
		{
			FAudio_zero(
				voice->audio->decodeCache + (
					decoded * voice->src.format->nChannels
				),
				sizeof(float) * (
					(EXTRA_DECODE_PADDING - endRead) *
					voice->src.format->nChannels
				)
			);
		}
	}
	else
	{
		FAudio_zero(
			voice->audio->decodeCache + (
				decoded * voice->src.format->nChannels
			),
			sizeof(float) * (
				EXTRA_DECODE_PADDING *
				voice->src.format->nChannels
			)
		);
	}

	*toDecode = decoded;
	LOG_FUNC_EXIT(voice->audio)
}

static inline void FAudio_INTERNAL_FilterVoice(
	FAudio *audio,
	const FAudioFilterParametersEXT *filter,
	FAudioFilterState *filterState,
	float *samples,
	uint32_t numSamples,
	uint16_t numChannels
) {
	uint32_t j, ci;

	LOG_FUNC_ENTER(audio)

	/* Apply a digital state-variable filter to the voice.
	 * The difference equations of the filter are:
	 *
	 * Yl(n) = F Yb(n - 1) + Yl(n - 1)
	 * Yh(n) = x(n) - Yl(n) - OneOverQ Yb(n - 1)
	 * Yb(n) = F Yh(n) + Yb(n - 1)
	 * Yn(n) = Yl(n) + Yh(n)
	 *
	 * Please note that FAudioFilterParameters.Frequency is defined as:
	 *
	 * (2 * sin(pi * (desired filter cutoff frequency) / sampleRate))
	 *
	 * - @JohanSmet
	 */

	for (j = 0; j < numSamples; j += 1)
	for (ci = 0; ci < numChannels; ci += 1)
	{
		filterState[ci][FAudioLowPassFilter] = filterState[ci][FAudioLowPassFilter] + (filter->Frequency * filterState[ci][FAudioBandPassFilter]);
		filterState[ci][FAudioHighPassFilter] = samples[j * numChannels + ci] - filterState[ci][FAudioLowPassFilter] - (filter->OneOverQ * filterState[ci][FAudioBandPassFilter]);
		filterState[ci][FAudioBandPassFilter] = (filter->Frequency * filterState[ci][FAudioHighPassFilter]) + filterState[ci][FAudioBandPassFilter];
		filterState[ci][FAudioNotchFilter] = filterState[ci][FAudioHighPassFilter] + filterState[ci][FAudioLowPassFilter];
		samples[j * numChannels + ci] = filterState[ci][filter->Type] * filter->WetDryMix + samples[j * numChannels + ci] * (1.0f - filter->WetDryMix);
	}

	LOG_FUNC_EXIT(audio)
}

static void FAudio_INTERNAL_ResizeEffectChainCache(FAudio *audio, uint32_t samples)
{
	LOG_FUNC_ENTER(audio)
	if (samples > audio->effectChainSamples)
	{
		audio->effectChainSamples = samples;
		audio->effectChainCache = (float*) audio->pRealloc(
			audio->effectChainCache,
			sizeof(float) * audio->effectChainSamples
		);
	}
	LOG_FUNC_EXIT(audio)
}

static inline float *FAudio_INTERNAL_ProcessEffectChain(
	FAudioVoice *voice,
	float *buffer,
	uint32_t *samples
) {
	uint32_t i;
	FAPO *fapo;
	FAPOProcessBufferParameters srcParams, dstParams;

	LOG_FUNC_ENTER(voice->audio)

	/* Set up the buffer to be written into */
	srcParams.pBuffer = buffer;
	srcParams.BufferFlags = FAPO_BUFFER_SILENT;
	srcParams.ValidFrameCount = *samples;
	for (i = 0; i < srcParams.ValidFrameCount; i += 1)
	{
		if (buffer[i] != 0.0f) /* Arbitrary! */
		{
			srcParams.BufferFlags = FAPO_BUFFER_VALID;
			break;
		}
	}

	/* Initialize output parameters to something sane */
	dstParams.pBuffer = srcParams.pBuffer;
	dstParams.BufferFlags = FAPO_BUFFER_VALID;
	dstParams.ValidFrameCount = srcParams.ValidFrameCount;

	/* Update parameters, process! */
	for (i = 0; i < voice->effects.count; i += 1)
	{
		fapo = voice->effects.desc[i].pEffect;

		if (!voice->effects.inPlaceProcessing[i])
		{
			if (dstParams.pBuffer == buffer)
			{
				FAudio_INTERNAL_ResizeEffectChainCache(
					voice->audio,
					voice->effects.desc[i].OutputChannels * srcParams.ValidFrameCount
				);
				dstParams.pBuffer = voice->audio->effectChainCache;
			}
			else
			{
				/* FIXME: What if this is smaller because
				 * inputChannels < desc[i].OutputChannels?
				 */
				dstParams.pBuffer = buffer;
			}

			FAudio_zero(
				dstParams.pBuffer,
				voice->effects.desc[i].OutputChannels * srcParams.ValidFrameCount * sizeof(float)
			);
		}

		if (voice->effects.parameterUpdates[i])
		{
			fapo->SetParameters(
				fapo,
				voice->effects.parameters[i],
				voice->effects.parameterSizes[i]
			);
			voice->effects.parameterUpdates[i] = 0;
		}

		fapo->Process(
			fapo,
			1,
			&srcParams,
			1,
			&dstParams,
			voice->effects.desc[i].InitialState
		);

		FAudio_memcpy(&srcParams, &dstParams, sizeof(dstParams));
	}

	*samples = dstParams.ValidFrameCount;

	/* Save the output buffer-flags so the mixer-function can determine when it's save to stop processing the effect chain */
	voice->effects.state = dstParams.BufferFlags;

	LOG_FUNC_EXIT(voice->audio)
	return (float*) dstParams.pBuffer;
}

static void FAudio_INTERNAL_ResizeResampleCache(FAudio *audio, uint32_t samples)
{
       LOG_FUNC_ENTER(audio)
       if (samples > audio->resampleSamples)
       {
               audio->resampleSamples = samples;
               audio->resampleCache = (float*) audio->pRealloc(
                       audio->resampleCache,
                       sizeof(float) * audio->resampleSamples
               );
       }
       LOG_FUNC_EXIT(audio)
}

static void FAudio_INTERNAL_MixSource(FAudioSourceVoice *voice)
{
	/* Iterators */
	uint32_t i;
	/* Decode/Resample variables */
	uint64_t toDecode;
	uint64_t toResample;
	/* Output mix variables */
	float *stream;
	uint32_t mixed;
	uint32_t oChan;
	FAudioVoice *out;
	uint32_t outputRate;
	double stepd;
	float *finalSamples;

	LOG_FUNC_ENTER(voice->audio)

	FAudio_PlatformLockMutex(voice->sendLock);
	LOG_MUTEX_LOCK(voice->audio, voice->sendLock)

	/* Calculate the resample stepping value */
	if (voice->src.resampleFreq != voice->src.freqRatio * voice->src.format->nSamplesPerSec)
	{
		out = (voice->sends.SendCount == 0) ?
			voice->audio->master : /* Barf */
			voice->sends.pSends->pOutputVoice;
		outputRate = (out->type == FAUDIO_VOICE_MASTER) ?
			out->master.inputSampleRate :
			out->mix.inputSampleRate;
		stepd = (
			voice->src.freqRatio *
			(double) voice->src.format->nSamplesPerSec /
			(double) outputRate
		);
		voice->src.resampleStep = DOUBLE_TO_FIXED(stepd);
		voice->src.resampleFreq = voice->src.freqRatio * voice->src.format->nSamplesPerSec;
	}

	if (voice->src.active == 2)
	{
		/* We're just playing tails, skip all buffer stuff */
		FAudio_INTERNAL_ResizeResampleCache(
				voice->audio,
				voice->src.resampleSamples * voice->src.format->nChannels
		);
		mixed = voice->src.resampleSamples;
		FAudio_zero(
			voice->audio->resampleCache,
			mixed * voice->src.format->nChannels * sizeof(float)
		);
		finalSamples = voice->audio->resampleCache;
		goto sendwork;
	}

	/* Base decode size, int to fixed... */
	toDecode = voice->src.resampleSamples * voice->src.resampleStep;
	/* ... rounded up based on current offset... */
	toDecode += voice->src.curBufferOffsetDec + FIXED_FRACTION_MASK;
	/* ... fixed to int, truncating extra fraction from rounding. */
	toDecode >>= FIXED_PRECISION;

	/* First voice callback */
	if (	voice->src.callback != NULL &&
		voice->src.callback->OnVoiceProcessingPassStart != NULL	)
	{
		FAudio_PlatformUnlockMutex(voice->sendLock);
		LOG_MUTEX_UNLOCK(voice->audio, voice->sendLock)

		FAudio_PlatformUnlockMutex(voice->audio->sourceLock);
		LOG_MUTEX_UNLOCK(voice->audio, voice->audio->sourceLock)

		voice->src.callback->OnVoiceProcessingPassStart(
			voice->src.callback,
			FAudio_INTERNAL_GetBytesRequested(voice, (uint32_t) toDecode)
		);

		FAudio_PlatformLockMutex(voice->audio->sourceLock);
		LOG_MUTEX_LOCK(voice->audio, voice->audio->sourceLock)

		FAudio_PlatformLockMutex(voice->sendLock);
		LOG_MUTEX_LOCK(voice->audio, voice->sendLock)
	}

	FAudio_PlatformLockMutex(voice->src.bufferLock);
	LOG_MUTEX_LOCK(voice->audio, voice->src.bufferLock)

	/* Nothing to do? */
	if (voice->src.bufferList == NULL)
	{
		FAudio_PlatformUnlockMutex(voice->src.bufferLock);
		LOG_MUTEX_UNLOCK(voice->audio, voice->src.bufferLock)

		if (voice->effects.count > 0 && voice->effects.state != FAPO_BUFFER_SILENT)
		{
			/* do not stop while the effect chain generates a non-silent buffer */
			FAudio_INTERNAL_ResizeResampleCache(
					voice->audio,
					voice->src.resampleSamples * voice->src.format->nChannels
			);
			mixed = voice->src.resampleSamples;
			FAudio_zero(
				voice->audio->resampleCache,
				mixed * voice->src.format->nChannels * sizeof(float)
			);
			finalSamples = voice->audio->resampleCache;
			goto sendwork;
		}

		FAudio_PlatformUnlockMutex(voice->sendLock);
		LOG_MUTEX_UNLOCK(voice->audio, voice->sendLock)

		FAudio_PlatformUnlockMutex(voice->audio->sourceLock);
		LOG_MUTEX_UNLOCK(voice->audio, voice->audio->sourceLock)

		if (	voice->src.callback != NULL &&
			voice->src.callback->OnVoiceProcessingPassEnd != NULL)
		{
			voice->src.callback->OnVoiceProcessingPassEnd(
				voice->src.callback
			);
		}

		FAudio_PlatformLockMutex(voice->audio->sourceLock);
		LOG_MUTEX_LOCK(voice->audio, voice->audio->sourceLock)

		LOG_FUNC_EXIT(voice->audio)
		return;
	}

	/* Decode... */
	{
		FAudioBufferEntry *cur_buf = voice->src.bufferList;
		uint32_t buf_idx = 0;
		LOG_INFO(voice->audio, "%p: DECODE_START toDecode=%u curOffset=%u buffers=%u resampleStep=%u",
			(void*)voice, toDecode, voice->src.curBufferOffset,
			cur_buf ? 1 : 0, voice->src.resampleStep)
	}
	FAudio_INTERNAL_DecodeBuffers(voice, &toDecode);

	/* Subtract any padding samples from the total, if applicable */
	if (	voice->src.curBufferOffsetDec > 0 &&
		voice->src.totalSamples > 0	)
	{
		voice->src.totalSamples -= 1;
	}

	/* Okay, we're done messing with client data */
	if (	voice->src.callback != NULL &&
		voice->src.callback->OnVoiceProcessingPassEnd != NULL)
	{
		FAudio_PlatformUnlockMutex(voice->src.bufferLock);
		LOG_MUTEX_UNLOCK(voice->audio, voice->src.bufferLock)

		FAudio_PlatformUnlockMutex(voice->sendLock);
		LOG_MUTEX_UNLOCK(voice->audio, voice->sendLock)

		FAudio_PlatformUnlockMutex(voice->audio->sourceLock);
		LOG_MUTEX_UNLOCK(voice->audio, voice->audio->sourceLock)

		voice->src.callback->OnVoiceProcessingPassEnd(
			voice->src.callback
		);

		FAudio_PlatformLockMutex(voice->audio->sourceLock);
		LOG_MUTEX_LOCK(voice->audio, voice->audio->sourceLock)

		FAudio_PlatformLockMutex(voice->sendLock);
		LOG_MUTEX_LOCK(voice->audio, voice->sendLock)

		FAudio_PlatformLockMutex(voice->src.bufferLock);
		LOG_MUTEX_LOCK(voice->audio, voice->src.bufferLock)
	}

	/* Nothing to resample? */
	if (toDecode == 0)
	{
		FAudio_PlatformUnlockMutex(voice->src.bufferLock);
		LOG_MUTEX_UNLOCK(voice->audio, voice->src.bufferLock)

		FAudio_PlatformUnlockMutex(voice->sendLock);
		LOG_MUTEX_UNLOCK(voice->audio, voice->sendLock)

		LOG_FUNC_EXIT(voice->audio)
		return;
	}

	/* int to fixed... */
	toResample = toDecode << FIXED_PRECISION;
	/* ... round back down based on current offset... */
	toResample -= voice->src.curBufferOffsetDec;
	/* ... but also ceil for any fraction value... */
	toResample += FIXED_FRACTION_MASK;
	/* ... undo step size, fixed to int. */
	toResample /= voice->src.resampleStep;
	/* Add the padding, for some reason this helps? */
	toResample += EXTRA_DECODE_PADDING;
	/* FIXME: I feel like this should be an assert but I suck */
	toResample = FAudio_min(toResample, voice->src.resampleSamples);

	/* Resample... */
	if (voice->src.resampleStep == FIXED_ONE)
	{
		/* Actually, just use the existing buffer... */
		finalSamples = voice->audio->decodeCache;
		LOG_INFO(voice->audio, "%p: RESAMPLE_BYPASS (no resampling needed)", (void*)voice)
	}
	else
	{
		LOG_INFO(voice->audio, "%p: RESAMPLE_ACTIVE resampleStep=0x%x samples=%u",
			(void*)voice, voice->src.resampleStep, toResample)
		FAudio_INTERNAL_ResizeResampleCache(
				voice->audio,
				voice->src.resampleSamples * voice->src.format->nChannels
		);
		voice->src.resample(
			voice->audio->decodeCache,
			voice->audio->resampleCache,
			&voice->src.resampleOffset,
			voice->src.resampleStep,
			toResample,
			(uint8_t) voice->src.format->nChannels
		);
		finalSamples = voice->audio->resampleCache;
	}

	/* Update buffer offsets */
	if (voice->src.bufferList != NULL)
	{
		/* Increment fixed offset by resample size, int to fixed... */
		voice->src.curBufferOffsetDec += toResample * voice->src.resampleStep;
		/* ... chop off any ints we got from the above increment */
		voice->src.curBufferOffsetDec &= FIXED_FRACTION_MASK;

		/* Dec >0? We need one frame from the past...
		 * FIXME: We can't go back to a prev buffer though?
		 */
		if (	voice->src.curBufferOffsetDec > 0 &&
			voice->src.curBufferOffset > 0	)
		{
			voice->src.curBufferOffset -= 1;
		}
	}
	else
	{
		voice->src.curBufferOffsetDec = 0;
		voice->src.curBufferOffset = 0;
	}

	/* Done with buffers, finally. */
	FAudio_PlatformUnlockMutex(voice->src.bufferLock);
	LOG_MUTEX_UNLOCK(voice->audio, voice->src.bufferLock)
	mixed = (uint32_t) toResample;

sendwork:

	/* Filters */
	if (voice->flags & FAUDIO_VOICE_USEFILTER)
	{
		FAudio_PlatformLockMutex(voice->filterLock);
		LOG_MUTEX_LOCK(voice->audio, voice->filterLock)
		LOG_INFO(voice->audio, "%p: FILTER_APPLY channels=%u mixed=%u", (void*)voice, voice->src.format->nChannels, mixed)
		FAudio_INTERNAL_FilterVoice(
			voice->audio,
			&voice->filter,
			voice->filterState,
			finalSamples,
			mixed,
			voice->src.format->nChannels
		);
		FAudio_PlatformUnlockMutex(voice->filterLock);
		LOG_MUTEX_UNLOCK(voice->audio, voice->filterLock)
	}

	/* Process effect chain */
	FAudio_PlatformLockMutex(voice->effectLock);
	LOG_MUTEX_LOCK(voice->audio, voice->effectLock)
	if (voice->effects.count > 0)
	{
		LOG_INFO(voice->audio, "%p: EFFECTS_PROCESS count=%u mixed_before=%u", (void*)voice, voice->effects.count, mixed)
		/* If we didn't get the full size of the update, we have to fill
		 * it with silence so the effect can process a whole update
		 */
		if (mixed < voice->src.resampleSamples)
		{
			FAudio_zero(
				finalSamples + (mixed * voice->src.format->nChannels),
				(voice->src.resampleSamples - mixed) * voice->src.format->nChannels * sizeof(float)
			);
			mixed = voice->src.resampleSamples;
		}
		finalSamples = FAudio_INTERNAL_ProcessEffectChain(
			voice,
			finalSamples,
			&mixed
		);
	}
	FAudio_PlatformUnlockMutex(voice->effectLock);
	LOG_MUTEX_UNLOCK(voice->audio, voice->effectLock)

	/* Nowhere to send it? Just skip the rest...*/
	if (voice->sends.SendCount == 0)
	{
		FAudio_PlatformUnlockMutex(voice->sendLock);
		LOG_MUTEX_UNLOCK(voice->audio, voice->sendLock)
		LOG_FUNC_EXIT(voice->audio)
		return;
	}

	/* Send float cache to sends */
	FAudio_PlatformLockMutex(voice->volumeLock);
	LOG_MUTEX_LOCK(voice->audio, voice->volumeLock)
	for (i = 0; i < voice->sends.SendCount; i += 1)
	{
		out = voice->sends.pSends[i].pOutputVoice;
		if (out->type == FAUDIO_VOICE_MASTER)
		{
			stream = out->master.output;
			oChan = out->master.inputChannels;
		}
		else
		{
			stream = out->mix.inputCache;
			oChan = out->mix.inputChannels;
		}

		voice->sendMix[i](
			mixed,
			voice->outputChannels,
			oChan,
			finalSamples,
			stream,
			voice->mixCoefficients[i]
		);

		if (voice->sends.pSends[i].Flags & FAUDIO_SEND_USEFILTER)
		{
			FAudio_INTERNAL_FilterVoice(
				voice->audio,
				&voice->sendFilter[i],
				voice->sendFilterState[i],
				stream,
				mixed,
				oChan
			);
		}
	}
	FAudio_PlatformUnlockMutex(voice->volumeLock);
	LOG_MUTEX_UNLOCK(voice->audio, voice->volumeLock)

	FAudio_PlatformUnlockMutex(voice->sendLock);
	LOG_MUTEX_UNLOCK(voice->audio, voice->sendLock)
	LOG_FUNC_EXIT(voice->audio)
}

static void FAudio_INTERNAL_MixSubmix(FAudioSubmixVoice *voice)
{
	uint32_t i;
	float *stream;
	uint32_t oChan;
	FAudioVoice *out;
	uint32_t resampled;
	uint64_t resampleOffset = 0;
	float *finalSamples;

	LOG_FUNC_ENTER(voice->audio)
	FAudio_PlatformLockMutex(voice->sendLock);
	LOG_MUTEX_LOCK(voice->audio, voice->sendLock)

	/* Resample */
	if (voice->mix.resampleStep == FIXED_ONE)
	{
		/* Actually, just use the existing buffer... */
		finalSamples = voice->mix.inputCache;
	}
	else
	{
		FAudio_INTERNAL_ResizeResampleCache(
				voice->audio,
				voice->mix.outputSamples * voice->mix.inputChannels
		);
		voice->mix.resample(
			voice->mix.inputCache,
			voice->audio->resampleCache,
			&resampleOffset,
			voice->mix.resampleStep,
			voice->mix.outputSamples,
			(uint8_t) voice->mix.inputChannels
		);
		finalSamples = voice->audio->resampleCache;
	}
	resampled = voice->mix.outputSamples * voice->mix.inputChannels;

	/* Submix overall volume is applied _before_ effects/filters, blech! */
	if (voice->volume != 1.0f)
	{
		FAudio_INTERNAL_Amplify(
			finalSamples,
			resampled,
			voice->volume
		);
	}
	resampled /= voice->mix.inputChannels;

	/* Filters */
	if (voice->flags & FAUDIO_VOICE_USEFILTER)
	{
		FAudio_PlatformLockMutex(voice->filterLock);
		LOG_MUTEX_LOCK(voice->audio, voice->filterLock)
		FAudio_INTERNAL_FilterVoice(
			voice->audio,
			&voice->filter,
			voice->filterState,
			finalSamples,
			resampled,
			voice->mix.inputChannels
		);
		FAudio_PlatformUnlockMutex(voice->filterLock);
		LOG_MUTEX_UNLOCK(voice->audio, voice->filterLock)
	}

	/* Process effect chain */
	FAudio_PlatformLockMutex(voice->effectLock);
	LOG_MUTEX_LOCK(voice->audio, voice->effectLock)
	if (voice->effects.count > 0)
	{
		finalSamples = FAudio_INTERNAL_ProcessEffectChain(
			voice,
			finalSamples,
			&resampled
		);
	}
	FAudio_PlatformUnlockMutex(voice->effectLock);
	LOG_MUTEX_UNLOCK(voice->audio, voice->effectLock)

	/* Nothing more to do? */
	if (voice->sends.SendCount == 0)
	{
		goto end;
	}

	/* Send float cache to sends */
	FAudio_PlatformLockMutex(voice->volumeLock);
	LOG_MUTEX_LOCK(voice->audio, voice->volumeLock)
	for (i = 0; i < voice->sends.SendCount; i += 1)
	{
		out = voice->sends.pSends[i].pOutputVoice;
		if (out->type == FAUDIO_VOICE_MASTER)
		{
			stream = out->master.output;
			oChan = out->master.inputChannels;
		}
		else
		{
			stream = out->mix.inputCache;
			oChan = out->mix.inputChannels;
		}

		voice->sendMix[i](
			resampled,
			voice->outputChannels,
			oChan,
			finalSamples,
			stream,
			voice->mixCoefficients[i]
		);

		if (voice->sends.pSends[i].Flags & FAUDIO_SEND_USEFILTER)
		{
			FAudio_INTERNAL_FilterVoice(
				voice->audio,
				&voice->sendFilter[i],
				voice->sendFilterState[i],
				stream,
				resampled,
				oChan
			);
		}
	}
	FAudio_PlatformUnlockMutex(voice->volumeLock);
	LOG_MUTEX_UNLOCK(voice->audio, voice->volumeLock)

	/* Zero this at the end, for the next update */
end:
	FAudio_PlatformUnlockMutex(voice->sendLock);
	LOG_MUTEX_UNLOCK(voice->audio, voice->sendLock)
	FAudio_zero(
		voice->mix.inputCache,
		sizeof(float) * voice->mix.inputSamples
	);
	LOG_FUNC_EXIT(voice->audio)
}

static void FAudio_INTERNAL_FlushPendingBuffers(FAudioSourceVoice *voice)
{
	FAudioBufferEntry *entry;

	FAudio_PlatformLockMutex(voice->src.bufferLock);
	LOG_MUTEX_LOCK(voice->audio, voice->src.bufferLock)

	/* Remove pending flushed buffers and send an event for each one */
	while (voice->src.flushList != NULL)
	{
		entry = voice->src.flushList;
		voice->src.flushList = voice->src.flushList->next;

		if (voice->src.callback != NULL && voice->src.callback->OnBufferEnd != NULL)
		{
			FAudio_PlatformUnlockMutex(voice->src.bufferLock);
			LOG_MUTEX_UNLOCK(voice->audio, voice->src.bufferLock)

			FAudio_PlatformUnlockMutex(voice->audio->sourceLock);
			LOG_MUTEX_UNLOCK(voice->audio, voice->audio->sourceLock)

			voice->src.callback->OnBufferEnd(
				voice->src.callback,
				entry->buffer.pContext
			);

			FAudio_PlatformLockMutex(voice->audio->sourceLock);
			LOG_MUTEX_LOCK(voice->audio, voice->audio->sourceLock)

			FAudio_PlatformLockMutex(voice->src.bufferLock);
			LOG_MUTEX_LOCK(voice->audio, voice->src.bufferLock)
		}
		voice->audio->pFree(entry);
	}

	FAudio_PlatformUnlockMutex(voice->src.bufferLock);
	LOG_MUTEX_UNLOCK(voice->audio, voice->src.bufferLock)
}

static void FAUDIOCALL FAudio_INTERNAL_GenerateOutput(FAudio *audio, float *output)
{
	uint32_t totalSamples;
	LinkedList *list;
	float *effectOut;
	FAudioEngineCallback *callback;

	LOG_FUNC_ENTER(audio)
	if (!audio->active)
	{
		LOG_FUNC_EXIT(audio)
		return;
	}

	/* Apply any committed changes */
	FAudio_OPERATIONSET_Execute(audio);

	/* ProcessingPassStart callbacks */
	FAudio_PlatformLockMutex(audio->callbackLock);
	LOG_MUTEX_LOCK(audio, audio->callbackLock)
	list = audio->callbacks;
	while (list != NULL)
	{
		callback = (FAudioEngineCallback*) list->entry;
		if (callback->OnProcessingPassStart != NULL)
		{
			callback->OnProcessingPassStart(
				callback
			);
		}
		list = list->next;
	}
	FAudio_PlatformUnlockMutex(audio->callbackLock);
	LOG_MUTEX_UNLOCK(audio, audio->callbackLock)

	/* Writes to master will directly write to output, but ONLY if there
	 * isn't any channel-changing effect processing to do first.
	 */
	if (audio->master->master.effectCache != NULL)
	{
		audio->master->master.output = audio->master->master.effectCache;
		FAudio_zero(
			audio->master->master.effectCache,
			(
				sizeof(float) *
				audio->updateSize *
				audio->master->master.inputChannels
			)
		);
	}
	else
	{
		audio->master->master.output = output;
	}

	/* Mix sources */
	FAudio_PlatformLockMutex(audio->sourceLock);
	LOG_MUTEX_LOCK(audio, audio->sourceLock)
	list = audio->sources;
	while (list != NULL)
	{
		audio->processingSource = (FAudioSourceVoice*) list->entry;

		FAudio_INTERNAL_FlushPendingBuffers(audio->processingSource);
		if (audio->processingSource->src.active)
		{
			FAudio_INTERNAL_MixSource(audio->processingSource);
			FAudio_INTERNAL_FlushPendingBuffers(audio->processingSource);
		}

		list = list->next;
	}
	audio->processingSource = NULL;
	FAudio_PlatformUnlockMutex(audio->sourceLock);
	LOG_MUTEX_UNLOCK(audio, audio->sourceLock)

	/* Mix submixes, ordered by processing stage */
	FAudio_PlatformLockMutex(audio->submixLock);
	LOG_MUTEX_LOCK(audio, audio->submixLock)
	list = audio->submixes;
	while (list != NULL)
	{
		FAudio_INTERNAL_MixSubmix((FAudioSubmixVoice*) list->entry);
		list = list->next;
	}
	FAudio_PlatformUnlockMutex(audio->submixLock);
	LOG_MUTEX_UNLOCK(audio, audio->submixLock)

	/* Apply master volume */
	if (audio->master->volume != 1.0f)
	{
		FAudio_INTERNAL_Amplify(
			audio->master->master.output,
			audio->updateSize * audio->master->master.inputChannels,
			audio->master->volume
		);
	}

	/* Process master effect chain */
	FAudio_PlatformLockMutex(audio->master->effectLock);
	LOG_MUTEX_LOCK(audio, audio->master->effectLock)
	if (audio->master->effects.count > 0)
	{
		totalSamples = audio->updateSize;
		effectOut = FAudio_INTERNAL_ProcessEffectChain(
			audio->master,
			audio->master->master.output,
			&totalSamples
		);

		if (effectOut != output)
		{
			FAudio_memcpy(
				output,
				effectOut,
				totalSamples * audio->master->outputChannels * sizeof(float)
			);
		}
		if (totalSamples < audio->updateSize)
		{
			FAudio_zero(
				output + (totalSamples * audio->master->outputChannels),
				(audio->updateSize - totalSamples) * sizeof(float)
			);
		}
	}
	FAudio_PlatformUnlockMutex(audio->master->effectLock);
	LOG_MUTEX_UNLOCK(audio, audio->master->effectLock)

	/* OnProcessingPassEnd callbacks */
	FAudio_PlatformLockMutex(audio->callbackLock);
	LOG_MUTEX_LOCK(audio, audio->callbackLock)
	list = audio->callbacks;
	while (list != NULL)
	{
		callback = (FAudioEngineCallback*) list->entry;
		if (callback->OnProcessingPassEnd != NULL)
		{
			callback->OnProcessingPassEnd(
				callback
			);
		}
		list = list->next;
	}
	FAudio_PlatformUnlockMutex(audio->callbackLock);
	LOG_MUTEX_UNLOCK(audio, audio->callbackLock)

	LOG_FUNC_EXIT(audio)
}

/* WSOLA HELPER FUNCTIONS */

/* Compute normalized cross-correlation between two buffers */
static inline float FAudio_INTERNAL_NormalizedCorrelation(
	const float *signal1,
	const float *signal2,
	uint32_t length,
	uint32_t channels
) {
	float dotProduct = 0.0f;
	float norm1 = 0.0f;
	float norm2 = 0.0f;
	uint32_t i;

	for (i = 0; i < length; i++) {
		dotProduct += signal1[i] * signal2[i];
		norm1 += signal1[i] * signal1[i];
		norm2 += signal2[i] * signal2[i];
	}

	if (norm1 < 1e-10f || norm2 < 1e-10f) {
		return 0.0f;
	}

	return dotProduct / (FAudio_sqrtf(norm1) * FAudio_sqrtf(norm2));
}

/* Find best matching segment in history buffer */
static uint32_t FAudio_INTERNAL_FindBestSegment(
	FAudio *audio,
	const float *targetSegment,
	uint32_t segmentLength
) {
	uint32_t channels = audio->mixFormat.Format.nChannels;
	uint32_t searchRange = audio->wsolaWindowSize * 3; /* Search within 3 windows back */
	uint32_t bestOffset = 0;
	float bestCorr = -2.0f;
	uint32_t offset;
	uint32_t compareLength = FAudio_min(segmentLength, 1920);

	/* Search through history buffer for best match */
	for (offset = 0; offset < searchRange && offset < audio->historyMax; offset += audio->wsolaWindowSize / 2) {
		uint32_t pos = (audio->historyWriteIdx + audio->historyMax - (offset % audio->historyMax)) % audio->historyMax;
		float corr;

		/* Wrap-around safe buffer read */
		float tempBuf[1920]; /* Max WSOLA window size */
		uint32_t i, readPos;
		for (i = 0; i < compareLength; i++) {
			readPos = (pos + i) % audio->historyMax;
			tempBuf[i] = audio->historyBuffer[readPos];
		}

		corr = FAudio_INTERNAL_NormalizedCorrelation(targetSegment, tempBuf, compareLength, channels);

		if (corr > bestCorr) {
			bestCorr = corr;
			bestOffset = offset;
		}
	}

	return bestOffset;
}

/* Overlap-Add synthesis from history buffer */
static void FAudio_INTERNAL_WsolaOverlapAdd(
	FAudio *audio,
	float *output,
	uint32_t segmentLength,
	uint32_t offset
) {
	uint32_t i;
	uint32_t pos;
	float *window = audio->wsolaHannWindow;
	uint32_t channels = audio->mixFormat.Format.nChannels;

	/* Reconstruct segment from history with Hann window */
	for (i = 0; i < segmentLength && i < audio->wsolaWindowSize; i++) {
		pos = (audio->historyWriteIdx + audio->historyMax - (offset % audio->historyMax) + i) % audio->historyMax;
		float histSample = audio->historyBuffer[pos];
		float windowValue = window[i];
		
		/* Apply Hann window and overlap with existing output */
		output[i] = (histSample * windowValue) + (output[i] * (1.0f - windowValue));
	}
}

void FAudio_INTERNAL_UpdateEngine(FAudio *audio, float *output)
{
	LOG_FUNC_ENTER(audio)
	if (audio->pClientEngineProc)
	{
		audio->pClientEngineProc(
			&FAudio_INTERNAL_GenerateOutput,
			audio,
			output,
			audio->clientEngineUser
		);
	}
	else
	{
		FAudio_INTERNAL_GenerateOutput(audio, output);
	}

	/* ERROR CONCEALMENT - WSOLA (Waveform Similarity-Based Overlap-Add) */
	{
		uint32_t channels = audio->mixFormat.Format.nChannels;
		uint32_t maxConcealmentSamples = channels * 2400; /* 50ms @ 48kHz */
		uint32_t resumeConfirmSamples = channels * 1920;  /* 40ms @ 48kHz */
		uint32_t sampleIdx;
		uint32_t samplesInBuffer = audio->updateSize * channels;
		uint32_t wsolaSegmentIdxBefore = audio->wsolaSegmentIdx;

		for (sampleIdx = 0; sampleIdx < samplesInBuffer; sampleIdx++) {
			uint8_t isSilent = (output[sampleIdx] == 0.0f);

			if (audio->wsolaState == 0) {
				/* STATE 0: NORMAL - Accumulate valid audio and track last segment */
				if (!isSilent) {
					if (audio->wsolaIntentionalSilence) {
						audio->wsolaIntentionalSilence = 0;
						audio->wsolaSilenceDurationSamples = 0;
						audio->wsolaConcealmentDurationSamples = 0;
						LOG_INFO(audio, "%s", "WSOLA: Intentional silence ended, resuming normal tracking");
					}

					/* Optional short release crossfade after short-gap concealment */
					if (audio->wsolaInCrossfade) {
						uint32_t shortCrossfadeSamples = channels * 48; /* ~1ms at 48kHz */
						float fadeFactor;

						if (shortCrossfadeSamples > 0 && audio->wsolaCrossfadeIdx < shortCrossfadeSamples) {
							fadeFactor = (float) audio->wsolaCrossfadeIdx / (float) shortCrossfadeSamples;
							output[sampleIdx] = (audio->wsolaCurrentSynthSample * (1.0f - fadeFactor)) + (output[sampleIdx] * fadeFactor);
							audio->wsolaCrossfadeIdx += 1;
							if (audio->wsolaCrossfadeIdx >= shortCrossfadeSamples) {
								audio->wsolaInCrossfade = 0;
							}
						} else {
							audio->wsolaInCrossfade = 0;
						}
					}

					/* Store in history buffer */
					audio->historyBuffer[audio->historyWriteIdx] = output[sampleIdx];
					audio->historyWriteIdx = (audio->historyWriteIdx + 1) % audio->historyMax;
					audio->wsolaHasValidHistory = 1;
					audio->wsolaValidRunSamples = 0;
					
					/* Also store in Active segment buffer */
					if (audio->wsolaSegmentIdx < audio->wsolaWindowSize) {
						audio->wsolaLastValidSegment_Active[audio->wsolaSegmentIdx] = output[sampleIdx];
						audio->wsolaSegmentIdx += 1;
					}
				} else {
					if (!audio->wsolaHasValidHistory) {
						/* Starting from silence: stay silent until we have real audio history. */
						audio->wsolaSilenceDurationSamples = 0;
						audio->wsolaConcealmentDurationSamples = 0;
						audio->wsolaSynthesisIdx = 0;
						audio->wsolaInCrossfade = 0;
						continue;
					}

					if (audio->wsolaIntentionalSilence) {
						/* Intentional long silence: keep it silent until valid audio returns. */
						continue;
					}

					/* Silence detected: check if we have a complete 20ms snapshot */
					if (audio->wsolaSegmentCountdownMs >= 20 && audio->wsolaSegmentIdx >= audio->wsolaWindowSize) {
						/* Complete 20ms accumulated: snapshot it */
						FAudio_memcpy(
							audio->wsolaLastValidSegment_Snapshot,
							audio->wsolaLastValidSegment_Active,
							audio->wsolaWindowSize * sizeof(float)
						);
					} else {
						/* Incomplete snapshot: pad with zeros and take what we have */
						FAudio_zero(audio->wsolaLastValidSegment_Snapshot, audio->wsolaWindowSize * sizeof(float));
						if (audio->wsolaSegmentIdx > 0) {
							FAudio_memcpy(
								audio->wsolaLastValidSegment_Snapshot,
								audio->wsolaLastValidSegment_Active,
								audio->wsolaSegmentIdx * sizeof(float)
							);
						}
						LOG_INFO(audio, "WSOLA: Partial snapshot (only %u samples of 1920)", audio->wsolaSegmentIdx);
					}
					audio->wsolaSilenceDurationSamples = 0;
					audio->wsolaConcealmentDurationSamples = 0;
					audio->wsolaValidRunSamples = 0;
					audio->wsolaInCrossfade = 0;
					audio->wsolaCrossfadeIdx = 0;
					audio->wsolaSynthesisIdx = 0;
					audio->wsolaSynthesisStartPos = audio->historyWriteIdx;
					audio->wsolaBestOffset = 0;
					audio->wsolaState = 1;
					LOG_INFO(audio, "%s", "WSOLA: Silence detected, entering State 1 (detecting)");
				}
			} 
			else if (audio->wsolaState == 1) {
				/* STATE 1: DETECTING SILENCE - Accumulate only actual silence */
				if (isSilent) {
					audio->wsolaValidRunSamples = 0;

					if (!audio->wsolaHasValidHistory) {
						output[sampleIdx] = 0.0f;
						audio->wsolaState = 0;
						continue;
					}

					if (audio->wsolaConcealmentDurationSamples >= maxConcealmentSamples) {
						audio->wsolaState = 0;
						audio->wsolaIntentionalSilence = 1;
						audio->wsolaSilenceDurationSamples = 0;
						audio->wsolaSynthesisIdx = 0;
						audio->wsolaInCrossfade = 0;
						audio->wsolaCrossfadeIdx = 0;
						audio->wsolaCurrentSynthSample = 0.0f;
						output[sampleIdx] = 0.0f;
						LOG_INFO(audio, "%s", "WSOLA: Concealment exceeded 50ms, treating silence as intentional");
						continue;
					}

					/*
					 * Hybrid concealment for short holes:
					 *  - < 3ms: simple interpolation from last valid sample
					 *  - 3ms..20ms: short WSOLA/mini-OLA using history-only overlap
					 *  - >= 20ms: existing long WSOLA path below
					 */
					uint32_t interpSamples = channels * 144;     /* 3ms @ 48kHz */
					uint32_t shortWindowSamples = channels * 480; /* 10ms @ 48kHz */
					uint32_t lastValidPos = (audio->historyWriteIdx + audio->historyMax - 1) % audio->historyMax;
					uint32_t silenceProgress = audio->wsolaSilenceDurationSamples;

					if (silenceProgress == 0) {
						LOG_INFO(audio, "%s", "WSOLA-HYBRID: Entering <3ms interpolation branch");
					}

					if (silenceProgress < interpSamples) {
						uint32_t interpReadPos = (
							audio->historyWriteIdx +
							audio->historyMax -
							(interpSamples % audio->historyMax) +
							(silenceProgress % interpSamples)
						) % audio->historyMax;
						float interpFactor = (interpSamples > 0) ?
							FAudio_min(1.0f, (float) silenceProgress / (float) interpSamples) :
							1.0f;
						float lastValidSample = audio->historyBuffer[lastValidPos];
						float interpSample = audio->historyBuffer[interpReadPos];
						audio->wsolaCurrentSynthSample = (lastValidSample * (1.0f - interpFactor)) + (interpSample * interpFactor);
						output[sampleIdx] = audio->wsolaCurrentSynthSample;
					} else {
						uint32_t shortPosInWindow;
						uint32_t readPos;
						uint32_t baseReadPos;
						uint32_t overlapPos;
						uint32_t shortWinIdx;
						uint32_t hannIdx;
						float histA;
						float histB;
						float overlapWeight;

						if (silenceProgress == interpSamples) {
							LOG_INFO(audio, "%s", "WSOLA-HYBRID: Switching to 3-20ms short WSOLA branch");
						}

						/* Seed short WSOLA once when entering the 3-20ms bracket. */
						if (audio->wsolaSynthesisIdx == 0) {
							uint32_t i;
							uint32_t targetStart = (
								audio->historyWriteIdx +
								audio->historyMax -
								(shortWindowSamples % audio->historyMax)
							) % audio->historyMax;

							for (i = 0; i < shortWindowSamples; i++) {
								audio->wsolaLastValidSegment_Snapshot[i] = audio->historyBuffer[(targetStart + i) % audio->historyMax];
							}

							audio->wsolaBestOffset = FAudio_INTERNAL_FindBestSegment(
								audio,
								audio->wsolaLastValidSegment_Snapshot,
								shortWindowSamples
							);
							audio->wsolaSynthesisStartPos = audio->historyWriteIdx;
						}

						shortPosInWindow = (silenceProgress - interpSamples) % shortWindowSamples;
						baseReadPos = (
							audio->wsolaSynthesisStartPos +
							audio->historyMax -
							(audio->wsolaBestOffset % audio->historyMax)
						) % audio->historyMax;
						readPos = (baseReadPos + shortPosInWindow) % audio->historyMax;
						overlapPos = (
							audio->historyWriteIdx +
							audio->historyMax -
							(shortWindowSamples % audio->historyMax) +
							shortPosInWindow
						) % audio->historyMax;

						shortWinIdx = shortPosInWindow % shortWindowSamples;
						hannIdx = (shortWindowSamples > 0) ?
							(shortWinIdx * (audio->wsolaWindowSize - 1)) / shortWindowSamples :
							0;

						histA = audio->historyBuffer[overlapPos];
						histB = audio->historyBuffer[readPos];
						overlapWeight = audio->wsolaHannWindow[hannIdx];

						audio->wsolaCurrentSynthSample = (histA * (1.0f - overlapWeight)) + (histB * overlapWeight);
						output[sampleIdx] = audio->wsolaCurrentSynthSample;
						audio->wsolaSynthesisIdx += 1;
						if (audio->wsolaSynthesisIdx >= shortWindowSamples) {
							audio->wsolaSynthesisIdx = 0;
						}
					}

					audio->wsolaSilenceDurationSamples += 1;
					audio->wsolaConcealmentDurationSamples += 1;
					
					/* When 20ms of silence accumulated, start synthesis */
					if (audio->wsolaSilenceDurationSamples >= audio->wsolaWindowSize) {
						/* Find best matching segment in history */
						uint32_t bestOffset = FAudio_INTERNAL_FindBestSegment(
							audio,
							audio->wsolaLastValidSegment_Snapshot,
							audio->wsolaWindowSize
						);
						
						/* Snapshot synthesis parameters */
						audio->wsolaSynthesisStartPos = audio->historyWriteIdx;
						audio->wsolaBestOffset = bestOffset;
						audio->wsolaSynthesisIdx = 0;
						audio->wsolaInCrossfade = 0;
						audio->wsolaCrossfadeIdx = 0;
						audio->wsolaState = 2;
						
						LOG_INFO(
							audio,
							"WSOLA: Accumulated 20ms silence, entering State 2 (synthesizing, offset=%u)",
							bestOffset
						);
					}
				} else {
					/*
					 * Valid resumed, but only exit concealment after a stable run.
					 * This avoids audible patchwork from rapid valid/silent alternations.
					 */
					audio->wsolaValidRunSamples += 1;
					if (audio->wsolaValidRunSamples < resumeConfirmSamples) {
						if (audio->wsolaValidRunSamples == 1) {
							LOG_INFO(audio, "%s", "WSOLA-HYBRID: Suppressing short valid island, keeping concealment");
						}

						if (audio->wsolaConcealmentDurationSamples >= maxConcealmentSamples) {
							audio->wsolaState = 0;
							audio->wsolaIntentionalSilence = 1;
							audio->wsolaSilenceDurationSamples = 0;
							audio->wsolaSynthesisIdx = 0;
							audio->wsolaInCrossfade = 0;
							audio->wsolaCrossfadeIdx = 0;
							audio->wsolaCurrentSynthSample = 0.0f;
							output[sampleIdx] = 0.0f;
							LOG_INFO(audio, "%s", "WSOLA: Concealment exceeded 50ms while suppressing island, treating silence as intentional");
							continue;
						}

						output[sampleIdx] = audio->wsolaCurrentSynthSample;
						audio->wsolaSilenceDurationSamples += 1;
						audio->wsolaConcealmentDurationSamples += 1;

						if (audio->wsolaSilenceDurationSamples >= audio->wsolaWindowSize) {
							uint32_t bestOffset = FAudio_INTERNAL_FindBestSegment(
								audio,
								audio->wsolaLastValidSegment_Snapshot,
								audio->wsolaWindowSize
							);

							audio->wsolaSynthesisStartPos = audio->historyWriteIdx;
							audio->wsolaBestOffset = bestOffset;
							audio->wsolaSynthesisIdx = 0;
							audio->wsolaInCrossfade = 0;
							audio->wsolaCrossfadeIdx = 0;
							audio->wsolaValidRunSamples = 0;
							audio->wsolaState = 2;

							LOG_INFO(
								audio,
								"WSOLA: Accumulated 20ms (with suppressed islands), entering State 2 (synthesizing, offset=%u)",
								bestOffset
							);
						}
						continue;
					}

					/* Gap ended after stable valid run: abort concealment and resume normal tracking */
					if (audio->wsolaSilenceDurationSamples > 0) {
						uint32_t gapMs = (audio->wsolaConcealmentDurationSamples * 1000) / (channels * 48000);
						if (audio->wsolaSilenceDurationSamples < (channels * 144)) {
							LOG_INFO(audio, "WSOLA-HYBRID: Gap ended in interpolation branch (%u ms)", gapMs);
						} else {
							LOG_INFO(audio, "WSOLA-HYBRID: Gap ended in short WSOLA branch (%u ms)", gapMs);
						}

						/* Start short release crossfade (concealed -> valid) */
						uint32_t shortCrossfadeSamples = channels * 48; /* ~1ms at 48kHz */
						float fadeFactor = 0.0f;
						audio->wsolaInCrossfade = 1;
						audio->wsolaCrossfadeIdx = 0;
						if (shortCrossfadeSamples > 0) {
							fadeFactor = (float) audio->wsolaCrossfadeIdx / (float) shortCrossfadeSamples;
							output[sampleIdx] = (audio->wsolaCurrentSynthSample * (1.0f - fadeFactor)) + (output[sampleIdx] * fadeFactor);
							audio->wsolaCrossfadeIdx = 1;
							if (audio->wsolaCrossfadeIdx >= shortCrossfadeSamples) {
								audio->wsolaInCrossfade = 0;
							}
						}
					}

					audio->wsolaState = 0;
					audio->wsolaSilenceDurationSamples = 0;
					audio->wsolaConcealmentDurationSamples = 0;
					audio->wsolaValidRunSamples = 0;
					audio->wsolaSynthesisIdx = 0;
					
					/* Process current valid sample in normal path so it is not lost */
					audio->historyBuffer[audio->historyWriteIdx] = output[sampleIdx];
					audio->historyWriteIdx = (audio->historyWriteIdx + 1) % audio->historyMax;
					if (audio->wsolaSegmentIdx < audio->wsolaWindowSize) {
						audio->wsolaLastValidSegment_Active[audio->wsolaSegmentIdx] = output[sampleIdx];
						audio->wsolaSegmentIdx += 1;
					}
				}
			}
			
			if (audio->wsolaState == 2) {
				/* STATE 2: SYNTHESIZING - Fill gap with pattern-matched audio */
				if (isSilent) {
					audio->wsolaValidRunSamples = 0;
				}

				if (isSilent && audio->wsolaConcealmentDurationSamples >= maxConcealmentSamples) {
					audio->wsolaState = 0;
					audio->wsolaIntentionalSilence = 1;
					audio->wsolaSilenceDurationSamples = 0;
					audio->wsolaValidRunSamples = 0;
					audio->wsolaSynthesisIdx = 0;
					audio->wsolaInCrossfade = 0;
					audio->wsolaCrossfadeIdx = 0;
					audio->wsolaCurrentSynthSample = 0.0f;
					output[sampleIdx] = 0.0f;
					LOG_INFO(audio, "%s", "WSOLA: Long synthesis exceeded 50ms, switching to intentional silence");
					continue;
				}
				
				/* Calculate fixed read position from history */
				uint32_t readPos = (
					audio->wsolaSynthesisStartPos +
					audio->historyMax -
					(audio->wsolaBestOffset % audio->historyMax) +
					audio->wsolaSynthesisIdx
				) % audio->historyMax;
				float histSample = audio->historyBuffer[readPos];
				float windowVal = audio->wsolaHannWindow[audio->wsolaSynthesisIdx % audio->wsolaWindowSize];
				
				/* Apply Hann window to synthesized sample */
				audio->wsolaCurrentSynthSample = histSample * windowVal;
				
				/* Check if valid audio has resumed */
				if (!isSilent && !audio->wsolaInCrossfade) {
					audio->wsolaValidRunSamples += 1;
					if (audio->wsolaValidRunSamples >= resumeConfirmSamples) {
						/* Start crossfade from synthesis to valid audio */
						audio->wsolaInCrossfade = 1;
						audio->wsolaCrossfadeIdx = 0;
						audio->wsolaValidRunSamples = 0;
						LOG_INFO(audio, "%s", "WSOLA: Stable valid audio resumed, starting crossfade");
					} else {
						if (audio->wsolaValidRunSamples == 1) {
							LOG_INFO(audio, "%s", "WSOLA: Suppressing short valid island during long synthesis");
						}
						output[sampleIdx] = audio->wsolaCurrentSynthSample;
						audio->wsolaConcealmentDurationSamples += 1;
						audio->wsolaSynthesisIdx++;
						continue;
					}
				}
				
				if (audio->wsolaInCrossfade) {
					/* Crossfade: fade from synthesis ? valid audio over 10ms (960 samples) */
					uint32_t crossfadeSamples = 960;
					float fadeFactor = (float)audio->wsolaCrossfadeIdx / (float)crossfadeSamples;
					
					/* Blend synthesized + valid */
					output[sampleIdx] = (audio->wsolaCurrentSynthSample * (1.0f - fadeFactor)) 
					                   + (output[sampleIdx] * fadeFactor);
					
					audio->wsolaCrossfadeIdx++;
					
					/* Crossfade complete? Return to State 0 */
					if (audio->wsolaCrossfadeIdx >= crossfadeSamples) {
						audio->wsolaState = 0;
						audio->wsolaSegmentIdx = 0;
						audio->wsolaSegmentCountdownMs = 0;
						audio->wsolaInCrossfade = 0;
						LOG_INFO(audio, "%s", "WSOLA: Crossfade complete, returning to State 0");
						
						/* Update history with valid audio */
						audio->historyBuffer[audio->historyWriteIdx] = output[sampleIdx];
						audio->historyWriteIdx = (audio->historyWriteIdx + 1) % audio->historyMax;
						
						/* Reset segment accumulation completely (will be filled fresh in State 0) */
						FAudio_zero(audio->wsolaLastValidSegment_Active, audio->wsolaWindowSize * sizeof(float));
						audio->wsolaSegmentIdx = 0;
						
						/* Note: Do NOT pre-fill with current sample - let natural State 0 accumulation handle it */
						/* The current valid sample will be added in next cycle's State 0 processing */
					}
				} else {
					/* No crossfade yet: output synthesized sample */
					output[sampleIdx] = audio->wsolaCurrentSynthSample;
					audio->wsolaConcealmentDurationSamples += 1;
					audio->wsolaSynthesisIdx++;
					
					/* After 1 window of synthesis, check if silence continues */
					if (audio->wsolaSynthesisIdx >= audio->wsolaWindowSize) {
						/* Check if current sample is still silent (gap continues) */
						if (isSilent) {
							/* Silence persists: return to State 1 to re-detect and potentially synthesize more */
							audio->wsolaState = 1;
							audio->wsolaSilenceDurationSamples = 0;
							audio->wsolaSynthesisIdx = 0;
							LOG_INFO(audio, "%s", "WSOLA: Completed 1 synthesis window, silence continues - re-detecting");
						} else {
							/* Valid audio resumed after synthesis window - trigger crossfade */
							audio->wsolaInCrossfade = 1;
							audio->wsolaCrossfadeIdx = 0;
							LOG_INFO(audio, "%s", "WSOLA: Synthesis window complete + valid audio, starting crossfade");
						}
					}
				}
			}
		}

		/* Track milliseconds only when valid segment accumulation progressed this callback */
		if (audio->wsolaState == 0 && audio->wsolaSegmentIdx > wsolaSegmentIdxBefore) {
			audio->wsolaSegmentCountdownMs += 10;
			
			/* Every 20ms: snapshot Active ? Snapshot, then reset Active */
			if (audio->wsolaSegmentCountdownMs >= 20) {
				FAudio_memcpy(
					audio->wsolaLastValidSegment_Snapshot,
					audio->wsolaLastValidSegment_Active,
					audio->wsolaWindowSize * sizeof(float)
				);
				FAudio_zero(audio->wsolaLastValidSegment_Active, audio->wsolaWindowSize * sizeof(float));
				audio->wsolaSegmentIdx = 0;
				audio->wsolaSegmentCountdownMs = 0;
				LOG_INFO(audio, "%s", "WSOLA: Snapshotted 20ms valid segment");
			}
		}
	}

	/* Copy to async capture buffer */
	if (audio->captureActive && audio->active) {
		uint32_t outChannels = audio->mixFormat.Format.nChannels;
		uint32_t samplesCount = audio->updateSize * outChannels;
		uint32_t writeIdx, readIdx, availableSpace;
		float *buf = audio->captureBuffer;
		
		FAudio_PlatformLockMutex(audio->captureLock);
		writeIdx = audio->captureWriteIdx;
		readIdx = audio->captureReadIdx;
		
		/* Simple ring buffer "available" calculation */
		if (writeIdx >= readIdx) {
			availableSpace = (48000 * 2 * 10) - writeIdx + readIdx - 1;
		} else {
			availableSpace = readIdx - writeIdx - 1;
		}

		if (availableSpace >= samplesCount) {
			/* Enough space to write */
			if (writeIdx + samplesCount <= (48000 * 2 * 10)) {
				/* Contiguous copy */
				FAudio_memcpy(buf + writeIdx, output, samplesCount * sizeof(float));
				audio->captureWriteIdx = (writeIdx + samplesCount) % (48000 * 2 * 10);
			} else {
				/* Wrap-around copy */
				uint32_t firstPart = (48000 * 2 * 10) - writeIdx;
				uint32_t secondPart = samplesCount - firstPart;
				FAudio_memcpy(buf + writeIdx, output, firstPart * sizeof(float));
				FAudio_memcpy(buf, output + firstPart, secondPart * sizeof(float));
				audio->captureWriteIdx = secondPart;
			}
		}
		FAudio_PlatformUnlockMutex(audio->captureLock);
	}

	LOG_FUNC_EXIT(audio)
}

void FAudio_INTERNAL_ResizeDecodeCache(FAudio *audio, uint32_t samples)
{
	LOG_FUNC_ENTER(audio)
	FAudio_PlatformLockMutex(audio->sourceLock);
	LOG_MUTEX_LOCK(audio, audio->sourceLock)
	if (samples > audio->decodeSamples)
	{
		audio->decodeSamples = samples;
		audio->decodeCache = (float*) audio->pRealloc(
			audio->decodeCache,
			sizeof(float) * audio->decodeSamples
		);
	}
	FAudio_PlatformUnlockMutex(audio->sourceLock);
	LOG_MUTEX_UNLOCK(audio, audio->sourceLock)
	LOG_FUNC_EXIT(audio)
}

void FAudio_INTERNAL_AllocEffectChain(
	FAudioVoice *voice,
	const FAudioEffectChain *pEffectChain
) {
	uint32_t i;

	LOG_FUNC_ENTER(voice->audio)
	voice->effects.state = FAPO_BUFFER_VALID;
	voice->effects.count = pEffectChain->EffectCount;
	if (voice->effects.count == 0)
	{
		LOG_FUNC_EXIT(voice->audio)
		return;
	}

	for (i = 0; i < pEffectChain->EffectCount; i += 1)
	{
		pEffectChain->pEffectDescriptors[i].pEffect->AddRef(pEffectChain->pEffectDescriptors[i].pEffect);
	}

	voice->effects.desc = (FAudioEffectDescriptor*) voice->audio->pMalloc(
		voice->effects.count * sizeof(FAudioEffectDescriptor)
	);
	FAudio_memcpy(
		voice->effects.desc,
		pEffectChain->pEffectDescriptors,
		voice->effects.count * sizeof(FAudioEffectDescriptor)
	);
	#define ALLOC_EFFECT_PROPERTY(prop, type) \
		voice->effects.prop = (type*) voice->audio->pMalloc( \
			voice->effects.count * sizeof(type) \
		); \
		FAudio_zero( \
			voice->effects.prop, \
			voice->effects.count * sizeof(type) \
		);
	ALLOC_EFFECT_PROPERTY(parameters, void*)
	ALLOC_EFFECT_PROPERTY(parameterSizes, uint32_t)
	ALLOC_EFFECT_PROPERTY(parameterUpdates, uint8_t)
	ALLOC_EFFECT_PROPERTY(inPlaceProcessing, uint8_t)
	#undef ALLOC_EFFECT_PROPERTY
	LOG_FUNC_EXIT(voice->audio)
}

void FAudio_INTERNAL_FreeEffectChain(FAudioVoice *voice)
{
	uint32_t i;

	LOG_FUNC_ENTER(voice->audio)
	if (voice->effects.count == 0)
	{
		LOG_FUNC_EXIT(voice->audio)
		return;
	}

	for (i = 0; i < voice->effects.count; i += 1)
	{
		voice->effects.desc[i].pEffect->UnlockForProcess(voice->effects.desc[i].pEffect);
		voice->effects.desc[i].pEffect->Release(voice->effects.desc[i].pEffect);
	}

	voice->audio->pFree(voice->effects.desc);
	voice->audio->pFree(voice->effects.parameters);
	voice->audio->pFree(voice->effects.parameterSizes);
	voice->audio->pFree(voice->effects.parameterUpdates);
	voice->audio->pFree(voice->effects.inPlaceProcessing);
	LOG_FUNC_EXIT(voice->audio)
}

uint32_t FAudio_INTERNAL_VoiceOutputFrequency(
	FAudioVoice *voice,
	const FAudioVoiceSends *pSendList
) {
	uint32_t outSampleRate;
	uint32_t newResampleSamples;
	uint64_t resampleSanityCheck;

	LOG_FUNC_ENTER(voice->audio)

	if ((pSendList == NULL) || (pSendList->SendCount == 0))
	{
		/* When we're deliberately given no sends, use master rate! */
		outSampleRate = voice->audio->master->master.inputSampleRate;
	}
	else
	{
		outSampleRate = pSendList->pSends[0].pOutputVoice->type == FAUDIO_VOICE_MASTER ?
			pSendList->pSends[0].pOutputVoice->master.inputSampleRate :
			pSendList->pSends[0].pOutputVoice->mix.inputSampleRate;
	}
	newResampleSamples = (uint32_t) FAudio_ceil(
		voice->audio->updateSize *
		(double) outSampleRate /
		(double) voice->audio->master->master.inputSampleRate
	);
	if (voice->type == FAUDIO_VOICE_SOURCE)
	{
		if (	(voice->src.resampleSamples != 0) &&
			(newResampleSamples != voice->src.resampleSamples) &&
			(voice->effects.count > 0)	)
		{
			LOG_FUNC_EXIT(voice->audio)
			return FAUDIO_E_INVALID_CALL;
		}
		voice->src.resampleSamples = newResampleSamples;
	}
	else /* (voice->type == FAUDIO_VOICE_SUBMIX) */
	{
		if (	(voice->mix.outputSamples != 0) &&
			(newResampleSamples != voice->mix.outputSamples) &&
			(voice->effects.count > 0)	)
		{
			LOG_FUNC_EXIT(voice->audio)
			return FAUDIO_E_INVALID_CALL;
		}
		voice->mix.outputSamples = newResampleSamples;

		voice->mix.resampleStep = DOUBLE_TO_FIXED((
			(double) voice->mix.inputSampleRate /
			(double) outSampleRate
		));

		/* Because we used ceil earlier, there's a chance that
		 * downsampling submixes will go past the number of samples
		 * available. Sources can do this thanks to padding, but we
		 * don't have that luxury for submixes, so unfortunately we
		 * just have to undo the ceil and turn it into a floor.
		 * -flibit
		 */
		resampleSanityCheck = (
			voice->mix.resampleStep * voice->mix.outputSamples
		) >> FIXED_PRECISION;
		if (resampleSanityCheck > (voice->mix.inputSamples / voice->mix.inputChannels))
		{
			voice->mix.outputSamples -= 1;
		}
	}

	LOG_FUNC_EXIT(voice->audio)
	return 0;
}

const float FAUDIO_INTERNAL_MATRIX_DEFAULTS[8][8][64] =
{
	#include "matrix_defaults.inl"
};

/* PCM Decoding */

void FAudio_INTERNAL_DecodePCM8(
	FAudioVoice *voice,
	FAudioBuffer *buffer,
	float *decodeCache,
	uint32_t samples
) {
	LOG_FUNC_ENTER(voice->audio)
	FAudio_INTERNAL_Convert_U8_To_F32(
		((uint8_t*) buffer->pAudioData) + (
			voice->src.curBufferOffset * voice->src.format->nChannels
		),
		decodeCache,
		samples * voice->src.format->nChannels
	);
	LOG_FUNC_EXIT(voice->audio)
}

void FAudio_INTERNAL_DecodePCM16(
	FAudioVoice *voice,
	FAudioBuffer *buffer,
	float *decodeCache,
	uint32_t samples
) {
	LOG_FUNC_ENTER(voice->audio)
	FAudio_INTERNAL_Convert_S16_To_F32(
		((int16_t*) buffer->pAudioData) + (
			voice->src.curBufferOffset * voice->src.format->nChannels
		),
		decodeCache,
		samples * voice->src.format->nChannels
	);
	LOG_FUNC_EXIT(voice->audio)
}

void FAudio_INTERNAL_DecodePCM24(
	FAudioVoice *voice,
	FAudioBuffer *buffer,
	float *decodeCache,
	uint32_t samples
) {
	uint32_t i, j;
	const uint8_t *buf;
	LOG_FUNC_ENTER(voice->audio)

	/* FIXME: Uh... is this something that can be SIMD-ified? */
	buf = buffer->pAudioData + (
		voice->src.curBufferOffset * voice->src.format->nBlockAlign
	);
	for (i = 0; i < samples; i += 1, buf += voice->src.format->nBlockAlign)
	for (j = 0; j < voice->src.format->nChannels; j += 1)
	{
		*decodeCache++ = ((int32_t) (
			((uint32_t) buf[(j * 3) + 2] << 24) |
			((uint32_t) buf[(j * 3) + 1] << 16) |
			((uint32_t) buf[(j * 3) + 0] << 8)
		) >> 8) / 8388607.0f;
	}

	LOG_FUNC_EXIT(voice->audio)
}

void FAudio_INTERNAL_DecodePCM32(
	FAudioVoice *voice,
	FAudioBuffer *buffer,
	float *decodeCache,
	uint32_t samples
) {
	LOG_FUNC_ENTER(voice->audio)
	FAudio_INTERNAL_Convert_S32_To_F32(
		((int32_t*) buffer->pAudioData) + (
			voice->src.curBufferOffset * voice->src.format->nChannels
		),
		decodeCache,
		samples * voice->src.format->nChannels
	);
	LOG_FUNC_EXIT(voice->audio)
}

void FAudio_INTERNAL_DecodePCM32F(
	FAudioVoice *voice,
	FAudioBuffer *buffer,
	float *decodeCache,
	uint32_t samples
) {
	LOG_FUNC_ENTER(voice->audio)
	FAudio_memcpy(
		decodeCache,
		((float*) buffer->pAudioData) + (
			voice->src.curBufferOffset * voice->src.format->nChannels
		),
		sizeof(float) * samples * voice->src.format->nChannels
	);
	LOG_FUNC_EXIT(voice->audio)
}

/* MSADPCM Decoding */

static inline int16_t FAudio_INTERNAL_ParseNibble(
	uint8_t nibble,
	uint8_t predictor,
	int16_t *delta,
	int16_t *sample1,
	int16_t *sample2
) {
	static const int32_t AdaptionTable[16] =
	{
		230, 230, 230, 230, 307, 409, 512, 614,
		768, 614, 512, 409, 307, 230, 230, 230
	};
	static const int32_t AdaptCoeff_1[7] =
	{
		256, 512, 0, 192, 240, 460, 392
	};
	static const int32_t AdaptCoeff_2[7] =
	{
		0, -256, 0, 64, 0, -208, -232
	};

	int8_t signedNibble;
	int32_t sampleInt;
	int16_t sample;

	signedNibble = (int8_t) nibble;
	if (signedNibble & 0x08)
	{
		signedNibble -= 0x10;
	}

	sampleInt = (
		(*sample1 * AdaptCoeff_1[predictor]) +
		(*sample2 * AdaptCoeff_2[predictor])
	) / 256;
	sampleInt += signedNibble * (*delta);
	sample = FAudio_clamp(sampleInt, -32768, 32767);

	*sample2 = *sample1;
	*sample1 = sample;
	*delta = (int16_t) (AdaptionTable[nibble] * (int32_t) (*delta) / 256);
	if (*delta < 16)
	{
		*delta = 16;
	}
	return sample;
}

#define READ(item, type) \
	item = *((type*) *buf); \
	*buf += sizeof(type);

static inline void FAudio_INTERNAL_DecodeMonoMSADPCMBlock(
	uint8_t **buf,
	int16_t *blockCache,
	uint32_t align
) {
	uint32_t i;

	/* Temp storage for ADPCM blocks */
	uint8_t predictor;
	int16_t delta;
	int16_t sample1;
	int16_t sample2;

	/* Preamble */
	READ(predictor, uint8_t)
	READ(delta, int16_t)
	READ(sample1, int16_t)
	READ(sample2, int16_t)
	align -= 7;

	/* Samples */
	*blockCache++ = sample2;
	*blockCache++ = sample1;
	for (i = 0; i < align; i += 1, *buf += 1)
	{
		*blockCache++ = FAudio_INTERNAL_ParseNibble(
			*(*buf) >> 4,
			predictor,
			&delta,
			&sample1,
			&sample2
		);
		*blockCache++ = FAudio_INTERNAL_ParseNibble(
			*(*buf) & 0x0F,
			predictor,
			&delta,
			&sample1,
			&sample2
		);
	}
}

static inline void FAudio_INTERNAL_DecodeStereoMSADPCMBlock(
	uint8_t **buf,
	int16_t *blockCache,
	uint32_t align
) {
	uint32_t i;

	/* Temp storage for ADPCM blocks */
	uint8_t l_predictor;
	uint8_t r_predictor;
	int16_t l_delta;
	int16_t r_delta;
	int16_t l_sample1;
	int16_t r_sample1;
	int16_t l_sample2;
	int16_t r_sample2;

	/* Preamble */
	READ(l_predictor, uint8_t)
	READ(r_predictor, uint8_t)
	READ(l_delta, int16_t)
	READ(r_delta, int16_t)
	READ(l_sample1, int16_t)
	READ(r_sample1, int16_t)
	READ(l_sample2, int16_t)
	READ(r_sample2, int16_t)
	align -= 14;

	/* Samples */
	*blockCache++ = l_sample2;
	*blockCache++ = r_sample2;
	*blockCache++ = l_sample1;
	*blockCache++ = r_sample1;
	for (i = 0; i < align; i += 1, *buf += 1)
	{
		*blockCache++ = FAudio_INTERNAL_ParseNibble(
			*(*buf) >> 4,
			l_predictor,
			&l_delta,
			&l_sample1,
			&l_sample2
		);
		*blockCache++ = FAudio_INTERNAL_ParseNibble(
			*(*buf) & 0x0F,
			r_predictor,
			&r_delta,
			&r_sample1,
			&r_sample2
		);
	}
}

#undef READ

void FAudio_INTERNAL_DecodeMonoMSADPCM(
	FAudioVoice *voice,
	FAudioBuffer *buffer,
	float *decodeCache,
	uint32_t samples
) {
	/* Loop variables */
	uint32_t copy, done = 0;

	/* Read pointers */
	uint8_t *buf;
	int32_t midOffset;

	/* PCM block cache */
	int16_t *blockCache;

	/* Block size */
	uint32_t bsize = ((FAudioADPCMWaveFormat*) voice->src.format)->wSamplesPerBlock;

	LOG_FUNC_ENTER(voice->audio)

	/* Where are we starting? */
	buf = (uint8_t*) buffer->pAudioData + (
		(voice->src.curBufferOffset / bsize) *
		voice->src.format->nBlockAlign
	);

	/* Are we starting in the middle? */
	midOffset = (voice->src.curBufferOffset % bsize);

	/* Read in each block directly to the decode cache */
	blockCache = (int16_t*) FAudio_alloca(bsize * sizeof(int16_t));
	while (done < samples)
	{
		copy = FAudio_min(samples - done, bsize - midOffset);
		FAudio_INTERNAL_DecodeMonoMSADPCMBlock(
			&buf,
			blockCache,
			voice->src.format->nBlockAlign
		);
		FAudio_INTERNAL_Convert_S16_To_F32(
			blockCache + midOffset,
			decodeCache,
			copy
		);
		decodeCache += copy;
		done += copy;
		midOffset = 0;
	}
	FAudio_dealloca(blockCache);
	LOG_FUNC_EXIT(voice->audio)
}

void FAudio_INTERNAL_DecodeStereoMSADPCM(
	FAudioVoice *voice,
	FAudioBuffer *buffer,
	float *decodeCache,
	uint32_t samples
) {
	/* Loop variables */
	uint32_t copy, done = 0;

	/* Read pointers */
	uint8_t *buf;
	int32_t midOffset;

	/* PCM block cache */
	int16_t *blockCache;

	/* Align, block size */
	uint32_t bsize = ((FAudioADPCMWaveFormat*) voice->src.format)->wSamplesPerBlock;

	LOG_FUNC_ENTER(voice->audio)

	/* Where are we starting? */
	buf = (uint8_t*) buffer->pAudioData + (
		(voice->src.curBufferOffset / bsize) *
		voice->src.format->nBlockAlign
	);

	/* Are we starting in the middle? */
	midOffset = (voice->src.curBufferOffset % bsize);

	/* Read in each block directly to the decode cache */
	blockCache = (int16_t*) FAudio_alloca(bsize * 2 * sizeof(int16_t));
	while (done < samples)
	{
		copy = FAudio_min(samples - done, bsize - midOffset);
		FAudio_INTERNAL_DecodeStereoMSADPCMBlock(
			&buf,
			blockCache,
			voice->src.format->nBlockAlign
		);
		FAudio_INTERNAL_Convert_S16_To_F32(
			blockCache + (midOffset * 2),
			decodeCache,
			copy * 2
		);
		decodeCache += copy * 2;
		done += copy;
		midOffset = 0;
	}
	FAudio_dealloca(blockCache);
	LOG_FUNC_EXIT(voice->audio)
}

/* Fallback WMA decoder, get ready for spam! */

void FAudio_INTERNAL_DecodeWMAERROR(
	FAudioVoice *voice,
	FAudioBuffer *buffer,
	float *decodeCache,
	uint32_t samples
) {
	LOG_FUNC_ENTER(voice->audio)
	LOG_ERROR(voice->audio, "%s", "WMA IS NOT SUPPORTED IN THIS BUILD!")
	FAudio_zero(decodeCache, samples * voice->src.format->nChannels * sizeof(float));
	LOG_FUNC_EXIT(voice->audio)
}

/* vim: set noexpandtab shiftwidth=8 tabstop=8: */
