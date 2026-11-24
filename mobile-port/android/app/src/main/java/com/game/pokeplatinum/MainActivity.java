package com.game.pokeplatinum;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.RectF;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;

public class MainActivity extends Activity {
    private static final String TAG = "PokePlatinum";

    static {
        System.loadLibrary("retro");
        System.loadLibrary("pokeplatinum");
    }

    /* Méthodes JNI du vrai cœur melonDS */
    public static native boolean nativeInitEmulator(String romPath, String saveDir);
    public static native boolean nativeRunFrame(int[] topPixels, int[] bottomPixels,
                                               int inputMask, int touchX, int touchY, boolean isTouching);
    public static native int nativeGetAudioSamples(short[] audioBuffer);
    public static native void nativeShutdownEmulator();

    private FrameLayout rootLayout;
    private DSGameView gameView;
    private AudioTrack audioTrack;
    private Thread audioThread;
    private volatile boolean isRunning = false;
    private String loadedRomPath = null;

    private static final int REQUEST_CODE_PICK_ROM = 1001;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        requestWindowFeature(Window.FEATURE_NO_TITLE);
        getWindow().setFlags(
                WindowManager.LayoutParams.FLAG_FULLSCREEN,
                WindowManager.LayoutParams.FLAG_FULLSCREEN);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        /* Permissions stockage */
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.M) {
            try {
                requestPermissions(new String[]{
                        "android.permission.READ_EXTERNAL_STORAGE",
                        "android.permission.WRITE_EXTERNAL_STORAGE",
                        "android.permission.READ_MEDIA_AUDIO",
                        "android.permission.READ_MEDIA_IMAGES",
                        "android.permission.READ_MEDIA_VIDEO"
                }, 100);
            } catch (Exception ignored) {}
        }

        rootLayout = new FrameLayout(this);
        rootLayout.setBackgroundColor(Color.BLACK);
        setContentView(rootLayout);

        /* Vérifier si une ROM existe déjà dans les dossiers standards */
        File appFilesDir = getExternalFilesDir(null);
        String[] possibleRoms = {
                appFilesDir != null ? new File(appFilesDir, "PokemonPlatinum.nds").getAbsolutePath() : null,
                "/sdcard/Android/data/com.game.pokeplatinum/files/PokemonPlatinum.nds",
                "/sdcard/Download/PokemonPlatinum.nds",
                "/sdcard/Download/pokemon_platine.nds",
                "/sdcard/Download/pokemon.nds"
        };

        String foundRom = null;
        for (String p : possibleRoms) {
            if (p == null) continue;
            File f = new File(p);
            Log.i(TAG, "Recherche ROM dans: " + p + " (existe=" + f.exists() + ", taille=" + (f.exists() ? f.length() : 0) + ")");
            if (f.exists() && f.length() > 1024 * 1024) {
                foundRom = p;
                Log.i(TAG, "ROM détectée avec succès: " + p);
                break;
            }
        }

        if (foundRom != null) {
            startEmulatorWithRom(foundRom);
        } else {
            showRomPickerUI();
        }
    }

    private void showRomPickerUI() {
        rootLayout.removeAllViews();

        LinearLayout panel = new LinearLayout(this);
        panel.setOrientation(LinearLayout.VERTICAL);
        panel.setGravity(Gravity.CENTER);
        panel.setPadding(40, 40, 40, 40);

        TextView title = new TextView(this);
        title.setText("Pokémon Platine\n(Cœur melonDS Natif ARM64)");
        title.setTextColor(Color.WHITE);
        title.setTextSize(24);
        title.setGravity(Gravity.CENTER);
        title.setPadding(0, 0, 0, 60);

        Button btnPick = new Button(this);
        btnPick.setText("📂 CHARGER UNE ROM .NDS");
        btnPick.setTextSize(18);
        btnPick.setBackgroundColor(Color.parseColor("#E53935"));
        btnPick.setTextColor(Color.WHITE);
        btnPick.setPadding(40, 30, 40, 30);
        btnPick.setOnClickListener(v -> {
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType("*/*");
            startActivityForResult(intent, REQUEST_CODE_PICK_ROM);
        });

        TextView hint = new TextView(this);
        hint.setText("\nSélectionne ton fichier Pokémon Platine (.nds) légitime.\nLe jeu tournera à 100% sur le vrai cœur Nintendo DS melonDS.");
        hint.setTextColor(Color.LTGRAY);
        hint.setTextSize(14);
        hint.setGravity(Gravity.CENTER);

        panel.addView(title);
        panel.addView(btnPick);
        panel.addView(hint);

        rootLayout.addView(panel, new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT,
                Gravity.CENTER));
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == REQUEST_CODE_PICK_ROM && resultCode == RESULT_OK && data != null) {
            Uri uri = data.getData();
            if (uri != null) {
                try {
                    File dest = new File(getExternalFilesDir(null), "PokemonPlatinum.nds");
                    InputStream in = getContentResolver().openInputStream(uri);
                    OutputStream out = new FileOutputStream(dest);
                    byte[] buf = new byte[65536];
                    int r;
                    while ((r = in.read(buf)) > 0) {
                        out.write(buf, 0, r);
                    }
                    in.close();
                    out.close();
                    startEmulatorWithRom(dest.getAbsolutePath());
                } catch (Exception e) {
                    Log.e(TAG, "Erreur copie ROM", e);
                    Toast.makeText(this, "Erreur chargement ROM: " + e.getMessage(), Toast.LENGTH_LONG).show();
                }
            }
        }
    }

    private void startEmulatorWithRom(String romPath) {
        loadedRomPath = romPath;
        File saveDir = getExternalFilesDir("saves");
        if (saveDir == null) saveDir = getFilesDir();
        saveDir.mkdirs();

        Log.i(TAG, "Démarrage melonDS avec ROM: " + romPath + ", saveDir: " + saveDir.getAbsolutePath());

        boolean ok = nativeInitEmulator(romPath, saveDir.getAbsolutePath());
        if (!ok) {
            Log.e(TAG, "Échec de nativeInitEmulator !");
            Toast.makeText(this, "Échec du chargement de la ROM dans melonDS !", Toast.LENGTH_LONG).show();
            showRomPickerUI();
            return;
        }

        initAudio();

        rootLayout.removeAllViews();
        gameView = new DSGameView(this);
        rootLayout.addView(gameView, new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT));

        isRunning = true;
    }

    private void initAudio() {
        try {
            /* melonDS SPU audio: 32768 Hz stéréo 16-bit PCM */
            final int sampleRate = 32768;
            final int channelConfig = AudioFormat.CHANNEL_OUT_STEREO;
            final int encoding = AudioFormat.ENCODING_PCM_16BIT;

            int minBuf = AudioTrack.getMinBufferSize(sampleRate, channelConfig, encoding);
            int bufSize = Math.max(minBuf * 2, 4096 * 4);

            audioTrack = new AudioTrack(
                    AudioManager.STREAM_MUSIC,
                    sampleRate,
                    channelConfig,
                    encoding,
                    bufSize,
                    AudioTrack.MODE_STREAM);
            audioTrack.play();

            audioThread = new Thread(() -> {
                short[] pcmBuf = new short[2048];
                while (isRunning) {
                    int samples = nativeGetAudioSamples(pcmBuf);
                    if (samples > 0) {
                        audioTrack.write(pcmBuf, 0, samples);
                    } else {
                        try {
                            Thread.sleep(2);
                        } catch (InterruptedException ignored) {}
                    }
                }
            }, "MelonDS_AudioPump");
            audioThread.setPriority(Thread.MAX_PRIORITY);
            audioThread.start();
            Log.i(TAG, "AudioTrack initialisé avec succès (32768 Hz Stéréo PCM)");
        } catch (Exception e) {
            Log.e(TAG, "Erreur initialisation audio", e);
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
        isRunning = false;
        if (gameView != null) gameView.pause();
        nativeShutdownEmulator();
        if (audioTrack != null) {
            try {
                audioTrack.stop();
                audioTrack.release();
            } catch (Exception ignored) {}
        }
    }

    /* ================================================================ */
    /* DSGameView — Rendu réel des 2 écrans DS + Contrôles tactiles     */
    /* ================================================================ */
    class DSGameView extends SurfaceView implements SurfaceHolder.Callback, Runnable {

        private Thread renderThread = null;
        private volatile boolean threadRunning = false;
        private final SurfaceHolder surfaceHolder;

        /* Bitmaps des deux écrans Nintendo DS natifs (256x192 chacun) */
        private final Bitmap topBitmap;
        private final Bitmap bottomBitmap;
        private final int[] topPixels = new int[256 * 192];
        private final int[] bottomPixels = new int[256 * 192];

        /* Masque d'entrées et coordonnées tactiles */
        private int inputMask = 0;
        private int dsTouchX = 0;
        private int dsTouchY = 0;
        private boolean dsTouching = false;

        /* Définitions des touches DS (identiques au pont C) */
        static final int BTN_A      = 1 << 0;
        static final int BTN_B      = 1 << 1;
        static final int BTN_SELECT = 1 << 2;
        static final int BTN_START  = 1 << 3;
        static final int BTN_RIGHT  = 1 << 4;
        static final int BTN_LEFT   = 1 << 5;
        static final int BTN_UP     = 1 << 6;
        static final int BTN_DOWN   = 1 << 7;
        static final int BTN_R      = 1 << 8;
        static final int BTN_L      = 1 << 9;
        static final int BTN_X      = 1 << 10;
        static final int BTN_Y      = 1 << 11;

        /* Rectangles d'affichage à l'écran */
        private final Rect srcRect = new Rect(0, 0, 256, 192);
        private final RectF dstTopRect = new RectF();
        private final RectF dstBottomRect = new RectF();

        /* Peintures de rendu */
        private final Paint screenPaint = new Paint(Paint.FILTER_BITMAP_FLAG);
        private final Paint bgPaint = new Paint();
        private final Paint ctrlPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Paint ctrlTextPaint = new Paint(Paint.ANTI_ALIAS_FLAG);

        public DSGameView(Context context) {
            super(context);
            surfaceHolder = getHolder();
            surfaceHolder.addCallback(this);

            topBitmap = Bitmap.createBitmap(256, 192, Bitmap.Config.ARGB_8888);
            bottomBitmap = Bitmap.createBitmap(256, 192, Bitmap.Config.ARGB_8888);

            bgPaint.setColor(Color.parseColor("#121212"));

            ctrlPaint.setStyle(Paint.Style.FILL);
            ctrlPaint.setColor(Color.argb(120, 200, 200, 200));

            ctrlTextPaint.setColor(Color.WHITE);
            ctrlTextPaint.setTextSize(36f);
            ctrlTextPaint.setTextAlign(Paint.Align.CENTER);
            ctrlTextPaint.setFakeBoldText(true);
        }

        public void resume() {
            threadRunning = true;
            if (renderThread == null || !renderThread.isAlive()) {
                renderThread = new Thread(this, "MelonDS_RenderLoop");
                renderThread.start();
            }
        }

        public void pause() {
            threadRunning = false;
            if (renderThread != null) {
                try {
                    renderThread.join(1000);
                } catch (InterruptedException ignored) {}
                renderThread = null;
            }
        }

        @Override
        public void surfaceCreated(SurfaceHolder holder) {
            resume();
        }

        @Override
        public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
            computeScreenLayout(width, height);
        }

        @Override
        public void surfaceDestroyed(SurfaceHolder holder) {
            pause();
        }

        private void computeScreenLayout(int w, int h) {
            /* Ratio 4:3 DS = 256 / 192 = 1.333 */
            float targetRatio = 256f / 192f;
            
            /* On réserve la moitié supérieure pour l'écran 1, et la moitié inférieure pour l'écran 2 */
            float screenH = (h * 0.44f);
            float screenW = screenH * targetRatio;
            if (screenW > w * 0.95f) {
                screenW = w * 0.95f;
                screenH = screenW / targetRatio;
            }

            float left = (w - screenW) / 2f;
            dstTopRect.set(left, 10f, left + screenW, 10f + screenH);
            dstBottomRect.set(left, 20f + screenH, left + screenW, 20f + (screenH * 2f));
        }

        @Override
        public void run() {
            final long FRAME_TIME_NS = 1000000000L / 60L; /* 60 FPS */

            while (threadRunning) {
                long startTime = System.nanoTime();

                /* 1. Émulation matérielle NDS : exécution d'une frame melonDS */
                boolean frameOk = nativeRunFrame(topPixels, bottomPixels, inputMask, dsTouchX, dsTouchY, dsTouching);

                if (frameOk) {
                    topBitmap.setPixels(topPixels, 0, 256, 0, 0, 256, 192);
                    bottomBitmap.setPixels(bottomPixels, 0, 256, 0, 0, 256, 192);
                }

                /* 2. Rendu graphique natif sur SurfaceView */
                Canvas canvas = surfaceHolder.lockCanvas();
                if (canvas != null) {
                    try {
                        canvas.drawPaint(bgPaint);

                        /* Dessin direct des 2 vrais framebuffers Nintendo DS */
                        canvas.drawBitmap(topBitmap, srcRect, dstTopRect, screenPaint);
                        canvas.drawBitmap(bottomBitmap, srcRect, dstBottomRect, screenPaint);

                        /* Bordure des écrans DS */
                        Paint borderPaint = new Paint();
                        borderPaint.setColor(Color.DKGRAY);
                        borderPaint.setStyle(Paint.Style.STROKE);
                        borderPaint.setStrokeWidth(3f);
                        canvas.drawRect(dstTopRect, borderPaint);
                        canvas.drawRect(dstBottomRect, borderPaint);

                        /* Rendu des contrôles virtuels transparents */
                        drawOnScreenControls(canvas);
                    } finally {
                        surfaceHolder.unlockCanvasAndPost(canvas);
                    }
                }

                /* 3. Régulation 60 FPS */
                long elapsed = System.nanoTime() - startTime;
                long sleepNs = FRAME_TIME_NS - elapsed;
                if (sleepNs > 0) {
                    try {
                        Thread.sleep(sleepNs / 1000000L, (int) (sleepNs % 1000000L));
                    } catch (InterruptedException ignored) {}
                }
            }
        }

        private void drawOnScreenControls(Canvas canvas) {
            int w = getWidth();
            int h = getHeight();
            if (w == 0 || h == 0) return;

            /* Boutons directionnels D-Pad à gauche */
            float dpadCenterX = w * 0.15f;
            float dpadCenterY = h * 0.82f;
            float btnSize = w * 0.07f;

            drawButton(canvas, dpadCenterX, dpadCenterY - btnSize * 1.5f, btnSize, "▲", (inputMask & BTN_UP) != 0);
            drawButton(canvas, dpadCenterX, dpadCenterY + btnSize * 1.5f, btnSize, "▼", (inputMask & BTN_DOWN) != 0);
            drawButton(canvas, dpadCenterX - btnSize * 1.5f, dpadCenterY, btnSize, "◀", (inputMask & BTN_LEFT) != 0);
            drawButton(canvas, dpadCenterX + btnSize * 1.5f, dpadCenterY, btnSize, "▶", (inputMask & BTN_RIGHT) != 0);

            /* Boutons d'action A, B, X, Y à droite */
            float actionCenterX = w * 0.85f;
            float actionCenterY = h * 0.82f;

            drawButton(canvas, actionCenterX + btnSize * 1.4f, actionCenterY, btnSize, "A", (inputMask & BTN_A) != 0);
            drawButton(canvas, actionCenterX, actionCenterY + btnSize * 1.4f, btnSize, "B", (inputMask & BTN_B) != 0);
            drawButton(canvas, actionCenterX, actionCenterY - btnSize * 1.4f, btnSize, "X", (inputMask & BTN_X) != 0);
            drawButton(canvas, actionCenterX - btnSize * 1.4f, actionCenterY, btnSize, "Y", (inputMask & BTN_Y) != 0);

            /* Triggers L et R en haut */
            drawButton(canvas, w * 0.10f, dstTopRect.top + 30f, btnSize * 1.2f, "L", (inputMask & BTN_L) != 0);
            drawButton(canvas, w * 0.90f, dstTopRect.top + 30f, btnSize * 1.2f, "R", (inputMask & BTN_R) != 0);

            /* Select & Start au centre bas */
            drawButton(canvas, w * 0.40f, h * 0.95f, btnSize * 0.8f, "SEL", (inputMask & BTN_SELECT) != 0);
            drawButton(canvas, w * 0.60f, h * 0.95f, btnSize * 0.8f, "START", (inputMask & BTN_START) != 0);
        }

        private void drawButton(Canvas canvas, float cx, float cy, float r, String label, boolean pressed) {
            ctrlPaint.setColor(pressed ? Color.argb(200, 255, 100, 100) : Color.argb(120, 200, 200, 200));
            canvas.drawCircle(cx, cy, r, ctrlPaint);
            ctrlTextPaint.setTextSize(r * 0.8f);
            canvas.drawText(label, cx, cy + (r * 0.3f), ctrlTextPaint);
        }

        @Override
        public boolean onTouchEvent(MotionEvent event) {
            int actionMasked = event.getActionMasked();
            int pointerCount = event.getPointerCount();

            int mask = 0;
            boolean touchingSub = false;
            int touchX = 0;
            int touchY = 0;

            int w = getWidth();
            int h = getHeight();
            float btnSize = w * 0.07f;
            float dpadCenterX = w * 0.15f;
            float dpadCenterY = h * 0.82f;
            float actionCenterX = w * 0.85f;
            float actionCenterY = h * 0.82f;

            if (actionMasked != MotionEvent.ACTION_UP && actionMasked != MotionEvent.ACTION_CANCEL) {
                for (int i = 0; i < pointerCount; i++) {
                    float px = event.getX(i);
                    float py = event.getY(i);

                    /* 1. Détection du toucher direct sur l'écran tactile du bas (Sub Screen) */
                    if (dstBottomRect.contains(px, py)) {
                        touchingSub = true;
                        float relX = (px - dstBottomRect.left) / dstBottomRect.width();
                        float relY = (py - dstBottomRect.top) / dstBottomRect.height();
                        touchX = (int) (relX * 256f);
                        touchY = (int) (relY * 192f);
                        continue;
                    }

                    /* 2. Boutons directionnels D-Pad */
                    if (Math.hypot(px - dpadCenterX, py - (dpadCenterY - btnSize * 1.5f)) < btnSize * 1.5f) mask |= BTN_UP;
                    if (Math.hypot(px - dpadCenterX, py - (dpadCenterY + btnSize * 1.5f)) < btnSize * 1.5f) mask |= BTN_DOWN;
                    if (Math.hypot(px - (dpadCenterX - btnSize * 1.5f), py - dpadCenterY) < btnSize * 1.5f) mask |= BTN_LEFT;
                    if (Math.hypot(px - (dpadCenterX + btnSize * 1.5f), py - dpadCenterY) < btnSize * 1.5f) mask |= BTN_RIGHT;

                    /* 3. Boutons A, B, X, Y */
                    if (Math.hypot(px - (actionCenterX + btnSize * 1.4f), py - actionCenterY) < btnSize * 1.5f) mask |= BTN_A;
                    if (Math.hypot(px - actionCenterX, py - (actionCenterY + btnSize * 1.4f)) < btnSize * 1.5f) mask |= BTN_B;
                    if (Math.hypot(px - actionCenterX, py - (actionCenterY - btnSize * 1.4f)) < btnSize * 1.5f) mask |= BTN_X;
                    if (Math.hypot(px - (actionCenterX - btnSize * 1.4f), py - actionCenterY) < btnSize * 1.5f) mask |= BTN_Y;

                    /* 4. Triggers L, R */
                    if (Math.hypot(px - (w * 0.10f), py - (dstTopRect.top + 30f)) < btnSize * 1.8f) mask |= BTN_L;
                    if (Math.hypot(px - (w * 0.90f), py - (dstTopRect.top + 30f)) < btnSize * 1.8f) mask |= BTN_R;

                    /* 5. Select, Start */
                    if (Math.hypot(px - (w * 0.40f), py - (h * 0.95f)) < btnSize * 1.2f) mask |= BTN_SELECT;
                    if (Math.hypot(px - (w * 0.60f), py - (h * 0.95f)) < btnSize * 1.2f) mask |= BTN_START;
                }
            }

            inputMask = mask;
            dsTouching = touchingSub;
            dsTouchX = touchX;
            dsTouchY = touchY;

            return true;
        }
    }
}
