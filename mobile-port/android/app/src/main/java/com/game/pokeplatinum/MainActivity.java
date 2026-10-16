package com.game.pokeplatinum;

import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Rect;
import android.os.Bundle;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;

public class MainActivity extends Activity {
    private static final int PAL_BUTTON_A = 1 << 0;
    private static final int PAL_BUTTON_B = 1 << 1;
    private static final int PAL_BUTTON_START = 1 << 3;
    private static final int PAL_BUTTON_RIGHT = 1 << 4;
    private static final int PAL_BUTTON_LEFT = 1 << 5;
    private static final int PAL_BUTTON_UP = 1 << 6;
    private static final int PAL_BUTTON_DOWN = 1 << 7;

    static {
        System.loadLibrary("pokeplatinum");
    }

    private GameView gameView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        requestWindowFeature(Window.FEATURE_NO_TITLE);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN,
                WindowManager.LayoutParams.FLAG_FULLSCREEN);

        copyAssetsToFilesDir(this, "psp-assets");

        File assetDir = new File(getFilesDir(), "psp-assets");
        nativeInit(assetDir.getAbsolutePath());

        gameView = new GameView(this);
        setContentView(gameView);
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
        nativeShutdown();
    }

    public static native void nativeInit(String assetRootPath);
    public static native void nativeShutdown();
    public static native void nativeUpdate(int inputFlags, int touchX, int touchY, boolean touchDown);
    public static native void nativeRender(int[] mainPixels, int[] subPixels);

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
            e.printStackTrace();
        }
    }

    private static void copySingleFile(Context context, String srcPath) {
        File outFile = new File(context.getFilesDir(), srcPath);
        if (outFile.exists()) return;
        outFile.getParentFile().mkdirs();
        try (InputStream in = context.getAssets().open(srcPath);
             OutputStream out = new FileOutputStream(outFile)) {
            byte[] buf = new byte[8192];
            int read;
            while ((read = in.read(buf)) != -1) {
                out.write(buf, 0, read);
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    static class GameView extends SurfaceView implements Runnable {
        private Thread renderThread = null;
        private volatile boolean running = false;
        private SurfaceHolder holder;

        private Bitmap topBitmap;
        private Bitmap bottomBitmap;
        private int[] topPixels = new int[256 * 192];
        private int[] subPixels = new int[256 * 192];

        private int inputFlags = 0;
        private int touchX = 0, touchY = 0;
        private boolean touchDown = false;

        private final Paint controlFillPaint = new Paint();
        private final Paint controlStrokePaint = new Paint();
        private final Paint controlTextPaint = new Paint();

        public GameView(Context context) {
            super(context);
            holder = getHolder();
            topBitmap = Bitmap.createBitmap(256, 192, Bitmap.Config.ARGB_8888);
            bottomBitmap = Bitmap.createBitmap(256, 192, Bitmap.Config.ARGB_8888);

            controlFillPaint.setColor(Color.argb(90, 40, 80, 180));
            controlFillPaint.setStyle(Paint.Style.FILL);
            controlFillPaint.setAntiAlias(true);

            controlStrokePaint.setColor(Color.argb(180, 200, 220, 255));
            controlStrokePaint.setStyle(Paint.Style.STROKE);
            controlStrokePaint.setStrokeWidth(3f);
            controlStrokePaint.setAntiAlias(true);

            controlTextPaint.setColor(Color.argb(220, 255, 255, 255));
            controlTextPaint.setTextSize(32f);
            controlTextPaint.setTextAlign(Paint.Align.CENTER);
            controlTextPaint.setAntiAlias(true);
        }

        public void resume() {
            running = true;
            renderThread = new Thread(this);
            renderThread.start();
        }

        public void pause() {
            running = false;
            try {
                if (renderThread != null) renderThread.join();
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }

        @Override
        public boolean onTouchEvent(MotionEvent event) {
            updateInputFromTouchEvent(event);
            return true;
        }

        private boolean isPointerActive(MotionEvent event, int pointerIndex) {
            int action = event.getActionMasked();
            int actionIndex = event.getActionIndex();
            if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_POINTER_UP || action == MotionEvent.ACTION_CANCEL) {
                return pointerIndex != actionIndex;
            }
            return true;
        }

        private void updateInputFromTouchEvent(MotionEvent event) {
            int viewW = Math.max(getWidth(), 1);
            int viewH = Math.max(getHeight(), 1);
            float scale = (float) viewW / 256.0f;
            int scaledH = (int) (192.0f * scale);
            int gap = 16;
            int totalH = scaledH * 2 + gap;
            int offsetY = (viewH - totalH) / 2;
            if (offsetY < 0) offsetY = 0;
            int bottomY = offsetY + scaledH + gap;

            int nextFlags = 0;
            boolean nextTouchDown = false;
            int nextTouchX = touchX;
            int nextTouchY = touchY;

            for (int i = 0; i < event.getPointerCount(); i++) {
                if (!isPointerActive(event, i)) continue;
                float x = event.getX(i);
                float y = event.getY(i);

                if (y >= bottomY && y <= bottomY + scaledH) {
                    nextTouchDown = true;
                    nextTouchX = clamp((int) ((x / viewW) * 256.0f), 0, 255);
                    nextTouchY = clamp((int) (((y - bottomY) / scaledH) * 192.0f), 0, 191);
                    continue;
                }

                if (y < viewH * 0.16f) {
                    nextFlags |= PAL_BUTTON_START;
                }

                if (x < viewW * 0.42f && y > viewH * 0.48f) {
                    float cx = viewW * 0.20f;
                    float cy = viewH * 0.76f;
                    float dx = x - cx;
                    float dy = y - cy;
                    if (Math.abs(dx) > Math.abs(dy)) {
                        nextFlags |= (dx < 0.0f) ? PAL_BUTTON_LEFT : PAL_BUTTON_RIGHT;
                    } else {
                        nextFlags |= (dy < 0.0f) ? PAL_BUTTON_UP : PAL_BUTTON_DOWN;
                    }
                }

                if (x > viewW * 0.58f && y > viewH * 0.50f) {
                    if (y < viewH * 0.75f) {
                        nextFlags |= PAL_BUTTON_A;
                    } else {
                        nextFlags |= PAL_BUTTON_B;
                    }
                }
            }

            if (event.getActionMasked() == MotionEvent.ACTION_CANCEL) {
                nextFlags = 0;
                nextTouchDown = false;
            }

            inputFlags = nextFlags;
            touchDown = nextTouchDown;
            touchX = nextTouchX;
            touchY = nextTouchY;
        }

        private int clamp(int value, int min, int max) {
            if (value < min) return min;
            if (value > max) return max;
            return value;
        }

        private void drawControlsOverlay(Canvas canvas, int w, int h) {
            float dpadCx = w * 0.20f;
            float dpadCy = h * 0.76f;
            float dpadR = Math.min(w, h) * 0.085f;
            canvas.drawCircle(dpadCx, dpadCy, dpadR, controlFillPaint);
            canvas.drawCircle(dpadCx, dpadCy, dpadR, controlStrokePaint);

            float aCx = w * 0.80f;
            float aCy = h * 0.67f;
            float bCx = w * 0.70f;
            float bCy = h * 0.80f;
            float btnR = Math.min(w, h) * 0.060f;

            canvas.drawCircle(aCx, aCy, btnR, controlFillPaint);
            canvas.drawCircle(aCx, aCy, btnR, controlStrokePaint);
            canvas.drawText("A", aCx, aCy + 11f, controlTextPaint);

            canvas.drawCircle(bCx, bCy, btnR, controlFillPaint);
            canvas.drawCircle(bCx, bCy, btnR, controlStrokePaint);
            canvas.drawText("B", bCx, bCy + 11f, controlTextPaint);

            float startW = Math.max(120f, w * 0.24f);
            float startH = Math.max(44f, h * 0.06f);
            Rect startRect = new Rect((int) (w * 0.5f - startW / 2f), (int) (h * 0.04f),
                    (int) (w * 0.5f + startW / 2f), (int) (h * 0.04f + startH));
            canvas.drawRect(startRect, controlFillPaint);
            canvas.drawRect(startRect, controlStrokePaint);
            canvas.drawText("START", startRect.exactCenterX(), startRect.exactCenterY() + 11f, controlTextPaint);
        }

        @Override
        public void run() {
            Paint paint = new Paint();
            paint.setFilterBitmap(false);

            while (running) {
                if (!holder.getSurface().isValid()) continue;

                nativeUpdate(inputFlags, touchX, touchY, touchDown);
                nativeRender(topPixels, subPixels);

                topBitmap.setPixels(topPixels, 0, 256, 0, 0, 256, 192);
                bottomBitmap.setPixels(subPixels, 0, 256, 0, 0, 256, 192);

                Canvas canvas = holder.lockCanvas();
                if (canvas != null) {
                    canvas.drawColor(Color.BLACK);

                    int w = getWidth();
                    int h = getHeight();

                    float scale = (float)w / 256.0f;
                    int scaledH = (int)(192.0f * scale);
                    int gap = 16;
                    int totalH = scaledH * 2 + gap;
                    int offsetY = (h - totalH) / 2;
                    if (offsetY < 0) offsetY = 0;

                    Rect dstTop = new Rect(0, offsetY, w, offsetY + scaledH);
                    Rect dstBottom = new Rect(0, offsetY + scaledH + gap, w, offsetY + totalH);

                    canvas.drawBitmap(topBitmap, null, dstTop, paint);
                    canvas.drawBitmap(bottomBitmap, null, dstBottom, paint);
                    drawControlsOverlay(canvas, w, h);

                    holder.unlockCanvasAndPost(canvas);
                }

                try {
                    Thread.sleep(16);
                } catch (InterruptedException ignored) {}
            }
        }
    }
}
