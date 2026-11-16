/**
 * @file pal_audio_android.c
 * @brief Backend audio Android via AudioTrack JNI
 *
 * Architecture :
 *   - Java côté : AudioTrack en mode STREAM, 16-bit, mono/stéréo, 22050 Hz
 *   - C côté    : buffer circulaire 60 ms, thread dédié write()→AudioTrack
 *   - JNI       : nativeAudioWrite(short[] data, int len)
 *
 * Ce fichier implémente la couche PAL audio définie dans pal_audio.h.
 * Il est compilé uniquement pour la cible Android (NDK).
 */

/* jni.h doit être inclus EN PREMIER pour définir JavaVM, JNIEnv, jobject... */
#include <jni.h>
#include "platform/pal_audio.h"
#include "audio_assets.h"
#include "audio_sdat.h"
#include "bgm_synth.h"
#include <android/log.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>

#define LOG_TAG "PokePlatAudio"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

/* ------------------------------------------------------------------ */
/* État audio global                                                    */
/* ------------------------------------------------------------------ */
#define AUDIO_SAMPLE_RATE   22050
#define AUDIO_CHANNELS      1       /* mono — DS natif */
#define AUDIO_BUF_FRAMES    2048    /* frames par write (~93ms à 22050 Hz) */

typedef struct {
    /* Source audio en cours (peut être NULL) */
    const int16_t *src_samples;
    size_t         src_total;   /* total échantillons */
    size_t         src_pos;     /* position lecture courante */
    int            src_loop;    /* boucle ? */
    size_t         src_loop_start; /* en échantillons */

    /* Mix buffer pour le JNI write */
    int16_t        mix_buf[AUDIO_BUF_FRAMES];

    /* Mutex d'accès au src */
    pthread_mutex_t lock;

    /* Contrôle du thread de mixing */
    atomic_int running;
    pthread_t  thread;

    /* Référence Java vers l'objet AudioTrack */
    JavaVM  *jvm;
    jobject  audio_track_obj; /* ref globale */
    jmethodID write_method;
    jmethodID play_method;
    jmethodID stop_method;
    jmethodID flush_method;
    jarray   jbuf; /* ref globale short[] de taille AUDIO_BUF_FRAMES */

    int initialized;
} AudioState;

static AudioState s_audio = {0};

/* ------------------------------------------------------------------ */
/* Thread de mixing                                                     */
/* ------------------------------------------------------------------ */
static void *audio_mix_thread(void *arg) {
    (void)arg;
    JNIEnv *env = NULL;
    JavaVM *jvm = s_audio.jvm;
    if (!jvm) return NULL;

    (*jvm)->AttachCurrentThread(jvm, &env, NULL);
    LOGI("Audio mix thread started");

    while (atomic_load(&s_audio.running)) {
        /* Remplir le mix buffer */
        pthread_mutex_lock(&s_audio.lock);

        int16_t *buf  = s_audio.mix_buf;
        int      need = AUDIO_BUF_FRAMES;

        if (s_audio.src_samples && s_audio.src_total > 0) {
            int written = 0;
            while (written < need) {
                if (s_audio.src_pos >= s_audio.src_total) {
                    if (s_audio.src_loop) {
                        s_audio.src_pos = s_audio.src_loop_start;
                    } else {
                        /* Fin sans boucle : silence */
                        memset(buf + written, 0, (size_t)(need - written) * sizeof(int16_t));
                        written = need;
                        break;
                    }
                }
                size_t avail = s_audio.src_total - s_audio.src_pos;
                size_t copy  = (size_t)(need - written);
                if (copy > avail) copy = avail;
                memcpy(buf + written, s_audio.src_samples + s_audio.src_pos,
                       copy * sizeof(int16_t));
                s_audio.src_pos += copy;
                written += (int)copy;
            }
        } else {
            memset(buf, 0, (size_t)need * sizeof(int16_t));
        }

        pthread_mutex_unlock(&s_audio.lock);

        /* Écriture vers AudioTrack via JNI */
        if (s_audio.audio_track_obj && s_audio.write_method && s_audio.jbuf) {
            (*env)->SetShortArrayRegion(env, (jshortArray)s_audio.jbuf, 0,
                                        AUDIO_BUF_FRAMES, (const jshort *)buf);
            (*env)->CallIntMethod(env, s_audio.audio_track_obj,
                                  s_audio.write_method, s_audio.jbuf,
                                  0, (jint)AUDIO_BUF_FRAMES);
            if ((*env)->ExceptionCheck(env)) {
                (*env)->ExceptionClear(env);
            }
        }
    }

    (*jvm)->DetachCurrentThread(jvm);
    LOGI("Audio mix thread stopped");
    return NULL;
}

/* ------------------------------------------------------------------ */
/* PAL_Audio_Init                                                      */
/* ------------------------------------------------------------------ */
BOOL PAL_Audio_Init(int frequency, int channels) {
    (void)frequency; (void)channels;
    /* L'initialisation réelle se fait via nativeAudioInit() appelé depuis Java */
    pthread_mutex_init(&s_audio.lock, NULL);
    s_audio.initialized = 1;
    LOGI("PAL Audio Init (Android)");
    return TRUE;
}

/* ------------------------------------------------------------------ */
/* PAL_Audio_Shutdown                                                  */
/* ------------------------------------------------------------------ */
void PAL_Audio_Shutdown(void) {
    atomic_store(&s_audio.running, 0);
    if (s_audio.thread) {
        pthread_join(s_audio.thread, NULL);
        s_audio.thread = 0;
    }
    pthread_mutex_lock(&s_audio.lock);
    s_audio.src_samples = NULL;
    pthread_mutex_unlock(&s_audio.lock);
    pthread_mutex_destroy(&s_audio.lock);

    if (s_audio.jvm && s_audio.audio_track_obj) {
        JNIEnv *env = NULL;
        (*s_audio.jvm)->AttachCurrentThread(s_audio.jvm, &env, NULL);
        if (s_audio.stop_method)
            (*env)->CallVoidMethod(env, s_audio.audio_track_obj, s_audio.stop_method);
        if (s_audio.flush_method)
            (*env)->CallVoidMethod(env, s_audio.audio_track_obj, s_audio.flush_method);
        (*env)->DeleteGlobalRef(env, s_audio.audio_track_obj);
        if (s_audio.jbuf)
            (*env)->DeleteGlobalRef(env, s_audio.jbuf);
        (*s_audio.jvm)->DetachCurrentThread(s_audio.jvm);
    }

    memset(&s_audio, 0, sizeof(s_audio));
    LOGI("PAL Audio Shutdown");
}

/* ------------------------------------------------------------------ */
/* PAL_Audio_LoadMusic                                                 */
/* Charge un WAV depuis le disque et retourne une WavBuffer* opaque   */
/* ------------------------------------------------------------------ */
PAL_AudioHandle PAL_Audio_LoadMusic(const char *path) {
    if (!path) return NULL;
    WavBuffer *buf = (WavBuffer *)malloc(sizeof(WavBuffer));
    if (!buf) return NULL;
    *buf = AudioAssets_LoadWav(path);
    if (!buf->samples) {
        free(buf);
        LOGE("PAL_Audio_LoadMusic: failed to load %s", path);
        return NULL;
    }
    LOGI("PAL_Audio_LoadMusic: %s (%d Hz, %d ch, %d frames)",
         path, buf->sample_rate, buf->channels, buf->frames);
    return (PAL_AudioHandle)buf;
}

/* ------------------------------------------------------------------ */
/* PAL_Audio_PlayMusic                                                 */
/* ------------------------------------------------------------------ */
void PAL_Audio_PlayMusic(PAL_AudioHandle music, BOOL loop) {
    if (!music || !s_audio.initialized) return;
    WavBuffer *buf = (WavBuffer *)music;

    pthread_mutex_lock(&s_audio.lock);
    s_audio.src_samples    = buf->samples;
    s_audio.src_total      = buf->num_samples;
    s_audio.src_pos        = 0;
    s_audio.src_loop       = loop ? 1 : 0;
    s_audio.src_loop_start = 0;
    pthread_mutex_unlock(&s_audio.lock);

    LOGI("PAL_Audio_PlayMusic: %zu samples, loop=%d", buf->num_samples, loop);
}

void PAL_Audio_StopMusic(PAL_AudioHandle music) {
    (void)music;
    pthread_mutex_lock(&s_audio.lock);
    s_audio.src_samples = NULL;
    s_audio.src_total   = 0;
    s_audio.src_pos     = 0;
    pthread_mutex_unlock(&s_audio.lock);
}

void PAL_Audio_FreeMusic(PAL_AudioHandle music) {
    if (!music) return;
    WavBuffer *buf = (WavBuffer *)music;
    AudioAssets_Free(buf);
    free(buf);
}

void PAL_Audio_SetVolume(float volume) {
    (void)volume;
}

/* ================================================================== */
/* JNI Entry Points (appelés depuis MainActivity.java)                */
/* ================================================================== */

/**
 * nativeAudioInit(jobject audioTrack)
 * Appelé depuis Java après avoir créé l'AudioTrack.
 * Configure le thread de mixing C → AudioTrack.write().
 */
JNIEXPORT void JNICALL
Java_com_game_pokeplatinum_MainActivity_nativeAudioInit(
        JNIEnv *env, jclass clazz, jobject audio_track) {
    (void)clazz;

    (*env)->GetJavaVM(env, &s_audio.jvm);
    s_audio.audio_track_obj = (*env)->NewGlobalRef(env, audio_track);

    /* Récupérer la méthode write(short[], int, int) → int */
    jclass at_class = (*env)->GetObjectClass(env, audio_track);
    s_audio.write_method = (*env)->GetMethodID(env, at_class,
        "write", "([SII)I");
    s_audio.play_method  = (*env)->GetMethodID(env, at_class,
        "play", "()V");
    s_audio.stop_method  = (*env)->GetMethodID(env, at_class,
        "stop", "()V");
    s_audio.flush_method = (*env)->GetMethodID(env, at_class,
        "flush", "()V");

    /* Buffer Java partagé */
    jshortArray jarr = (*env)->NewShortArray(env, AUDIO_BUF_FRAMES);
    s_audio.jbuf = (*env)->NewGlobalRef(env, jarr);

    /* Démarrer AudioTrack */
    if (s_audio.play_method)
        (*env)->CallVoidMethod(env, s_audio.audio_track_obj, s_audio.play_method);

    /* Démarrer le thread de mixing */
    PAL_Audio_Init(AUDIO_SAMPLE_RATE, AUDIO_CHANNELS);
    atomic_store(&s_audio.running, 1);
    pthread_create(&s_audio.thread, NULL, audio_mix_thread, NULL);

    LOGI("nativeAudioInit: AudioTrack thread started");
}

/**
 * nativeAudioShutdown()
 */
JNIEXPORT void JNICALL
Java_com_game_pokeplatinum_MainActivity_nativeAudioShutdown(
        JNIEnv *env, jclass clazz) {
    (void)env; (void)clazz;
    PAL_Audio_Shutdown();
}

/**
 * nativeAudioSetSource(String wavPath, boolean loop)
 * Change la source audio en cours depuis Java (chargement d'un WAV).
 */
JNIEXPORT void JNICALL
Java_com_game_pokeplatinum_MainActivity_nativeAudioSetSource(
        JNIEnv *env, jclass clazz, jstring wav_path, jboolean loop) {
    (void)clazz;
    const char *path = (*env)->GetStringUTFChars(env, wav_path, NULL);
    PAL_AudioHandle h = PAL_Audio_LoadMusic(path);
    (*env)->ReleaseStringUTFChars(env, wav_path, path);
    if (h) {
        PAL_Audio_PlayMusic(h, loop ? TRUE : FALSE);
        /* Note : WavBuffer non libéré ici — vivra jusqu'au shutdown */
        /* En production, implémenter un gestionnaire de handles */
    }
}

/**
 * nativeAudioPlayBgm(int trackId)
 * Joue une musique synthétisée DS en continu
 */
JNIEXPORT void JNICALL
Java_com_game_pokeplatinum_MainActivity_nativeAudioPlayBgm(
        JNIEnv *env, jclass clazz, jint track_id) {
    (void)env; (void)clazz;
    BgmSynth_PlayTrack((BgmTrack)track_id);
}
