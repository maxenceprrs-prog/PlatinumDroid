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

        public GameView(Context context) {
            super(context);
            holder = getHolder();
            topBitmap = Bitmap.createBitmap(256, 192, Bitmap.Config.ARGB_8888);
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
                if (renderThread != null) renderThread.join();
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }

        @Override
        public boolean onTouchEvent(MotionEvent event) {
            int action = event.getActionMasked();
            float x = event.getX();
            float y = event.getY();

            int viewW = getWidth();
            int viewH = getHeight();
            int screenH = (int)(256.0f * ((float)viewW / 256.0f) * (192.0f / 256.0f));
            int gap = 16;
            int totalH = screenH * 2 + gap;
            int offsetY = (viewH - totalH) / 2;
            int bottomY = offsetY + screenH + gap;

            if (y >= bottomY && y <= bottomY + screenH) {
                touchDown = (action == MotionEvent.ACTION_DOWN || action == MotionEvent.ACTION_MOVE);
                touchX = (int)((x / viewW) * 256.0f);
                touchY = (int)(((y - bottomY) / screenH) * 192.0f);
            } else {
                touchDown = false;
                /* Touch sur l'écran du haut ou bord = action A / Start */
                if (action == MotionEvent.ACTION_DOWN) {
                    inputFlags |= 1; /* A button */
                } else if (action == MotionEvent.ACTION_UP) {
                    inputFlags &= ~1;
                }
            }

            if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL) {
                touchDown = false;
            }
            return true;
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

                    holder.unlockCanvasAndPost(canvas);
                }

                try {
                    Thread.sleep(16);
                } catch (InterruptedException ignored) {}
            }
        }
    }
}
