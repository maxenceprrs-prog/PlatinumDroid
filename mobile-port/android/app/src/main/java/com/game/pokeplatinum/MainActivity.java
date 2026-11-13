package com.game.pokeplatinum;

import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.RectF;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.os.Bundle;
import android.util.Log;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.Window;
import android.view.WindowManager;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;

public class MainActivity extends Activity {
    private static final String TAG = "PokePlatinum";

    static {
        System.loadLibrary("pokeplatinum");
    }

    private GameView   gameView;
    private AudioTrack audioTrack;

    /* ---------------------------------------------------------------- */
    /* Lifecycle                                                         */
    /* ---------------------------------------------------------------- */
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        requestWindowFeature(Window.FEATURE_NO_TITLE);
        getWindow().setFlags(
                WindowManager.LayoutParams.FLAG_FULLSCREEN,
                WindowManager.LayoutParams.FLAG_FULLSCREEN);

        /* 1. Extraire les assets (y compris audio_wav) vers le dossier interne */
        copyAssetsToFilesDir(this, "psp-assets");

        /* 2. Init moteur graphique C via JNI */
        File assetDir = new File(getFilesDir(), "psp-assets");
        nativeInit(assetDir.getAbsolutePath());

        /* 3. Init AudioTrack + mixer C */
        initAudio();

        /* 4. Démarrer la musique de fond avec un des WAVs extraits du SDAT */
        startInitialMusic();

        /* 5. Afficher la vue de jeu */
        gameView = new GameView(this);
        setContentView(gameView);
    }

    private void initAudio() {
        try {
            final int SAMPLE_RATE  = 22050;
            final int CHANNEL_CONF = AudioFormat.CHANNEL_OUT_MONO;
            final int ENCODING     = AudioFormat.ENCODING_PCM_16BIT;

            int minBuf = AudioTrack.getMinBufferSize(SAMPLE_RATE, CHANNEL_CONF, ENCODING);
            int bufSize = Math.max(minBuf * 2, 4096 * 2);

            audioTrack = new AudioTrack(
                    AudioManager.STREAM_MUSIC,
                    SAMPLE_RATE,
                    CHANNEL_CONF,
                    ENCODING,
                    bufSize,
                    AudioTrack.MODE_STREAM);

            nativeAudioInit(audioTrack);
            Log.i(TAG, "AudioTrack initialisé avec succès");
        } catch (Exception e) {
            Log.e(TAG, "Erreur initialisation audio", e);
        }
    }

    private void startInitialMusic() {
        try {
            File wavDir = new File(getFilesDir(), "psp-assets/audio_wav");
            if (wavDir.exists() && wavDir.isDirectory()) {
                File[] list = wavDir.listFiles((d, name) -> name.endsWith(".wav"));
                if (list != null && list.length > 0) {
                    File target = list[0];
                    Log.i(TAG, "Lancement audio : " + target.getAbsolutePath());
                    nativeAudioSetSource(target.getAbsolutePath(), true);
                }
            }
        } catch (Exception e) {
            Log.e(TAG, "Erreur démarrage musique", e);
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (gameView != null) gameView.resume();
    }

    @Override
    protected void onPause() {
        super.onPause();
        if (gameView != null) gameView.pause();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        nativeAudioShutdown();
        nativeShutdown();
        if (audioTrack != null) {
            try {
                audioTrack.stop();
                audioTrack.release();
            } catch (Exception ignored) {}
        }
    }

    /* ---------------------------------------------------------------- */
    /* JNI Graphique & Système                                           */
    /* ---------------------------------------------------------------- */
    public static native void nativeInit(String assetRootPath);
    public static native void nativeShutdown();
    public static native void nativeUpdate(int inputFlags, int touchX, int touchY, boolean touchDown);
    public static native void nativeRender(int[] mainPixels, int[] subPixels);

    /* ---------------------------------------------------------------- */
    /* JNI Audio                                                         */
    /* ---------------------------------------------------------------- */
    public static native void nativeAudioInit(AudioTrack track);
    public static native void nativeAudioShutdown();
    public static native void nativeAudioSetSource(String wavPath, boolean loop);

    /* ---------------------------------------------------------------- */
    /* Copie récursive des assets                                       */
    /* ---------------------------------------------------------------- */
    private static void copyAssetsToFilesDir(Context context, String path) {
        try {
            String[] list = context.getAssets().list(path);
            if (list == null || list.length == 0) {
                copySingleFile(context, path);
            } else {
                File dir = new File(context.getFilesDir(), path);
                if (!dir.exists()) dir.mkdirs();
                for (String file : list) {
                    copyAssetsToFilesDir(context, path + "/" + file);
                }
            }
        } catch (Exception e) {
            Log.e(TAG, "Erreur copie assets " + path, e);
        }
    }

    private static void copySingleFile(Context context, String srcPath) {
        File outFile = new File(context.getFilesDir(), srcPath);
        if (outFile.exists() && outFile.length() > 0) return;
        outFile.getParentFile().mkdirs();
        try (InputStream in  = context.getAssets().open(srcPath);
             OutputStream out = new FileOutputStream(outFile)) {
            byte[] buf = new byte[8192];
            int read;
            while ((read = in.read(buf)) != -1) out.write(buf, 0, read);
        } catch (Exception e) {
            Log.e(TAG, "Erreur copie fichier " + srcPath, e);
        }
    }

    /* ================================================================ */
    /* GameView — Rendu double-écran DS + Joystick virtuel analogique    */
    /* ================================================================ */
    static class GameView extends SurfaceView implements Runnable {

        private Thread           renderThread = null;
        private volatile boolean running      = false;
        private final SurfaceHolder holder;

        /* Bitmaps Nintendo DS (256x192 chacun) */
        private final Bitmap topBitmap;
        private final Bitmap bottomBitmap;
        private final int[]  topPixels = new int[256 * 192];
        private final int[]  subPixels = new int[256 * 192];

        /* Entrées */
        private int     inputFlags = 0;
        private int     touchX     = 0, touchY = 0;
        private boolean touchDown  = false;

        /* Constantes de boutons identiques à pal_input.h */
        static final int PAL_BUTTON_A      = 1 << 0;
        static final int PAL_BUTTON_B      = 1 << 1;
        static final int PAL_BUTTON_SELECT = 1 << 2;
        static final int PAL_BUTTON_START  = 1 << 3;
        static final int PAL_BUTTON_RIGHT  = 1 << 4;
        static final int PAL_BUTTON_LEFT   = 1 << 5;
        static final int PAL_BUTTON_UP     = 1 << 6;
        static final int PAL_BUTTON_DOWN   = 1 << 7;
        static final int PAL_BUTTON_R      = 1 << 8;
        static final int PAL_BUTTON_L      = 1 << 9;
        static final int PAL_BUTTON_X      = 1 << 10;
        static final int PAL_BUTTON_Y      = 1 << 11;

        /* Joystick virtuel analogique */
        private float stickBaseX = 0, stickBaseY = 0;
        private float stickKnobX = 0, stickKnobY = 0;
        private float stickRadius = 0;
        private boolean stickActive = false;

        public GameView(Context context) {
            super(context);
            holder       = getHolder();
            topBitmap    = Bitmap.createBitmap(256, 192, Bitmap.Config.ARGB_8888);
            bottomBitmap = Bitmap.createBitmap(256, 192, Bitmap.Config.ARGB_8888);
        }

        public void resume() {
            running = true;
            renderThread = new Thread(this);
            renderThread.start();
        }

        public void pause() {
            running = false;
            try {
                if (renderThread != null) renderThread.join(1000);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }

        /* ------------------------------------------------------------ */
        /* Gestion Multi-Touch complète (Pointeurs multiples)           */
        /* ------------------------------------------------------------ */
        @Override
        public boolean onTouchEvent(MotionEvent event) {
            int actionMasked = event.getActionMasked();
            int pointerCount = event.getPointerCount();

            int w = getWidth();
            int h = getHeight();
            if (w <= 0 || h <= 0) return true;

            float scale   = (float)w / 256.0f;
            int scaledH   = (int)(192.0f * scale);
            int gap       = 8;
            int btnH      = Math.max(120, (int)(h * 0.28f));
            int availH    = h - btnH;
            int totalDSH  = scaledH * 2 + gap;
            int offsetY   = (availH - totalDSH) / 2;
            if (offsetY < 0) offsetY = 0;

            int topY       = offsetY;
            int bottomY    = offsetY + scaledH + gap;
            int bottomEndY = bottomY + scaledH;
            int btnTopY    = h - btnH;

            /* Calcul dimensions joystick et boutons */
            stickBaseX = w * 0.22f;
            stickBaseY = btnTopY + btnH * 0.55f;
            stickRadius = btnH * 0.38f;

            float btnRadius = btnH * 0.17f;
            float btnCenterRx = w * 0.82f;
            float btnCenterRy = btnTopY + btnH * 0.55f;
            float btnSpacing  = btnRadius * 1.55f;

            /* Coordonnées des boutons A, B, X, Y */
            float aX = btnCenterRx + btnSpacing, aY = btnCenterRy;
            float bX = btnCenterRx, bY = btnCenterRy + btnSpacing;
            float xX = btnCenterRx, xY = btnCenterRy - btnSpacing;
            float yX = btnCenterRx - btnSpacing, yY = btnCenterRy;

            /* START / SELECT */
            float selX = w * 0.44f, selY = btnTopY + btnH * 0.82f;
            float staX = w * 0.56f, staY = btnTopY + btnH * 0.82f;
            float smR  = btnH * 0.12f;

            /* L & R Buttons (en haut de la zone de contrôle) */
            float lBtnX = w * 0.15f, lBtnY = btnTopY + btnH * 0.12f;
            float rBtnX = w * 0.85f, rBtnY = btnTopY + btnH * 0.12f;
            float shoulderW = w * 0.14f, shoulderH = btnH * 0.16f;

            /* Recalculer les états à partir de tous les doigts actifs */
            int newFlags = 0;
            boolean newTouchDown = false;
            int newTouchX = 0, newTouchY = 0;
            boolean stickFound = false;

            for (int i = 0; i < pointerCount; i++) {
                if (actionMasked == MotionEvent.ACTION_UP ||
                    actionMasked == MotionEvent.ACTION_CANCEL) {
                    break;
                }
                int actionIndex = event.getActionIndex();
                if ((actionMasked == MotionEvent.ACTION_POINTER_UP) && (i == actionIndex)) {
                    continue; // Doigt en train d'être levé
                }

                float px = event.getX(i);
                float py = event.getY(i);

                /* 1. Écran DS inférieur (stylet tactile) */
                if (py >= bottomY && py <= bottomEndY) {
                    newTouchDown = true;
                    newTouchX = (int)Math.max(0, Math.min(255, (px / w) * 256.0f));
                    newTouchY = (int)Math.max(0, Math.min(191, ((py - bottomY) / (float)scaledH) * 192.0f));
                }
                /* 2. Écran DS supérieur (tap = bouton A d'appoint) */
                else if (py >= topY && py < bottomY) {
                    newFlags |= PAL_BUTTON_A;
                }
                /* 3. Zone manette (boutons et joystick) */
                else if (py >= btnTopY) {
                    /* Joystick tactile (partie gauche) */
                    float distStick = dist(px, py, stickBaseX, stickBaseY);
                    if (distStick <= stickRadius * 1.6f || (stickActive && px < w * 0.45f)) {
                        stickFound = true;
                        float dx = px - stickBaseX;
                        float dy = py - stickBaseY;
                        float len = (float)Math.sqrt(dx * dx + dy * dy);
                        if (len > stickRadius) {
                            dx = (dx / len) * stickRadius;
                            dy = (dy / len) * stickRadius;
                        }
                        stickKnobX = stickBaseX + dx;
                        stickKnobY = stickBaseY + dy;

                        /* Deadzone (25% du rayon) */
                        if (len > stickRadius * 0.25f) {
                            double angle = Math.toDegrees(Math.atan2(dy, dx)); // -180 à 180
                            // 8 directions (angles)
                            if (angle >= -157.5 && angle <= -22.5)  newFlags |= PAL_BUTTON_UP;
                            if (angle >= 22.5   && angle <= 157.5)  newFlags |= PAL_BUTTON_DOWN;
                            if (angle >= 112.5  || angle <= -112.5) newFlags |= PAL_BUTTON_LEFT;
                            if (angle >= -67.5  && angle <= 67.5)   newFlags |= PAL_BUTTON_RIGHT;
                        }
                    }

                    /* Boutons A, B, X, Y */
                    if (dist(px, py, aX, aY) <= btnRadius * 1.3f) newFlags |= PAL_BUTTON_A;
                    if (dist(px, py, bX, bY) <= btnRadius * 1.3f) newFlags |= PAL_BUTTON_B;
                    if (dist(px, py, xX, xY) <= btnRadius * 1.3f) newFlags |= PAL_BUTTON_X;
                    if (dist(px, py, yX, yY) <= btnRadius * 1.3f) newFlags |= PAL_BUTTON_Y;

                    /* Boutons START / SELECT */
                    if (dist(px, py, selX, selY) <= smR * 1.3f) newFlags |= PAL_BUTTON_SELECT;
                    if (dist(px, py, staX, staY) <= smR * 1.3f) newFlags |= PAL_BUTTON_START;

                    /* Gâchettes L / R */
                    if (Math.abs(px - lBtnX) <= shoulderW && Math.abs(py - lBtnY) <= shoulderH) {
                        newFlags |= PAL_BUTTON_L;
                    }
                    if (Math.abs(px - rBtnX) <= shoulderW && Math.abs(py - rBtnY) <= shoulderH) {
                        newFlags |= PAL_BUTTON_R;
                    }
                }
            }

            stickActive = stickFound;
            if (!stickActive) {
                stickKnobX = stickBaseX;
                stickKnobY = stickBaseY;
            }

            inputFlags = newFlags;
            touchDown  = newTouchDown;
            touchX     = newTouchX;
            touchY     = newTouchY;

            return true;
        }

        private static float dist(float x1, float y1, float x2, float y2) {
            float dx = x1 - x2;
            float dy = y1 - y2;
            return (float)Math.sqrt(dx * dx + dy * dy);
        }

        /* ------------------------------------------------------------ */
        /* Boucle de rendu 60 FPS                                       */
        /* ------------------------------------------------------------ */
        @Override
        public void run() {
            Paint paint = new Paint();
            paint.setFilterBitmap(false);

            Paint fillPaint = new Paint();
            fillPaint.setAntiAlias(true);

            Paint strokePaint = new Paint();
            strokePaint.setAntiAlias(true);
            strokePaint.setStyle(Paint.Style.STROKE);

            Paint textPaint = new Paint();
            textPaint.setAntiAlias(true);
            textPaint.setColor(Color.WHITE);
            textPaint.setTextAlign(Paint.Align.CENTER);

            while (running) {
                if (!holder.getSurface().isValid()) {
                    try { Thread.sleep(8); } catch (InterruptedException ignored) {}
                    continue;
                }

                /* 1. Mise à jour moteur C */
                nativeUpdate(inputFlags, touchX, touchY, touchDown);

                /* 2. Rendu pixels C */
                nativeRender(topPixels, subPixels);

                topBitmap.setPixels(   topPixels, 0, 256, 0, 0, 256, 192);
                bottomBitmap.setPixels(subPixels,  0, 256, 0, 0, 256, 192);

                /* 3. Dessin sur Canvas Android */
                Canvas canvas = holder.lockCanvas();
                if (canvas != null) {
                    canvas.drawColor(0xFF101018);

                    int w = getWidth();
                    int h = getHeight();

                    float scale   = (float)w / 256.0f;
                    int scaledH   = (int)(192.0f * scale);
                    int gap       = 8;
                    int btnH      = Math.max(120, (int)(h * 0.28f));
                    int availH    = h - btnH;
                    int totalDSH  = scaledH * 2 + gap;
                    int offsetY   = (availH - totalDSH) / 2;
                    if (offsetY < 0) offsetY = 0;

                    /* Écran DS Haut */
                    Rect dstTop = new Rect(0, offsetY, w, offsetY + scaledH);
                    canvas.drawBitmap(topBitmap, null, dstTop, paint);

                    /* Écran DS Bas (Tactile) */
                    Rect dstBottom = new Rect(0, offsetY + scaledH + gap, w, offsetY + scaledH * 2 + gap);
                    canvas.drawBitmap(bottomBitmap, null, dstBottom, paint);

                    /* Bordures d'écran DS */
                    strokePaint.setColor(0xFF303040);
                    strokePaint.setStrokeWidth(2f);
                    canvas.drawRect(dstTop, strokePaint);
                    canvas.drawRect(dstBottom, strokePaint);

                    /* Zone manette de jeu */
                    int btnTopY = h - btnH;
                    drawControls(canvas, fillPaint, strokePaint, textPaint, w, btnTopY, btnH);

                    holder.unlockCanvasAndPost(canvas);
                }

                try { Thread.sleep(16); } catch (InterruptedException ignored) {}
            }
        }

        /* ------------------------------------------------------------ */
        /* Dessin des contrôles tactiles                                */
        /* ------------------------------------------------------------ */
        private void drawControls(Canvas canvas, Paint fill, Paint stroke, Paint text,
                                  int w, int top, int bh) {
            /* Fond dégradé subtil */
            fill.setColor(0xCC151520);
            canvas.drawRect(0, top, w, top + bh, fill);

            /* 1. Joystick virtuel analogique */
            float jRadius = bh * 0.38f;
            float jBaseX  = w * 0.22f;
            float jBaseY  = top + bh * 0.55f;

            if (!stickActive) {
                stickKnobX = jBaseX;
                stickKnobY = jBaseY;
            }

            // Base extérieure
            fill.setColor(0x44FFFFFF);
            canvas.drawCircle(jBaseX, jBaseY, jRadius, fill);
            stroke.setColor(0x88FFFFFF);
            stroke.setStrokeWidth(3f);
            canvas.drawCircle(jBaseX, jBaseY, jRadius, stroke);

            // Flèches directrices subtiles
            fill.setColor(0x66FFFFFF);
            canvas.drawCircle(jBaseX, jBaseY - jRadius * 0.65f, 5f, fill);
            canvas.drawCircle(jBaseX, jBaseY + jRadius * 0.65f, 5f, fill);
            canvas.drawCircle(jBaseX - jRadius * 0.65f, jBaseY, 5f, fill);
            canvas.drawCircle(jBaseX + jRadius * 0.65f, jBaseY, 5f, fill);

            // Knob central mobile
            fill.setColor(stickActive ? 0xFF00B4D8 : 0xAA90E0EF);
            canvas.drawCircle(stickKnobX, stickKnobY, jRadius * 0.45f, fill);
            stroke.setColor(0xFFFFFFFF);
            stroke.setStrokeWidth(2.5f);
            canvas.drawCircle(stickKnobX, stickKnobY, jRadius * 0.45f, stroke);

            /* 2. Boutons A, B, X, Y */
            float btnRadius = bh * 0.17f;
            float btnCenterRx = w * 0.82f;
            float btnCenterRy = top + bh * 0.55f;
            float btnSpacing  = btnRadius * 1.55f;

            drawButton(canvas, fill, stroke, text, btnCenterRx + btnSpacing, btnCenterRy, btnRadius, "A", 0xFF2A9D8F, (inputFlags & PAL_BUTTON_A) != 0);
            drawButton(canvas, fill, stroke, text, btnCenterRx, btnCenterRy + btnSpacing, btnRadius, "B", 0xFFE76F51, (inputFlags & PAL_BUTTON_B) != 0);
            drawButton(canvas, fill, stroke, text, btnCenterRx, btnCenterRy - btnSpacing, btnRadius, "X", 0xFF457B9D, (inputFlags & PAL_BUTTON_X) != 0);
            drawButton(canvas, fill, stroke, text, btnCenterRx - btnSpacing, btnCenterRy, btnRadius, "Y", 0xFFE9C46A, (inputFlags & PAL_BUTTON_Y) != 0);

            /* 3. Boutons START / SELECT */
            float selX = w * 0.44f, selY = top + bh * 0.82f;
            float staX = w * 0.56f, staY = top + bh * 0.82f;
            float smW  = bh * 0.22f, smH = bh * 0.10f;

            drawCapsule(canvas, fill, stroke, text, selX, selY, smW, smH, "SELECT", (inputFlags & PAL_BUTTON_SELECT) != 0);
            drawCapsule(canvas, fill, stroke, text, staX, staY, smW, smH, "START",  (inputFlags & PAL_BUTTON_START) != 0);

            /* 4. Gâchettes L / R */
            float lBtnX = w * 0.15f, lBtnY = top + bh * 0.12f;
            float rBtnX = w * 0.85f, rBtnY = top + bh * 0.12f;
            float shW   = w * 0.14f, shH   = bh * 0.16f;

            drawShoulder(canvas, fill, stroke, text, lBtnX, lBtnY, shW, shH, "L", (inputFlags & PAL_BUTTON_L) != 0);
            drawShoulder(canvas, fill, stroke, text, rBtnX, rBtnY, shW, shH, "R", (inputFlags & PAL_BUTTON_R) != 0);
        }

        private void drawButton(Canvas canvas, Paint fill, Paint stroke, Paint text,
                                float cx, float cy, float r, String label, int baseColor, boolean pressed) {
            fill.setColor(pressed ? 0xFFFFFFFF : baseColor);
            canvas.drawCircle(cx, cy, r, fill);

            stroke.setColor(pressed ? baseColor : 0xFFFFFFFF);
            stroke.setStrokeWidth(3f);
            canvas.drawCircle(cx, cy, r, stroke);

            text.setColor(pressed ? 0xFF000000 : 0xFFFFFFFF);
            text.setTextSize(r * 0.9f);
            text.setFakeBoldText(true);
            canvas.drawText(label, cx, cy + r * 0.32f, text);
        }

        private void drawCapsule(Canvas canvas, Paint fill, Paint stroke, Paint text,
                                 float cx, float cy, float w, float h, String label, boolean pressed) {
            RectF rect = new RectF(cx - w * 0.5f, cy - h * 0.5f, cx + w * 0.5f, cy + h * 0.5f);
            fill.setColor(pressed ? 0xFFFFFFFF : 0xFF353545);
            canvas.drawRoundRect(rect, h * 0.5f, h * 0.5f, fill);

            stroke.setColor(0x88FFFFFF);
            stroke.setStrokeWidth(2f);
            canvas.drawRoundRect(rect, h * 0.5f, h * 0.5f, stroke);

            text.setColor(pressed ? 0xFF000000 : 0xFFDDDDDD);
            text.setTextSize(h * 0.65f);
            text.setFakeBoldText(true);
            canvas.drawText(label, cx, cy + h * 0.25f, text);
        }

        private void drawShoulder(Canvas canvas, Paint fill, Paint stroke, Paint text,
                                  float cx, float cy, float w, float h, String label, boolean pressed) {
            RectF rect = new RectF(cx - w * 0.5f, cy - h * 0.5f, cx + w * 0.5f, cy + h * 0.5f);
            fill.setColor(pressed ? 0xFFFFFFFF : 0xFF2A2A38);
            canvas.drawRoundRect(rect, 8f, 8f, fill);

            stroke.setColor(0xAAFFFFFF);
            stroke.setStrokeWidth(2f);
            canvas.drawRoundRect(rect, 8f, 8f, stroke);

            text.setColor(pressed ? 0xFF000000 : 0xFFFFFFFF);
            text.setTextSize(h * 0.65f);
            text.setFakeBoldText(true);
            canvas.drawText(label, cx, cy + h * 0.28f, text);
        }
    }
}
